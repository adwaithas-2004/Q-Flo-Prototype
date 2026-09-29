#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ---------------- PINS ----------------
#define FREQ_PIN 32
#define FLOW_PIN 27

// ---------------- PETROL RANGE ----------------
float F_petrol_min = 23950.0;
float F_petrol_max = 34196.0;

// ---------------- FLOW VARIABLES ----------------
volatile int pulseCount = 0;
float flowRate = 0;
float totalLitres = 0;

// ---------------- BLE ----------------
BLECharacteristic *pCharacteristic;

// ---------------- FLOW INTERRUPT ----------------
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

// ---------------- FREQUENCY ----------------
float measureFrequency() {
  unsigned long tH = pulseIn(FREQ_PIN, HIGH);
  unsigned long tL = pulseIn(FREQ_PIN, LOW);

  if (tH == 0 || tL == 0) return 0;

  return 1000000.0 / (tH + tL);
}

float getStableFreq() {
  float sum = 0;
  for(int i=0;i<20;i++) {
    sum += measureFrequency();
    delay(10);
  }
  return sum / 20;
}

// ---------------- FLOW CALCULATION ----------------
void calculateFlow() {
  static unsigned long lastTime = 0;

  if (millis() - lastTime >= 1000) {

    detachInterrupt(digitalPinToInterrupt(FLOW_PIN));

    flowRate = (pulseCount / 7.5);   // L/min
    float flowLitres = flowRate / 60.0;

    totalLitres += flowLitres;

    pulseCount = 0;

    attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, RISING);

    lastTime = millis();
  }
}

// ---------------- CLASSIFICATION ----------------
String classifyFuel(float f) {

  if (f >= F_petrol_min && f <= F_petrol_max) {
    return "PURE PETROL";
  }


  float deviation = f - F_petrol_max;

  if (deviation < 15000) {
    return "SLIGHTLY ADULTERATED";
  }
  else if (deviation < 25000) {
    return "MODERATELY ADULTERATED";
  }
  else {
    return "HIGHLY ADULTERATED";
  }
}

// ---------------- BLE SETUP ----------------
void setupBLE() {
  BLEDevice::init("Q-FLO");

  BLEServer *server = BLEDevice::createServer();
  BLEService *service = server->createService("1234");

  pCharacteristic = service->createCharacteristic(
    "5678",
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );

  pCharacteristic->addDescriptor(new BLE2902());

  service->start();
  BLEDevice::getAdvertising()->start();
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(FREQ_PIN, INPUT);
  pinMode(FLOW_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, RISING);

  setupBLE();
}

// ---------------- LOOP ----------------
void loop() {

  // 🔷 Frequency
  float F_now = getStableFreq();
  String fuelStatus = classifyFuel(F_now);

  // 🔷 Flow
  calculateFlow();

  // ---------------- SERIAL OUTPUT ----------------
  Serial.println("------ Q-FLO ANALYSIS ------");

  Serial.print("Frequency: ");
  Serial.print(F_now);
  Serial.println(" Hz");

  Serial.print("Fuel Quality: ");
  Serial.println(fuelStatus);

  Serial.print("Flow Rate: ");
  Serial.print(flowRate);
  Serial.println(" L/min");

  Serial.print("Total Fuel: ");
  Serial.print(totalLitres);
  Serial.println(" L");

  // ---------------- BLE DATA ----------------
  String data = "{";
  data += "\"freq\":" + String(F_now) + ",";
  data += "\"quality\":\"" + fuelStatus + "\",";
  data += "\"flow\":" + String(flowRate) + ",";
  data += "\"total\":" + String(totalLitres);
  data += "}";

  pCharacteristic->setValue(data.c_str());
  pCharacteristic->notify();

  delay(1000);
}

