#include "Settings.h"

#include <Preferences.h>

#include "config.h"

static const char* kNamespace = "qflo";

void Settings::setDefaults() {
  th.pureMinHz = PURE_MIN_HZ_DEFAULT;
  th.pureMaxHz = PURE_MAX_HZ_DEFAULT;
  th.slightBandHz = SLIGHT_BAND_HZ_DEFAULT;
  th.moderateBandHz = MODERATE_BAND_HZ_DEFAULT;
  pulsesPerLitre = FLOW_PULSES_PER_LITRE_DEFAULT;
  benchMode = false;
}

void Settings::load() {
  setDefaults();
  lifetimeLitres = 0;

  Preferences prefs;
  // Read-write so the namespace is created on first boot (read-only open logs NOT_FOUND).
  if (!prefs.begin(kNamespace, false)) return;
  Thresholds saved = {
    prefs.getFloat("pureMin", th.pureMinHz),
    prefs.getFloat("pureMax", th.pureMaxHz),
    prefs.getFloat("slight", th.slightBandHz),
    prefs.getFloat("moderate", th.moderateBandHz),
  };
  if (thresholdsValid(saved)) th = saved;

  float ppl = prefs.getFloat("ppl", pulsesPerLitre);
  if (ppl > 1 && ppl < 100000) pulsesPerLitre = ppl;

  benchMode = prefs.getBool("bench", benchMode);
  lifetimeLitres = prefs.getDouble("lifetime", 0);
  prefs.end();
}

void Settings::save() const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return;
  prefs.putFloat("pureMin", th.pureMinHz);
  prefs.putFloat("pureMax", th.pureMaxHz);
  prefs.putFloat("slight", th.slightBandHz);
  prefs.putFloat("moderate", th.moderateBandHz);
  prefs.putFloat("ppl", pulsesPerLitre);
  prefs.putBool("bench", benchMode);
  prefs.end();
}

void Settings::saveLifetime() const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return;
  prefs.putDouble("lifetime", lifetimeLitres);
  prefs.end();
}
