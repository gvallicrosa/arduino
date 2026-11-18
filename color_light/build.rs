use std::fmt::Write;

fn main() {
    // Extra ROM symbols needed by the BLE blob, see ld/esp32_rom_bt_fix.x.
    // Passed as an input file: the linker treats it as an implicit linker script.
    let dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    println!("cargo:rustc-link-arg={dir}/ld/esp32_rom_bt_fix.x");
    println!("cargo:rerun-if-changed=ld/esp32_rom_bt_fix.x");

    // 16-bit gamma table (same 2.8 curve as smart_leds::gamma, more precision).
    let mut table = String::from("const GAMMA16: [u16; 256] = [");
    for i in 0..256 {
        let v = ((i as f64 / 255.0).powf(2.8) * 65535.0).round() as u16;
        write!(table, "{v},").unwrap();
    }
    table.push_str("];\n");
    let out = std::env::var("OUT_DIR").unwrap();
    std::fs::write(format!("{out}/gamma.rs"), table).unwrap();
    println!("cargo:rerun-if-changed=build.rs");
}
