// ============================================================
//  RECIVER
// ============================================================

#include <WiFi.h>
#include <esp_now.h>

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
unsigned long lastRecvTime = 0;
int totalPacketsReceived = 0;

//HELPER FUNCTION
void printState(uint8_t state) {
  switch (state) {
    case 0: Serial.print("IDLE "); break;
    case 1: Serial.print("RUN  "); break;
    case 2: Serial.print("STOP "); break;
    default: Serial.print("???  "); break;
  }
}

const char* boolToStr(bool val, const char* onText, const char* offText) {
  return val ? onText : offText;
}

//CALLBACK
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  // Cek apakah ukuran data sesuai
  if (len != sizeof(DataPacket)) {
    Serial.println("⚠Error: Ukuran paket tidak sesuai!");
    return;
  }

  DataPacket receivedData;
  memcpy(&receivedData, incomingData, sizeof(DataPacket));

  // Update statistik
  totalPacketsReceived++;
  lastRecvTime = millis();

  // ============================================================
  //  TAMPILKAN DATA KE SERIAL MONITOR
  // ============================================================
  Serial.print("[Node ");
  Serial.print(receivedData.nodeID);
  Serial.print("] || Weight");
  Serial.print(receivedData.nodeID);
  Serial.print(": ");

  if (receivedData.scaleValid) {
    Serial.printf("%.2f kg ", receivedData.scaleWeight);
  } else {
    Serial.print("--.-- kg ");
  }

  // HEAD A
  Serial.print("HEAD A");
  Serial.print(receivedData.nodeID);
  Serial.print(": ");
  printState(receivedData.headA_state);
  Serial.print(" | Start:");
  Serial.print(boolToStr(receivedData.headA_start, "ON", "OFF"));
  Serial.print(" Timer:");
  Serial.print(boolToStr(receivedData.headA_timer, "ON", "OFF"));
  Serial.print(" Emerg:");
  Serial.print(boolToStr(receivedData.headA_emergency, "!!!", "OK"));

  // Pemisah Head A dan B
  Serial.print(" || ");

  // HEAD B
  Serial.print("HEAD B");
  Serial.print(receivedData.nodeID);
  Serial.print(": ");
  printState(receivedData.headB_state);
  Serial.print(" | Start:");
  Serial.print(boolToStr(receivedData.headB_start, "ON", "OFF"));
  Serial.print(" Timer:");
  Serial.print(boolToStr(receivedData.headB_timer, "ON", "OFF"));
  Serial.print(" Emerg:");
  Serial.print(boolToStr(receivedData.headB_emergency, "!!!", "OK"));

  // Akhir baris
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Inisialisasi WiFi
  WiFi.mode(WIFI_STA);
  WiFi.STA.begin();

  // Inisialisasi ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Error initializing ESP-NOW");
    return;
  }
  // Daftarkan callback untuk terima data
  esp_now_register_recv_cb(OnDataRecv);
  String mac = WiFi.macAddress();

  // Jika masih 00:00, coba tunggu dan ambil lagi
  if (mac == "00:00:00:00:00:00") {
    delay(50);
    mac = WiFi.macAddress();
  }

  Serial.print("✅ Receiver Ready | MAC: ");
  Serial.println(mac);
}

void loop() {
  // Cek timeout
  if (millis() - lastRecvTime > 5000 && lastRecvTime > 0) {
    Serial.println("Tidak ada data masuk selama 5 detik!");
    lastRecvTime = millis(); // Reset agar tidak spam
  }
  delay(100);
}
