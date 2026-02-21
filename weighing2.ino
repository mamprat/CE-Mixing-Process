#include <HardwareSerial.h>

// --- Konfigurasi Pin ESP32 ---
#define RXD2 16  // Hubungkan ke TXD pada modul MAX232
#define TXD2 17  // Hubungkan ke RXD pada modul MAX232

HardwareSerial SerialTimbangan(0);

void setup() {
  Serial.begin(115200);
  SerialTimbangan.begin(9600, SERIAL_8N1, RXD2, TXD2);
}

void loop() {
  // Timbangan mengirim paket data sebanyak 6 Byte
  if (Serial2.available() >= 6) {
    uint8_t data[6];
    size_t bytesRead = Serial2.readBytes(data, 6);
    if (bytesRead == 6) {
      prosesDataTimbangan(data);
    }
    delay(200); 
  }
}

void prosesDataTimbangan(uint8_t *data) {
  // --- STRUKTUR DATA (Berdasarkan Manual Book V.2.(2)) ---
  // data[0] = Byte 1: Message Flag (OFFH)
  // data[1] = Byte 2: Status (Stabil, Desimal, Negatif, dll)
  // data[2] = Byte 3: BCD LSB (Least Significant Byte)
  // data[3] = Byte 4: BCD MSB (Medium Significant Byte)
  // data[4] = Byte 5: BCD HSB (Most Significant Byte)
  // data[5] = Byte 6: Satuan (0=Kg, 1=lb)

  // 1. Cek Status (Byte 2)
  uint8_t status = data[1];
  
  // Cek Bit 6: Stability Flag (1 = Stabil, 0 = Tidak Stabil)
  bool isStable = (status >> 6) & 0x01;
  
  // Cek Bit 7: Overflow Flag (1 = Overload)
  bool isOverflow = (status >> 7) & 0x01;

  if (!isStable) {
    Serial.println("Status: Menunggu Stabil...");
    return; 
  }

  if (isOverflow) {
    Serial.println("Status: OVERLOAD (--OF--)");
    return;
  }

  // 2. Ambil Nilai Berat (BCD Conversion)
  // Setiap byte BCD menyimpan 2 digit angka (contoh: 0x25 = angka 25)
  uint32_t valLSB = bcdToDec(data[2]);
  uint32_t valMSB = bcdToDec(data[3]);
  uint32_t valHSB = bcdToDec(data[4]);

  // Gabungkan menjadi satu angka utuh
  // Asumsi format: HSB (2 digit) | MSB (2 digit) | LSB (2 digit) = 6 digit total
  uint32_t rawWeight = (valHSB * 10000) + (valMSB * 100) + valLSB;

  // 3. Atur Posisi Desimal
  // Bit 0-2 pada Byte Status menunjukkan jumlah digit desimal (0-5)
  uint8_t decimalPoint = status & 0x07;
  
  float finalWeight = (float)rawWeight;
  for (int i = 0; i < decimalPoint; i++) {
    finalWeight /= 10.0;
  }

  // 4. Cek Tanda Negatif (Bit 5 pada Byte Status)
  bool isNegative = (status >> 5) & 0x01;
  if (isNegative) {
    finalWeight = -finalWeight;
  }

  // 5. Cek Satuan (Byte 6)
  // 0 = Kg, 1 = lb
  String unit = (data[5] == 1) ? "lb" : "Kg";

  // 6. Tampilkan Hasil
  Serial.print("Berat Stabil: ");
  Serial.print(finalWeight, decimalPoint); // Tampilkan sesuai jumlah desimal asli
  Serial.print(" ");
  Serial.println(unit);
}

// Fungsi mengubah nilai Hex BCD (0x99) ke Desimal (99)
uint32_t bcdToDec(uint8_t val) {
  return ((val >> 4) * 10) + (val & 0x0F);
}
