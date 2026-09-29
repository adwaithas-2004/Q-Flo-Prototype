#include "PulseCounter.h"

bool PulseCounter::begin(int gpio, uint32_t glitchNs, gpio_pull_mode_t pull) {
  pcnt_unit_config_t unitConfig = {};
  unitConfig.low_limit = -1;
  unitConfig.high_limit = kHighLimit;
  unitConfig.flags.accum_count = 1;  // keep counting past the 16-bit limit
  if (pcnt_new_unit(&unitConfig, &unit_) != ESP_OK) return false;

  if (glitchNs > 0) {
    pcnt_glitch_filter_config_t filter = {};
    filter.max_glitch_ns = glitchNs;
    if (pcnt_unit_set_glitch_filter(unit_, &filter) != ESP_OK) return false;
  }

  pcnt_chan_config_t channelConfig = {};
  channelConfig.edge_gpio_num = gpio;
  channelConfig.level_gpio_num = -1;
  if (pcnt_new_channel(unit_, &channelConfig, &channel_) != ESP_OK) return false;

  // Count rising edges only.
  pcnt_channel_set_edge_action(channel_, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                               PCNT_CHANNEL_EDGE_ACTION_HOLD);

  // accum_count needs the limit registered as a watch point.
  if (pcnt_unit_add_watch_point(unit_, kHighLimit) != ESP_OK) return false;

  // The driver enables a pull-up by default; override it when asked.
  gpio_set_pull_mode((gpio_num_t)gpio, pull);

  if (pcnt_unit_enable(unit_) != ESP_OK) return false;
  if (pcnt_unit_clear_count(unit_) != ESP_OK) return false;
  return pcnt_unit_start(unit_) == ESP_OK;
}

uint32_t PulseCounter::take() {
  if (!unit_) return 0;
  int count = 0;
  pcnt_unit_get_count(unit_, &count);
  pcnt_unit_clear_count(unit_);
  return count > 0 ? (uint32_t)count : 0;
}
