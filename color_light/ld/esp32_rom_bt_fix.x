/* Classic-BT ROM callbacks referenced by the esp32 BLE blob (libbtdm_app.a,
 * esp-wifi-sys-esp32 0.3.0) but missing from esp-rom-sys 0.1.5's esp32.rom.ld.
 * Addresses copied from ESP-IDF components/esp_rom/esp32/ld/esp32.rom.ld.
 * Remove once esp-rom-sys ships them. */
PROVIDE ( ld_pscan_evt_start_cbk = 0x4003e924 );
PROVIDE ( ld_iscan_evt_start_cbk = 0x4003b58c );
PROVIDE ( ld_page_evt_start_cbk = 0x4003cf40 );
