#define SENSOR_PIN 32

// 🔧 PURE PETROL REFERENCE
float F_petrol = 23950.0;

// ---------------- FREQUENCY ----------------

float measureFrequency() {
  unsigned long tH = pulseIn(SENSOR_PIN, HIGH);
  unsigned long tL = pulseIn(SENSOR_PIN, LOW);

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

// ---------------- CLASSIFICATION ----------------

String classifyFuel(float f) {

  float deviation = f - F_petrol;

  if (deviation < 3000) {
    return "PURE FUEL";
  }
  else if (deviation < 22000) {
    return "SLIGHTLY ADULTERATED";
  }
  else if (deviation < 26000) {
    return "MODERATELY ADULTERATED";
  }
  else {
    return "HIGHLY ADULTERATED";
  }
}

// ---------------- MAIN ----------------

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_PIN, INPUT);
}

void loop() {

  float F_now = getStableFreq();
  float deviation = F_now - F_petrol;

  Serial.println("------ Q-FLO ANALYSIS ------");

  Serial.print("Frequency: ");
  Serial.print(F_now);
  Serial.println(" Hz");

  Serial.print("Deviation: ");
  Serial.print(deviation);
  Serial.println(" Hz");

  String status = classifyFuel(F_now);

  Serial.print("Fuel Quality: ");
  Serial.println(status);

  delay(1500);
}