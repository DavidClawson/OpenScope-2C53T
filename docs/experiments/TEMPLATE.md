# EXP-NN — <short title>

- **Date:** YYYY-MM-DD
- **Unit:** bench unit #1 / other
- **Build:** `make <target>`, commit `<sha>`
- **Status:** OPEN | CONFIRMED | REFUTED | **VOID (control failed)**

## 1. Problem
<The open project question this serves. Not a restatement of the procedure.>

## 2. Hypothesis
<Prediction WITH its falsifier: if true we see A, if false we see B.>

## 3. Procedure
<Exact commands, pin states, build flags, signal settings.>

**Preconditions verified by readback** (not assumed):
| what | expected | measured |
|---|---|---|
| e.g. IDCODE anchor | `0x0120681B` | |
| e.g. SPI3 CTRL1 | `0x0347` | |

## 4. Control
<The known-good case, same session, same path. Record it FIRST.>

| control | expected | measured | passed? |
|---|---|---|---|

> If the control fails, this experiment is **VOID, not negative**. Say so.

## 5. Results
<Raw numbers. Screen observations are weak evidence — label them.>

## 6. Blind spots
<What this test could NOT have detected. If you can't name one, you don't
understand the instrument yet.>

## 7. Conclusion
- **Established:**
- **Excluded:**
- **NOT excluded (explicitly):**
- **Follow-up:**
