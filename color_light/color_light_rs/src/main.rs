//! Embassy BLE Example
//!
//! - starts Bluetooth advertising
//! - offers one service with three characteristics (one is read/write, one is write only, one is read/write/notify)
//! - pressing the boot-button on a dev-board will send a notification if it is subscribed

//% FEATURES: async embassy embassy-time-timg0 embassy-generic-timers esp-wifi esp-wifi/async esp-wifi/ble
//% CHIPS: esp32 esp32s3 esp32c2 esp32c3 esp32c6 esp32h2

#![no_std]
#![no_main]

use bleps::{
    ad_structure::{
        create_advertising_data, AdStructure, BR_EDR_NOT_SUPPORTED, LE_GENERAL_DISCOVERABLE,
    },
    async_attribute_server::AttributeServer,
    asynch::Ble,
    // attribute_server::NotificationData,
    gatt,
};
// use core::cell::RefCell;
use embassy_executor::Spawner;
use embassy_sync::blocking_mutex::raw::CriticalSectionRawMutex;
use embassy_sync::signal::Signal;
use embassy_time::Timer;
use esp_backtrace as _;
use esp_hal::{
    clock::ClockControl, gpio::Io, peripherals::Peripherals, prelude::*, rmt, rng::Rng,
    system::SystemControl, timer::timg::TimerGroup,
};
use esp_println::println;
use esp_wifi::{ble::controller::asynch::BleConnector, initialize, EspWifiInitFor};
use smart_leds::{SmartLedsWrite, RGB8};

enum CommandMode {
    Rainbow,
    Fixed,
    Pulsating,
}

static SIGNAL_MODE: Signal<CriticalSectionRawMutex, CommandMode> = Signal::new();
static SIGNAL_COLOR: Signal<CriticalSectionRawMutex, RGB8> = Signal::new();
const LED_NUM: usize = 22; // DINOSAUR
                           // const LED_NUM: usize = 20; // DUCK
                           // const LED_NUM: usize = 16; // BANYETES
                           // const LED_NUM: usize = 18; // TANJIRO
                           // const LED_NUM: usize = 15; // TEST
const LED_BUFFER: usize = LED_NUM * 24 + 1; // from smartLedBuffer(LED_NUM)

// macro_rules! NUM_LEDS {
//     () => {
//         15
//     };
// }

#[main]
async fn main(spawner: Spawner) -> ! {
    esp_println::logger::init_logger_from_env();

    // setup basic
    let peripherals = Peripherals::take();
    let system = SystemControl::new(peripherals.SYSTEM);
    let clocks = ClockControl::max(system.clock_control).freeze();
    let io = Io::new(peripherals.GPIO, peripherals.IO_MUX);

    // setup neopixels
    let rmt = rmt::Rmt::new(peripherals.RMT, 80.MHz(), &clocks, None).unwrap();
    let rmt_buffer = esp_hal_smartled::smartLedBuffer!(22); // TODO: constant
    let led_strip =
        esp_hal_smartled::SmartLedsAdapter::new(rmt.channel0, io.pins.gpio33, rmt_buffer, &clocks);
    println!("noepixels ready");

    // setup embassy
    let timer = esp_hal::timer::timg::TimerGroup::new(peripherals.TIMG1, &clocks, None).timer0;
    let init = initialize(
        EspWifiInitFor::Ble,
        timer,
        Rng::new(peripherals.RNG),
        peripherals.RADIO_CLK,
        &clocks,
    )
    .unwrap();
    let timer_group0 = TimerGroup::new_async(peripherals.TIMG0, &clocks);
    esp_hal_embassy::init(&clocks, timer_group0);
    println!("embassy ready");

    // setup ble
    let mut bluetooth = peripherals.BT;
    let connector = BleConnector::new(&init, &mut bluetooth);
    let mut ble = Ble::new(connector, esp_wifi::current_millis);
    println!("ble connector created");

    // Initialize the embassy executor
    spawner.spawn(led_task(led_strip)).ok();

    // ble task
    loop {
        println!("ble init {:?}", ble.init().await);
        println!(
            "ble cmd_set {:?}",
            ble.cmd_set_le_advertising_parameters().await
        );
        println!(
            "ble set_advertising {:?}",
            ble.cmd_set_le_advertising_data(
                create_advertising_data(&[
                    AdStructure::Flags(LE_GENERAL_DISCOVERABLE | BR_EDR_NOT_SUPPORTED),
                    AdStructure::ServiceUuids16(&[Uuid::Uuid16(0x1809)]),
                    AdStructure::CompleteLocalName("led_light"),
                    // AdStructure::CompleteLocalName(esp_hal::chip!()),
                ])
                .unwrap()
            )
            .await
        );
        println!(
            "ble advertise_enable{:?}",
            ble.cmd_set_le_advertise_enable(true).await
        );

        println!("started advertising");

        let mut r_mode = |_offset: usize, data: &mut [u8]| {
            data[..10].copy_from_slice(&b"Light Mode"[..]);
            10
        };
        let mut w_mode = |offset: usize, data: &[u8]| {
            println!("mode recv: {} {:?}", offset, data);
            if data.len() == 1 {
                match data[0] {
                    0 => SIGNAL_MODE.signal(CommandMode::Rainbow),
                    1 => SIGNAL_MODE.signal(CommandMode::Fixed),
                    2 => SIGNAL_MODE.signal(CommandMode::Pulsating),
                    _ => println!("invalid mode command '{}'", data[0]),
                }
            } else {
                println!("invalid mode command len '{}'", data.len());
            }
        };

        let mut r_color = |_offset: usize, data: &mut [u8]| {
            data[..11].copy_from_slice(&b"Light Color"[..]);
            11
        };
        let mut w_color = |offset: usize, data: &[u8]| {
            println!("color recv: {}, {:?}", offset, data);
            if data.len() == 3 {
                SIGNAL_COLOR.signal(RGB8::new(data[0], data[1], data[2]));
            } else {
                println!("invalid color command len '{}'", data.len());
            }
        };

        gatt!([service {
            uuid: "937312e0-2354-11eb-9f10-fbc30a62cf38",
            characteristics: [
                characteristic {
                    uuid: "937312e0-2354-11eb-9f10-fbc30a62cf38",
                    name: "mode",
                    read: r_mode,
                    write: w_mode,
                },
                characteristic {
                    uuid: "987312e0-2354-11eb-9f10-fbc30a62cf38",
                    name: "color",
                    read: r_color,
                    write: w_color,
                },
            ],
        },]);

        let mut rng = bleps::no_rng::NoRng;
        let mut srv = AttributeServer::new(&mut ble, &mut gatt_attributes, &mut rng);

        loop {
            let res = srv.do_work().await;
            if let Ok(bleps::attribute_server::WorkResult::GotDisconnected) = res {
                println!("ble disconnect");
                break;
            }
        }
        println!("ble end");
    }
}

fn modulo(v: f32, d: f32) -> f32 {
    if v < 0.0 {
        v + d
    } else {
        v
    }
}

fn rgb2hue(rgb: &RGB8) -> u8 {
    // Convert the RGB values to the range 0-1, this can be done by dividing the value by 255 for 8-bit color depth (r,g,b - are given values):
    let r = (rgb.r as f32) / 255.0;
    let g = (rgb.g as f32) / 255.0;
    let b = (rgb.b as f32) / 255.0;
    // find the minimum and maximum values of R, G and B
    let maximum = (core::cmp::max(core::cmp::max(rgb.r, rgb.g), rgb.b) as f32) / 255.0;
    let minimum = (core::cmp::min(core::cmp::min(rgb.r, rgb.g), rgb.b) as f32) / 255.0;
    let diff = (maximum - minimum) as f32;
    // depending on what RGB color channel is the max value
    if (rgb.r >= rgb.g) && (rgb.r >= rgb.b) {
        // If Red is max, then Hue = (G-B)/(max-min)
        (modulo(0.0 + ((g - b) / diff), 6.0) * 255.0 / 6.0) as u8
    } else if (rgb.g >= rgb.r) && (rgb.g >= rgb.b) {
        // If Green is max, then Hue = 2.0 + (B-R)/(max-min)
        ((2.0 + ((b - r) / diff)) * 255.0 / 6.0) as u8
    } else {
        // If Blue is max, then Hue = 4.0 + (R-G)/(max-min)
        ((4.0 + ((r - g) / diff)) * 255.0 / 6.0) as u8
    }
}

const LED_MIN: u8 = 100;

#[embassy_executor::task]
async fn led_task(
    mut led_strip: esp_hal_smartled::SmartLedsAdapter<
        rmt::Channel<esp_hal::Blocking, 0>,
        LED_BUFFER,
    >,
) {
    println!("init led_task");
    // initial values
    let mut mode = CommandMode::Rainbow;
    let mut color_rgb = RGB8::new(255, 0, 0);
    let mut color_buffer: [RGB8; LED_NUM] = [RGB8::new(0, 0, 0); LED_NUM];
    // pulsating
    let mut pulsating_color = smart_leds::hsv::Hsv {
        hue: 0,
        sat: 255,
        val: 255,
    };
    let mut pulsating_value = LED_MIN;
    let mut pulsating_add = true;
    let mut pulsating_rgb: RGB8;
    // rainbow
    let rainbow_offset = (255 / (LED_NUM * 4)) as u8;
    let mut rainbow_hue = 0u8;
    let mut rainbow_color = smart_leds::hsv::Hsv {
        hue: 0,
        sat: 255,
        val: 255,
    };
    loop {
        // look for changes
        if let Some(m) = SIGNAL_MODE.try_take() {
            mode = m;
        }
        if let Some(c) = SIGNAL_COLOR.try_take() {
            color_rgb = c;
            println!("hue {}", rgb2hue(&color_rgb));
        }
        // execute current mode one iteration
        match mode {
            CommandMode::Fixed => {
                for i in 0..color_buffer.len() {
                    color_buffer[i] = color_rgb;
                }
            }
            CommandMode::Pulsating => {
                if pulsating_add {
                    pulsating_value = pulsating_value.saturating_add(5);
                    if pulsating_value == u8::MAX {
                        pulsating_add = false;
                    }
                } else {
                    pulsating_value = pulsating_value.saturating_sub(5);
                    if pulsating_value == LED_MIN {
                        pulsating_add = true;
                    }
                }
                pulsating_color.val = pulsating_value;
                pulsating_color.hue = rgb2hue(&color_rgb);
                pulsating_rgb = smart_leds::hsv::hsv2rgb(pulsating_color);
                for i in 0..color_buffer.len() {
                    color_buffer[i] = pulsating_rgb;
                }
            }
            CommandMode::Rainbow => {
                rainbow_hue = rainbow_hue.wrapping_add(1); // advance
                for i in 0..color_buffer.len() {
                    rainbow_color.hue = rainbow_hue.wrapping_add(rainbow_offset * (i as u8));
                    color_buffer[i] = smart_leds::hsv::hsv2rgb(rainbow_color);
                }
            }
        }
        // apply
        led_strip
            .write(smart_leds::brightness(
                smart_leds::gamma(color_buffer.iter().cloned()),
                16,
            ))
            .unwrap(); // TODO: brightness control

        // wait
        Timer::after_millis(100).await;
    }
}
