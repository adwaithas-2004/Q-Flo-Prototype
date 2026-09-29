/*
   ESP32 Flow Sensor Test
   Sensor: YF-S201 or similar
*/

#define FLOW_PIN 27   // Signal pin

volatile unsigned long pulseCount = 0;

float calibrationFactor = 7.5;  
// Typical for YF-S201 (pulses per second per L/min)
// Adjust after calibration

unsigned long lastTime = 0;
float flowRate = 0.0;
float totalLiters = 0.0;

// ================= INTERRUPT =================
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, RISING);

  Serial.println("Flow Sensor Test Started");
}

// ================= LOOP =================
void loop() {

  if ((millis() - lastTime) >= 1000) {

    // Calculate flow rate (L/min)
    flowRate = (pulseCount / calibrationFactor);

    // Convert to L/min (since pulses counted in 1 sec)
    flowRate = flowRate;

    // Calculate volume for this second
    float flowLiters = flowRate / 60.0;

    totalLiters += flowLiters;

    // Print results
    Serial.print("Flow Rate: ");
    Serial.print(flowRate);
    Serial.print(" L/min");

    Serial.print(" | Total Volume: ");
    Serial.print(totalLiters, 3);
    Serial.println(" L");

    pulseCount = 0;
    lastTime = millis();
  }
}