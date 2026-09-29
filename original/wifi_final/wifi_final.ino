#include <WiFi.h>
#include <WebServer.h>

// ---------------- WIFI ----------------
const char* ssid = "YOUR_WIFI_SSID";          // credentials removed before publishing
const char* password = "YOUR_WIFI_PASSWORD";

WebServer server(80);

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

// ---------------- GLOBAL DATA ----------------
float F_now = 0;
String fuelStatus = "";

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

  if (f < F_petrol_min) {
    return "SENSOR ERROR";
  }

  float deviation = f - F_petrol_max;

  if (deviation < 15000) return "SLIGHTLY ADULTERATED";
  else if (deviation < 25000) return "MODERATELY ADULTERATED";
  else return "HIGHLY ADULTERATED";
}

// ---------------- CORS HEADERS ----------------
void sendCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ---------------- OPTIONS HANDLER ----------------
void handleOptions() {
  sendCORSHeaders();
  server.send(204); // No content
}

// ---------------- API HANDLER ----------------
void handleData() {

  String data = "{";
  data += "\"freq\":" + String(F_now) + ",";
  data += "\"quality\":\"" + fuelStatus + "\",";
  data += "\"flow\":" + String(flowRate) + ",";
  data += "\"total\":" + String(totalLitres);
  data += "}";

  sendCORSHeaders();
  server.send(200, "application/json", data);
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(FREQ_PIN, INPUT);
  pinMode(FLOW_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, RISING);

  // 🔷 WiFi Connect
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // 🔷 ROUTES
  server.on("/data", HTTP_GET, handleData);
  server.on("/data", HTTP_OPTIONS, handleOptions);

  server.begin();
}

// ---------------- LOOP ----------------
void loop() {

  // 🔥 ALWAYS FIRST (important)
  server.handleClient();

  // 🔷 Frequency
  F_now = getStableFreq();
  fuelStatus = classifyFuel(F_now);

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

  delay(1000);
}