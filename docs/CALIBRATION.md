# Calibration and validation

Everything here works with the firmware through either the **dashboard**
(*Calibration & settings*) or the **serial console** at 115200 baud (type `help`).

> Work outdoors or in a fume hood with small volumes, no ignition sources and an
> extinguisher at hand. Keep fuel samples in labelled, closed containers.

## 1. Flow sensor (pulses per litre)

The YF-S201 datasheet figure (7.5 Hz per L/min = 450 pulses/L) is for **water**.
Petrol is less viscous, so calibrate with the fuel you'll measure.

1. Run `reset` so the session counter starts at zero.
2. Pour a known volume (at least 2 L, measured in a graduated cylinder or a 5 L
   legal-metrology measure) through the sensor at a steady, realistic rate.
3. Read **This refuel** (litres) on the dashboard.
4. New factor = `current_ppl × displayed_litres / actual_litres`. Set it with `ppl <value>`
   or in the settings form.
5. Repeat at three flow rates (slow, medium, fast), then use the average. If the factor varies
   by more than a few percent across rates, the sensor is outside its linear range at those rates.

## 2. Dielectric reference (pure band)

1. Clean and dry the cell, then flush it twice with the reference fuel.
2. Fill it completely, with no bubbles. Wait about 60 s for the reading to settle.
3. Turn on **bench mode** (`bench on`) so the device classifies without flow.
4. Start calibration (`cal`, or the dashboard button). It averages 10 s of readings and sets the pure band to
   `mean ± max(3·sd, 3 % of mean)`. Settings are stored in flash, so no re-flash is needed.
5. Do this three times with fresh fills. If the three means differ by more than the band
   width, the cell isn't repeatable yet.

Record the reference fuel for every calibration: pump or brand, date, **E-grade (E20?)**, and temperature.

## 3. Collecting a validation dataset

Turn on CSV streaming with `csv on`. The device prints five lines per second:

```
ms,raw_hz,filtered_hz,flow_lpm,session_l,class,state
123400,34118.2,34105.0,0.000,0.0000,pure,idle
```

Capture it with the Arduino IDE serial monitor (copy and paste), PuTTY logging, or a small
Python script. Save one file per run, named like `2026-10-02_run07_P+K10.csv`.

### Suggested protocol

| Factor | Levels |
|--------|--------|
| Base fuel | market E20 petrol from ≥ 3 pumps; diesel |
| Adulterant | kerosene, diesel (in petrol); kerosene (in diesel); water (small %) |
| Fraction (by volume) | 0, 2, 5, 10, 20, 30, 50 % |
| Replicates | ≥ 5 independent fills per level, in randomised order |
| Temperature | log it, and test at least a cool and a warm condition |

Keep a sample sheet (`data/samples.csv`) alongside the logs:

```
run_id,date,operator,base_fuel,source,adulterant,fraction_pct,temp_c,file,notes
run07,2026-10-02,AD,E20 petrol,Pump A,kerosene,10,29.5,2026-10-02_run07_P+K10.csv,
```

From this you can compute:
- A calibration curve (frequency against fraction), with its linearity and slope (sensitivity).
- A **limit of detection**: the smallest fraction distinguishable from 0 % at, say, 95 % confidence.
- Repeatability (same day) and reproducibility (different days, fills and temperatures).
- A **confusion matrix** from a blind test, where one teammate prepares the samples and another measures them.

These numbers are what a paper reviewer, a customer pilot or a grant panel will ask for.
They also form the dataset any future ML model needs.
