// ============================================================
//  NODE SENDER
// ============================================================

#include <WiFi.h>
#include <esp_now.h>

//  KONFIGURASI
uint8_t receiverMacAddress[] = {0x10, 0x52, 0x1c, 0x68, 0xe8, 0xe0};
#define NODE_ID 2
#define SEND_INTERVAL 500

// Pin Timbangan RS232 (Serial2)
#define SCALE_RX_PIN  16  // GPIO 16 (RX2) ← dari TX MAX3232
#define SCALE_TX_PIN  17  // GPIO 17 (TX2) → ke RX MAX3232

// Pin Input Head A
#define PIN_START_A     32
#define PIN_TIMER_A     33
#define PIN_EMERG_A     34  //Perlu Pull-up eksternal 10k!

// Pin Input Head B
#define PIN_START_B     35  //Perlu Pull-up eksternal 10k!
#define PIN_TIMER_B     39  //Perlu Pull-up eksternal 10k!
#define PIN_EMERG_B     36  //Perlu Pull-up eksternal 10k!

//STRUKTUR DATA
typedef struct {
  uint8_t nodeID;           // 1 atau 2
  float scaleWeight;        // Data timbangan (contoh: 12.50)
  bool scaleValid;          // true jika data timbangan valid

  // Head A
  bool headA_start;
  bool headA_timer;
  bool headA_emergency;
  uint8_t headA_state;      // 0:IDLE, 1:RUN, 2:STOP

  // Head B
  bool headB_start;
  bool headB_timer;
  bool headB_emergency;
  uint8_t headB_state;      // 0:IDLE, 1:RUN, 2:STOP

  unsigned long timestamp;  // Waktu kirim
} DataPacket;

//GLOBAL VARIABLES
HardwareSerial scaleSerial(2);  // Serial2 untuk timbangan
DataPacket packetData;
unsigned long lastSendTime = 0;
unsigned long lastScaleReadTime = 0;
String scaleBuffer = "";

// State Head (0:IDLE, 1:RUN, 2:STOP)
uint8_t headA_state = 0;
uint8_t headB_state = 0;

//READ Input Digital HEAD A (Active LOW)
bool readStartA() {
  //  return digitalRead(PIN_START_A) == LOW;
  return (random(0, 100) < 10);
}
bool readTimerA() {
  //  return digitalRead(PIN_TIMER_A) == LOW;
  return (random(0, 100) < 15);  // 15% chance ON
}
bool readEmergA() {
  //  return digitalRead(PIN_EMERG_A) == LOW;
  return (random(0, 100) < 3);   // 3% chance ON (jarang, karena emergency)
}

//READ Input Digital HEAD B (Active LOW)
bool readStartB() {
  //  return digitalRead(PIN_START_B) == LOW;
  return (random(0, 100) < 10);
}
bool readTimerB() {
  //  return digitalRead(PIN_TIMER_B) == LOW;
  return (random(0, 100) < 15);  // 15% chance ON
}
bool readEmergB() {
  //  return digitalRead(PIN_EMERG_B) == LOW;
  return (random(0, 100) < 3);   // 3% chance ON (jarang, karena emergency)
}

// Simulasi berat timbangan (random 0.00 - 50.00 kg)
float simReadScale() {
  return (float)random(0, 5000) / 100.0;  // 0.00 sampai 49.99
}

// ============================================================
//  FUNGSI: Parse Data Timbangan (masih salah)
// ============================================================
//float parseScaleData(String data) {
//  data.trim();
//
//  // Cari angka desimal (format: 12.50 atau 12.50 kg)
//  String numStr = "";
//  bool foundDigit = false;
//
//  for (int i = 0; i < data.length(); i++) {
//    char c = data[i];
//    if (isDigit(c) || c == '.' || c == '-' || c == '+') {
//      numStr += c;
//      foundDigit = true;
//    } else if (foundDigit && c != ' ' && c != 'k' && c != 'g') {
//      break;  // Stop setelah angka selesai
//    }
//  }
//
//  if (numStr.length() > 0) {
//    return numStr.toFloat();
//  }
//  return -1.0;  // Invalid
//}

// ============================================================
//  FUNGSI: Baca Timbangan (Non-Blocking)
// ============================================================
//void readScale() {
//  while (scaleSerial.available()) {
//    char c = scaleSerial.read();
//
//    if (c == '\n' || c == '\r') {
//      if (scaleBuffer.length() > 0) {
//        float weight = parseScaleData(scaleBuffer);
//        if (weight >= 0) {
//          packetData.scaleWeight = weight;
//          packetData.scaleValid = true;
//        }
//        scaleBuffer = "";
//      }
//    } else {
//      scaleBuffer += c;
//      if (scaleBuffer.length() > 20) {
//        scaleBuffer = "";  // Clear buffer jika terlalu panjang
//      }
//    }
//  }
//}
// ============================================================

//Proses Logika Head A
void processHeadA() {
  bool emerg = readEmergA();
  bool start = readStartA();
  bool timer = readTimerA();

  packetData.headA_emergency = emerg;
  packetData.headA_start = start;
  packetData.headA_timer = timer;

  // Logika State Machine
  if (emerg) {
    headA_state = 2;  // STOP (Emergency)
  } else if (start) {
    headA_state = 1;  // RUN
  } else if (timer) {
    headA_state = 1;  // RUN (timer aktif)
  } else {
    headA_state = 0;  // IDLE
  }
  packetData.headA_state = headA_state;
}

//Proses Logika Head B
void processHeadB() {
  bool emerg = readEmergB();
  bool start = readStartB();
  bool timer = readTimerB();

  packetData.headB_emergency = emerg;
  packetData.headB_start = start;
  packetData.headB_timer = timer;

  // Logika State Machine
  if (emerg) {
    headB_state = 2;  // STOP (Emergency)
  } else if (start) {
    headB_state = 1;  // RUN
  } else if (timer) {
    headB_state = 1;  // RUN (timer aktif)
  } else {
    headB_state = 0;  // IDLE
  }
  packetData.headB_state = headB_state;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Inisialisasi random seed dari noise analog
  randomSeed(analogRead(0));

  // Init Pin Input
  pinMode(PIN_START_A, INPUT_PULLUP);
  pinMode(PIN_TIMER_A, INPUT_PULLUP);
  pinMode(PIN_EMERG_A, INPUT);  //Perlu Pull-up eksternal 10k!

  pinMode(PIN_START_B, INPUT);  //Perlu Pull-up eksternal 10k!
  pinMode(PIN_TIMER_B, INPUT);  //Perlu Pull-up eksternal 10k!
  pinMode(PIN_EMERG_B, INPUT);  //Perlu Pull-up eksternal 10k!

  // Init Serial2 untuk Timbangan
  scaleSerial.begin(9600, SERIAL_8N1, SCALE_RX_PIN, SCALE_TX_PIN);

  // Init WiFi & ESP-NOW
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Error initializing ESP-NOW");
    return;
  }

  // Register Peer (Receiver)
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, receiverMacAddress, 6);

  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Failed to add peer");
    return;
  }

  // Init packet data
  packetData.nodeID = NODE_ID;
  packetData.scaleWeight = 0.0;
  packetData.scaleValid = false;
  packetData.timestamp = 0;
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", receiverMacAddress[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
}

void loop() {
  unsigned long now = millis();
  // 1. Baca data timbangan (terus menerus)
  //  readScale();
  // 1. Simulasi baca timbangan (random weight)
  packetData.scaleWeight = simReadScale();
  packetData.scaleValid = true;  // Selalu valid di simulasi
  
  // 2. Proses Head
  processHeadA();
  processHeadB();

  // 3. Kirim data periodik ke Receiver
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    packetData.timestamp = millis();

    // Kirim tanpa konfirmasi callback
    esp_now_send(receiverMacAddress, (uint8_t *)&packetData, sizeof(DataPacket));

    lastSendTime = millis();
  }
  delay(50);
}
