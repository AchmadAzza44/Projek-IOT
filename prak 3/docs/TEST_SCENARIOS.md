# Test Scenarios — Release 1.1.0

| ID | Test | Expected evidence |
|---|---|---|
| T01 | Build PlatformIO environments | `wokwi` and `esp32doit-devkit-v1` build without compile/link errors |
| T02 | Start Wokwi diagram | ESP32 starts; Serial Monitor at 115200 |
| T03 | I2C scanner | Wokwi BMP180: detect 0x77 and 0x3C; physical BMP280: detect 0x76/0x77 and 0x3C |
| T04 | Sensor initialization | `[BMP180]` or `[BMP280]` initialization succeeds; no sensor error |
| T05 | OLED | Temperature, pressure, altitude, boot cause and min/max visible |
| T06 | FreeRTOS task placement | TaskSensor Core 1 and TaskVisualJSON Core 0 report CREATED |
| T07 | JSON first boot | Every line parses as JSON; unavailable previous values are null |
| T08 | JSON later boot | Previous-cycle temperature/pressure are numeric |
| T09 | Sleep phase | LED GPIO2 turns off; Wokwi labels its 10-second wait as emulated |
| T10 | Timer wake | Wokwi short real Deep Sleep wakes by TIMER and RTC boot count increments; reset reason is DEEP_SLEEP; physical build sleeps for 10 seconds |
| T11 | EXT0 wake | Hold GPIO15 through the Wokwi real-sleep probe or physical Deep Sleep; wakeup cause EXT0 |
| T12 | Held-button test | Keep button held through wake; firmware waits for release, then continues |
| T13 | RTC extrema | Vary sensor temperature across cycles; min/max retain extremes |
| T14 | I2C mutex | OLED and sensor run concurrently without overlapping bus access |

## Test evidence to capture
1. Serial Monitor showing both I2C addresses.
2. OLED showing valid measurements and min/max.
3. Serial Monitor showing both task creation messages.
4. JSON line copied into a JSON validator (first boot and later boot).
5. GPIO2 LED off during Deep Sleep.
6. Timer wake and EXT0 wake logs.
7. RTC boot count and extrema across at least three cycles.

Wokwi BMP180 verifies the sensor-data pipeline, but does not satisfy the PDF's exact BMP280/BME280 component check. Verify that item with BMP280/BME280 hardware. Do not mark a runtime test as passed without observing the corresponding evidence.

The Wokwi build emulates the long inactive interval and performs only a 100 ms real Deep Sleep probe to avoid the simulator watchdog race. It can demonstrate a real reset/wakeup and RTC retention if observed, but it does not verify a 10-second Deep Sleep interval or power consumption.
The startup log and JSON include `reset_reason`; a watchdog reset must be reported as a failure, not counted as a Timer or EXT0 wake.
