/*
 * UNIB IoT Lab-2 — I2C + BMP180/BMP280 + OLED SSD1306 + FreeRTOS + Deep Sleep
 * Firmware release 1.1.0
 *
 * Hardware mapping from the practical guide:
 * SDA=GPIO21, SCL=GPIO22, EXT0=GPIO15 (active LOW), status LED=GPIO2.
 *
 * TaskSensor: Core 1. Reads the detected barometer and updates telemetry/RTC.
 * TaskVisualJSON: Core 0. Renders OLED and emits JSON telemetry.
 * mutexData protects telemetry and RTC record snapshots.
 * mutexI2C serializes access to the shared I2C bus.
 *
 * Note: GPIO15 is an ESP32 strapping pin. Verify board-specific behavior
 * before using this design on physical hardware.
 */
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include "esp_err.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "driver/rtc_io.h"
#include <math.h>

static constexpr uint8_t SCREEN_WIDTH = 128;
static constexpr uint8_t SCREEN_HEIGHT = 64;
static constexpr uint8_t OLED_ADDRESS = 0x3C;
static constexpr uint8_t BMP_ADDRESS_PRIMARY = 0x76;
static constexpr uint8_t BMP_ADDRESS_ALTERNATE = 0x77;
static constexpr uint8_t PIN_SDA = 21;
static constexpr uint8_t PIN_SCL = 22;
static constexpr uint8_t PIN_STATUS_LED = 2;
static constexpr gpio_num_t PIN_WAKE_BUTTON = GPIO_NUM_15;

static constexpr uint32_t SLEEP_SECONDS = 10;
static constexpr uint32_t ACTIVE_WINDOW_MS = 5000;
static constexpr uint32_t SENSOR_PERIOD_MS = 1000;
static constexpr uint32_t DISPLAY_PERIOD_MS = 500;
static constexpr uint32_t JSON_PERIOD_MS = 1000;
static constexpr float SEA_LEVEL_HPA = 1013.25f;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BMP085 bmp180;
Adafruit_BMP280 bmp280;

enum class SensorKind : uint8_t {
  None,
  BMP180,
  BMP280
};

// RTC slow-memory values survive Deep Sleep wakeups while RTC memory remains powered.
RTC_DATA_ATTR uint32_t nomorSiklusBoot = 0;
RTC_DATA_ATTR float riwayatSuhuTerakhir = NAN;
RTC_DATA_ATTR float riwayatTekananTerakhir = NAN;
RTC_DATA_ATTR float suhuMaks = -1000.0f;
RTC_DATA_ATTR float suhuMin = 1000.0f;

struct DataSensor {
  float suhu = NAN;
  float tekanan = NAN;
  float elevasi = NAN;
  float suhuSiklusSebelumnya = NAN;
  float tekananSiklusSebelumnya = NAN;
  float suhuMaksimum = NAN;
  float suhuMinimum = NAN;
  bool valid = false;
  uint32_t jumlahSampel = 0;
};

DataSensor dataCuaca;
SemaphoreHandle_t mutexData = nullptr;
SemaphoreHandle_t mutexI2C = nullptr;
TaskHandle_t taskSensorHandle = nullptr;
TaskHandle_t taskVisualHandle = nullptr;

bool sensorReady = false;
bool oledReady = false;
bool sleepScheduled = false;
uint8_t sensorAddressDetected = 0;
SensorKind sensorKind = SensorKind::None;
uint32_t jumlahPerantiI2C = 0;
uint32_t waktuMulaiAktif = 0;
esp_sleep_wakeup_cause_t penyebabWakeup;

const char* labelWakeup(esp_sleep_wakeup_cause_t cause) {
  switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER: return "TIMER";
    case ESP_SLEEP_WAKEUP_EXT0: return "EXT0";
    default: return "UNDEFINED";
  }
}

const char* labelResetReason(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WATCHDOG";
    case ESP_RST_TASK_WDT: return "TASK_WATCHDOG";
    case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_DEEPSLEEP: return "DEEP_SLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}

const char* labelSensorModel() {
  switch (sensorKind) {
    case SensorKind::BMP180: return "BMP180";
    case SensorKind::BMP280: return "BMP280";
    default: return "NOT_FOUND";
  }
}

void scanI2CBus() {
  Serial.println("\n[I2C] Scanner mulai: SDA GPIO21, SCL GPIO22");
  jumlahPerantiI2C = 0;

  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    const uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("[I2C] Peranti ditemukan pada 0x%02X\n", address);
      ++jumlahPerantiI2C;
    }
    delay(2);
  }

  Serial.printf("[I2C] Selesai. Total peranti: %lu\n",
                static_cast<unsigned long>(jumlahPerantiI2C));
  Serial.println("[I2C] Target: BMP180 0x77 or BMP280 0x76/0x77; OLED 0x3C.");
}

void cetakDiagnostikWakeup() {
  Serial.println("\n========================================");
  Serial.printf("[RTC] Siklus boot ke-%lu\n",
                static_cast<unsigned long>(nomorSiklusBoot));
  Serial.printf("[RTC WAKEUP] Sumber: %s\n", labelWakeup(penyebabWakeup));
  Serial.printf("[RTC MEM] Riwayat suhu: %.2f C | tekanan: %.2f hPa\n",
                riwayatSuhuTerakhir, riwayatTekananTerakhir);
  Serial.printf("[RTC RECORD] Minimum: %.2f C | maksimum: %.2f C\n",
                suhuMin, suhuMaks);
}

void tungguTombolDilepas() {
  // Prevent an EXT0 wake held LOW from causing an immediate wake/sleep loop.
  if (digitalRead(static_cast<uint8_t>(PIN_WAKE_BUTTON)) == LOW) {
    Serial.println("[WAKE] Tombol masih ditekan; menunggu dilepas...");
    while (digitalRead(static_cast<uint8_t>(PIN_WAKE_BUTTON)) == LOW) {
      delay(20);
    }
    delay(50); // simple release debounce
    Serial.println("[WAKE] Tombol sudah dilepas.");
  }
}

void taskSensor(void* parameter) {
  TickType_t waktuBangunTerakhir = xTaskGetTickCount();

  for (;;) {
    bool pembacaanValid = false;
    float suhu = NAN;
    float tekanan = NAN;
    float elevasi = NAN;

    if (sensorReady &&
        xSemaphoreTake(mutexI2C, pdMS_TO_TICKS(500)) == pdTRUE) {
      if (sensorKind == SensorKind::BMP280 &&
          bmp280.takeForcedMeasurement()) {
        suhu = bmp280.readTemperature();
        tekanan = bmp280.readPressure() / 100.0f; // Pa -> hPa
        elevasi = bmp280.readAltitude(SEA_LEVEL_HPA);
        pembacaanValid =
            isfinite(suhu) && isfinite(tekanan) && isfinite(elevasi);
      } else if (sensorKind == SensorKind::BMP180) {
        suhu = bmp180.readTemperature();
        const int32_t tekananPa = bmp180.readPressure();
        if (tekananPa > 0) {
          tekanan = tekananPa / 100.0f; // Pa -> hPa
          elevasi = bmp180.readAltitude(SEA_LEVEL_HPA * 100.0f);
          pembacaanValid =
              isfinite(suhu) && isfinite(tekanan) && isfinite(elevasi);
        }
      }
      xSemaphoreGive(mutexI2C);
    }

    if (xSemaphoreTake(mutexData, pdMS_TO_TICKS(300)) == pdTRUE) {
      dataCuaca.valid = pembacaanValid;

      if (pembacaanValid) {
        dataCuaca.suhu = suhu;
        dataCuaca.tekanan = tekanan;
        dataCuaca.elevasi = elevasi;
        ++dataCuaca.jumlahSampel;

        // Persist the latest successful measurement in RTC memory.
        riwayatSuhuTerakhir = suhu;
        riwayatTekananTerakhir = tekanan;

        if (suhu > suhuMaks) suhuMaks = suhu;
        if (suhu < suhuMin) suhuMin = suhu;

        dataCuaca.suhuMaksimum = suhuMaks;
        dataCuaca.suhuMinimum = suhuMin;
      }

      xSemaphoreGive(mutexData);
    }

    vTaskDelayUntil(&waktuBangunTerakhir, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
  }
}

void renderOLED(const DataSensor& snapshot) {
  if (!oledReady) return;
  if (xSemaphoreTake(mutexI2C, pdMS_TO_TICKS(500)) != pdTRUE) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("UNIB IoT - STASIUN CUACA");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(0, 13);
  if (snapshot.valid) {
    display.printf("Suhu     : %.1f C\n", snapshot.suhu);
    display.printf("Tekanan  : %.1f hPa\n", snapshot.tekanan);
    display.printf("Elevasi  : %.1f m\n", snapshot.elevasi);
  } else {
    display.println("BMP180/280: MENUNGGU DATA");
    display.println("Periksa sensor/I2C");
  }

  display.drawLine(0, 42, 127, 42, SSD1306_WHITE);
  display.setCursor(0, 45);
  display.printf("#%lu | %s\n",
                 static_cast<unsigned long>(nomorSiklusBoot),
                 labelWakeup(penyebabWakeup));

  if (isfinite(snapshot.suhuMinimum) && isfinite(snapshot.suhuMaksimum)) {
    display.printf("MIN %.1f MAX %.1f C",
                   snapshot.suhuMinimum, snapshot.suhuMaksimum);
  } else {
    display.print("MIN/MAX: menunggu data");
  }

  display.display();
  xSemaphoreGive(mutexI2C);
}

void cetakJSON(const DataSensor& snapshot) {
  JsonDocument doc;
  doc["device_id"] = "unib-esp32-weather-lab03";
  doc["firmware_version"] = "1.1.0";
  doc["boot_count"] = nomorSiklusBoot;
  doc["wakeup_cause"] = labelWakeup(penyebabWakeup);
  doc["reset_reason"] = labelResetReason(esp_reset_reason());
  doc["i2c_device_count"] = jumlahPerantiI2C;
  doc["sensor_model"] = labelSensorModel();
  doc["sensor_i2c_address"] =
      sensorAddressDetected
          ? String("0x") + String(sensorAddressDetected, HEX)
          : String("not-found");
  doc["oled_address"] = "0x3C";
  doc["sensor_valid"] = snapshot.valid;

  if (snapshot.valid) {
    doc["temp_c"] = snapshot.suhu;
    doc["pressure_hpa"] = snapshot.tekanan;
    doc["altitude_m"] = snapshot.elevasi;
  } else {
    doc["temp_c"] = nullptr;
    doc["pressure_hpa"] = nullptr;
    doc["altitude_m"] = nullptr;
  }

  if (isfinite(snapshot.suhuSiklusSebelumnya)) {
    doc["previous_cycle_temp_c"] = snapshot.suhuSiklusSebelumnya;
  } else {
    doc["previous_cycle_temp_c"] = nullptr;
  }

  if (isfinite(snapshot.tekananSiklusSebelumnya)) {
    doc["previous_cycle_pressure_hpa"] = snapshot.tekananSiklusSebelumnya;
  } else {
    doc["previous_cycle_pressure_hpa"] = nullptr;
  }

  if (isfinite(snapshot.suhuMinimum)) doc["temperature_min_c"] = snapshot.suhuMinimum;
  else doc["temperature_min_c"] = nullptr;

  if (isfinite(snapshot.suhuMaksimum)) doc["temperature_max_c"] = snapshot.suhuMaksimum;
  else doc["temperature_max_c"] = nullptr;

  doc["sample_count_this_boot"] = snapshot.jumlahSampel;

  serializeJson(doc, Serial);
  Serial.println();
}

void taskVisualisasiDanSerial(void* parameter) {
  uint32_t waktuOLED = 0;
  uint32_t waktuJSON = 0;

  for (;;) {
    DataSensor snapshot;

    if (xSemaphoreTake(mutexData, pdMS_TO_TICKS(300)) == pdTRUE) {
      snapshot = dataCuaca;
      xSemaphoreGive(mutexData);
    }

    const uint32_t sekarang = millis();

    if (sekarang - waktuOLED >= DISPLAY_PERIOD_MS) {
      renderOLED(snapshot);
      waktuOLED = sekarang;
    }

    if (sekarang - waktuJSON >= JSON_PERIOD_MS) {
      cetakJSON(snapshot);
      waktuJSON = sekarang;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void masukDeepSleep() {
  if (sleepScheduled) return;
  sleepScheduled = true;

  // Ensure EXT0 is not armed while its active-low button is held.
  tungguTombolDilepas();

  if (taskSensorHandle) vTaskSuspend(taskSensorHandle);
  if (taskVisualHandle) vTaskSuspend(taskVisualHandle);

  digitalWrite(PIN_STATUS_LED, LOW);

  Serial.printf("[POWER] Fase aktif: %lu ms\n",
                static_cast<unsigned long>(millis() - waktuMulaiAktif));
#ifdef WOKWI_SIMULATION
  Serial.printf("[WOKWI] Fase tidur %lu detik diemulasikan; Deep Sleep nyata 100 ms untuk menguji wakeup dan RTC.\n",
                static_cast<unsigned long>(SLEEP_SECONDS));
  Serial.println("[WOKWI] Untuk EXT0, tahan tombol GPIO15 sampai perangkat boot ulang.");
  Serial.flush();

  const uint32_t waktuMulaiSimulasi = millis();
  while (millis() - waktuMulaiSimulasi < SLEEP_SECONDS * 1000UL) {
    if (digitalRead(static_cast<uint8_t>(PIN_WAKE_BUTTON)) == LOW) {
      Serial.println("[WOKWI] Tombol terdeteksi; melanjutkan ke uji EXT0.");
      break;
    }
    delay(20);
  }
  const uint64_t timerWakeDurationUs = 100000ULL;
#else
  Serial.printf("[POWER] Deep Sleep selama %lu detik\n",
                static_cast<unsigned long>(SLEEP_SECONDS));
  const uint64_t timerWakeDurationUs =
      static_cast<uint64_t>(SLEEP_SECONDS) * 1000000ULL;
#endif
  Serial.println("[POWER] Wakeup: RTC Timer atau EXT0 GPIO15 LOW.");
  Serial.flush();

  // Configure GPIO15 as an RTC input with internal pull-up before enabling EXT0.
  rtc_gpio_deinit(PIN_WAKE_BUTTON);
  rtc_gpio_init(PIN_WAKE_BUTTON);
  rtc_gpio_set_direction(PIN_WAKE_BUTTON, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(PIN_WAKE_BUTTON);
  rtc_gpio_pulldown_dis(PIN_WAKE_BUTTON);

  const esp_err_t rtcPowerStatus =
      esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  const esp_err_t rtcMemoryStatus =
      esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
  const esp_err_t timerWakeStatus =
      esp_sleep_enable_timer_wakeup(timerWakeDurationUs);
  const esp_err_t ext0WakeStatus =
      esp_sleep_enable_ext0_wakeup(PIN_WAKE_BUTTON, 0);

  if (rtcPowerStatus != ESP_OK || rtcMemoryStatus != ESP_OK ||
      timerWakeStatus != ESP_OK || ext0WakeStatus != ESP_OK) {
    Serial.printf("[POWER] Konfigurasi sleep gagal: RTC_PERIPH=%s, RTC_SLOW_MEM=%s, TIMER=%s, EXT0=%s\n",
                  esp_err_to_name(rtcPowerStatus),
                  esp_err_to_name(rtcMemoryStatus),
                  esp_err_to_name(timerWakeStatus),
                  esp_err_to_name(ext0WakeStatus));
    Serial.println("[POWER] Deep Sleep dibatalkan; task dilanjutkan.");
    digitalWrite(PIN_STATUS_LED, HIGH);
    sleepScheduled = false;
    if (taskSensorHandle) vTaskResume(taskSensorHandle);
    if (taskVisualHandle) vTaskResume(taskVisualHandle);
    Serial.flush();
    return;
  }

  esp_deep_sleep_start();

  Serial.println("[POWER] Deep Sleep kembali tanpa reset; membatalkan siklus.");
  digitalWrite(PIN_STATUS_LED, HIGH);
  sleepScheduled = false;
  if (taskSensorHandle) vTaskResume(taskSensorHandle);
  if (taskVisualHandle) vTaskResume(taskVisualHandle);
}

void setup() {
  Serial.begin(115200);
  delay(250);

  // Capture the previous cycle's readings before the current task updates RTC values.
  const float suhuSiklusSebelumnya = riwayatSuhuTerakhir;
  const float tekananSiklusSebelumnya = riwayatTekananTerakhir;

  penyebabWakeup = esp_sleep_get_wakeup_cause();
  Serial.printf("[RESET] Penyebab reset: %s\n",
                labelResetReason(esp_reset_reason()));
  if (penyebabWakeup == ESP_SLEEP_WAKEUP_EXT0) {
    rtc_gpio_deinit(PIN_WAKE_BUTTON);
  }
  ++nomorSiklusBoot;
  cetakDiagnostikWakeup();

  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, HIGH);
  pinMode(static_cast<uint8_t>(PIN_WAKE_BUTTON), INPUT_PULLUP);
  tungguTombolDilepas();

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);

  mutexData = xSemaphoreCreateMutex();
  mutexI2C = xSemaphoreCreateMutex();

  if (mutexData == nullptr || mutexI2C == nullptr) {
    Serial.println("[FATAL] Gagal membuat mutex FreeRTOS.");
    while (true) delay(1000);
  }

  if (xSemaphoreTake(mutexData, portMAX_DELAY) == pdTRUE) {
    dataCuaca.suhuSiklusSebelumnya = suhuSiklusSebelumnya;
    dataCuaca.tekananSiklusSebelumnya = tekananSiklusSebelumnya;
    dataCuaca.suhuMinimum = (suhuMin <= 900.0f) ? suhuMin : NAN;
    dataCuaca.suhuMaksimum = (suhuMaks >= -900.0f) ? suhuMaks : NAN;
    xSemaphoreGive(mutexData);
  }

  scanI2CBus();

  oledReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  Serial.println(oledReady
      ? "[OLED] SSD1306 berhasil diinisialisasi pada 0x3C."
      : "[OLED] Gagal inisialisasi pada 0x3C.");

  sensorReady = bmp280.begin(BMP_ADDRESS_PRIMARY);
  if (sensorReady) {
    sensorAddressDetected = BMP_ADDRESS_PRIMARY;
    sensorKind = SensorKind::BMP280;
  } else {
    sensorReady = bmp280.begin(BMP_ADDRESS_ALTERNATE);
    if (sensorReady) {
      sensorAddressDetected = BMP_ADDRESS_ALTERNATE;
      sensorKind = SensorKind::BMP280;
    }
  }

  if (!sensorReady && bmp180.begin(BMP085_ULTRAHIGHRES)) {
    sensorReady = true;
    sensorAddressDetected = BMP_ADDRESS_ALTERNATE;
    sensorKind = SensorKind::BMP180;
  }

  if (sensorKind == SensorKind::BMP280) {
    bmp280.setSampling(
        Adafruit_BMP280::MODE_FORCED,
        Adafruit_BMP280::SAMPLING_X2,
        Adafruit_BMP280::SAMPLING_X16,
        Adafruit_BMP280::FILTER_X16,
        Adafruit_BMP280::STANDBY_MS_500);
    Serial.printf("[BMP280] Sensor berhasil diinisialisasi pada 0x%02X.\n",
                  sensorAddressDetected);
  } else if (sensorKind == SensorKind::BMP180) {
    Serial.printf("[BMP180] Sensor berhasil diinisialisasi pada 0x%02X.\n",
                  sensorAddressDetected);
  } else {
    Serial.println("[SENSOR] BMP180/BMP280 tidak ditemukan pada 0x76 maupun 0x77.");
  }

  waktuMulaiAktif = millis();

  const BaseType_t sensorTaskStatus = xTaskCreatePinnedToCore(
      taskSensor, "TaskSensor", 4096, nullptr, 2, &taskSensorHandle, 1);

  const BaseType_t visualTaskStatus = xTaskCreatePinnedToCore(
      taskVisualisasiDanSerial, "TaskVisualJSON", 6144, nullptr, 1,
      &taskVisualHandle, 0);

  Serial.printf("[RTOS] TaskSensor Core 1: %s\n",
                sensorTaskStatus == pdPASS ? "CREATED" : "FAILED");
  Serial.printf("[RTOS] TaskVisualJSON Core 0: %s\n",
                visualTaskStatus == pdPASS ? "CREATED" : "FAILED");

  if (sensorTaskStatus != pdPASS || visualTaskStatus != pdPASS) {
    Serial.println("[FATAL] Pembuatan task gagal; Deep Sleep dibatalkan.");
    while (true) delay(1000);
  }

  Serial.printf("[SYSTEM] Monitoring aktif %lu ms sebelum Deep Sleep.\n",
                static_cast<unsigned long>(ACTIVE_WINDOW_MS));
}

void loop() {
  if (!sleepScheduled &&
      millis() - waktuMulaiAktif >= ACTIVE_WINDOW_MS) {
    masukDeepSleep();
  }
  delay(20);
}
