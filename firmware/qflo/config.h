#pragma once

// Q-FLO build-time configuration.
// Values marked "default" are only used until you change them from the
// dashboard / serial console; after that the saved values (NVS) win.

#define QFLO_FW_VERSION "2.0.0"

// ---------------- PINS (ESP32 DevKit) ----------------
// Both inputs are 3.3 V only. If the NE555 or the YF-S201 runs from 5 V,
// put a divider / level shifter in front of the pin (see docs/HARDWARE.md).
#define PIN_FREQ 32   // NE555 output (capacitive fuel sensor)
#define PIN_FLOW 27   // YF-S201 flow sensor signal

// ---------------- FREQUENCY MEASUREMENT ----------------
// The 555 output is counted in hardware (PCNT) over a fixed gate.
// 200 ms gate -> 1 count = 5 Hz resolution (pulseIn() gave ~1.2 kHz per sample).
#define FREQ_GATE_MS        200
#define FREQ_MEDIAN_WINDOW  5        // median of the last 5 gates (~1 s)
#define FREQ_MIN_VALID_HZ   1000.0f  // below this the oscillator is treated as dead
#define FREQ_GLITCH_NS      200      // keep short: a 555 with R1 >> R2 has very short LOW pulses

// ---------------- FLOW MEASUREMENT ----------------
// YF-S201 datasheet: f(Hz) = 7.5 x Q(L/min)  ->  7.5 x 60 = 450 pulses per litre.
// That figure is for water; calibrate with petrol (docs/CALIBRATION.md).
#define FLOW_PULSES_PER_LITRE_DEFAULT 450.0f
#define FLOW_WINDOW_MS       1000
#define FLOW_GLITCH_NS       10000
#define FLOW_START_LPM       0.5f    // flow above this starts / sustains a refuelling session
#define SESSION_END_IDLE_MS  10000   // a session ends after this long without flow

// ---------------- CLASSIFICATION DEFAULTS ----------------
// Reproduces Table 2 of the Q-FLO paper. Re-run the calibration ("cal") with
// known-pure fuel on your own sensor cell before trusting these numbers.
#define PURE_MIN_HZ_DEFAULT      23950.0f
#define PURE_MAX_HZ_DEFAULT      34196.0f
#define SLIGHT_BAND_HZ_DEFAULT    5000.0f   // pureMax < f <= pureMax + 5 kHz   (~ up to 10 %)
#define MODERATE_BAND_HZ_DEFAULT 15000.0f   // pureMax + 5 kHz < f <= + 15 kHz  (~ 10-30 %)

// ---------------- CALIBRATION ----------------
// Pure band = mean +/- max(CAL_SIGMA_K * sd, CAL_MIN_HALF_BAND_PCT * mean)
#define CAL_DURATION_MS        10000
#define CAL_MIN_SAMPLES        20
#define CAL_SIGMA_K            3.0f
#define CAL_MIN_HALF_BAND_PCT  0.03f

// ---------------- NETWORK ----------------
// The device always starts its own access point (Q-FLO-XXXX, dashboard at
// http://192.168.4.1). Put credentials in secrets.h to also join a network.
#define AP_SSID_PREFIX "Q-FLO-"
#define HOSTNAME       "qflo"        // http://qflo.local on a shared network

// ---------------- REPORTING ----------------
#define REPORT_INTERVAL_MS 1000

// ---------------- OPTIONAL BLE ----------------
// Wi-Fi + Bluetooth together do not fit the default partition table:
// set Tools > Partition Scheme > "Huge APP (3MB No OTA/1MB SPIFFS)" when enabling.
#ifndef QFLO_ENABLE_BLE
#define QFLO_ENABLE_BLE 0
#endif
#define BLE_SERVICE_UUID "5f1b0001-8c3e-4d6a-9a57-3b2f6e0c9a10"
#define BLE_DATA_UUID    "5f1b0002-8c3e-4d6a-9a57-3b2f6e0c9a10"
