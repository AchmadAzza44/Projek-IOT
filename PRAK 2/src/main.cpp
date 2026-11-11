
#include <Arduino.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ==========================================
// 1. KONFIGURASI PIN ESP32
// ==========================================

#define PIN_DHT            4
#define TIPE_DHT           DHT22

#define PIN_SOIL_ADC1      34
#define PIN_TOMBOL_MODE    15
#define PIN_RELAY_POMPA    26
#define PIN_PWM_KIPAS      18
#define PIN_LED_ALERT      19

// 0 = Ambang tunggal (chattering)
// 1 = Histeresis
#define GUNAKAN_HISTERESIS 1

// ==========================================
// 2. KONFIGURASI SENSOR TANAH
// ==========================================

const int MV_TANAH_KERING = 2650;
const int MV_TANAH_BASAH  = 1100;

const float AMBANG_POMPA_ON  = 35.0;
const float AMBANG_POMPA_OFF = 60.0;
const float AMBANG_TUNGGAL   = 45.0;

// ==========================================
// 3. KONFIGURASI PWM LED
// ==========================================

const int PWM_FREQ_HZ = 5000;
const int PWM_RES_BIT = 8;
const int PWM_KANAL   = 0;

// ==========================================
// 4. FILTER MOVING AVERAGE
// ==========================================

const int UKURAN_FILTER = 5;

uint32_t bufferMv[UKURAN_FILTER] = {0};
int indeksFilter = 0;
bool filterPenuh = false;

// ==========================================
// 5. VARIABEL SISTEM
// ==========================================

DHT dht(PIN_DHT, TIPE_DHT);

unsigned long waktuTelemetriTerakhir = 0;
unsigned long waktuDebounceTombol = 0;

const unsigned long INTERVAL_BACA_MS = 1000;

bool statusPompa = false;
bool modeOverridePaksa = false;
bool tombolSebelumnya = HIGH;

unsigned long seqPaket = 0;
unsigned long hitungTransisiRelai = 0;

// ==========================================
// 6. VARIABEL DRY-RUN PROTECTION (C6)
// ==========================================

const unsigned long BATAS_POMPA_MS = 15000;
const unsigned long INTERVAL_KEDIP_MS = 300;
const float MIN_KENAIKAN_SOIL = 1.0;

bool faultDryRun = false;
bool pemantauanPompaAktif = false;

unsigned long waktuMulaiPompa = 0;
unsigned long waktuKedipTerakhir = 0;

float soilPctAwalPompa = 0.0;
bool statusLedAlert = false;

// ==========================================
// 7. FUNGSI KONFIGURASI PWM
// ==========================================

void setupPwmKipas() {

#if defined(ESP_ARDUINO_VERSION) && \
    (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))

  ledcAttach(PIN_PWM_KIPAS, PWM_FREQ_HZ, PWM_RES_BIT);

#else

  ledcSetup(PWM_KANAL, PWM_FREQ_HZ, PWM_RES_BIT);
  ledcAttachPin(PIN_PWM_KIPAS, PWM_KANAL);

#endif
}

void tulisPwmKipas(uint8_t duty) {

#if defined(ESP_ARDUINO_VERSION) && \
    (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))

  ledcWrite(PIN_PWM_KIPAS, duty);

#else

  ledcWrite(PWM_KANAL, duty);

#endif
}

// ==========================================
// 8. PEMBACAAN SENSOR TANAH
//    MOVING AVERAGE 5 SAMPEL
// ==========================================

uint32_t bacaSoilMilliVoltsTerfilter() {

  uint32_t mvMentah = analogReadMilliVolts(PIN_SOIL_ADC1);

  bufferMv[indeksFilter] = mvMentah;

  indeksFilter = (indeksFilter + 1) % UKURAN_FILTER;

  if (indeksFilter == 0) {
    filterPenuh = true;
  }

  int jumlahSampel = filterPenuh
                     ? UKURAN_FILTER
                     : indeksFilter;

  uint64_t total = 0;

  for (int i = 0; i < jumlahSampel; i++) {
    total += bufferMv[i];
  }

  return (uint32_t)(total / jumlahSampel);
}

// ==========================================
// 9. LED PERINGATAN DRY-RUN
// ==========================================

void kendaliLedPeringatan() {

  if (!faultDryRun) {
    digitalWrite(PIN_LED_ALERT, LOW);
    statusLedAlert = false;
    return;
  }

  unsigned long sekarang = millis();

  if (sekarang - waktuKedipTerakhir >= INTERVAL_KEDIP_MS) {

    waktuKedipTerakhir = sekarang;
    statusLedAlert = !statusLedAlert;

    digitalWrite(
      PIN_LED_ALERT,
      statusLedAlert ? HIGH : LOW
    );
  }
}

// ==========================================
// 10. SETUP
// ==========================================

void setup() {

  Serial.begin(115200);

  pinMode(PIN_SOIL_ADC1, INPUT);
  pinMode(PIN_TOMBOL_MODE, INPUT_PULLUP);
  pinMode(PIN_RELAY_POMPA, OUTPUT);
  pinMode(PIN_LED_ALERT, OUTPUT);

  digitalWrite(PIN_RELAY_POMPA, LOW);
  digitalWrite(PIN_LED_ALERT, LOW);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_SOIL_ADC1, ADC_11db);

  setupPwmKipas();

  dht.begin();

  Serial.println();
  Serial.println(
    "{\"event\":\"boot\",\"lab\":\"Lab-01 Greenhouse Kopi UNIB\"}"
  );
}

// ==========================================
// 11. LOOP UTAMA
// ==========================================

void loop() {

  unsigned long sekarang = millis();

  // ----------------------------------------
  // A. TOMBOL OVERRIDE MANUAL
  // ----------------------------------------

  bool bacaTombol = digitalRead(PIN_TOMBOL_MODE);

  if (bacaTombol == LOW &&
      tombolSebelumnya == HIGH &&
      (sekarang - waktuDebounceTombol > 60)) {

    waktuDebounceTombol = sekarang;

    modeOverridePaksa = !modeOverridePaksa;

    Serial.printf(
      "{\"event\":\"mode_change\",\"manual_override\":%s}\n",
      modeOverridePaksa ? "true" : "false"
    );
  }

  tombolSebelumnya = bacaTombol;

  // ----------------------------------------
  // B. KEDIPKAN LED PERINGATAN
  // ----------------------------------------

  kendaliLedPeringatan();

  // ----------------------------------------
  // C. PEMBACAAN SENSOR SETIAP 1 DETIK
  // ----------------------------------------

  if (sekarang - waktuTelemetriTerakhir >= INTERVAL_BACA_MS) {

    waktuTelemetriTerakhir = sekarang;

    int adcRaw = analogRead(PIN_SOIL_ADC1);

    uint32_t soilMv = bacaSoilMilliVoltsTerfilter();

    // Kalibrasi terbalik 0% - 100%
    float soilPct =
      (float)(MV_TANAH_KERING - (int)soilMv) * 100.0f /
      (float)(MV_TANAH_KERING - MV_TANAH_BASAH);

    soilPct = constrain(soilPct, 0.0f, 100.0f);

    // --------------------------------------
    // D. PEMBACAAN DHT22
    // --------------------------------------

    float suhuUdara = dht.readTemperature();
    float lembapUdara = dht.readHumidity();

    bool dhtValid = !isnan(suhuUdara) && !isnan(lembapUdara);

    if (!dhtValid) {
      Serial.println("{\"error\":\"Pembacaan DHT22 gagal\"}");
    }

    // --------------------------------------
    // E. KENDALI POMPA
    // --------------------------------------

    bool pompaSebelum = statusPompa;
    bool permintaanPompa = statusPompa;

    if (modeOverridePaksa) {

      permintaanPompa = true;

    } else {

#if GUNAKAN_HISTERESIS == 1

      if (soilPct < AMBANG_POMPA_ON) {
        permintaanPompa = true;
      }
      else if (soilPct >= AMBANG_POMPA_OFF) {
        permintaanPompa = false;
      }

#else

      permintaanPompa = (soilPct < AMBANG_TUNGGAL);

#endif

    }

    // --------------------------------------
    // F. DRY-RUN PROTECTION (C6)
    // --------------------------------------

    // Fault yang sudah terjadi mengunci pompa OFF
    if (faultDryRun) {

      permintaanPompa = false;
      pemantauanPompaAktif = false;

    }
    else if (permintaanPompa) {

      // Pompa baru mulai menyala
      if (!pemantauanPompaAktif) {

        pemantauanPompaAktif = true;
        waktuMulaiPompa = millis();
        soilPctAwalPompa = soilPct;

      }
      else {

        // Periksa apakah kelembapan meningkat
        if (soilPct >= soilPctAwalPompa + MIN_KENAIKAN_SOIL) {

          // Respons terdeteksi, mulai pemantauan ulang
          soilPctAwalPompa = soilPct;
          waktuMulaiPompa = millis();

        }
        else if (millis() - waktuMulaiPompa >= BATAS_POMPA_MS) {

          // Tidak ada peningkatan selama 15 detik
          faultDryRun = true;

          permintaanPompa = false;
          pemantauanPompaAktif = false;

          Serial.println(
            "{\"event\":\"safety_cutoff\","
            "\"fault\":\"DRY_RUN_PROTECTION\","
            "\"pump_on\":false}"
          );
        }
      }

    }
    else {

      // Pompa berhenti, hentikan pemantauan
      pemantauanPompaAktif = false;

    }

    // Terapkan status pompa akhir
    statusPompa = permintaanPompa;

    if (statusPompa != pompaSebelum) {
      hitungTransisiRelai++;
    }

    digitalWrite(
      PIN_RELAY_POMPA,
      statusPompa ? HIGH : LOW
    );

    // --------------------------------------
    // G. KENDALI PWM BERDASARKAN SUHU
    // --------------------------------------

    int nilaiDuty = 0;

    if (dhtValid) {

      nilaiDuty = map(
        (long)(suhuUdara * 10.0f),
        280,
        380,
        0,
        255
      );

      nilaiDuty = constrain(nilaiDuty, 0, 255);

      tulisPwmKipas((uint8_t)nilaiDuty);
    }

    float fanPct = (nilaiDuty * 100.0f) / 255.0f;

    // --------------------------------------
    // H. TELEMETRI JSON
    // --------------------------------------

    JsonDocument doc;

    doc["device_id"] = "esp32-kopi-gh01";
    doc["seq"] = ++seqPaket;

    doc["adc_raw"] = adcRaw;
    doc["soil_mv_avg"] = soilMv;

    doc["soil_pct"] = serialized(String(soilPct, 1));

    if (dhtValid) {
      doc["temp_c"] = serialized(String(suhuUdara, 1));
      doc["air_hum_pct"] = serialized(String(lembapUdara, 1));
    } else {
      doc["temp_c"] = nullptr;
      doc["air_hum_pct"] = nullptr;
    }

    doc["pump_on"] = statusPompa;
    doc["relay_switches"] = hitungTransisiRelai;

    doc["fan_pwm_pct"] = serialized(String(fanPct, 1));

    doc["ctrl_mode"] = modeOverridePaksa
                       ? "MANUAL"
                       : (GUNAKAN_HISTERESIS
                          ? "HYSTERESIS"
                          : "SINGLE_THR");

    doc["fault"] = faultDryRun
                   ? "DRY_RUN_PROTECTION"
                   : "NONE";

    doc["warning_led"] = statusLedAlert;

    doc["pump_runtime_ms"] = pemantauanPompaAktif
                             ? millis() - waktuMulaiPompa
                             : 0;

    serializeJson(doc, Serial);
    Serial.println();
  }
}
