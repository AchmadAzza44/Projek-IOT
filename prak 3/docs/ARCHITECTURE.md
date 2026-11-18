# Arsitektur Sistem — Release 1.1.0

```text
                 Shared I2C Bus
         SDA GPIO21       SCL GPIO22
             |                |
       +-----+-----+    +-----+-----+
       |           |    |           |
 BMP180 (Wokwi)  OLED SSD1306 128x64
     0x77             0x3C
 BMP280 fisik: 0x76/0x77
       |                |
       +------ ESP32 ---+
                  |
       +----------+-----------+
       |                      |
 TaskSensor                TaskVisualJSON
 Core 1                    Core 0
 - forced measurement      - snapshot shared telemetry
 - read T/P/altitude       - OLED rendering
 - update RTC values       - JSON Serial output
       |                      |
       +---- mutexData -------+
       +---- mutexI2C --------+

 GPIO15 active-low button --> EXT0 wake
 RTC timer (physical: 10 s; Wokwi probe: 100 ms) --> Timer wake
 GPIO2 + 220 ohm -----------> Active/sleep indicator
```

## Shared state and synchronization
- `mutexData` protects `DataSensor` and RTC extrema/history updates.
- `mutexI2C` serializes BMP180/BMP280 and OLED transactions on the same I2C bus.
- The UI task takes a consistent snapshot before rendering or serializing.
- Previous-cycle readings are captured at boot before the current sensor task updates RTC history.

## Power-cycle sequence
1. Boot/wakeup; increment RTC boot count.
2. Capture previous-cycle readings and print wakeup cause.
3. Wait for EXT0 button release if it is still held.
4. Scan I2C and initialize peripherals.
5. Run sensor and visualization tasks for the active window.
6. Suspend tasks and turn status LED off.
7. Configure RTC GPIO15 pull-up and arm timer + EXT0.
8. Physical build enters 10-second Deep Sleep. Wokwi build emulates the 10-second inactive interval, then enters 100 ms of real Deep Sleep to verify reset/wakeup and RTC behavior without the long-sleep watchdog race.
