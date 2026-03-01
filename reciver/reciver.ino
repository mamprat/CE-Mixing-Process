// ============================================================
//  RECEIVER (ESP-NOW) - MULTI NODE (NODE 1 & NODE 2)
// ============================================================

#include <WiFi.h>
#include <esp_now.h>

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

// Variabel untuk menyimpan data terakhir dari masing-masing node
DataPacket dataNode1, dataNode2;
unsigned long lastRecvNode1 = 0;
unsigned long lastRecvNode2 = 0;

// Fungsi pembantu simbol (1=ON/!!!, 0=OFF/OK)
const char* s(bool v) { return v ? "1" : "0"; }

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  DataPacket temp;
  memcpy(&temp, incomingData, sizeof(DataPacket));

  // Simpan ke variabel sesuai ID Pengirim
  if (temp.nodeID == 1) {
    dataNode1 = temp;
    lastRecvNode1 = millis();
  } else if (temp.nodeID == 2) {
    dataNode2 = temp;
    lastRecvNode2 = millis();
  }

  // PRINT FORMAT SIMPEL (Satu baris untuk setiap data masuk)
  // Format: [ID] Berat | A:[St-Ti-Em] | B:[St-Ti-Em]
  Serial.printf("[N%d] W:%5.2f | A:[%s-%s-%s] | B:[%s-%s-%s]\n",
                temp.nodeID,
                temp.scaleWeight,
                s(temp.headA_start), s(temp.headA_timer), s(temp.headA_emergency),
                s(temp.headB_start), s(temp.headB_timer), s(temp.headB_emergency));
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  Serial.println("\n--- RECEIVER DUAL NODE READY ---");
  Serial.print("MAC Address: "); Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // Cek jika ada node yang mati (Timeout 5 detik)
  if (lastRecvNode1 > 0 && millis() - lastRecvNode1 > 5000) {
    Serial.println(">>> ALERT: NODE 1 DISCONNECT! <<<");
    lastRecvNode1 = 0; 
  }
  if (lastRecvNode2 > 0 && millis() - lastRecvNode2 > 5000) {
    Serial.println(">>> ALERT: NODE 2 DISCONNECT! <<<");
    lastRecvNode2 = 0;
  }
  delay(1000);
}
