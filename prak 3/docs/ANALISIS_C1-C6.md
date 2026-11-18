# Bahan Jawaban Analisis C1–C6

Jawaban ini mengikuti pertanyaan pada halaman 6–7 panduan praktikum. Untuk pertanyaan yang meminta hasil pengamatan, masukkan angka dan bukti yang benar-benar diperoleh saat simulasi.

## C1 — Arus aktif dan Deep Sleep
Panduan menyebut contoh arus Deep Sleep sekitar 10–150 µA. Arus aktif tidak memiliki satu angka universal karena bergantung pada board, sensor, LED, periferal, dan radio yang aktif. Pada Deep Sleep, CPU utama dan sebagian besar periferal digital dimatikan; RTC controller, RTC memory yang dipertahankan, dan sumber wakeup terpilih tetap aktif. Pengukuran aktual perlu dilakukan pada board yang digunakan.

## C2 — RTC_DATA_ATTR
Deep Sleep membangunkan ESP32 melalui proses reset, sehingga variabel global biasa diinisialisasi ulang. `RTC_DATA_ATTR` menempatkan variabel pada RTC slow memory yang tetap diberi daya selama Deep Sleep, sehingga nilainya bertahan selama domain RTC tidak kehilangan daya.

## C3 — Arus rata-rata dan estimasi baterai
Diketahui: waktu tidur 899 s, arus tidur 40 µA = 0,040 mA; waktu aktif 1 s, arus aktif 85 mA; total siklus 900 s; kapasitas baterai 3000 mAh; efisiensi 0,80.

`Iavg = (Iaktif × taktif + Itidur × tt tidur) / Tsiklus`

`Iavg = (85 × 1 + 0,040 × 899) / 900`
`Iavg = 120,96 / 900 = 0,1344 mA`.

Kapasitas efektif = `3000 × 0,80 = 2400 mAh`.
Estimasi durasi = `2400 / 0,1344 = 17.857 jam`, sekitar 744 hari atau 24,5 bulan (menggunakan 30,44 hari/bulan). Ini estimasi ideal; rugi regulator, self-discharge, suhu, dan beban eksternal dapat mengurangi masa pakai.

Perhitungan di atas memakai parameter skenario soal, bukan waktu pada firmware demo. Build perangkat fisik memakai jendela aktif 5 detik dan Deep Sleep 10 detik; build Wokwi mengemulasikan jeda 10 detik lalu menjalankan Deep Sleep nyata 100 ms untuk menghindari reset watchdog simulator. Jika angka arus yang sama diterapkan pada jadwal fisik tersebut, perkiraan sederhana menjadi `Iavg = (85 × 5 + 0,040 × 10) / 15 = 28,36 mA`, atau sekitar `2400 / 28,36 = 84,6 jam`. Angka ini hanya ilustrasi berdasarkan asumsi arus; bukan hasil pengukuran dan belum memperhitungkan waktu inisialisasi sebelum jendela aktif.

## C4 — FreeRTOS dual-core
Task sensor dan visualisasi dipisahkan agar operasi OLED/serial yang relatif lambat tidak menahan jalur akuisisi sensor. Task dapat dijadwalkan pada core berbeda. Karena sensor barometrik dan OLED berbagi bus I2C, mutex I2C diperlukan agar transaksi tidak bertabrakan; mutex data menjaga snapshot telemetri konsisten. Implementasi paket menjalankan TaskSensor pada Core 1 dan TaskVisualJSON pada Core 0. Diagram Wokwi memakai BMP180; BMP280/BME280 fisik tetap diperlukan bila checklist PDF menuntut tipe sensor itu secara persis.

## C5 — EXT0 dibanding EXT1
- EXT0 memakai satu RTC GPIO dan satu level logika; sederhana untuk satu tombol atau satu sumber wakeup.
- EXT1 memantau beberapa RTC GPIO dalam satu konfigurasi dan mendukung kondisi kombinasi tertentu (ANY_HIGH atau ALL_LOW pada target ESP32 klasik), sehingga lebih sesuai untuk beberapa sensor ambang.
- Untuk tiga sensor muka air, pilih pin RTC GPIO yang didukung, tentukan polaritas dan mode kombinasi, lalu baca masing-masing sensor setelah bangun untuk mengidentifikasi sumber alarm. Dukungan pin dan mode harus diverifikasi terhadap varian ESP32 yang dipakai.

## C6 — Penyimpan ekstrem cuaca
Firmware menambahkan `RTC_DATA_ATTR float suhuMaks` dan `suhuMin`. Nilai diperbarui setiap sampel BMP180/BMP280 valid, disalin ke snapshot data dengan mutex, lalu ditampilkan pada baris bawah OLED. Bukti harus diambil setelah mengubah suhu sensor pada beberapa siklus: screenshot OLED dan Serial Monitor yang memperlihatkan boot count serta nilai minimum/maksimum. Rekor RTC dapat hilang ketika daya RTC memory terputus.
