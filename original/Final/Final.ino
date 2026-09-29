#include <WiFi.h>
#include <WebServer.h>

// -------- WIFI --------
const char* ssid = "YOUR_WIFI_SSID";          // credentials removed before publishing
const char* password = "YOUR_WIFI_PASSWORD";

WebServer server(80);

// -------- API HANDLER --------
void handleData() {
  String json = "{";
  json += "\"freq\":12345,";
  json += "\"quality\":\"TEST DATA\",";
  json += "\"flow\":1.23,";
  json += "\"total\":5.67";
  json += "}";

  // CORS (so browser can access)
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// -------- SETUP --------
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Route
  server.on("/data", handleData);

  server.begin();
}

// -------- LOOP --------
void loop() {
  server.handleClient();  // must run always
}