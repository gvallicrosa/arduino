//! BLE controlled WS2812B light.
//!
//! - advertises as `led_light_<figure>` with one custom GATT service
//! - `mode` characteristic (1 byte, read/write): 0 = rainbow, 1 = fixed, 2 = pulsating,
//!   3 = candle, 4 = fade between two colors, 5 = twinkle
//! - `color` characteristic (3 bytes RGB, read/write): color used by every mode but rainbow
//! - `color2`, `brightness` and speed characteristics (read/write), see `LightService` and the
//!   README
//! - a connection with no writes for `IDLE_DISCONNECT` is dropped
//!
//! The LED count and name are chosen per figure with a cargo feature (see `Cargo.toml`).

#![no_std]
#![no_main]

use core::{cell::Cell, ops::RangeInclusive};

use embassy_executor::Spawner;
use embassy_futures::{
    join::join,
    select::{Either, select},
};
use embassy_sync::blocking_mutex::{Mutex, raw::CriticalSectionRawMutex};
use embassy_time::{Duration, Instant, Ticker, Timer};
use esp_alloc as _;
use esp_backtrace as _;
use esp_hal::{
    Async, clock::CpuClock, efuse, interrupt::Priority, rmt::Rmt, time::Rate,
    timer::timg::TimerGroup,
};
use esp_hal::peripherals::{GPIO33, RMT};
use esp_rtos::embassy::InterruptExecutor;
use static_cell::StaticCell;
use esp_hal_smartled::{RmtSmartLeds, WS2812B_TIMING, buffer_size, color_order};
use esp_radio::ble::controller::BleConnector;
use log::{error, info, warn};
use smart_leds::{RGB8, SmartLedsWriteAsync};
use trouble_host::prelude::*;

esp_bootloader_esp_idf::esp_app_desc!();

// ---------------------------------------------------------------------------
// Build configuration
// ---------------------------------------------------------------------------

struct Figure {
    leds: usize,
    /// Bluetooth name, so several figures can be told apart in the phone's device list.
    /// Keep the `led_light` prefix: the web app looks for it. At most 22 bytes.
    name: &'static str,
}

/// The figure, selected with a cargo feature.
const FIGURE: Figure = if cfg!(feature = "duck") {
    Figure { leds: 20, name: "led_light_duck" }
} else if cfg!(feature = "banyetes") {
    Figure { leds: 16, name: "led_light_banyetes" }
} else if cfg!(feature = "tanjiro") {
    Figure { leds: 18, name: "led_light_tanjiro" }
} else if cfg!(feature = "test") {
    Figure { leds: 15, name: "led_light_test" }
} else {
    Figure { leds: 22, name: "led_light_dinosaur" }
};
const LED_NUM: usize = FIGURE.leds;
const BLE_NAME: &str = FIGURE.name;
const _: () = assert!(BLE_NAME.len() <= 22, "trouble-host limits the GAP name to 22 bytes");
const _: () = assert!(
    cfg!(feature = "dinosaur") as u8
        + cfg!(feature = "duck") as u8
        + cfg!(feature = "banyetes") as u8
        + cfg!(feature = "tanjiro") as u8
        + cfg!(feature = "test") as u8
        == 1,
    "enable exactly one figure feature (use --no-default-features --features <figure>)"
);

/// Current available for the LEDs: 500 mA from USB minus ~150 mA for the ESP32 with BLE.
/// Raise it if the figure is powered from something stronger than plain USB.
const LED_BUDGET_MA: u32 = 350;
/// Worst case draw of one LED at full white: 20 mA per channel (WS2812B datasheet).
const LED_FULL_WHITE_MA: u32 = 60;
/// Output scale at 100 % brightness, in 1/65536 of full output: the most `LED_BUDGET_MA` allows
/// with every LED at full white. Brightness is a percentage of this, so no setting can exceed
/// the budget.
///
///   | figure   | LEDs | 100 % = full output x |
///   |----------|------|-----------------------|
///   | test     |  15  | 0.39                  |
///   | banyetes |  16  | 0.36                  |
///   | tanjiro  |  18  | 0.32                  |
///   | duck     |  20  | 0.29                  |
///   | dinosaur |  22  | 0.27                  |
///
/// Conservative: real LEDs often draw 12-15 mA per channel and rainbow mode has at most two
/// channels on.
const BRIGHTNESS_MAX_SCALE: u32 = {
    let s = 65536 * LED_BUDGET_MA / (LED_NUM as u32 * LED_FULL_WHITE_MA);
    if s < 65536 { s } else { 65536 }
};
/// Brightness at boot, in percent of `BRIGHTNESS_MAX_SCALE`. 25 % on the dinosaur is the
/// brightness the firmware used before it was configurable; smaller figures start a bit brighter
/// (same share of their power budget).
const BRIGHTNESS_DEFAULT: u8 = 25;
/// Accepted brightness values, in percent.
const BRIGHTNESS_RANGE: RangeInclusive<u8> = 1..=100;

/// Lowest value reached by the pulsating mode (0-255).
const PULSE_MIN: u8 = 100;
/// Duration of one full pulse (dim -> bright -> dim) at 100 % speed.
const PULSE_PERIOD_MS: u64 = 6_200;
/// Duration of one full trip around the color wheel in rainbow mode at 100 % speed.
const RAINBOW_PERIOD_MS: u64 = 25_600;
/// Part of the color wheel shown along the strip in rainbow mode at boot, in degrees
/// (360 = whole wheel, 0 = all LEDs the same color). The strip is a window over the rainbow that
/// slides by one full wheel per rainbow period.
const RAINBOW_SPAN_DEFAULT: u16 = 90;
const RAINBOW_SPAN_RANGE: RangeInclusive<u16> = 0..=360;
/// Duration of one full fade (first color -> second color -> first color) at 100 % speed.
const FADE_PERIOD_MS: u64 = 12_000;
/// Twinkle mode: LEDs glow at `TWINKLE_MIN` (0-255) and now and then one flares up to full and
/// back down over `TWINKLE_MS`. Each LED waits `TWINKLE_GAP_MS` on average between flares. Both
/// times are for 100 % speed.
const TWINKLE_MIN: u8 = 90;
const TWINKLE_MS: u64 = 2_000;
const TWINKLE_GAP_MS: u64 = 6_000;
/// Candle mode: the flame level (0-255) wanders between `CANDLE_CALM_MIN` and full, with a dip
/// down to `CANDLE_DIP_MIN` for one in `CANDLE_DIP_ONE_IN` new targets. Each LED adds its own
/// small variation, down to `CANDLE_LED_MIN`. Dimmer means redder, like a real flame.
const CANDLE_CALM_MIN: u8 = 225;
const CANDLE_DIP_MIN: u8 = 170;
const CANDLE_DIP_ONE_IN: u32 = 40;
const CANDLE_LED_MIN: u8 = 235;
/// How long the flame and each LED keep a target before picking a new one, in frames.
const CANDLE_FLAME_HOLD: RangeInclusive<u32> = ms_to_frames(40)..=ms_to_frames(160);
const CANDLE_LED_HOLD: RangeInclusive<u32> = ms_to_frames(60)..=ms_to_frames(300);
/// Animation speeds are a percentage of the periods above (200 = twice as fast).
const SPEED_DEFAULT: u16 = 100;
/// Accepted speeds. The upper bound also keeps `cycle_fraction` from overflowing.
const SPEED_RANGE: RangeInclusive<u16> = 10..=1000;
/// Time between LED frames. Kept short (200 Hz) so temporal dithering is not visible.
const FRAME_MS: u64 = 5;

const fn ms_to_frames(ms: u64) -> u32 {
    (ms / FRAME_MS) as u32
}

/// Light service UUID `937312e0-2354-11eb-9f10-fbc30a62cf38`, little-endian for advertising.
const LIGHT_SERVICE_UUID_LE: [u8; 16] = [
    0x38, 0xcf, 0x62, 0x0a, 0xc3, 0xfb, 0x10, 0x9f, 0xeb, 0x11, 0x54, 0x23, 0xe0, 0x12, 0x73, 0x93,
];
const INITIAL_COLOR: [u8; 3] = [255, 0, 0];
/// Second color of the fade mode.
const INITIAL_COLOR2: [u8; 3] = [0, 0, 255];
/// A connection with no writes for this long is dropped. Only one central can be connected at a
/// time, so this keeps a forgotten client (e.g. a browser tab left open) from locking others out.
const IDLE_DISCONNECT: Duration = Duration::from_secs(120);

// ---------------------------------------------------------------------------
// Shared state between BLE and the LED task
// ---------------------------------------------------------------------------

#[derive(Clone, Copy, Debug)]
enum Mode {
    Rainbow,
    Fixed,
    Pulsating,
    Candle,
    Fade,
    Twinkle,
}

impl Mode {
    fn from_byte(b: u8) -> Option<Self> {
        match b {
            0 => Some(Mode::Rainbow),
            1 => Some(Mode::Fixed),
            2 => Some(Mode::Pulsating),
            3 => Some(Mode::Candle),
            4 => Some(Mode::Fade),
            5 => Some(Mode::Twinkle),
            _ => None,
        }
    }
}

/// Everything the BLE client can change. Written by the GATT handler, read by the LED task
/// every frame.
#[derive(Clone, Copy, Debug)]
struct Settings {
    mode: Mode,
    /// Used by every mode but rainbow.
    color: RGB8,
    /// Second color of the fade mode.
    color2: RGB8,
    /// Percent of `BRIGHTNESS_MAX_SCALE`.
    brightness: u8,
    /// Percent of the speed set by `RAINBOW_PERIOD_MS`.
    rainbow_speed: u16,
    /// Degrees of the color wheel shown along the strip.
    rainbow_span: u16,
    /// Percent of the speed set by `PULSE_PERIOD_MS`.
    pulse_speed: u16,
    /// Percent of the speed set by `FADE_PERIOD_MS`.
    fade_speed: u16,
    /// Percent of the speed set by `TWINKLE_MS` and `TWINKLE_GAP_MS`.
    twinkle_speed: u16,
}

const fn rgb8([r, g, b]: [u8; 3]) -> RGB8 {
    RGB8 { r, g, b }
}

/// Values at boot; they must match the initial values of `LightService`.
static SETTINGS: Mutex<CriticalSectionRawMutex, Cell<Settings>> = Mutex::new(Cell::new(Settings {
    mode: Mode::Rainbow,
    color: rgb8(INITIAL_COLOR),
    color2: rgb8(INITIAL_COLOR2),
    brightness: BRIGHTNESS_DEFAULT,
    rainbow_speed: SPEED_DEFAULT,
    rainbow_span: RAINBOW_SPAN_DEFAULT,
    pulse_speed: SPEED_DEFAULT,
    fade_speed: SPEED_DEFAULT,
    twinkle_speed: SPEED_DEFAULT,
}));

fn update_settings(f: impl FnOnce(&mut Settings)) {
    let s = SETTINGS.lock(|cell| {
        let mut s = cell.get();
        f(&mut s);
        cell.set(s);
        s
    });
    info!("[gatt] {s:?}");
}

// ---------------------------------------------------------------------------
// Main: hardware setup + BLE
// ---------------------------------------------------------------------------

#[esp_rtos::main]
async fn main(_spawner: Spawner) -> ! {
    esp_println::logger::init_logger_from_env();
    let peripherals = esp_hal::init(esp_hal::Config::default().with_cpu_clock(CpuClock::max()));
    esp_alloc::heap_allocator!(size: 72 * 1024);

    // Scheduler must run before the radio is initialized.
    let timg0 = TimerGroup::new(peripherals.TIMG0);
    esp_rtos::start(timg0.timer0, peripherals.FROM_CPU_INTR0);

    // LEDs. The task runs on a high priority interrupt executor, not the main one shared with BLE:
    // frames longer than the RMT memory (448 pulses = 18 LEDs) are refilled from the task mid-frame,
    // and a late refill sends stale data to the last LEDs (wrong colors, flicker).
    static LED_EXECUTOR: StaticCell<InterruptExecutor<2>> = StaticCell::new();
    let led_executor = LED_EXECUTOR.init(InterruptExecutor::new(peripherals.FROM_CPU_INTR2));
    led_executor
        .start(Priority::max())
        .spawn(led_task(peripherals.RMT, peripherals.GPIO33).unwrap());
    info!("LED task started with {LED_NUM} LEDs");

    // BLE
    let connector = BleConnector::new(peripherals.BT, Default::default()).expect("BLE init");
    let controller: ExternalController<_, 1> = ExternalController::new(connector);
    ble_run(controller).await
}

// ---------------------------------------------------------------------------
// BLE
// ---------------------------------------------------------------------------

const CONNECTIONS_MAX: usize = 1;
const L2CAP_CHANNELS_MAX: usize = 2; // signal + att

#[gatt_server]
struct Server {
    light: LightService,
}

/// Mode and color UUIDs are kept from the previous firmware so existing clients keep working.
/// Multi-byte numbers are little-endian.
#[gatt_service(uuid = "937312e0-2354-11eb-9f10-fbc30a62cf38")]
struct LightService {
    #[descriptor(uuid = descriptors::CHARACTERISTIC_USER_DESCRIPTION, read, value = "Light Mode")]
    #[characteristic(uuid = "937312e0-2354-11eb-9f10-fbc30a62cf38", read, write, value = 0)]
    mode: u8,
    #[descriptor(uuid = descriptors::CHARACTERISTIC_USER_DESCRIPTION, read, value = "Light Color")]
    #[characteristic(uuid = "987312e0-2354-11eb-9f10-fbc30a62cf38", read, write, value = INITIAL_COLOR)]
    color: [u8; 3],
    #[descriptor(uuid = descriptors::CHARACTERISTIC_USER_DESCRIPTION, read, value = "Brightness (%)")]
    #[characteristic(uuid = "9a7312e0-2354-11eb-9f10-fbc30a62cf38", read, write, value = BRIGHTNESS_DEFAULT)]
    brightness: u8,
    #[descriptor(uuid = descriptors::CHARACTERISTIC_USER_DESCRIPTION, read, value = "Rainbow Speed (%)")]
    #[characteristic(uuid = "9b7312e0-2354-11eb-9f10-fbc30a62cf38", read, write, value = SPEED_DEFAULT)]
    rainbow_speed: u16,
    #[descriptor(uuid = descriptors::CHARACTERISTIC_USER_DESCRIPTION, read, value = "Rainbow Span (deg)")]
    #[characteristic(uuid = "9c7312e0-2354-11eb-9f10-fbc30a62cf38", read, write, value = RAINBOW_SPAN_DEFAULT)]
    rainbow_span: u16,
    #[descriptor(uuid = descriptors::CHARACTERISTIC_USER_DESCRIPTION, read, value = "Pulse Speed (%)")]
    #[characteristic(uuid = "9d7312e0-2354-11eb-9f10-fbc30a62cf38", read, write, value = SPEED_DEFAULT)]
    pulse_speed: u16,
    #[descriptor(uuid = descriptors::CHARACTERISTIC_USER_DESCRIPTION, read, value = "Second Color")]
    #[characteristic(uuid = "9e7312e0-2354-11eb-9f10-fbc30a62cf38", read, write, value = INITIAL_COLOR2)]
    color2: [u8; 3],
    #[descriptor(uuid = descriptors::CHARACTERISTIC_USER_DESCRIPTION, read, value = "Fade Speed (%)")]
    #[characteristic(uuid = "9f7312e0-2354-11eb-9f10-fbc30a62cf38", read, write, value = SPEED_DEFAULT)]
    fade_speed: u16,
    #[descriptor(uuid = descriptors::CHARACTERISTIC_USER_DESCRIPTION, read, value = "Twinkle Speed (%)")]
    #[characteristic(uuid = "a07312e0-2354-11eb-9f10-fbc30a62cf38", read, write, value = SPEED_DEFAULT)]
    twinkle_speed: u16,
}

async fn ble_run<C: Controller>(controller: C) -> ! {
    let address = static_random_address();
    info!("BLE address {address:?}");

    let mut resources: HostResources<_, DefaultPacketPool, CONNECTIONS_MAX, L2CAP_CHANNELS_MAX> =
        HostResources::new();
    let stack = trouble_host::new(controller, &mut resources)
        .set_random_address(address)
        .build();
    let mut runner = stack.runner();
    let mut peripheral = stack.peripheral();

    let server = Server::new_with_config(GapConfig::Peripheral(PeripheralConfig {
        name: BLE_NAME,
        appearance: &appearance::light_fixtures::GENERIC_LIGHT_FIXTURES,
    }))
    .expect("GATT server init");

    let host = async {
        loop {
            if let Err(e) = runner.run().await {
                error!("[ble] host error: {e:?}");
                Timer::after_secs(1).await;
            }
        }
    };

    let app = async {
        loop {
            match advertise(&mut peripheral, &server).await {
                Ok(conn) => gatt_events(&server, &conn).await,
                Err(e) => {
                    warn!("[adv] error: {e:?}, retrying");
                    Timer::after_secs(1).await;
                }
            }
        }
    };

    join(host, app).await;
    unreachable!()
}

/// Static random address derived from the chip MAC, so it is stable across reboots.
fn static_random_address() -> Address {
    let mac = efuse::base_mac_address();
    let mac = mac.as_bytes();
    let mut addr = [0u8; 6];
    for (i, b) in addr.iter_mut().enumerate() {
        *b = mac[5 - i]; // BLE addresses are little-endian
    }
    addr[5] |= 0xC0; // two top bits set = static random address
    Address::random(addr)
}

async fn advertise<'values, 'server, C: Controller>(
    peripheral: &mut Peripheral<'values, C, DefaultPacketPool>,
    server: &'server Server<'values>,
) -> Result<GattConnection<'values, 'server, DefaultPacketPool>, BleHostError<C::Error>> {
    // Flags + 128-bit UUID fill the advertising packet; the name goes in the scan response.
    let mut adv_data = [0; 31];
    let adv_len = AdStructure::encode_slice(
        &[
            AdStructure::Flags(LE_GENERAL_DISCOVERABLE | BR_EDR_NOT_SUPPORTED),
            AdStructure::CompleteServiceUuids128(&[LIGHT_SERVICE_UUID_LE]),
        ],
        &mut adv_data[..],
    )?;
    let mut scan_data = [0; 31];
    let scan_len = AdStructure::encode_slice(
        &[AdStructure::CompleteLocalName(BLE_NAME.as_bytes())],
        &mut scan_data[..],
    )?;

    let advertiser = peripheral
        .advertise(
            &Default::default(),
            Advertisement::ConnectableScannableUndirected {
                adv_data: &adv_data[..adv_len],
                scan_data: &scan_data[..scan_len],
            },
        )
        .await?;
    info!("[adv] advertising");
    let conn = advertiser.accept().await?.with_attribute_server(server)?;
    info!("[adv] connected");
    Ok(conn)
}

/// Value written to `c`, or the ATT error to reject the write with. The length must match exactly:
/// `[u8; N]` would otherwise accept shorter writes (zero padded).
fn written<T: FromGatt + AsGatt, P: PacketPool>(
    w: &WriteEvent<'_, '_, P>,
    c: &Characteristic<T>,
) -> Result<T, AttErrorCode> {
    let exact = w.with_data(|offset, d| offset == 0 && d.len() == T::MAX_SIZE);
    w.value(c)
        .ok()
        .filter(|_| exact)
        .ok_or(AttErrorCode::INVALID_ATTRIBUTE_VALUE_LENGTH)
}

fn in_range<T: PartialOrd>(range: RangeInclusive<T>) -> impl FnOnce(T) -> Result<T, AttErrorCode> {
    move |v| range.contains(&v).then_some(v).ok_or(AttErrorCode::VALUE_NOT_ALLOWED)
}

/// Handle GATT requests until the central disconnects. Valid writes update `SETTINGS` and are
/// stored by the server (so reads return them); invalid ones are rejected and change nothing.
/// The connection is dropped after `IDLE_DISCONNECT` without writes.
async fn gatt_events<P: PacketPool>(server: &Server<'_>, conn: &GattConnection<'_, '_, P>) {
    let light = &server.light;
    let mut last_write = Instant::now();
    let reason = loop {
        let event = match select(conn.next(), Timer::at(last_write + IDLE_DISCONNECT)).await {
            Either::First(event) => event,
            Either::Second(()) => {
                info!("[gatt] no writes for {} s, disconnecting", IDLE_DISCONNECT.as_secs());
                conn.raw().disconnect();
                last_write = Instant::now(); // ask again later if the link doesn't go down
                continue;
            }
        };
        let reply = match event {
            GattConnectionEvent::Disconnected { reason } => break reason,
            GattConnectionEvent::Gatt { event: GattEvent::Write(w) } => {
                last_write = Instant::now();
                let h = w.handle();
                let result = if h == light.mode.handle {
                    written(&w, &light.mode)
                        .and_then(|m| Mode::from_byte(m).ok_or(AttErrorCode::VALUE_NOT_ALLOWED))
                        .map(|m| update_settings(|s| s.mode = m))
                } else if h == light.color.handle {
                    written(&w, &light.color).map(|c| update_settings(|s| s.color = rgb8(c)))
                } else if h == light.brightness.handle {
                    written(&w, &light.brightness)
                        .and_then(in_range(BRIGHTNESS_RANGE))
                        .map(|v| update_settings(|s| s.brightness = v))
                } else if h == light.rainbow_speed.handle {
                    written(&w, &light.rainbow_speed)
                        .and_then(in_range(SPEED_RANGE))
                        .map(|v| update_settings(|s| s.rainbow_speed = v))
                } else if h == light.rainbow_span.handle {
                    written(&w, &light.rainbow_span)
                        .and_then(in_range(RAINBOW_SPAN_RANGE))
                        .map(|v| update_settings(|s| s.rainbow_span = v))
                } else if h == light.pulse_speed.handle {
                    written(&w, &light.pulse_speed)
                        .and_then(in_range(SPEED_RANGE))
                        .map(|v| update_settings(|s| s.pulse_speed = v))
                } else if h == light.color2.handle {
                    written(&w, &light.color2).map(|c| update_settings(|s| s.color2 = rgb8(c)))
                } else if h == light.fade_speed.handle {
                    written(&w, &light.fade_speed)
                        .and_then(in_range(SPEED_RANGE))
                        .map(|v| update_settings(|s| s.fade_speed = v))
                } else if h == light.twinkle_speed.handle {
                    written(&w, &light.twinkle_speed)
                        .and_then(in_range(SPEED_RANGE))
                        .map(|v| update_settings(|s| s.twinkle_speed = v))
                } else {
                    Ok(()) // not ours: let the server validate it
                };
                match result {
                    Ok(()) => w.accept(),
                    Err(code) => {
                        w.with_data(|_, d| warn!("[gatt] rejected write {d:?} to handle {h}: {code:?}"));
                        w.reject(code)
                    }
                }
            }
            GattConnectionEvent::Gatt { event } => event.accept(),
            _ => continue,
        };
        match reply {
            Ok(reply) => reply.send().await,
            Err(e) => warn!("[gatt] error sending response: {e:?}"),
        }
    };
    info!("[gatt] disconnected: {reason:?}");
}

// ---------------------------------------------------------------------------
// LEDs
// ---------------------------------------------------------------------------

const LED_BUFFER: usize = buffer_size::<RGB8>(LED_NUM);
/// RMT memory blocks for the LED channel (64 pulses each, 8 in total on the ESP32).
/// Not 8: esp-hal 1.2 sets the refill threshold to `(blocks * 64 / 2) as u8`, which is 0 for 8
/// blocks, so the refill never happens and frames longer than the buffer never end (the first
/// LEDs freeze and the last ones show garbage). 7 blocks = 448 pulses, threshold 224.
const RMT_MEMSIZE: u8 = 7;
type LedStrip = RmtSmartLeds<'static, LED_BUFFER, Async, RGB8, color_order::Grb>;

// `GAMMA16`: 8-bit input -> 16-bit gamma corrected output, generated by build.rs.
include!(concat!(env!("OUT_DIR"), "/gamma.rs"));

/// Gamma of an 8.8 fixed-point level (0..=255.0), interpolated in `GAMMA16`. Returns 0..=65535.
fn gamma_level(level: u16) -> u32 {
    let i = (level >> 8) as usize;
    let f = (level & 0xff) as u32;
    let a = GAMMA16[i] as u32;
    let b = GAMMA16[(i + 1).min(255)] as u32;
    a + (b - a) * f / 256
}

/// 8.8 fixed-point channels (0..=255.0) before gamma, so slow fades keep sub-step precision.
type Rgb16 = [u16; 3];
/// Light output per channel (gamma applied), 0..=65535. Colors are dimmed and blended in this space.
type Linear = [u16; 3];

/// Full level (255.0) in 8.8 fixed point.
const FULL: u16 = 255 << 8;

fn rgb16(c: RGB8) -> Rgb16 {
    [(c.r as u16) << 8, (c.g as u16) << 8, (c.b as u16) << 8]
}

fn linear(c: Rgb16) -> Linear {
    c.map(|x| gamma_level(x) as u16)
}

/// `c` dimmed to `level` (8.8 fixed point, gamma corrected like a color channel).
fn dim(c: Linear, level: u16) -> Linear {
    let l = gamma_level(level);
    c.map(|x| (x as u32 * l / 65535) as u16)
}

/// `a` blended towards `b` by `t` (65536 = all `b`). Blending linear light keeps the brightness
/// steady; blending before gamma would go dark halfway between two different colors.
fn mix(a: Linear, b: Linear, t: u32) -> Linear {
    // 65535 * 65536 still fits in u32.
    core::array::from_fn(|i| ((a[i] as u32 * (65536 - t) + b[i] as u32 * t) >> 16) as u16)
}

/// Fully saturated, full value color at `hue` (0..=65535 is one turn of the wheel: red, yellow,
/// green, cyan, blue, magenta). Unlike `smart_leds::hsv2rgb` (8-bit hue, 8-bit channels, steps of
/// 6/255), channels change continuously with the hue.
fn rainbow(hue: u16) -> Rgb16 {
    let pos = hue as u32 * 6;
    let up = (((pos & 0xffff) * FULL as u32) >> 16) as u16;
    let down = FULL - up;
    match pos >> 16 {
        0 => [FULL, up, 0],
        1 => [down, FULL, 0],
        2 => [0, FULL, up],
        3 => [0, down, FULL],
        4 => [up, 0, FULL],
        _ => [FULL, 0, down],
    }
}

/// Part of a cycle (2^32 = one cycle) that passes in `elapsed_us`, for a cycle that lasts
/// `period_ms` at 100 % `speed`.
fn cycle_fraction(elapsed_us: u64, period_ms: u64, speed: u16) -> u64 {
    // elapsed_us <= 1 s and speed <= 1000 % keep this below 2^64.
    (elapsed_us.min(1_000_000) << 32) * speed as u64 / (period_ms * 1000 * 100)
}

/// Advance an animation phase (2^32 = one cycle) by `elapsed_us`, for a cycle that lasts
/// `period_ms` at 100 % `speed`. Accumulating the phase, instead of deriving it from the clock,
/// keeps the animation continuous when the speed changes.
fn advance(phase: u32, elapsed_us: u64, period_ms: u64, speed: u16) -> u32 {
    phase.wrapping_add(cycle_fraction(elapsed_us, period_ms, speed) as u32) // whole cycles wrap away
}

/// Triangle wave over one cycle of `phase`: 0, up to 2^31 halfway, back to 0.
fn triangle(phase: u32) -> u32 {
    if phase < 1 << 31 { phase } else { phase.wrapping_neg() }
}

/// Level (8.8 fixed point) going from `min` (0-255) up to full and back down over one cycle.
fn swell(phase: u32, min: u8) -> u16 {
    let min = (min as u64) << 8;
    (min + (((FULL as u64 - min) * triangle(phase) as u64) >> 31)) as u16
}

/// Blend amount of the fade mode at `phase` (0..=65536): from the first color to the second and
/// back, easing in and out so it lingers on each color (smoothstep).
fn fade_amount(phase: u32) -> u32 {
    let t = (triangle(phase) >> 15) as u64; // 0..=65536
    ((t * t * (3 * 65536 - 2 * t)) >> 32) as u32
}

/// `c` dimmed to `level` like a flame: green and blue are dimmed by the square of the level, so
/// dips also turn redder.
fn flame(c: Linear, level: u16) -> Linear {
    let l = gamma_level(level);
    let [r, g, b] = dim(c, level);
    [r, (g as u32 * l / 65535) as u16, (b as u32 * l / 65535) as u16]
}

/// Xorshift PRNG for the candle and twinkle modes, which only need noise.
struct Rng(u32);

impl Rng {
    fn next(&mut self) -> u32 {
        let mut x = self.0;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        self.0 = x;
        x
    }

    fn in_range(&mut self, range: RangeInclusive<u32>) -> u32 {
        let span = (range.end() - range.start()) as u64 + 1;
        range.start() + ((self.next() as u64 * span) >> 32) as u32
    }
}

/// A level (8.8 fixed point) that eases towards a random target, picking a new one every few frames.
#[derive(Clone, Copy)]
struct Flicker {
    level: u16,
    target: u16,
    frames_left: u32,
}

impl Flicker {
    const STEADY: Self = Self { level: FULL, target: FULL, frames_left: 0 };

    /// Advance one frame. A new target is taken from `pick` when the last one has been held for
    /// its random number of frames from `hold`.
    fn step(
        &mut self,
        rng: &mut Rng,
        hold: RangeInclusive<u32>,
        pick: impl FnOnce(&mut Rng) -> u32,
    ) -> u16 {
        if self.frames_left == 0 {
            self.target = pick(rng) as u16;
            self.frames_left = rng.in_range(hold);
        }
        self.frames_left = self.frames_left.saturating_sub(1);
        // Move 1/8 of the way each frame (~40 ms time constant): smooth, but still a flicker.
        let diff = self.target as i32 - self.level as i32;
        self.level = (self.level as i32 + diff / 8) as u16;
        self.level
    }
}

/// 8.8 fixed-point level of a 0-255 value.
fn level(v: u8) -> u32 {
    (v as u32) << 8
}

/// Turn the colors into the bytes sent to the LEDs.
///
/// The brightness `scale` (1/65536 of full output) is applied with 8 fractional bits; the
/// fraction is then spread over successive frames (temporal dithering, `dither` keeps the
/// per-channel error). Without this, low brightness leaves only ~16 levels per channel and fades
/// visibly step.
fn render(
    base: &[Linear; LED_NUM],
    scale: u32,
    dither: &mut [[u8; 3]; LED_NUM],
) -> [RGB8; LED_NUM] {
    let channel = |x: u16, acc: &mut u8| -> u8 {
        // 8.8 fixed point output value; 65535 * 65536 still fits in u32.
        let v = x as u32 * scale / 65536;
        let sum = *acc as u32 + (v & 0xff);
        *acc = (sum & 0xff) as u8;
        ((v >> 8) + (sum >> 8)) as u8
    };
    let mut out = [RGB8::default(); LED_NUM];
    for ((o, c), acc) in out.iter_mut().zip(base).zip(dither.iter_mut()) {
        let [ar, ag, ab] = acc;
        *o = RGB8::new(channel(c[0], ar), channel(c[1], ag), channel(c[2], ab));
    }
    out
}

#[embassy_executor::task]
async fn led_task(rmt: RMT<'static>, pin: GPIO33<'static>) {
    // Created here because async drivers can't be sent to another executor.
    let rmt_freq = Rate::from_mhz(80);
    let rmt = Rmt::new(rmt, rmt_freq).expect("RMT init").into_async();
    // The RMT is refilled from this task, not the interrupt. With 1 block (64 pulses, ~2.7 LEDs)
    // the task must refill every ~40 us. With `RMT_MEMSIZE` blocks there is at most one refill per
    // frame, with ~280 us to do it.
    let mut led_strip =
        LedStrip::new_with_memsize(WS2812B_TIMING, rmt.channel0, pin, RMT_MEMSIZE, rmt_freq)
            .expect("LED strip init");

    let mut base = [Linear::default(); LED_NUM];
    // Start each LED at a different dither phase so leftover blips don't hit all LEDs together.
    let mut dither = [[0u8; 3]; LED_NUM];
    for (i, acc) in dither.iter_mut().enumerate() {
        *acc = [(i * 256 / LED_NUM) as u8; 3];
    }

    // Animation state only advances while its mode is shown, so switching back resumes.
    let mut rainbow_phase = 0u32;
    let mut pulse_phase = 0u32;
    let mut fade_phase = 0u32;
    let mut flame_flicker = Flicker::STEADY;
    let mut led_flicker = [Flicker::STEADY; LED_NUM];
    // Phase of each LED's twinkle, `None` while it waits for the next one.
    let mut stars = [None::<u32>; LED_NUM];
    let mut rng = Rng(0x9e37_79b9);
    let mut last = Instant::now();
    let mut ticker = Ticker::every(Duration::from_millis(FRAME_MS));

    loop {
        let s = SETTINGS.lock(Cell::get);
        let now = Instant::now();
        let elapsed_us = (now - last).as_micros();
        last = now;
        let color = linear(rgb16(s.color));

        match s.mode {
            Mode::Fixed => base.fill(color),
            Mode::Pulsating => {
                pulse_phase = advance(pulse_phase, elapsed_us, PULSE_PERIOD_MS, s.pulse_speed);
                base.fill(dim(color, swell(pulse_phase, PULSE_MIN)));
            }
            Mode::Rainbow => {
                rainbow_phase =
                    advance(rainbow_phase, elapsed_us, RAINBOW_PERIOD_MS, s.rainbow_speed);
                let hue = (rainbow_phase >> 16) as u16;
                // Hue spread over the strip, 65536 = whole wheel.
                let span = s.rainbow_span as u32 * 65536 / 360;
                for (i, led) in base.iter_mut().enumerate() {
                    let offset = (span * i as u32 / LED_NUM as u32) as u16;
                    *led = linear(rainbow(hue.wrapping_add(offset)));
                }
            }
            Mode::Fade => {
                fade_phase = advance(fade_phase, elapsed_us, FADE_PERIOD_MS, s.fade_speed);
                let color2 = linear(rgb16(s.color2));
                base.fill(mix(color, color2, fade_amount(fade_phase)));
            }
            Mode::Candle => {
                // One flame for the whole figure, plus a small flicker of its own on each LED.
                let flame_level = flame_flicker.step(&mut rng, CANDLE_FLAME_HOLD, |rng| {
                    if rng.next() % CANDLE_DIP_ONE_IN == 0 {
                        rng.in_range(level(CANDLE_DIP_MIN)..=level(CANDLE_CALM_MIN))
                    } else {
                        rng.in_range(level(CANDLE_CALM_MIN)..=level(255))
                    }
                });
                for (led, flicker) in base.iter_mut().zip(&mut led_flicker) {
                    let own = flicker.step(&mut rng, CANDLE_LED_HOLD, |rng| {
                        rng.in_range(level(CANDLE_LED_MIN)..=level(255))
                    });
                    *led = flame(color, (flame_level as u32 * own as u32 / FULL as u32) as u16);
                }
            }
            Mode::Twinkle => {
                // Chance of a waiting LED starting a twinkle this frame, 2^32 = certain.
                let start = cycle_fraction(elapsed_us, TWINKLE_GAP_MS, s.twinkle_speed);
                for (led, star) in base.iter_mut().zip(&mut stars) {
                    *star = match *star {
                        None => ((rng.next() as u64) < start).then_some(0),
                        Some(phase) => {
                            let next = advance(phase, elapsed_us, TWINKLE_MS, s.twinkle_speed);
                            (next >= phase).then_some(next) // wrapped around: done
                        }
                    };
                    let level = star.map_or(level(TWINKLE_MIN) as u16, |p| swell(p, TWINKLE_MIN));
                    *led = dim(color, level);
                }
            }
        }

        let scale = BRIGHTNESS_MAX_SCALE * s.brightness as u32 / 100;
        let frame = render(&base, scale, &mut dither);
        if let Err(e) = led_strip.write(frame).await {
            warn!("[led] write error: {e:?}");
        }

        ticker.next().await;
    }
}
