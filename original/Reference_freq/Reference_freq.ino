#define SENSOR_PIN 32

float measureFrequency() {
  unsigned long tHigh = pulseIn(SENSOR_PIN, HIGH);
  unsigned long tLow  = pulseIn(SENSOR_PIN, LOW);

  if (tHigh == 0 || tLow == 0) return 0;

  float period = tHigh + tLow;
  return 1000000.0 / period;
}

float getAverageFreq(int samples) {
  float sum = 0;
  int count = 0;

  for(int i=0;i<samples;i++) {
    float f = measureFrequency();
    if(f > 0) {
      sum += f;
      count++;
    }
    delay(15);
  }

  return (count > 0) ? sum / count : 0;
}

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_PIN, INPUT);

  Serial.println("PURE PETROL REFERENCE CALIBRATION");
}

void loop() {
  float F_ref = getAverageFreq(25);

  Serial.print("F_ref (Pure Petrol): ");
  Serial.print(F_ref);
  Serial.println(" Hz");

  delay(1500);
}