#pragma once

#include <Arduino.h>
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "soc/soc_caps.h"

#if !SOC_PCNT_SUPPORTED
#error "Q-FLO needs a chip with the PCNT peripheral (ESP32, ESP32-S2/S3, ESP32-C6/H2). ESP32-C3 is not supported."
#endif

// Counts rising edges on a GPIO with the ESP32 hardware pulse counter.
// No CPU interrupt per edge, so it keeps up with the 20-70 kHz 555 output
// while Wi-Fi is running, and it never blocks (unlike pulseIn()).
class PulseCounter {
 public:
  bool begin(int gpio, uint32_t glitchNs, gpio_pull_mode_t pull);

  // Edges counted since the previous call; resets the counter.
  uint32_t take();

 private:
  static constexpr int kHighLimit = 30000;  // hardware counter is 16-bit signed
  pcnt_unit_handle_t unit_ = nullptr;
  pcnt_channel_handle_t channel_ = nullptr;
};
