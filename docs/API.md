# Q-FLO device API

The device always runs its own access point, **`Q-FLO-XXXX`** (password in `secrets.h`,
default `qflo1234`). The dashboard is at **http://192.168.4.1**. If it also joined a network
(`secrets.h`), it is reachable at its LAN IP and at **http://qflo.local**.

All JSON responses send `Access-Control-Allow-Origin: *`, so an external web dashboard
can call the device directly.

## Endpoints

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/` | Built-in dashboard (single self-contained page) |
| GET | `/api/data` | Live readings, session, calibration, thresholds |
| GET | `/data` | Same as `/api/data`. The v1 path, kept so older dashboards still work |
| POST | `/api/settings` | Form fields (all optional): `pureMin`, `pureMax`, `slight`, `moderate` (Hz), `ppl` (pulses/L), `bench` (`0`/`1`), `defaults=1` |
| POST | `/api/calibrate` | Start a 10 s pure-fuel reference capture → `202` |
| POST | `/api/session/reset` | Clear the current refuelling session |

Example:

```bash
curl -X POST http://192.168.4.1/api/settings -d "pureMin=33000&pureMax=35400&bench=1"
```

## `/api/data` response

```json
{
  "freq": 34105.0,            // Hz, median-filtered            (v1 field)
  "quality": "PURE PETROL",   // label shown to the user         (v1 field)
  "flow": 12.40,              // L/min                           (v1 field)
  "total": 5.672,             // litres in the current/last refuel (v1: since boot)
  "class": "pure",            // machine key, see below
  "signal": true,             // false = no 555 signal
  "rawFreq": 34118.2,         // Hz, last 200 ms gate
  "state": "fueling",         // idle | fueling | done
  "bench": false,
  "session": {
    "litres": 5.672, "durationS": 42, "avgFreq": 34101.2, "sdFreq": 85.3, "samples": 210,
    "verdict": "PURE PETROL", "verdictClass": "pure",
    "counts": { "pure": 200, "slight": 10, "moderate": 0, "high": 0, "fault": 0, "no_signal": 0 }
  },
  "lifetime": 123.45,
  "cal": { "state": "ok", "progress": 0, "mean": 34150.0, "sd": 60.2, "samples": 50 },
  "th": { "pureMin": 33125.5, "pureMax": 35174.5, "slight": 5000, "moderate": 15000, "ppl": 450 },
  "uptimeS": 3600, "fw": "2.0.0", "ap": "Q-FLO-3A7C", "staIp": "192.168.1.42"
}
```

### Quality classes

| `class` | `quality` label | Meaning | Dashboard colour |
|---------|-----------------|---------|------------------|
| `pure` | PURE PETROL | `pureMin ≤ f ≤ pureMax` | green |
| `slight` | SLIGHTLY ADULTERATED | `0 < f − pureMax ≤ slight` | yellow |
| `moderate` | MODERATELY ADULTERATED | `slight < f − pureMax ≤ moderate` | orange |
| `high` | HIGHLY ADULTERATED | `f − pureMax > moderate` | red |
| `fault` | SENSOR ERROR | `f < pureMin` | purple |
| `no_signal` | NO SIGNAL | `f < 1 kHz` (timer dead or disconnected) | grey |
| `waiting` | WAITING FOR FUEL | no flow, so quality is not judged | grey |

`quality` shows the **live** class while fuel flows, the **session verdict** (from the session's
mean frequency) after the refuel ends, and `WAITING FOR FUEL` before the first refuel.

## BLE (optional, `QFLO_ENABLE_BLE 1`)

- Device name: same as the access point (`Q-FLO-XXXX`)
- Service `5f1b0001-8c3e-4d6a-9a57-3b2f6e0c9a10`
- Characteristic `5f1b0002-8c3e-4d6a-9a57-3b2f6e0c9a10`: read + notify, once per second:
  `{"f":34105,"q":"pure","fl":12.40,"t":5.672,"s":"fueling"}`
- The payload is about 70 bytes, so the client must request an MTU above 23 (for example 185).
  nRF Connect and most Android/iOS libraries let you do this.
- Wi-Fi + BLE needs *Tools → Partition Scheme → Huge APP*.

## Serial console (115200 baud)

`help` · `status` · `cal` · `reset` · `bench on|off` · `csv on|off` · `ppl <n>` · `defaults`

## Security note

The API has no authentication. The access-point password is the only protection, so change it
in `secrets.h`. Before joining shared networks, add an API token and move to HTTPS/MQTT over
TLS.
