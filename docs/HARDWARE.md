# Hardware

## Block diagram

```
                    ┌────────────── fuel path ──────────────┐
 nozzle ──► inlet ──► YF-S201 flow sensor ──► capacitive cell ──► tank
                         │ pulses (7.5 Hz per L/min)   │ C = f(ε_fuel)
                         │                              ▼
                         │                     NE555 / TLC555 astable
                         │                              │ 20–70 kHz square wave
                         ▼                              ▼
                  GPIO27 (PCNT unit)            GPIO32 (PCNT unit)
                         └──────────► ESP32 ◄───────────┘
                                        │ Wi-Fi AP + dashboard, JSON API, (BLE)
                                        ▼
                                  phone / laptop
```

## Pin map (`firmware/qflo/config.h`)

| Signal | ESP32 pin | Notes |
|--------|-----------|-------|
| 555 output | GPIO32 | Internal pull-down enabled, so an open wire reads 0 Hz ("NO SIGNAL"). **Max 3.3 V.** |
| YF-S201 signal | GPIO27 | Internal pull-up enabled (open-collector output). **Max 3.3 V.** |
| Sensor/timer supply | 3V3 or 5V | See level-shifting below |
| GND | GND | Star-ground the 555, the cell shield and the ESP32 |

Both inputs use the ESP32's PCNT hardware counter. Any PCNT-capable GPIO works;
avoid GPIO34–39 (no internal pull resistors) and the strapping pins (0, 2, 5, 12, 15).

## Level shifting: don't skip this

The ESP32's GPIO absolute maximum is 3.6 V.

- **Timer.** Preferably use a **TLC555 or LMC555 (CMOS 555) powered from 3.3 V**. It's pin-compatible,
  runs from about 2 V, and has pA-level input currents. That matters with the MΩ timing resistors
  a pF-scale sensor needs. The bipolar NE555 is only specified from 4.5 V. If you keep an NE555 at 5 V,
  put a **10 kΩ / 20 kΩ divider** between pin 3 and GPIO32.
- **YF-S201.** It needs 5 V. If your module has a pull-up to 5 V on the signal wire (measure the idle
  voltage), add the same 10 kΩ / 20 kΩ divider. If it idles near 0 V or floats, the ESP32's internal
  pull-up to 3.3 V is enough.

## 555 timer (astable)

`f = 1.44 / ((R_A + 2·R_B) · C)` with C = sensor capacitance + stray capacitance.

Correct connections, as implemented in the **rev B schematic** ([image](../images/schematic.svg),
KiCad files in [`hardware/kicad`](../hardware/kicad)). The starred lines were wrong on the first
(rev A) board. The PCB layout isn't included because it still carries the rev A nets.

```
+3.3V ── pin 8 VCC, pin 4 RESET; C4 100 nF to GND at pin 8
U1  = TLC555CP (CMOS 555, DIP-8) at +3.3 V
R2  = R_A 10 kΩ : VCC   → pin 7 DISCH
R3  = R_B 1 MΩ  : pin 7 → pins 6 + 2            * (schematic: +5V → pin 6)
pins 2 TRIG + 6 THRES tied together             * (schematic: not connected)
J1  sensor plates: pins 2/6 → GND               (the timing capacitor)
C2  2.2 nF: remove                              * (swamps the ~1–2 pF sensor)
C3  10 nF: pin 5 CONT → GND
pin 3 OUT → GPIO32 directly (TLC555 at 3.3 V; an NE555 at 5 V would need a divider)
flow: J2 pin 3 → D1 (BAT85, cathode to sensor) → GPIO27, R4 10 kΩ pull-up to 3.3 V
```

- With R_A = 10 kΩ and R_B = 1 MΩ the duty cycle is about 50 %, and 34 kHz means about 21 pF total
  timing capacitance, most of it stray. The next cell should raise the sensor's share (see below).
- The firmware's PCNT glitch filter is 200 ns (`FREQ_GLITCH_NS`). If you change R_A/R_B so the LOW
  pulse gets very short, check the duty cycle on a scope.
- Use 1 % metal-film resistors with low temperature coefficient. Put 100 nF decoupling at pin 8
  and 10 nF at pin 5.
- Keep the sensor leads short, rigid and shielded, with the shield grounded. With only a few pF
  of sensor capacitance, moving a wire changes the reading.

## Capacitive cell

| | Prototype v1 | Lessons learned: what the next version should do |
|---|---|---|
| Electrodes | 2 copper plates, 25–35 mm, 5–7 mm gap | **SS316** concentric cylinders or interdigitated plates |
| Insulation | not specified | PTFE / PEEK spacers, fuel-resistant sealing |
| Capacitance | ~1–2 pF (smaller than stray capacitance) | ≥ 20–50 pF, so the fuel term dominates the parasitics |
| Filling | depends on flow | Flooded cell with no trapped air: vertical flow upward, fixed volume |
| Readout | 555 + pulse timing | 555 + PCNT, or better a capacitance-to-digital converter (e.g. AD7746 / FDC1004) with a guard driver |
| Temperature | none | DS18B20 or NTC inside the cell (for compensation) |
| Self-check | none | Switchable reference capacitor, so the ratio cancels 555 and resistor drift |

Copper catalyses petrol oxidation and gum formation. Avoid copper, brass and zinc in the wetted path.

## Flow sensor

The YF-S201 is a **water** flow sensor. It is rated 1–30 L/min (car nozzles often run
30–40 L/min), has hobby-grade accuracy, and its plastics are not rated for petrol. It's fine
for a bench demo with small volumes. It is not suitable for real fuel; that needs a
fuel-rated meter.

## Power

- Bench: 5 V USB is fine.
- Vehicle: 12 V battery → **automotive buck converter** (with reverse-polarity protection,
  a load-dump TVS and a fuse) → 5 V / 3.3 V. A linear AMS1117 or LM7805 dropping 12 V wastes 2 W as heat.
- The engine and usually the ignition are **off during refuelling**. The device therefore needs a
  permanent (unswitched) supply plus a low-power sleep mode, woken by a fuel-lid switch.

## Current prototype bill of materials

| Qty | Part |
|-----|------|
| 1 | ESP32 DevKit (ESP32-WROOM-32) |
| 1 | NE555 (→ replace with TLC555) + R1, R2, 10 nF, 100 nF |
| 2 | Copper plates 25–35 mm (→ SS316) |
| 1 | YF-S201 hall-effect flow sensor |
| 1 | Voltage regulator (AMS1117 / LM7805) (→ buck converter) |
| – | PCB, connectors, tubing, flow chamber |

## Safety

Petrol vapour is explosive at very low concentrations. Test outdoors or in a fume hood with
small volumes and no ignition sources, and keep an extinguisher at hand. Never power the
prototype inside a vehicle's fuel system.
