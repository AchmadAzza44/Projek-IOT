# Validation Report — Release 1.1.0

## Static checks performed
- `diagram.json` parses as valid JSON.
- Every connection endpoint references a declared component.
- Shared I2C wiring is present on GPIO21/GPIO22 for the Wokwi BMP180 and OLED.
- The Wokwi diagram uses the BMP180 component at its fixed I2C address `0x77`, positioned inside the visible canvas.
- EXT0 button is connected between GPIO15 and GND.
- GPIO2 status LED has a 220-ohm series resistor.
- Firmware pin constants agree with the diagram.
- `sketch.ino` and `src/main.cpp` are byte-identical.
- ZIP archive integrity check passes.

## Build verification
- PlatformIO build for `esp32doit-devkit-v1` completed successfully on 2026-11-18.
- Build output after BMP180 support: RAM 7.3%, Flash 25.1%; `firmware.bin` and `firmware.elf` were generated at the paths configured in `wokwi.toml`.
- After the Wokwi watchdog mitigation, both `pio run -e wokwi` and `pio run -e esp32doit-devkit-v1` completed successfully. The Wokwi firmware and ELF exist at the paths configured in `wokwi.toml`.

## Code-level corrections in this release
- JSON output uses numeric JSON values and `null` for unavailable first-cycle values; it does not emit invalid bare `NaN` tokens.
- Previous-cycle readings are captured at boot before the current cycle updates RTC history.
- RTC extrema/history are copied into the shared snapshot under `mutexData`.
- Firmware supports BMP280 at 0x76/0x77 and falls back to BMP180 at 0x77, which is the sensor used by the current Wokwi diagram.
- EXT0 wake handling waits for the active-low button to be released, preventing immediate repeated wakeups when the button remains held.
- Deep Sleep setup now keeps the RTC peripheral powered for the GPIO15 pull-up and RTC slow memory powered for `RTC_DATA_ATTR`. It checks all sleep/wakeup configuration results and reports errors before resuming the tasks if setup fails.

## Wokwi runtime observation
- In the latest live run, the I2C scanner found two devices, BMP180 at `0x77` and OLED at `0x3C`. BMP180 initialization succeeded and later JSON samples reported `sensor_valid: true`, `24 C`, and `1013.27 hPa`.
- Observed boot counts 20, 22, 23, and 27 reported `reset_reason: DEEP_SLEEP` and `wakeup_cause: TIMER`; RTC history retained the previous `24 C` / `1013.27 hPa` sample. Timer wake and RTC persistence pass for the Wokwi 100 ms real-sleep probe.
- The first JSON line after a boot can have `sensor_valid: false` and zero samples because the sensor task has not completed its first read; later lines in the same cycle were valid.
- An earlier run, before this working probe was observed, ended with `rst:0x8 (TG1WDT_SYS_RESET)`, `POWER-ON`, and boot count `1`. Do not treat that historical run as a passing wake test.
- OLED initialization was confirmed in Serial, but its rendered screen was not visually captured in this run. EXT0 button wake and temperature changes for C6 were also not tested.

## Wokwi sleep probe limitations
- PlatformIO has separate `wokwi` and `esp32doit-devkit-v1` environments. Wokwi emulates the 10-second inactive interval, then performs 100 ms of actual Deep Sleep; the physical build still uses 10 seconds of actual Deep Sleep.
- The observed Timer/RTC result validates only the 100 ms Wokwi probe. It does not verify a 10-second real Deep Sleep interval, power consumption, physical-board behavior, or EXT0 wake.

## Runtime and submission items still to verify
- Visually verify and capture the OLED showing measurements and C6 extrema.
- EXT0 wake with the GPIO15 button and LED-off behavior during actual sleep.
- C6 extrema across changing sensor temperatures and multiple observed cycles; no temperature changes were made during this run.
- Verify the PDF's exact BMP280/BME280 sensor requirement using that physical sensor; Wokwi BMP180 is only a simulation substitute.
- Physical-board current consumption and electrical behavior.
- Completed report tables, screenshots, and GitHub branch push.

A successful build alone is not proof of runtime behavior. Save the observed Wokwi serial evidence and complete the remaining physical/manual tests before submission.
