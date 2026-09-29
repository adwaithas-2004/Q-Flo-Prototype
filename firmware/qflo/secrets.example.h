#pragma once

// Copy this file to secrets.h (git-ignored) and fill it in.
// Without secrets.h the firmware still works: it only runs its own access point.

// Password of the access point the device creates (Q-FLO-XXXX). Min. 8 characters.
#define QFLO_AP_PASSWORD "change-me-please"

// Optional: also join an existing network (phone hotspot, home/lab router).
// Delete these two lines to stay in access-point-only mode.
#define QFLO_STA_SSID     "your-network-name"
#define QFLO_STA_PASSWORD "your-network-password"
