# Changelog

Semua perubahan penting pada proyek ini dicatat di dokumen ini.

Format catatan mengikuti prinsip Keep a Changelog. Versi awal ini mendokumentasikan komponen dan fitur proyek yang tersedia; tanggal rilis resmi belum ditetapkan.

## [Unreleased]

### Dokumentasi
- Menambahkan README sebagai dokumentasi utama proyek.
- Menambahkan panduan kontribusi.
- Menambahkan file lisensi MIT.

### Sistem
- Sistem menggunakan ESP32 sebagai mikrokontroler.
- Sensor PIR digunakan untuk mendeteksi gerakan.
- Sensor DHT22 digunakan untuk membaca suhu dan kelembapan.
- RTC DS1307 menyediakan informasi waktu untuk pemeriksaan jadwal.
- Relay dikendalikan berdasarkan kondisi jadwal dan deteksi gerakan.
- LCD I2C digunakan untuk menampilkan informasi sistem.
