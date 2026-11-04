# Rencana Pengujian Sistem

Dokumen ini merupakan template pencatatan pengujian Projek-IOT. Isi kolom hasil aktual berdasarkan simulasi Wokwi atau pengujian perangkat fisik. Jangan menganggap pengujian berhasil sebelum benar-benar dilakukan.

## Tujuan

Memeriksa pembacaan sensor, fungsi waktu RTC, tampilan LCD, indikator, dan kendali relay.

## Skenario Pengujian

| No. | Kondisi | Hasil yang Diharapkan | Hasil Aktual | Status |
|---|---|---|---|---|
| 1 | Sistem dinyalakan | ESP32 berjalan dan LCD menampilkan informasi | Belum diuji | Belum diuji |
| 2 | PIR tidak mendeteksi gerakan saat jadwal aktif | Relay tidak aktif | Belum diuji | Belum diuji |
| 3 | PIR mendeteksi gerakan saat jadwal tidak aktif | Relay tidak aktif | Belum diuji | Belum diuji |
| 4 | PIR mendeteksi gerakan saat jadwal aktif | Relay aktif | Belum diuji | Belum diuji |
| 5 | Waktu berada di luar jadwal | Relay tidak aktif | Belum diuji | Belum diuji |
| 6 | DHT22 membaca suhu dan kelembapan | Nilai sensor dapat dibaca dan ditampilkan sesuai implementasi | Belum diuji | Belum diuji |
| 7 | RTC menyediakan waktu | Waktu sistem sesuai pengaturan RTC | Belum diuji | Belum diuji |

## Kriteria Lulus

- Relay hanya aktif ketika jadwal aktif dan PIR mendeteksi gerakan.
- Relay nonaktif jika salah satu kondisi tersebut tidak terpenuhi.
- RTC memberikan waktu yang benar.
- Pembacaan DHT22 dan tampilan LCD berjalan sesuai implementasi kode.

## Catatan Pengujian

- Tanggal pengujian:
- Penguji:
- Platform: Wokwi / perangkat fisik
- Tautan simulasi atau bukti:
- Catatan kendala:

Perbarui kolom hasil aktual dan status setelah setiap skenario benar-benar diuji.
