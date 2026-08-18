# EXP-02 — Does the FPGA accept scope channel config over USART2?

- **Date:** 2026-08-17
- **Unit:** bench unit #1
- **Build:** `guest-coldtrace` + runtime `fpga usart on`, then `guest-coldtrace-usart`
- **Status:** **VOID — control failed.** Not a negative result.

## 1. Problem
Stock's command table puts scope channel configuration on USART2 (cmd `0x01` =
"configure channel", type 0 = CH1 / type 1 = CH2; `0x0B`–`0x11` =
channel/trigger/timebase). Our coldtrace builds define
`FPGA_USART_SILENT_SCOPE`, leaving that bus electrically dark. CH2 has been
hunted on SPI3 for a month while the command that names itself "configure
channel" sits on a bus we switched off.

## 2. Hypothesis
If channel selection is a USART2 command, sending cmd `0x01` type 1 (and/or
`0x0B`–`0x11`) makes the CH2 tone appear in one of the capture buffers.
Falsifier: the tone stays absent while a known-good USART command still gets a
reply.

## 3. Procedure
Added `fpga usart [on|off]`, which brings USART2 up the same way `fpga_init`
does and **reports the registers back** rather than assuming them.

**Preconditions verified by readback:**
| what | expected | measured |
|---|---|---|
| USART2 CTRL1 before | dark | `0x00000000` |
| USART2 CTRL1 after | UEN/TE/RE/RDBFIEN | `0x0000202C` ✅ |
| USART2 BAUDR | stock's `0x30D4` | `0x000030D4` ✅ |

## 4. Control
| control | expected | measured | passed? |
|---|---|---|---|
| `0x0508` meter configure (sent by `fpga_init` itself in non-silent builds) | echo frame | rx_bytes=0 | ❌ |
| `0x0509` meter start | echo frame | rx_bytes=0 | ❌ |
| `0x0009` | echo frame | rx_bytes=0 | ❌ |

**The control failed, so this experiment is VOID.** With no known-good command
being answered, silence from a scope command carries no information.

## 5. Results
No scope command changed either buffer. **This result is not usable.**

Two instrument defects found while investigating the control failure:
1. **`usart tx` QUEUES its frame**, and coldtrace never creates the task that
   drains the queue — those commands were never transmitted at all. This also
   **voids the 2026-08-16 "no echo ever returned" negative**, which has been
   recorded as a closed door ever since. Use `usart raw` (direct) instead.
2. `USART2 STS = 0x000000C0` — TDC and TDBE set, so the MCU genuinely
   transmits; RDBF/ORERR/FERR/NERR all **0**, so not one byte ever arrives on
   PA3. Not an ISR or counter artifact: the receive line is silent.

## 6. Blind spots
- Absence of RDBF proves no bytes arrived; it does **not** prove the RX path
  would count them if they did. RX remains unvalidated on this build.
- PA2 is configured as AF push-pull even under the silent flag, so the failure
  is *not* a pin-configuration artifact — but that is the only pin checked.

## 7. Conclusion
- **Established:** nothing about scope channel config.
- **Established incidentally, and it matters more:** USART2 answers nothing on
  builds that have configured the FPGA — see EXP-03.
- **NOT excluded:** USART2 as the channel-config path. It remains the only
  unexhausted channel, and **every USART negative this project has recorded was
  taken through this same dead bus.**
