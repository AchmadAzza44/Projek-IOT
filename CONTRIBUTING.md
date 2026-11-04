# Panduan Kontribusi

Terima kasih telah tertarik untuk berkontribusi pada **Projek-IOT**. Proyek ini merupakan simulasi sistem IoT berbasis ESP32 yang menggunakan sensor PIR, DHT22, RTC DS1307, LCD I2C, dan relay.

## Cara Berkontribusi

1. **Fork** repository ini ke akun GitHub kamu.
2. Buat branch baru untuk perubahan yang akan dikerjakan.
   ```bash
   git checkout -b fitur/nama-fitur
   ```
3. Lakukan perubahan dan pastikan file terkait tetap konsisten.
4. Uji perubahan pada simulasi Wokwi sebelum mengirimkannya, jika perubahan memengaruhi rangkaian atau program.
5. Commit dengan pesan yang jelas.
   ```bash
   git add .
   git commit -m "feat: jelaskan perubahan"
   ```
6. Push branch ke fork kamu, lalu buat **Pull Request** ke repository ini.

## Pedoman Perubahan

- Pertahankan kompatibilitas kode dengan ESP32 dan konfigurasi pin yang digunakan proyek.
- Jika mengubah pin atau komponen, perbarui `sketch.ino`, `diagram.json`, dan dokumentasi yang relevan.
- Jika menambahkan library, cantumkan pada `libraries.txt`.
- Jelaskan perubahan perilaku sistem, khususnya logika jadwal, sensor PIR, dan relay.
- Jangan menyertakan kredensial, token, atau informasi pribadi di dalam commit.
- Gunakan komentar seperlunya agar kode mudah dipahami.

## Melaporkan Bug atau Mengusulkan Fitur

Saat membuat Issue, sertakan informasi berikut bila relevan:

- Deskripsi masalah atau fitur yang diusulkan.
- Langkah untuk mereproduksi masalah.
- Perilaku yang diharapkan dan yang terjadi.
- Screenshot atau tautan simulasi Wokwi jika tersedia.

## Lisensi

Dengan mengirimkan kontribusi, kamu menyetujui bahwa kontribusi tersebut didistribusikan di bawah lisensi MIT yang digunakan repository ini.
