# Arsitektur Sistem

## Gambaran Umum

ESP32 berperan sebagai pengendali utama. Sistem menggabungkan informasi waktu dari RTC, deteksi gerakan dari PIR, serta pemantauan suhu dan kelembapan dari DHT22.

## Komponen dan Peran

| Komponen | Peran |
|---|---|
| ESP32 | Membaca input, menjalankan logika, dan mengendalikan output |
| PIR HC-SR501 | Mendeteksi gerakan |
| DHT22 | Mengukur suhu dan kelembapan |
| RTC DS1307 | Menyediakan informasi waktu untuk jadwal |
| Relay | Aktuator yang dikendalikan sistem |
| LCD I2C 16x2 | Menampilkan informasi sistem |
| LED dan buzzer | Indikator status sesuai implementasi program |

## Logika Kendali

Relay hanya diaktifkan ketika dua kondisi terpenuhi bersamaan:

`jadwalAktif == true` **dan** `adaGerakan == true`.

DHT22 digunakan untuk pemantauan lingkungan dan tidak menjadi syarat aktivasi relay, kecuali logika program diubah.

## Antarmuka

- RTC dan LCD menggunakan komunikasi I2C.
- PIR memberikan sinyal digital ke ESP32.
- DHT22 menggunakan jalur data digital.
- ESP32 mengendalikan input relay dan indikator.

Untuk pemetaan GPIO yang tepat, lihat konfigurasi aktual pada `diagram.json` dan `sketch.ino`.
