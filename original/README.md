# Original mini-project sketches

These are the sketches as they were written and demonstrated for the B.Tech mini project,
kept unchanged apart from removing Wi-Fi credentials. The maintained firmware is
[`firmware/qflo`](../firmware/qflo); use that one for new builds.

| Sketch | Purpose |
|--------|---------|
| `Flow_sensor/` | YF-S201 flow sensor bench test (interrupt counting) |
| `Reference_freq/` | Prints the 555 frequency to record the pure-petrol reference |
| `Comparison/` | Early classifier against a single reference frequency (23,950 Hz) |
| `Final/` | Wi-Fi web-server stub that returns test data at `/data` |
| `wifi_final/` | Fuel quality + flow over Wi-Fi (`/data` JSON), the version in the project report |
| `ble_final/` | Fuel quality + flow over BLE notifications |
| `dashboard/qflo_dashboard_dark.html` | The first web dashboard (replaced by [`dashboard/index.html`](../dashboard/index.html)) |

Wi-Fi credentials were replaced with `YOUR_WIFI_SSID` / `YOUR_WIFI_PASSWORD` placeholders.
