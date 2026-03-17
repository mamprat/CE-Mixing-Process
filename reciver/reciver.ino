#include <WiFi.h>
#include <esp_now.h>

// STRUKTUR DATA HARUS SAMA PERSIS DENGAN SENDER
typedef struct {
  uint8_t nodeID;
  float scaleWeight;
  bool scaleValid;
  
  // Head A
  bool headA_start;
  bool headA_timer;
  bool headA_emergency;
  uint8_t headA_state; // Tambahkan ini agar sinkron

  // Head B
  bool headB_start;
  bool headB_timer;
  bool headB_emergency;
  uint8_t headB_state; // Tambahkan ini agar sinkron

  unsigned long timestamp;
} DataPacket;

DataPacket dataNode1, dataNode2;
unsigned long lastRecvNode1 = 0;
unsigned long lastRecvNode2 = 0;

// Fungsi pembantu simbol lebih aman
const char* s(bool v) { return v ? "1" : "0"; }

// Update parameter OnDataRecv agar kompatibel dengan ESP32 Core terbaru
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  DataPacket temp;
  
  // Validasi panjang data sebelum copy untuk mencegah crash
  if (len != sizeof(DataPacket)) {
    Serial.printf("Error: Ukuran data tidak cocok! Terima: %d, Seharusnya: %d\n", len, sizeof(DataPacket));
    return;
  }

  memcpy(&temp, incomingData, sizeof(DataPacket));

  if (temp.nodeID == 1) {
    dataNode1 = temp;
    lastRecvNode1 = millis();
  } else if (temp.nodeID == 2) {
    dataNode2 = temp;
    lastRecvNode2 = millis();
  }

  // Print dengan format yang lebih rapi
  Serial.printf("[ID:%d] Berat:%5.2f | A:[St:%s Ti:%s Em:%s Mode:%d] | B:[St:%s Ti:%s Em:%s Mode:%d]\n",
                temp.nodeID,
                temp.scaleWeight,
                s(temp.headA_start), s(temp.headA_timer), s(temp.headA_emergency), temp.headA_state,
                s(temp.headB_start), s(temp.headB_timer), s(temp.headB_emergency), temp.headB_state);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  
  Serial.println("\n--- RECEIVER READY (SINKRON) ---");
  Serial.print("MAC Address: "); Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}

void loop() {
  // Cek timeout (Interval 5 detik)
  if (lastRecvNode1 > 0 && (millis() - lastRecvNode1 > 5000)) {
    Serial.println(">>> ALERT: NODE 1 DISCONNECT! <<<");
    lastRecvNode1 = 0; 
  }
  if (lastRecvNode2 > 0 && (millis() - lastRecvNode2 > 5000)) {
    Serial.println(">>> ALERT: NODE 2 DISCONNECT! <<<");
    lastRecvNode2 = 0;
  }
  delay(1000); // Loop luar tidak perlu terlalu cepat
}
