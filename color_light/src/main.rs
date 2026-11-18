//! BLE controlled WS2812B light.
//!
//! - advertises as `led_light` with one custom GATT service
//! - `mode` characteristic (1 byte, read/write): 0 = rainbow, 1 = fixed, 2 = pulsating
//! - `color` characteristic (3 bytes RGB, read/write): color used by fixed and pulsating modes
//!
//! The LED count is chosen per figure with a cargo feature (see `Cargo.toml`).

#![no_std]
#![no_main]

use embassy_executor::Spawner;
use embassy_futures::join::join;
use embassy_sync::{blocking_mutex::raw::CriticalSectionRawMutex, signal::Signal};
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

/// Number of LEDs of the figure, selected with a cargo feature.
const LED_NUM: usize = if cfg!(feature = "duck") {
    20
} else if cfg!(feature = "banyetes") {
    16
} else if cfg!(feature = "tanjiro") {
    18
} else if cfg!(feature = "test") {
    15
} else {
    22 // dinosaur (default)
};
const _: () = assert!(
    cfg!(feature = "dinosaur") as u8
        + cfg!(feature = "duck") as u8
        + cfg!(feature = "banyetes") as u8
        + cfg!(feature = "tanjiro") as u8
        + cfg!(feature = "test") as u8
        == 1,
    "enable exactly one figure feature (use --no-default-features --features <figure>)"
);
/// Global brightness applied to every frame (0-255).
const BRIGHTNESS: u8 = 16;
/// Lowest value reached by the pulsating mode (0-255).
const PULSE_MIN: u8 = 100;
/// Duration of one full pulse (dim -> bright -> dim).
const PULSE_PERIOD_MS: u64 = 6_200;
/// Duration of one full trip around the color wheel in rainbow mode.
const RAINBOW_PERIOD_MS: u64 = 25_600;
/// Part of the color wheel shown along the strip in rainbow mode, in degrees (360 = whole wheel).
/// The strip is a window over the rainbow that slides by one full wheel every `RAINBOW_PERIOD_MS`.
const RAINBOW_SPAN_DEG: u32 = 90;
/// Time between LED frames. Kept short (200 Hz) so temporal dithering is not visible.
const FRAME_MS: u64 = 5;

const BLE_NAME: &str = "led_light";
/// Light service UUID `937312e0-2354-11eb-9f10-fbc30a62cf38`, little-endian for advertising.
const LIGHT_SERVICE_UUID_LE: [u8; 16] = [
    0x38, 0xcf, 0x62, 0x0a, 0xc3, 0xfb, 0x10, 0x9f, 0xeb, 0x11, 0x54, 0x23, 0xe0, 0x12, 0x73, 0x93,
];
const INITIAL_COLOR: [u8; 3] = [255, 0, 0];

// ---------------------------------------------------------------------------
// Shared state between BLE and the LED task
// ---------------------------------------------------------------------------

#[derive(Clone, Copy, Debug)]
enum Mode {
    Rainbow,
    Fixed,
    Pulsating,
}

impl Mode {
    fn from_byte(b: u8) -> Option<Self> {
        match b {
            0 => Some(Mode::Rainbow),
            1 => Some(Mode::Fixed),
            2 => Some(Mode::Pulsating),
            _ => None,
        }
    }
}

static SIGNAL_MODE: Signal<CriticalSectionRawMutex, Mode> = Signal::new();
static SIGNAL_COLOR: Signal<CriticalSectionRawMutex, RGB8> = Signal::new();

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

/// UUIDs kept from the previous firmware so existing clients keep working.
#[gatt_service(uuid = "937312e0-2354-11eb-9f10-fbc30a62cf38")]
struct LightService {
    #[descriptor(uuid = descriptors::CHARACTERISTIC_USER_DESCRIPTION, read, value = "Light Mode")]
    #[characteristic(uuid = "937312e0-2354-11eb-9f10-fbc30a62cf38", read, write, value = 0)]
    mode: u8,
    #[descriptor(uuid = descriptors::CHARACTERISTIC_USER_DESCRIPTION, read, value = "Light Color")]
    #[characteristic(uuid = "987312e0-2354-11eb-9f10-fbc30a62cf38", read, write, value = INITIAL_COLOR)]
    color: [u8; 3],
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

/// Handle GATT requests until the central disconnects.
async fn gatt_events<P: PacketPool>(server: &Server<'_>, conn: &GattConnection<'_, '_, P>) {
    let mode = server.light.mode;
    let color = server.light.color;
    let reason = loop {
        match conn.next().await {
            GattConnectionEvent::Disconnected { reason } => break reason,
            GattConnectionEvent::Gatt { event } => {
                let reply = match event {
                    GattEvent::Write(w) if w.handle() == mode.handle => {
                        match w.value(&mode).ok().and_then(Mode::from_byte) {
                            Some(m) => {
                                info!("[gatt] mode {m:?}");
                                SIGNAL_MODE.signal(m);
                                w.accept()
                            }
                            None => {
                                w.with_data(|_, d| warn!("[gatt] invalid mode {d:?}"));
                                w.reject(AttErrorCode::VALUE_NOT_ALLOWED)
                            }
                        }
                    }
                    GattEvent::Write(w) if w.handle() == color.handle => {
                        // [u8; 3] accepts shorter writes (zero padded), so check length explicitly.
                        match w.with_data(|offset, d| {
                            (offset == 0 && d.len() == 3).then(|| [d[0], d[1], d[2]])
                        }) {
                            Some([r, g, b]) => {
                                info!("[gatt] color {r} {g} {b}");
                                SIGNAL_COLOR.signal(RGB8::new(r, g, b));
                                w.accept()
                            }
                            None => {
                                w.with_data(|_, d| warn!("[gatt] invalid color {d:?}"));
                                w.reject(AttErrorCode::INVALID_ATTRIBUTE_VALUE_LENGTH)
                            }
                        }
                    }
                    other => other.accept(),
                };
                match reply {
                    Ok(reply) => reply.send().await,
                    Err(e) => warn!("[gatt] error sending response: {e:?}"),
                }
            }
            _ => {}
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

/// 8.8 fixed-point channels (0..=255.0), so slow fades keep sub-step precision until dithering.
type Rgb16 = [u16; 3];

/// Full level (255.0) in 8.8 fixed point.
const FULL: u16 = 255 << 8;

fn rgb16(c: RGB8) -> Rgb16 {
    [(c.r as u16) << 8, (c.g as u16) << 8, (c.b as u16) << 8]
}

/// Fully saturated, full value color at `hue` (0..=65535 is one turn of the wheel: red, yellow,
/// green, cyan, blue, magenta). Unlike `smart_leds::hsv2rgb` (8-bit hue, 8-bit channels, steps of
/// 6/255), channels change continuously with the hue.
fn rainbow(hue: u16) -> Rgb16 {
    let pos = hue as u32 * 6;
    let up = ((pos & 0xffff) * FULL as u32 >> 16) as u16;
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

/// Pulsating level (8.8 fixed point) at time `now_ms`: a triangle wave between
/// `PULSE_MIN` and 255 with period `PULSE_PERIOD_MS`.
fn pulse_level(now_ms: u64) -> u16 {
    let half = PULSE_PERIOD_MS / 2;
    let t = now_ms % PULSE_PERIOD_MS;
    let up = if t < half { t } else { PULSE_PERIOD_MS - t }; // 0..=half
    let min = (PULSE_MIN as u64) << 8;
    let max = 255u64 << 8;
    (min + (max - min) * up / half) as u16
}

/// Turn the base colors into the bytes sent to the LEDs.
///
/// Gamma, `level` and `BRIGHTNESS` are applied with 8 fractional bits; the fraction is then
/// spread over successive frames (temporal dithering, `dither` keeps the per-channel error).
/// Without this, low brightness leaves only ~16 levels per channel and fades visibly step.
fn render(base: &[Rgb16; LED_NUM], level: u16, dither: &mut [[u8; 3]; LED_NUM]) -> [RGB8; LED_NUM] {
    let level = gamma_level(level);
    let channel = |x: u16, acc: &mut u8| -> u8 {
        // 8.8 fixed point output value; max 65535 * 65535 fits in u32.
        let v = gamma_level(x) * level / 65535 * (BRIGHTNESS as u32 + 1) / 256;
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

    let mut mode = Mode::Rainbow;
    let mut color = RGB8::from(INITIAL_COLOR);
    let mut base = [Rgb16::default(); LED_NUM];
    // Start each LED at a different dither phase so leftover blips don't hit all LEDs together.
    let mut dither = [[0u8; 3]; LED_NUM];
    for (i, acc) in dither.iter_mut().enumerate() {
        *acc = [(i * 256 / LED_NUM) as u8; 3];
    }

    // Hue difference between neighbouring LEDs.
    let rainbow_offset = (RAINBOW_SPAN_DEG * 65536 / 360 / LED_NUM as u32) as u16;
    let mut ticker = Ticker::every(Duration::from_millis(FRAME_MS));

    loop {
        if let Some(m) = SIGNAL_MODE.try_take() {
            mode = m;
        }
        if let Some(c) = SIGNAL_COLOR.try_take() {
            color = c;
        }

        let now_ms = Instant::now().as_millis();
        let level = match mode {
            Mode::Fixed => {
                base.fill(rgb16(color));
                FULL
            }
            Mode::Pulsating => {
                base.fill(rgb16(color));
                pulse_level(now_ms)
            }
            Mode::Rainbow => {
                let hue = (now_ms * 65536 / RAINBOW_PERIOD_MS) as u16; // wraps every period
                for (i, led) in base.iter_mut().enumerate() {
                    *led = rainbow(hue.wrapping_add(rainbow_offset.wrapping_mul(i as u16)));
                }
                FULL
            }
        };

        let frame = render(&base, level, &mut dither);
        if let Err(e) = led_strip.write(frame).await {
            warn!("[led] write error: {e:?}");
        }

        ticker.next().await;
    }
}
