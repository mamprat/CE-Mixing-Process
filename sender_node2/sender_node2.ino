#include <WiFi.h>
#include <esp_now.h>
#include <HardwareSerial.h>

uint8_t receiverMacAddress[] = {0x7c, 0x9e, 0xbd, 0x61, 0x9b, 0x44}; 

#define NODE_ID          2
#define SEND_INTERVAL    500

#define SCALE_RX_PIN  16
#define SCALE_TX_PIN  17

#define PIN_START_A     32
#define PIN_TIMER_A     33
#define PIN_EMERG_A     25

#define PIN_START_B     26
#define PIN_TIMER_B     27
#define PIN_EMERG_B     21

typedef struct {
  uint8_t nodeID;
  float scaleWeight;
  bool scaleValid;
  bool headA_start;
  bool headA_timer;
  bool headA_emergency;
  uint8_t headA_state;
  bool headB_start;
  bool headB_timer;
  bool headB_emergency;
  uint8_t headB_state;
  unsigned long timestamp;
} DataPacket;

DataPacket packetData;
unsigned long lastSendTime = 0;

bool latchedStartA = false; 
bool lastTimerA = false;    
bool latchedStartB = false; 
bool lastTimerB = false;    

#define SerialTimbangan Serial2

// ================= PERBAIKAN SOFTWARE FILTER =================
bool filteredRead(int pin) {
  if (digitalRead(pin) == LOW) {
    delay(40); // Delay 10ms untuk memastikan sinyal bukan noise adaptor
    return digitalRead(pin) == LOW;
  }
  return false;
}

float bacaBeratKg() {
  if (SerialTimbangan.available() < 6) return -1;
  if (SerialTimbangan.peek() != 0xFF) { SerialTimbangan.read(); return -1; }
  uint8_t d[6];
  SerialTimbangan.readBytes(d, 6);
  uint32_t rawWeight = ((((uint32_t)(((d[4] >> 4) * 10) + (d[4] & 0x0F))) * 10000) + (((uint32_t)(((d[3] >> 4) * 10) + (d[3] & 0x0F))) * 100) + ((uint32_t)(((d[2] >> 4) * 10) + (d[2] & 0x0F))));
  float berat = (float)rawWeight;
  uint8_t decimalPos = d[1] & 0x07;
  for (int i = 0; i < decimalPos; i++) berat /= 10.0;
  if ((d[1] >> 5) & 0x01) berat = -berat;
  return berat;
}

void setup() {
  Serial.begin(115200);
  int pins[] = {PIN_START_A, PIN_TIMER_A, PIN_EMERG_A, PIN_START_B, PIN_TIMER_B, PIN_EMERG_B};
  for(int p : pins) pinMode(p, INPUT_PULLUP);

  SerialTimbangan.begin(9600, SERIAL_8N1, SCALE_RX_PIN, SCALE_TX_PIN);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;
  
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMacAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
  Serial.println("SENDER READY WITH EXECUTION GAP");
}

void loop() {
  // --- PROSES HEAD A ---
  bool curStartA = filteredRead(PIN_START_A);
  bool curTimerA = filteredRead(PIN_TIMER_A);
  bool curEmergA = filteredRead(PIN_EMERG_A);

  if (curEmergA) {
    latchedStartA = false;
  } else {
    if (curStartA) latchedStartA = true;
    if (curTimerA && !lastTimerA) latchedStartA = false;
  }
  lastTimerA = curTimerA;

  // --- EXECUTION GAP (Jeda antara A dan B) ---
  // Memberikan waktu bagi CPU dan tegangan untuk stabil sebelum baca Head B
  delay(1); 

  // --- PROSES HEAD B ---
  bool curStartB = filteredRead(PIN_START_B);
  bool curTimerB = filteredRead(PIN_TIMER_B);
  bool curEmergB = filteredRead(PIN_EMERG_B);

  if (curEmergB) {
    latchedStartB = false;
  } else {
    if (curStartB) latchedStartB = true;
    if (curTimerB && !lastTimerB) latchedStartB = false;
  }
  lastTimerB = curTimerB;

  // --- PENGIRIMAN DATA ---
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    packetData.nodeID = NODE_ID;
    
    packetData.headA_start = latchedStartA;
    packetData.headA_timer = curTimerA;
    packetData.headA_emergency = curEmergA;
    packetData.headA_state = curEmergA ? 2 : (latchedStartA ? 1 : 0);

    packetData.headB_start = latchedStartB;
    packetData.headB_timer = curTimerB;
    packetData.headB_emergency = curEmergB;
    packetData.headB_state = curEmergB ? 2 : (latchedStartB ? 1 : 0);

    float berat = bacaBeratKg();
    if (berat >= 0) { packetData.scaleWeight = berat; packetData.scaleValid = true; }
    else { packetData.scaleValid = false; }

    esp_now_send(receiverMacAddress, (uint8_t *)&packetData, sizeof(DataPacket));
    
    Serial.printf("W:%.2f | A:[%d-%d-%d] B:[%d-%d-%d]\n", 
                  packetData.scaleWeight, latchedStartA, curTimerA, curEmergA,
                  latchedStartB, curTimerB, curEmergB);
    lastSendTime = millis();
  }
}
