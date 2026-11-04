
#include <Wire.h>
#include <RTClib.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

// =================================
// KONFIGURASI PIN ESP32
// =================================

#define DHT_PIN 4
#define DHT_TYPE DHT22

#define PIR_PIN 27
#define RELAY_PIN 23
#define LED_PIN 19
#define BUZZER_PIN 18

// =================================
// OBJEK KOMPONEN
// =================================

RTC_DS1307 rtc;
DHT dht(DHT_PIN, DHT_TYPE);

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Ubah ke false jika relay bekerja terbalik
const bool RELAY_ACTIVE_LOW = true;

// =================================
// JADWAL OBAT NYAMUK
// =================================

// Jadwal 1: 18.00 - 18.30
// Jadwal 2: 20.00 - 20.30

const int MULAI_1 = 18 * 60;
const int SELESAI_1 = 18 * 60 + 30;

const int MULAI_2 = 20 * 60;
const int SELESAI_2 = 20 * 60 + 30;

// =================================
// VARIABEL
// =================================

float suhu = 0;
float kelembapan = 0;

bool statusRelaySebelumnya = false;

unsigned long waktuSensor = 0;
unsigned long waktuLCD = 0;

bool jadwalAktif(int jam, int menit) {
  int totalMenit = jam * 60 + menit;

  return (
    (totalMenit >= MULAI_1 && totalMenit < SELESAI_1) ||
    (totalMenit >= MULAI_2 && totalMenit < SELESAI_2)
  );
}

// =================================
// KONTROL RELAY
// =================================

void kontrolRelay(bool aktif) {
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(RELAY_PIN, aktif ? LOW : HIGH);
  } else {
    digitalWrite(RELAY_PIN, aktif ? HIGH : LOW);
  }
}

// =================================
// SETUP
// =================================

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Kondisi awal: semua OFF
  kontrolRelay(false);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Inisialisasi I2C
  Wire.begin(21, 22);

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();

  // Inisialisasi DHT22
  dht.begin();

  // Inisialisasi RTC
  if (!rtc.begin()) {
    Serial.println("RTC tidak ditemukan!");

    lcd.setCursor(0, 0);
    lcd.print("RTC ERROR!");

    while (true) {
      kontrolRelay(false);
      delay(100);
    }
  }

  // WAKTU AWAL PENGUJIAN
  // 17:55, 4 November 2026
  // Reset waktu ini setiap ESP32 dimulai ulang.

  rtc.adjust(DateTime(2026, 11, 4, 18, 5, 0));

  lcd.setCursor(0, 0);
  lcd.print("PENGUSIR NYAMUK");

  lcd.setCursor(0, 1);
  lcd.print("Sistem Aktif");

  delay(2000);

  Serial.println("========================");
  Serial.println("SISTEM PENGUSIR NYAMUK");
  Serial.println("========================");
}

// =================================
// LOOP UTAMA
// =================================

void loop() {

  // Membaca waktu RTC
  DateTime sekarang = rtc.now();

  int jam = sekarang.hour();
  int menit = sekarang.minute();

  // Membaca sensor PIR
  bool adaGerakan = digitalRead(PIR_PIN) == HIGH;

  // Memeriksa jadwal
  bool waktuAktif = jadwalAktif(jam, menit);

  // Membaca DHT22 setiap 2 detik
  if (millis() - waktuSensor >= 2000 ||
      waktuSensor == 0) {

    waktuSensor = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      suhu = t;
      kelembapan = h;
    } else {
      Serial.println("Gagal membaca DHT22");
    }
  }

  // =================================
  // LOGIKA UTAMA SISTEM
  // =================================

  // Relay ON hanya jika:
  // 1. Jadwal sedang aktif
  // 2. PIR mendeteksi gerakan

  bool relayAktif = waktuAktif && adaGerakan;

  // Mengendalikan aktuator
  kontrolRelay(relayAktif);

  // LED mengikuti relay
  digitalWrite(LED_PIN, relayAktif ? HIGH : LOW);

  // Buzzer berbunyi ketika status berubah
  if (relayAktif != statusRelaySebelumnya) {

    tone(BUZZER_PIN, 2000, 200);

    statusRelaySebelumnya = relayAktif;

    Serial.println(relayAktif
      ? "OBAT NYAMUK AKTIF"
      : "OBAT NYAMUK NONAKTIF");
  }

  // =================================
  // MENAMPILKAN DATA LCD
  // =================================

  if (millis() - waktuLCD >= 500) {

    waktuLCD = millis();

    // Baris pertama: suhu dan kelembapan
    lcd.setCursor(0, 0);

    lcd.print("T:");
    lcd.print(suhu, 1);
    lcd.print((char)223);
    lcd.print("C H:");
    lcd.print(kelembapan, 0);
    lcd.print("%  ");

    // Baris kedua: waktu dan status relay
    lcd.setCursor(0, 1);

    if (jam < 10) lcd.print("0");
    lcd.print(jam);

    lcd.print(":");

    if (menit < 10) lcd.print("0");
    lcd.print(menit);

    if (relayAktif) {
      lcd.print(" RELAY:ON ");
    } else {
      lcd.print(" RELAY:OFF");
    }
  }

  // =================================
  // SERIAL MONITOR
  // =================================

  static unsigned long waktuSerial = 0;

  if (millis() - waktuSerial >= 2000) {

    waktuSerial = millis();

    Serial.println("--------------------");

    Serial.print("Waktu       : ");
    Serial.print(jam);
    Serial.print(":");
    Serial.println(menit);

    Serial.print("Suhu        : ");
    Serial.print(suhu);
    Serial.println(" C");

    Serial.print("Kelembapan  : ");
    Serial.print(kelembapan);
    Serial.println(" %");

    Serial.print("PIR         : ");
    Serial.println(adaGerakan ? "TERDETEKSI" : "TIDAK ADA");

    Serial.print("Jadwal      : ");
    Serial.println(waktuAktif ? "AKTIF" : "NONAKTIF");

    Serial.print("Relay       : ");
    Serial.println(relayAktif ? "ON" : "OFF");
  }

  delay(100);
}
