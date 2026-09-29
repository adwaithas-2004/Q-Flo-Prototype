#pragma once

#include "FuelLogic.h"

// Calibration and user settings, persisted in NVS flash so a recalibration
// no longer needs a re-flash.
struct Settings {
  Thresholds th;
  float pulsesPerLitre;
  bool benchMode;         // classify without flow (static samples on the bench)
  double lifetimeLitres;

  void setDefaults();
  void load();
  void save() const;          // thresholds, flow factor, bench mode
  void saveLifetime() const;  // called once per refuelling session, not every second
};
