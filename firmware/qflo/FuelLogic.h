#pragma once

// Pure measurement / classification logic. No Arduino dependencies, so it can
// be unit-tested on a PC later.

#include <math.h>
#include <stdint.h>
#include <string.h>

// ---------------- CLASSES ----------------
enum class FuelClass : uint8_t {
  NoSignal = 0,  // oscillator dead or disconnected
  SensorFault,   // below the pure band (paper: "faulty signal")
  Waiting,       // no fuel flowing, so the chamber may hold air
  Pure,
  Slight,
  Moderate,
  High,
};
constexpr uint8_t kFuelClassCount = 7;

struct Thresholds {
  float pureMinHz;
  float pureMaxHz;
  float slightBandHz;    // deviation above pureMax still counted as "slight"
  float moderateBandHz;  // deviation above pureMax still counted as "moderate"
};

inline bool thresholdsValid(const Thresholds& t) {
  return t.pureMinHz > 0 && t.pureMinHz < t.pureMaxHz &&
         t.slightBandHz > 0 && t.slightBandHz < t.moderateBandHz;
}

// Deviation is measured from the upper edge of the pure band (paper, Table 2).
inline FuelClass classifyFrequency(float hz, const Thresholds& t, float minValidHz) {
  if (!(hz >= minValidHz)) return FuelClass::NoSignal;  // also catches NaN
  if (hz < t.pureMinHz) return FuelClass::SensorFault;
  if (hz <= t.pureMaxHz) return FuelClass::Pure;
  float deviation = hz - t.pureMaxHz;
  if (deviation <= t.slightBandHz) return FuelClass::Slight;
  if (deviation <= t.moderateBandHz) return FuelClass::Moderate;
  return FuelClass::High;
}

// Human-readable label. Pure/Slight/Moderate/High/SensorFault keep the exact
// strings the v1 firmware sent, so existing dashboards keep working.
inline const char* fuelClassLabel(FuelClass c) {
  switch (c) {
    case FuelClass::NoSignal:    return "NO SIGNAL";
    case FuelClass::SensorFault: return "SENSOR ERROR";
    case FuelClass::Waiting:     return "WAITING FOR FUEL";
    case FuelClass::Pure:        return "PURE PETROL";
    case FuelClass::Slight:      return "SLIGHTLY ADULTERATED";
    case FuelClass::Moderate:    return "MODERATELY ADULTERATED";
    case FuelClass::High:        return "HIGHLY ADULTERATED";
  }
  return "UNKNOWN";
}

// Short machine-readable key used in the JSON API and CSV log.
inline const char* fuelClassKey(FuelClass c) {
  switch (c) {
    case FuelClass::NoSignal:    return "no_signal";
    case FuelClass::SensorFault: return "fault";
    case FuelClass::Waiting:     return "waiting";
    case FuelClass::Pure:        return "pure";
    case FuelClass::Slight:      return "slight";
    case FuelClass::Moderate:    return "moderate";
    case FuelClass::High:        return "high";
  }
  return "unknown";
}

// ---------------- STATISTICS ----------------
// Welford running mean / standard deviation (numerically stable, O(1) memory).
struct RunningStats {
  uint32_t n = 0;
  double mean = 0;
  double m2 = 0;

  void reset() { n = 0; mean = 0; m2 = 0; }
  void add(double x) {
    n++;
    double d = x - mean;
    mean += d / n;
    m2 += d * (x - mean);
  }
  double sd() const { return n > 1 ? sqrt(m2 / (n - 1)) : 0; }
};

inline float medianOf(const float* values, uint8_t n) {
  float tmp[16];
  if (n == 0) return 0;
  if (n > 16) n = 16;
  memcpy(tmp, values, n * sizeof(float));
  for (uint8_t i = 1; i < n; i++) {
    float v = tmp[i];
    int8_t j = i - 1;
    while (j >= 0 && tmp[j] > v) { tmp[j + 1] = tmp[j]; j--; }
    tmp[j + 1] = v;
  }
  return (n & 1) ? tmp[n / 2] : 0.5f * (tmp[n / 2 - 1] + tmp[n / 2]);
}
