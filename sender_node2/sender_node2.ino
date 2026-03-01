// ============================================================
//  NODE SENDER
// ============================================================
#include <WiFi.h>
#include <esp_now.h>
#include <HardwareSerial.h>

uint8_t receiverMacAddress[] = {0x10, 0x52, 0x1c, 0x68, 0xe8, 0xe0};
#define NODE_ID          2
#define SEND_INTERVAL    500

// Pin Timbangan RS232 (Serial2)
#define SCALE_RX_PIN  16
#define SCALE_TX_PIN  17

// Pin Input Head A
#define PIN_START_A     32
#define PIN_TIMER_A     33
#define PIN_EMERG_A     25

// Pin Input Head B
#define PIN_START_B     26
#define PIN_TIMER_B     27
#define PIN_EMERG_B     14

typedef struct {
  uint8_t nodeID;
  float scaleWeight;
  bool scaleValid;
  
  // Head A
  bool headA_start;
  bool headA_timer;
  bool headA_emergency;

  // Head B
  bool headB_start;
  bool headB_timer;
  bool headB_emergency;

  unsigned long timestamp;
} DataPacket;

DataPacket packetData;
unsigned long lastSendTime = 0;

#define SerialTimbangan Serial2

float bacaBeratKg() {
  // 1. Pastikan buffer memiliki minimal 6 byte
  if (SerialTimbangan.available() < 6) return -1;

  // 2. SINKRONISASI: Byte pertama (Index 0) WAJIB 0xFF sesuai manual
  if (SerialTimbangan.peek() != 0xFF) {
    SerialTimbangan.read(); // Buang 1 byte salah, cari 0xFF di putaran berikutnya
    return -1;
  }

  // 3. Baca 6 byte yang sudah pasti diawali 0xFF
  uint8_t d[6];
  SerialTimbangan.readBytes(d, 6);

  // --- VALIDASI KETAT SESUAI MANUAL HALAMAN 6 ---

  // A. Cek Mode (Bit 3-4 dari Byte 2): Harus 00 (Mode Weighting)
  uint8_t mode = (d[1] >> 3) & 0x03;
  if (mode != 0) return -1; // Abaikan jika timbangan sedang mode counting/persen

  // B. Cek Kestabilan (Bit 6 dari Byte 2): Harus 1 (Stabil)
  bool isStable = (d[1] >> 6) & 0x01;
  if (!isStable) return -1; 

  // C. Cek Overflow/Overload (Bit 7 dari Byte 2): Harus 0 (Normal)
  bool isOverflow = (d[1] >> 7) & 0x01;
  if (isOverflow) return -1; // Timbangan kelebihan beban (menampilkan --OF--)

  // D. Cek Satuan (Byte 6): Harus 0 (Kg)
  if (d[5] != 0) return -1; 

  // --- MENGHITUNG BERAT ---
  auto bcdToUint = [](uint8_t v) {
    return (uint32_t)(((v >> 4) * 10) + (v & 0x0F));
  };

  // Gabungkan Byte 3 (LSB), Byte 4 (Mid), Byte 5 (HSB)
  uint32_t rawWeight = (bcdToUint(d[4]) * 10000) + (bcdToUint(d[3]) * 100) + bcdToUint(d[2]);
  float berat = (float)rawWeight;

  // --- MENENTUKAN TITIK DESIMAL ---
  // Bit 0-2 dari Byte 2
  uint8_t decimalPos = d[1] & 0x07;
  for (int i = 0; i < decimalPos; i++) {
    berat /= 10.0;
  }

  // --- CEK TANDA NEGATIF ---
  // Bit 5 dari Byte 2: 1 = Negatif
  bool isNegative = (d[1] >> 5) & 0x01;
  if (isNegative) berat = -berat;

  return berat;
}

void OnDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  // Callback pengiriman
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_START_A, INPUT_PULLUP);
  pinMode(PIN_TIMER_A, INPUT_PULLUP);
  pinMode(PIN_EMERG_A, INPUT_PULLUP);
  pinMode(PIN_START_B, INPUT_PULLUP);
  pinMode(PIN_TIMER_B, INPUT_PULLUP);
  pinMode(PIN_EMERG_B, INPUT_PULLUP);

  SerialTimbangan.begin(9600, SERIAL_8N1, SCALE_RX_PIN, SCALE_TX_PIN);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMacAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void loop() {
  if (millis() - lastSendTime >= SEND_INTERVAL) {

    // 1. Baca Input Fisik
    bool rawStartA = (digitalRead(PIN_START_A) == LOW);
    bool rawTimerA = (digitalRead(PIN_TIMER_A) == LOW);
    bool rawEmergA = (digitalRead(PIN_EMERG_A) == LOW);

    bool rawStartB = (digitalRead(PIN_START_B) == LOW);
    bool rawTimerB = (digitalRead(PIN_TIMER_B) == LOW);
    bool rawEmergB = (digitalRead(PIN_EMERG_B) == LOW);

    // 2. Logika Head A (Emergency Priority)
    packetData.headA_emergency = rawEmergA;
    if (rawEmergA) {
      packetData.headA_start = false;
      packetData.headA_timer = false;
    } else {
      packetData.headA_start = rawStartA;
      packetData.headA_timer = rawTimerA;
    }

    // 3. Logika Head B (Emergency Priority)
    packetData.headB_emergency = rawEmergB;
    if (rawEmergB) {
      packetData.headB_start = false;
      packetData.headB_timer = false;
    } else {
      packetData.headB_start = rawStartB;
      packetData.headB_timer = rawTimerB;
    }

    // 4. Update Timbangan & Metadata
    packetData.nodeID = NODE_ID;
    packetData.timestamp = millis();

    float berat = bacaBeratKg();
    if (berat >= 0) {
      packetData.scaleWeight = berat;
      packetData.scaleValid = true;
    } else {
      packetData.scaleValid = false;
    }

    // 5. Kirim Data
    esp_now_send(receiverMacAddress, (uint8_t *)&packetData, sizeof(DataPacket));

    // 6. Print Ringkas (Format: St-Ti-Em)
    Serial.printf("W:%.2f | A:[%d-%d-%d] B:[%d-%d-%d]\n", 
              packetData.scaleWeight,
              packetData.headA_start, packetData.headA_timer, packetData.headA_emergency,
              packetData.headB_start, packetData.headB_timer, packetData.headB_emergency);
                  
    lastSendTime = millis();
  }
}
