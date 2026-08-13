# OpenScope Modules — procedure data

**Status: PROVISIONAL.** This directory holds domain procedure files as flat JSON.
There is no loader in the firmware yet, and the schema below is a placeholder that
exists so there is real content to design a loader against.

**Dev plan item D1 owns the real schema and loader spec, and may change everything
here.** If you are working D1: treat these files as requirements input, not as a
contract. If a field below is awkward for the interpreter you are designing, change
it — the content is what matters, not this shape.

---

## Why the schema looks like this

It is deliberately flat and self-describing. Every value a user has to act on is a
string or a number in an obvious place. There are no cross-references between
sections (an earlier sketch in `docs/ideas/feature_catalog.md` had a `pass_fail`
list that referenced `measurement` ids by name, and its one worked example already
contained a dangling reference — so expected values are folded directly into each
measurement here instead).

## File layout

```
modules/
  automotive/   *.json
  hvac/         *.json
  ham_radio/    *.json
  education/    *.json
```

One procedure per file. Filename matches the `id` suffix.

## Schema (v0.1-provisional)

```jsonc
{
  "schema_version": "0.1-provisional",
  "id": "automotive/can_bus_wiring_ohms",   // "<directory>/<filename without .json>"
  "name": "Human-readable title",
  "category": "Automotive",                 // Automotive | HVAC | Ham Radio | Education
  "version": "1.0.0",
  "summary": "One sentence: what this procedure tells you.",

  "setup": {
    "mode": "multimeter",                   // multimeter | oscilloscope | signal_generator
    "meter_submode": "Resistance",          // multimeter only; one of the 10 UI sub-modes
    "firmware_build": "make guest",         // which build target this works under, TODAY
    "requires": [                           // things the user must supply
      "Probe leads",
      "OBD-II breakout or back-probe pins"
    ],

    // oscilloscope only, all optional:
    "channels": {
      "ch1": { "label": "CAN High", "probe": "1X", "coupling": "DC", "volts_div": "500mV" },
      "ch2": { "label": "CAN Low",  "probe": "1X", "coupling": "DC", "volts_div": "500mV" }
    },
    "timebase": "5us",                      // must be a label from scope_state.c timebase_table
    "trigger": { "source": "ch1", "mode": "Auto", "edge": "Rising" }
  },

  "safety": [                               // plain strings, shown before the procedure runs
    "Ignition OFF and key removed before any resistance measurement."
  ],

  "trust": {                                // honesty block — see "Instrument reality" below
    "quantitative": true,                   // may the user believe the numbers?
    "notes": [ "Resistance readings on this firmware are accurate to a few percent." ]
  },

  "instructions": [
    { "step": 1, "text": "What to do.", "expect": "What you should see." }
  ],

  "measurements": [
    // Numeric form — for meter readings with a defensible expected value:
    { "id": "termination", "label": "Bus termination", "type": "meter_resistance",
      "unit": "ohm", "nominal": 60, "min": 54, "max": 66,
      "fail": "Reads ~120 ohm: one terminating resistor is missing or the bus is open." },

    // Observation form — for anything the instrument cannot yet quantify:
    { "id": "mirror", "label": "CANH/CANL mirror symmetry", "type": "observation",
      "expect": "CH1 and CH2 move in opposite directions by a similar amount.",
      "fail": "One line flat or both moving together: shorted or open line." }
  ],

  "knowledge_base": {                       // free-form string map, key -> explanation
    "why_60_ohms": "Two 120 ohm terminators in parallel ..."
  }
}
```

### Fields a future loader must handle

- `setup.firmware_build` is load-bearing today. The firmware cannot currently do
  scope capture and multimeter in the same image (see below). A loader should
  refuse, or clearly warn, when a module's required build is not the running one.
- `trust.quantitative: false` means the module must not present its `expect`
  strings as measurements. These procedures are diagnostic-by-shape.
- `measurements[]` entries are either numeric (`nominal`/`min`/`max`) or
  observational (`expect`). Both carry `fail`.
- Nothing here assumes the interpreter can *set* the instrument. Every setting is
  also written into `instructions` as something the user does by hand, because the
  UI has no remote-configuration path yet.

---

## Instrument reality these modules were written against

Verified against the tree on 2026-08-13. This is the reason several obvious
procedures are absent.

| Capability | State |
|---|---|
| Meter resistance / continuity / diode | Real. Accurate to a few percent on bench unit #1. |
| Meter DC volts **below ~10 V** | Real. |
| Meter DC volts **above ~10 V** | Unreliable — decimal-point latch bug (`CLAUDE.md`). |
| Meter capacitance | Implemented, never bench-verified. Check against a known part first. |
| Scope capture | Real, in `make guest-coldtrace` only. |
| Scope amplitude scale | Placeholder calibration. Absolute volts are not trustworthy. |
| Scope timebase | The selector does not set a known sweep rate yet. Horizontal axis is relative. |
| Scope auto-measure badges (Freq/Vpp/Vrms/Duty/Period) | **Hardcoded display strings.** `measurement_compute()` has zero call sites. |
| Scope cursors | `time_per_pixel` / `volts_per_pixel` are fixed constants, not derived from the timebase. Same problem. |
| Signal generator output | Works in `make guest`; **inert in `guest-coldtrace`** (shares DAC1/PA4 with the CH1 trigger reference). |
| Meter in `guest-coldtrace` | Inactive — that build holds USART2 dark. |
| Protocol decoders (CAN/I2C/SPI/UART/K-Line) | Code exists, not reachable from any UI. |
| Bode / roll / XY modes | Code exists, not reachable from any UI. |

Two consequences shaped every file here:

1. **No module reads a number off the scope.** Not from a badge, not from a cursor.
   Scope-based procedures are qualitative: presence, shape, and CH1-vs-CH2
   comparison. Those are real; the numbers are not.
2. **No module needs two instrument functions at once.** Scope + meter, or
   scope + generator, cannot coexist in one build today.

### Grounding — applies to every scope procedure

CH1 and CH2 share one ground return. This is not a differential or isolated scope.
With USB connected to a mains-powered computer, that shared ground is earth
referenced. Do not clip the ground lead to anything that is not at your reference
potential, and do not attempt to scope line-voltage-referenced points at all.

---

## Contributing a module

Domain knowledge is the scarce ingredient, not JSON. If you know a trade, the most
useful thing you can do is:

- State expected values you can defend, with the source or the standard they come
  from. A module that teaches a wrong number is worse than no module.
- Say what the reading means when it is wrong, not just what "good" looks like.
- Include the failure that gets misdiagnosed most often in your trade.
- Be explicit about hazards. Assume the reader is competent with instruments and
  new to your domain.

See `CONTRIBUTING.md` — module contributions are green-light, file freely.
