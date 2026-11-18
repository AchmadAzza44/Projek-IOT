# UNIB IoT Lab-2 — I2C, OLED SSD1306, FreeRTOS & Deep Sleep

**Release:** 1.1.0  
**Target:** DOIT ESP32 DevKit V1 (classic ESP32)  
**Basis:** Panduan Praktikum Minggu 3 — Teknologi IoT, Prodi S1 Informatika FT UNIB.

## Fitur sesuai modul
- I2C Scanner: mendeteksi alamat peranti pada SDA GPIO21 dan SCL GPIO22.
- BMP180 untuk simulasi Wokwi pada 0x77; firmware juga mendukung BMP280 pada 0x76/0x77.
- OLED SSD1306 128×64 pada alamat 0x3C.
- FreeRTOS dual-core: TaskSensor pada Core 1; TaskVisualJSON pada Core 0.
- Mutex terpisah untuk telemetri dan transaksi bus I2C.
- RTC Timer wakeup 10 detik dan EXT0 GPIO15 aktif LOW.
- RTC_DATA_ATTR: boot count, pembacaan terakhir, suhu minimum dan maksimum.
- JSON Serial yang valid: nilai yang belum tersedia dikirim sebagai JSON null, bukan token NaN.
- Tombol EXT0 release guard agar tombol yang masih ditahan tidak memicu siklus wake/sleep berulang.

## Wiring
| Komponen | ESP32 |
|---|---|
| BMP180/BMP280 SDA + OLED SDA | GPIO21 |
| BMP180/BMP280 SCL + OLED SCL | GPIO22 |
| Sensor/OLED VCC | 3V3 |
| Sensor/OLED GND | GND |
| Tombol EXT0 | GPIO15 ke GND |
| LED biru status | GPIO2 melalui resistor 220 Ω |

Kedua perangkat I2C berbagi bus. BMP180 memakai alamat 0x77; BMP280 memakai 0x76 atau 0x77; OLED memakai 0x3C. Catu modul sesuai panduan adalah 3V3, bukan 5V/VIN.

**Catatan kesesuaian praktikum:** diagram Wokwi menggunakan BMP180 karena itulah sensor yang tersedia pada akun Wokwi gratis ini. BMP180 dapat mendemonstrasikan pembacaan suhu, tekanan, elevasi, dan I2C, tetapi bukan pengganti persis BMP280/BME280 pada checklist PDF. Untuk memenuhi butir sensor secara ketat, verifikasi juga dengan BMP280 atau BME280 fisik.

## Struktur
```text
├── diagram.json
├── sketch.ino
├── platformio.ini
├── wokwi.toml
├── libraries.txt
├── PROJECT_INFO.txt
├── README.md
├── src/main.cpp
├── examples/i2c_scanner/i2c_scanner.ino
└── docs/
    ├── ARCHITECTURE.md
    ├── TEST_SCENARIOS.md
    ├── VALIDATION_REPORT.md
    └── ANALISIS_C1-C6.md
```

## Menjalankan dengan PlatformIO
1. Ekstrak ZIP.
2. Buka folder project di VS Code dengan PlatformIO.
3. Build environment `wokwi` untuk simulasi, atau `esp32doit-devkit-v1` untuk firmware perangkat fisik.
4. Upload hanya environment perangkat fisik ke board; Wokwi membaca firmware dari environment `wokwi`.
5. Buka Serial Monitor pada 115200 baud.

## Menjalankan di Wokwi
1. Buat project ESP32 baru.
2. Salin `diagram.json` dan `sketch.ino` dari paket.
3. Pastikan dependensi pustaka tersedia dari `libraries.txt`.
4. Mulai simulasi dan buka Serial Monitor.
5. Untuk diagram Wokwi ini, verifikasi scanner menampilkan `0x3C` dan `0x77` (BMP180).
6. Amati OLED dan JSON. Pada build Wokwi, fase tidur 10 detik diemulasikan untuk menghindari reset watchdog simulator; firmware lalu menjalankan Deep Sleep nyata selama 100 ms untuk menguji reset, timer, dan RTC.
7. Untuk menguji EXT0 di Wokwi, tahan tombol GPIO15 sampai perangkat boot ulang. Pada perangkat fisik, tombol GPIO15 membangunkan ESP32 dari Deep Sleep sungguhan.

## Siklus operasi
- Saat boot, firmware mencatat penyebab wakeup dan mengambil snapshot nilai RTC dari siklus sebelumnya.
- Scanner memeriksa bus I2C.
- OLED dan sensor barometrik yang terdeteksi (BMP180 atau BMP280) diinisialisasi.
- TaskSensor berjalan pada Core 1 dan memperbarui data sensor serta RTC extrema.
- TaskVisualJSON berjalan pada Core 0, menampilkan OLED dan mencetak JSON.
- Setelah 5 detik aktif, task dihentikan sementara dan LED dimatikan.
- Build fisik masuk Deep Sleep sungguhan selama 10 detik; Timer atau tombol EXT0 dapat membangunkan perangkat.
- Build Wokwi mengemulasikan jeda 10 detik lalu memakai Deep Sleep nyata 100 ms. Jeda emulasi bukan bukti konsumsi daya atau Deep Sleep 10 detik; verifikasi durasi/pengukuran daya pada board fisik.

`ACTIVE_WINDOW_MS` dapat diubah untuk memperpanjang waktu pengamatan. `SLEEP_SECONDS` tetap 10 detik sesuai panduan.

## Pustaka
- Adafruit Unified Sensor ^1.1.14
- Adafruit BMP085 Library ^1.2.4
- Adafruit BMP280 Library ^2.6.8
- Adafruit SSD1306 ^2.5.9
- Adafruit GFX Library ^1.11.9
- ArduinoJson ^7.0.4

## Catatan teknis
- BMP180/BMP280 mengukur suhu dan tekanan; elevasi adalah estimasi relatif terhadap tekanan permukaan laut 1013.25 hPa. Keduanya tidak mengukur kelembapan.
- `RTC_DATA_ATTR` bertahan saat Deep Sleep selama domain RTC tetap mendapat daya; power-cycle penuh dapat menghapus data.
- GPIO15 adalah strapping pin pada ESP32 klasik. Pastikan perilaku board fisik sesuai sebelum dipakai pada produk.
- Tombol harus dilepas setelah wakeup EXT0. Firmware memiliki release guard.
- Ini target untuk ESP32 klasik / DOIT DevKit V1. Jangan menganggap pin dan API EXT0 identik pada ESP32-C3/S2/S3.

## Status verifikasi
Build Wokwi memakai jalur emulasi fase tidur dan Deep Sleep nyata 100 ms; build `esp32doit-devkit-v1` tetap menjalankan Deep Sleep 10 detik untuk perangkat fisik. Wokwi dapat mengalami reset watchdog pada Deep Sleep panjang; karena itu log emulasi tidak boleh dianggap sebagai bukti durasi Deep Sleep 10 detik atau konsumsi daya. Build dan uji runtime setelah perubahan tetap diperlukan. Lihat `docs/VALIDATION_REPORT.md`; jangan menyatakan uji lulus tanpa bukti yang diamati.
