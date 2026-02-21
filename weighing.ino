#include <HardwareSerial.h>

#define RXD2 16  // RX2 ESP32 → TXD MAX232
#define TXD2 17  // TX2 ESP32 → RXD MAX232
HardwareSerial SerialTimbangan(0);

// Fungsi membaca berat (return -1 jika data tidak valid/bukan Kg)
float bacaBeratKg() {
  if (Serial2.available() < 6) return -1;
  
  uint8_t d[6];
  Serial2.readBytes(d, 6);
  
  // Cek: Harus stabil (bit 6 byte 2) DAN satuan Kg (byte 6 = 0)
  if (((d[1] >> 6) & 0x01) == 0 || d[5] == 1) return -1;
  
  // Helper: BCD ke Desimal
  auto bcd = [](uint8_t v) { return ((v >> 4) * 10) + (v & 0x0F); };
  
  // Gabungkan 3 byte BCD menjadi angka utuh
  uint32_t raw = (bcd(d[4]) * 10000) + (bcd(d[3]) * 100) + bcd(d[2]);
  
  // Terapkan posisi desimal (bit 0-2 dari byte status)
  float berat = (float)raw;
  for (int i = 0; i < (d[1] & 0x07); i++) berat /= 10.0;
  
  // Handle tanda negatif jika ada
  if ((d[1] >> 5) & 0x01) berat = -berat;
  
  return berat;
}

void setup() {
  Serial.begin(115200);
  SerialTimbangan.begin(9600, SERIAL_8N1, RXD2, TXD2); // Sesuaikan baud rate dengan setting timbangan
}

void loop() {
  float kg = bacaBeratKg();
  if (kg >= 0) {
    Serial.println(kg, 3);  // Tampilkan dengan 3 angka desimal
  }
  delay(200);
}
