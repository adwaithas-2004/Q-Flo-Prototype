/*
   Q-FLO v2 - fuel quality + quantity monitor at the refuelling point

   Quality : capacitive cell (fuel = dielectric) timing an NE555 astable;
             the ESP32 counts the 555 frequency in hardware (PCNT).
   Quantity: YF-S201 turbine flow sensor, also counted in hardware.

   Board : ESP32 Dev Module, Arduino-ESP32 core 3.x
   Output: web dashboard at http://192.168.4.1 (device access point),
           JSON at /api/data (and /data for v1 dashboards), serial console.

   Wiring, calibration and API: see README.md and docs/.
*/

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "FuelLogic.h"
#include "PulseCounter.h"
#include "Settings.h"
#include "config.h"
#include "dashboard.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef QFLO_AP_PASSWORD
#define QFLO_AP_PASSWORD "qflo1234"  // default; override in secrets.h
#endif

#if QFLO_ENABLE_BLE
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#endif

#define QFLO_STR_(x) #x
#define QFLO_STR(x) QFLO_STR_(x)

#if ESP_ARDUINO_VERSION_MAJOR < 3
#error "Q-FLO needs the esp32 Arduino core 3.x (Boards Manager > esp32 by Espressif Systems)"
#endif

// ---------------- HARDWARE ----------------
PulseCounter freqCounter;
PulseCounter flowCounter;
Settings settings;
WebServer server(80);
char apSsid[24];

// ---------------- LIVE READINGS ----------------
float rawHz = 0;       // last gate
float filteredHz = 0;  // median of the last FREQ_MEDIAN_WINDOW gates
float freqWindow[FREQ_MEDIAN_WINDOW];
uint8_t freqIndex = 0;
uint8_t freqFill = 0;
bool signalOk = false;
float flowLpm = 0;
FuelClass liveClass = FuelClass::NoSignal;

uint32_t lastGateUs = 0;
uint32_t lastGateMs = 0;
uint32_t lastFlowWindowMs = 0;
uint32_t lastReportMs = 0;

// ---------------- REFUELLING SESSION ----------------
// Idle -> Fueling when flow starts; Fueling -> Done after SESSION_END_IDLE_MS
// without flow. Quality is only judged while fuel is flowing, because an
// empty chamber (air) would otherwise read as "highly adulterated".
enum class SessionState : uint8_t { Idle, Fueling, Done };
SessionState sessionState = SessionState::Idle;
uint32_t sessionPulses = 0;
uint32_t sessionStartMs = 0;
uint32_t sessionEndMs = 0;
uint32_t lastFlowSeenMs = 0;
RunningStats sessionFreq;
uint32_t sessionCounts[kFuelClassCount] = {};

// ---------------- CALIBRATION ----------------
enum class CalResult : uint8_t { None, Ok, Failed };
bool calActive = false;
uint32_t calStartMs = 0;
RunningStats calStats;
CalResult calResult = CalResult::None;
float calMean = 0;
float calSd = 0;
uint32_t calSamples = 0;

// ---------------- SERIAL ----------------
bool csvMode = false;
char serialLine[48];
uint8_t serialLen = 0;

// ---------------- HELPERS ----------------
const char* sessionStateKey() {
  switch (sessionState) {
    case SessionState::Fueling: return "fueling";
    case SessionState::Done:    return "done";
    default:                    return "idle";
  }
}

const char* calResultKey() {
  if (calActive) return "running";
  switch (calResult) {
    case CalResult::Ok:     return "ok";
    case CalResult::Failed: return "failed";
    default:                return "none";
  }
}

float sessionLitres() {
  return sessionPulses / settings.pulsesPerLitre;
}

uint32_t sessionDurationMs() {
  if (sessionState == SessionState::Idle) return 0;
  uint32_t end = sessionState == SessionState::Fueling ? millis() : sessionEndMs;
  return end - sessionStartMs;
}

bool fuelPresent() {
  return settings.benchMode ||
         (sessionState == SessionState::Fueling && flowLpm >= FLOW_START_LPM);
}

FuelClass sessionVerdict() {
  if (sessionFreq.n == 0) {
    return sessionCounts[(uint8_t)FuelClass::NoSignal] ? FuelClass::NoSignal : FuelClass::Waiting;
  }
  return classifyFrequency(sessionFreq.mean, settings.th, FREQ_MIN_VALID_HZ);
}

// What the user should see right now: the live reading while fuel flows,
// the result of the last refuel afterwards.
FuelClass displayedClass() {
  if (settings.benchMode || sessionState == SessionState::Fueling) return liveClass;
  if (sessionState == SessionState::Done) return sessionVerdict();
  return liveClass;  // Idle: Waiting or NoSignal
}

// ---------------- SESSION ----------------
void startSession(uint32_t nowMs) {
  sessionState = SessionState::Fueling;
  sessionPulses = 0;
  sessionStartMs = nowMs;
  sessionFreq.reset();
  memset(sessionCounts, 0, sizeof(sessionCounts));
  Serial.println(F(">> Refuelling started"));
}

void endSession() {
  sessionState = SessionState::Done;
  sessionEndMs = lastFlowSeenMs;
  settings.saveLifetime();
  Serial.printf(">> Refuelling finished: %.3f L in %lu s, avg %.0f Hz -> %s\n",
                sessionLitres(), (unsigned long)(sessionDurationMs() / 1000),
                sessionFreq.mean, fuelClassLabel(sessionVerdict()));
}

void resetSession() {
  sessionState = SessionState::Idle;
  sessionPulses = 0;
  sessionFreq.reset();
  memset(sessionCounts, 0, sizeof(sessionCounts));
}

// ---------------- CALIBRATION ----------------
// Fill the cell with known-pure fuel, then start: the pure band becomes
// mean +/- max(k*sd, pct*mean) of the readings over CAL_DURATION_MS.
void startCalibration() {
  calActive = true;
  calStartMs = millis();
  calStats.reset();
  Serial.println(F(">> Calibration started: keep known-pure fuel in the sensor"));
}

void updateCalibration() {
  if (!calActive || millis() - calStartMs < CAL_DURATION_MS) return;
  calActive = false;
  calSamples = calStats.n;
  if (calStats.n < CAL_MIN_SAMPLES) {
    calResult = CalResult::Failed;
    Serial.println(F(">> Calibration failed: no stable 555 signal"));
    return;
  }
  calMean = calStats.mean;
  calSd = calStats.sd();
  float halfBand = max(CAL_SIGMA_K * calSd, CAL_MIN_HALF_BAND_PCT * calMean);
  settings.th.pureMinHz = calMean - halfBand;
  settings.th.pureMaxHz = calMean + halfBand;
  settings.save();
  calResult = CalResult::Ok;
  Serial.printf(">> Calibration OK: mean %.1f Hz, sd %.1f Hz (n=%lu) -> pure band %.0f..%.0f Hz\n",
                calMean, calSd, (unsigned long)calSamples,
                settings.th.pureMinHz, settings.th.pureMaxHz);
}

// ---------------- MEASUREMENT ----------------
void updateFrequency() {
  uint32_t nowUs = micros();
  uint32_t edges = freqCounter.take();
  uint32_t elapsedUs = nowUs - lastGateUs;
  lastGateUs = nowUs;
  if (elapsedUs == 0) return;

  rawHz = edges * 1e6f / elapsedUs;
  freqWindow[freqIndex] = rawHz;
  freqIndex = (freqIndex + 1) % FREQ_MEDIAN_WINDOW;
  if (freqFill < FREQ_MEDIAN_WINDOW) freqFill++;
  filteredHz = medianOf(freqWindow, freqFill);
  signalOk = filteredHz >= FREQ_MIN_VALID_HZ;

  FuelClass c = classifyFrequency(filteredHz, settings.th, FREQ_MIN_VALID_HZ);
  liveClass = (c == FuelClass::NoSignal || fuelPresent()) ? c : FuelClass::Waiting;

  if (calActive && rawHz >= FREQ_MIN_VALID_HZ) calStats.add(rawHz);

  if (sessionState == SessionState::Fueling && fuelPresent()) {
    sessionCounts[(uint8_t)c]++;
    if (signalOk) sessionFreq.add(filteredHz);
  }

  if (csvMode) {
    Serial.printf("%lu,%.1f,%.1f,%.3f,%.4f,%s,%s\n", (unsigned long)millis(), rawHz, filteredHz,
                  flowLpm, sessionLitres(), fuelClassKey(liveClass), sessionStateKey());
  }
}

void updateFlow() {
  uint32_t nowMs = millis();
  uint32_t pulses = flowCounter.take();
  uint32_t elapsedMs = nowMs - lastFlowWindowMs;
  lastFlowWindowMs = nowMs;

  // Use the real window length: the v1 code assumed exactly 1 s, but its
  // loop took ~1.2 s (and up to 40 s with no 555 signal).
  float litres = pulses / settings.pulsesPerLitre;
  flowLpm = elapsedMs ? litres * 60000.0f / elapsedMs : 0;
  settings.lifetimeLitres += litres;

  if (flowLpm >= FLOW_START_LPM) {
    if (sessionState != SessionState::Fueling) startSession(nowMs);
    lastFlowSeenMs = nowMs;
  }
  if (sessionState == SessionState::Fueling) {
    sessionPulses += pulses;
    if (nowMs - lastFlowSeenMs >= SESSION_END_IDLE_MS) endSession();
  }
}

// ---------------- JSON ----------------
size_t buildDataJson(char* out, size_t cap) {
  FuelClass shown = displayedClass();
  FuelClass verdict = sessionVerdict();
  uint32_t calProgress = calActive ? min<uint32_t>(100, (millis() - calStartMs) * 100 / CAL_DURATION_MS) : 0;
  const uint32_t* k = sessionCounts;

  int n = snprintf(out, cap,
    "{\"freq\":%.1f,\"quality\":\"%s\",\"flow\":%.2f,\"total\":%.3f,"
    "\"class\":\"%s\",\"signal\":%s,\"rawFreq\":%.1f,\"state\":\"%s\",\"bench\":%s,"
    "\"session\":{\"litres\":%.3f,\"durationS\":%lu,\"avgFreq\":%.1f,\"sdFreq\":%.1f,\"samples\":%lu,"
    "\"verdict\":\"%s\",\"verdictClass\":\"%s\","
    "\"counts\":{\"pure\":%lu,\"slight\":%lu,\"moderate\":%lu,\"high\":%lu,\"fault\":%lu,\"no_signal\":%lu}},"
    "\"lifetime\":%.2f,"
    "\"cal\":{\"state\":\"%s\",\"progress\":%lu,\"mean\":%.1f,\"sd\":%.1f,\"samples\":%lu},"
    "\"th\":{\"pureMin\":%.1f,\"pureMax\":%.1f,\"slight\":%.1f,\"moderate\":%.1f,\"ppl\":%.2f},"
    "\"uptimeS\":%lu,\"fw\":\"%s\",\"ap\":\"%s\",\"staIp\":\"%s\"}",
    filteredHz, fuelClassLabel(shown), flowLpm, sessionLitres(),
    fuelClassKey(shown), signalOk ? "true" : "false", rawHz, sessionStateKey(),
    settings.benchMode ? "true" : "false",
    sessionLitres(), (unsigned long)(sessionDurationMs() / 1000), sessionFreq.mean,
    sessionFreq.sd(), (unsigned long)sessionFreq.n,
    fuelClassLabel(verdict), fuelClassKey(verdict),
    (unsigned long)k[(uint8_t)FuelClass::Pure], (unsigned long)k[(uint8_t)FuelClass::Slight],
    (unsigned long)k[(uint8_t)FuelClass::Moderate], (unsigned long)k[(uint8_t)FuelClass::High],
    (unsigned long)k[(uint8_t)FuelClass::SensorFault], (unsigned long)k[(uint8_t)FuelClass::NoSignal],
    settings.lifetimeLitres,
    calResultKey(), (unsigned long)calProgress, calMean, calSd, (unsigned long)calSamples,
    settings.th.pureMinHz, settings.th.pureMaxHz, settings.th.slightBandHz,
    settings.th.moderateBandHz, settings.pulsesPerLitre,
    (unsigned long)(millis() / 1000), QFLO_FW_VERSION, apSsid,
    WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "");
  return n > 0 ? min<size_t>(n, cap - 1) : 0;
}

// ---------------- WEB ----------------
void sendCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void sendJson(int code, const char* body) {
  sendCors();
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

void handleData() {
  static char json[1400];
  buildDataJson(json, sizeof(json));
  sendJson(200, json);
}

// POST /api/settings  (form fields, all optional)
// pureMin, pureMax, slight, moderate [Hz], ppl [pulses/L], bench [0|1], defaults [1]
void handleSettings() {
  if (server.hasArg("defaults")) {
    settings.setDefaults();
    settings.save();
    sendJson(200, "{\"ok\":true}");
    return;
  }
  Thresholds th = settings.th;
  if (server.hasArg("pureMin"))  th.pureMinHz = server.arg("pureMin").toFloat();
  if (server.hasArg("pureMax"))  th.pureMaxHz = server.arg("pureMax").toFloat();
  if (server.hasArg("slight"))   th.slightBandHz = server.arg("slight").toFloat();
  if (server.hasArg("moderate")) th.moderateBandHz = server.arg("moderate").toFloat();
  if (!thresholdsValid(th)) {
    sendJson(400, "{\"ok\":false,\"error\":\"need 0 < pureMin < pureMax and 0 < slight < moderate\"}");
    return;
  }
  float ppl = server.hasArg("ppl") ? server.arg("ppl").toFloat() : settings.pulsesPerLitre;
  if (!(ppl > 1 && ppl < 100000)) {
    sendJson(400, "{\"ok\":false,\"error\":\"ppl out of range\"}");
    return;
  }
  settings.th = th;
  settings.pulsesPerLitre = ppl;
  if (server.hasArg("bench")) settings.benchMode = server.arg("bench") == "1";
  settings.save();
  sendJson(200, "{\"ok\":true}");
}

void handleCalibrate() {
  startCalibration();
  sendJson(202, "{\"ok\":true,\"durationMs\":" QFLO_STR(CAL_DURATION_MS) "}");
}

void handleSessionReset() {
  resetSession();
  sendJson(200, "{\"ok\":true}");
}

void setupWeb() {
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML); });
  server.on("/data", HTTP_GET, handleData);      // v1 path, kept for existing dashboards
  server.on("/api/data", HTTP_GET, handleData);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/calibrate", HTTP_POST, handleCalibrate);
  server.on("/api/session/reset", HTTP_POST, handleSessionReset);
  server.onNotFound([] {
    if (server.method() == HTTP_OPTIONS) {  // CORS preflight for external dashboards
      sendCors();
      server.send(204);
    } else {
      sendJson(404, "{\"error\":\"not found\"}");
    }
  });
  server.begin();
}

// ---------------- WI-FI ----------------
void setupWifi() {
  uint64_t mac = ESP.getEfuseMac();
  snprintf(apSsid, sizeof(apSsid), "%s%02X%02X", AP_SSID_PREFIX,
           (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));

  WiFi.setHostname(HOSTNAME);
#ifdef QFLO_STA_SSID
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(QFLO_STA_SSID, QFLO_STA_PASSWORD);  // non-blocking; v1 waited forever here
#else
  WiFi.mode(WIFI_AP);
#endif
  WiFi.softAP(apSsid, QFLO_AP_PASSWORD);

  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);

  Serial.printf("Access point: %s  ->  http://%s\n", apSsid, WiFi.softAPIP().toString().c_str());
}

void announceStationIp() {
#ifdef QFLO_STA_SSID
  static bool announced = false;
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !announced) {
    Serial.printf("Joined %s  ->  http://%s  (or http://%s.local)\n", QFLO_STA_SSID,
                  WiFi.localIP().toString().c_str(), HOSTNAME);
  }
  announced = connected;
#endif
}

// ---------------- BLE (optional) ----------------
#if QFLO_ENABLE_BLE
BLECharacteristic* bleData = nullptr;

void setupBle() {
  BLEDevice::init(apSsid);
  BLEServer* bleServer = BLEDevice::createServer();
  bleServer->advertiseOnDisconnect(true);  // v1 became invisible after the first disconnect
  BLEService* service = bleServer->createService(BLE_SERVICE_UUID);
  bleData = service->createCharacteristic(
      BLE_DATA_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  bleData->addDescriptor(new BLE2902());
  service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(BLE_SERVICE_UUID);
  advertising->start();
}

// Compact JSON (~70 bytes): clients must request an MTU > 23 to receive it whole.
void bleNotify() {
  char buf[128];
  int n = snprintf(buf, sizeof(buf), "{\"f\":%.0f,\"q\":\"%s\",\"fl\":%.2f,\"t\":%.3f,\"s\":\"%s\"}",
                   filteredHz, fuelClassKey(displayedClass()), flowLpm, sessionLitres(),
                   sessionStateKey());
  if (n <= 0) return;
  bleData->setValue((uint8_t*)buf, min<size_t>(n, sizeof(buf) - 1));
  bleData->notify();
}
#endif

// ---------------- SERIAL ----------------
void printReport() {
  Serial.println(F("------ Q-FLO ------"));
  if (signalOk) {
    Serial.printf("Frequency : %.0f Hz (last gate %.0f Hz)\n", filteredHz, rawHz);
  } else {
    Serial.println(F("Frequency : NO SIGNAL - check 555 / sensor wiring"));
  }
  Serial.printf("Quality   : %s%s\n", fuelClassLabel(displayedClass()),
                settings.benchMode ? "  [bench mode]" : "");
  Serial.printf("Flow rate : %.2f L/min\n", flowLpm);
  Serial.printf("This fill : %.3f L (%s, %lu s)\n", sessionLitres(), sessionStateKey(),
                (unsigned long)(sessionDurationMs() / 1000));
  Serial.printf("Lifetime  : %.2f L\n", settings.lifetimeLitres);
}

void printHelp() {
  Serial.println(F(
    "Commands:\n"
    "  help          this list\n"
    "  status        print a report now\n"
    "  cal           capture the pure-fuel reference (10 s)\n"
    "  reset         clear the current refuelling session\n"
    "  bench on|off  classify without flow (static samples)\n"
    "  csv on|off    stream raw data as CSV, 5 lines/s\n"
    "  ppl <n>       set flow pulses per litre\n"
    "  defaults      restore factory thresholds"));
}

void runCommand(char* line) {
  char* cmd = strtok(line, " ");
  char* arg = strtok(nullptr, " ");
  if (!cmd) return;
  String c(cmd);
  c.toLowerCase();
  bool on = arg && (strcmp(arg, "on") == 0 || strcmp(arg, "1") == 0);

  if (c == "help") printHelp();
  else if (c == "status") printReport();
  else if (c == "cal") startCalibration();
  else if (c == "reset") { resetSession(); Serial.println(F(">> Session cleared")); }
  else if (c == "bench") { settings.benchMode = on; settings.save(); Serial.printf(">> Bench mode %s\n", on ? "ON" : "OFF"); }
  else if (c == "csv") {
    csvMode = on;
    if (on) Serial.println(F("ms,raw_hz,filtered_hz,flow_lpm,session_l,class,state"));
  }
  else if (c == "ppl" && arg) {
    float ppl = atof(arg);
    if (ppl > 1 && ppl < 100000) { settings.pulsesPerLitre = ppl; settings.save(); Serial.printf(">> %.2f pulses/L saved\n", ppl); }
    else Serial.println(F(">> ppl out of range"));
  }
  else if (c == "defaults") { settings.setDefaults(); settings.save(); Serial.println(F(">> Defaults restored")); }
  else Serial.println(F("Unknown command - type 'help'"));
}

void handleSerial() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      serialLine[serialLen] = '\0';
      runCommand(serialLine);
      serialLen = 0;
    } else if (serialLen < sizeof(serialLine) - 1) {
      serialLine[serialLen++] = ch;
    }
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\nQ-FLO firmware %s\n", QFLO_FW_VERSION);

  settings.load();

  // Pull-down on the 555 input: a disconnected sensor reads 0 Hz instead of noise.
  if (!freqCounter.begin(PIN_FREQ, FREQ_GLITCH_NS, GPIO_PULLDOWN_ONLY)) {
    Serial.println(F("ERROR: frequency counter init failed"));
  }
  // YF-S201 output is open-collector: needs a pull-up.
  if (!flowCounter.begin(PIN_FLOW, FLOW_GLITCH_NS, GPIO_PULLUP_ONLY)) {
    Serial.println(F("ERROR: flow counter init failed"));
  }
  lastGateUs = micros();
  lastGateMs = lastFlowWindowMs = millis();

  setupWifi();
  setupWeb();
#if QFLO_ENABLE_BLE
  setupBle();
#endif
  printHelp();
}

// ---------------- LOOP ----------------
// Nothing in here blocks, so the web server answers immediately
// (v1 blocked ~1.2 s per loop, and ~40 s when the 555 had no signal).
void loop() {
  server.handleClient();
  handleSerial();

  uint32_t now = millis();
  if (now - lastGateMs >= FREQ_GATE_MS) {
    lastGateMs = now;
    updateFrequency();
  }
  if (now - lastFlowWindowMs >= FLOW_WINDOW_MS) {
    updateFlow();
  }
  updateCalibration();

  if (now - lastReportMs >= REPORT_INTERVAL_MS) {
    lastReportMs = now;
    if (!csvMode) printReport();
    announceStationIp();
#if QFLO_ENABLE_BLE
    bleNotify();
#endif
  }
  delay(2);
}
