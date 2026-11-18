/* ============================================================
   Lab-2 IoT UNIB — Minggu 3 (Sub-CPMK 2.1, C3/P3)
   Firmware ESP32: I2C multi-sensor + OLED + FreeRTOS Dual-Core
   + ISR aman + Deep Sleep + RTC_DATA_ATTR
   ============================================================ */
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SSD1306.h>
#include <esp_sleep.h>
#include "driver/rtc_io.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// ================= KONFIGURASI =================
#define I2C_SDA        21
#define I2C_SCL        22
#define BTN_EXT0_PIN   15
#define OLED_ADDR      0x3C
#define BME_ADDR       0x76

#define SCREEN_W       128
#define SCREEN_H       64

// Demo Deep Sleep: 1 = aktif (bawaan Lab-2), 0 = streaming terus
// Set ke 0 saat integrasi dengan gateway Python Minggu 4.
#define ENABLE_DEEP_SLEEP    1
#define ACTIVE_TIME_MS       15000      // 15 detik aktif sebelum tidur
#define SLEEP_INTERVAL_US    10ULL * 1000000ULL  // 10 detik tidur

// ================= OBJEK =================
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
Adafruit_BME280  bme;
bool bmeOK  = false;
bool oledOK = false;

// ================= RTC DATA (SURVIVE DEEP SLEEP) =================
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int seqRTC    = 0;

// ================= INTERRUPT FLAG =================
volatile bool          flagTombol = false;
volatile unsigned long tTerakhir  = 0;

// ================= SHARED DATA ANTAR TASK =================
struct SensorData {
    float suhu;
    float tekanan;
    float altitude;
    int   seq;
};
SensorData        sharedData = {0, 0, 0, 0};
SemaphoreHandle_t dataMutex;

// =====================================================
// ISR — IRAM_ATTR, volatile, singkat, tanpa delay/Serial
// =====================================================
void IRAM_ATTR isrTombolDarurat() {
    unsigned long t = millis();
    if (t - tTerakhir > 50) {       // debounce 50 ms
        flagTombol = true;
        tTerakhir  = t;
    }
}

// =====================================================
// Rumus barometrik: h = 44330 * (1 - (P/P0)^(1/5.255))
// =====================================================
static float hitungAltitude(float tekanan_hPa) {
    return 44330.0f * (1.0f - powf(tekanan_hPa / 1013.25f, 1.0f / 5.255f));
}

// =====================================================
// TASK CORE 1 — Akuisisi BME280 (non-blocking)
// =====================================================
void taskSensor(void *pv) {
    Serial.printf("[SensorTask] running on Core %d\n", xPortGetCoreID());
    int readCount = 0;
    for (;;) {
        if (bmeOK) {
            float t = bme.readTemperature();
            float p = bme.readPressure() / 100.0F;
            float a = hitungAltitude(p);
            readCount++;

            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                sharedData = { t, p, a, readCount };
                xSemaphoreGive(dataMutex);
            }

            // Satu baris JSON — dipakai oleh gateway Python Minggu 4
            Serial.printf(
                "{\"device_id\":\"esp32-01\",\"boot\":%d,\"seq\":%d,"
                "\"suhu\":%.2f,\"tekanan\":%.2f,\"altitude\":%.1f}\n",
                bootCount, readCount, t, p, a);
        } else {
            Serial.println("[SensorTask] BME280 tidak terdeteksi!");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// =====================================================
// TASK CORE 0 — Render OLED (buffer 1 KB, flush 1x)
// =====================================================
void taskDisplay(void *pv) {
    Serial.printf("[DisplayTask] running on Core %d\n", xPortGetCoreID());
    for (;;) {
        if (oledOK) {
            SensorData d;
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                d = sharedData;
                xSemaphoreGive(dataMutex);
            }
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 0);  display.printf("Boot #%d  Seq #%d", bootCount, d.seq);
            display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
            display.setCursor(0, 16); display.printf("Suhu   : %.2f C",  d.suhu);
            display.setCursor(0, 28); display.printf("Tekanan: %.2f hP", d.tekanan);
            display.setCursor(0, 40); display.printf("Altitud: %.1f m",  d.altitude);
            display.setCursor(0, 54); display.printf("BTN    : %s", flagTombol ? "YES" : "no");
            display.display();   // flush 1 kali (efisien)
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// =====================================================
// SETUP — dipanggil ulang setelah tiap wakeup
// =====================================================
void setup() {
    Serial.begin(115200);
    delay(300);
    bootCount++;
    seqRTC++;
    Serial.printf("\n=== BOOT ke-%d ===\n", bootCount);

    // --- Cek sumber wakeup ---
    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_TIMER: Serial.println("[WAKE] Timer RTC"); break;
        case ESP_SLEEP_WAKEUP_EXT0:  Serial.println("[WAKE] EXT0 (GPIO 15)"); break;
        default:                     Serial.println("[WAKE] Cold boot / reset"); break;
    }

    // --- I2C + Scanner ---
    Wire.begin(I2C_SDA, I2C_SCL);
    Serial.println("[I2C] Scan bus...");
    for (uint8_t a = 1; a < 127; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0)
            Serial.printf("  found 0x%02X\n", a);
    }

    // --- BME280 ---
    bmeOK = bme.begin(BME_ADDR);
    Serial.printf("[BME280] %s @0x%02X\n", bmeOK ? "OK" : "FAIL", BME_ADDR);

    // --- OLED ---
    oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    Serial.printf("[OLED]   %s @0x%02X\n", oledOK ? "OK" : "FAIL", OLED_ADDR);
    if (oledOK) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("Lab-2 IoT UNIB");
        display.println("I2C+RTOS+Sleep");
        display.display();
    }

    // --- Mutex & Task ---
    dataMutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(taskSensor,  "SensorTask",  4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(taskDisplay, "DisplayTask", 4096, NULL, 1, NULL, 0);

    // --- Interrupt tombol EXT0 ---
    pinMode(BTN_EXT0_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_EXT0_PIN), isrTombolDarurat, FALLING);
    Serial.println("[ISR] GPIO 15 FALLING");

    // --- Konfigurasi RTC pin untuk EXT0 (pull-up bertahan saat sleep) ---
    rtc_gpio_pullup_en(GPIO_NUM_15);
    rtc_gpio_pulldown_dis(GPIO_NUM_15);
}

// =====================================================
// LOOP — dijalankan di Core 1 (default Arduino)
// =====================================================
void loop() {
#if ENABLE_DEEP_SLEEP
    vTaskDelay(pdMS_TO_TICKS(ACTIVE_TIME_MS));
    Serial.println("\n[SLEEP] Masuk Deep Sleep...");
    Serial.printf("[SLEEP] Timer %llu detik + EXT0 GPIO %d\n",
                  SLEEP_INTERVAL_US / 1000000ULL, BTN_EXT0_PIN);

    esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_US);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 0);   // 0 = LOW saat tombol ditekan
    esp_deep_sleep_start();
#else
    vTaskDelay(pdMS_TO_TICKS(1000));                // streaming mode
#endif
}