# Servo motion (animation)

## Purpose

Move the pan servo between `REST` and `MAX` (or any constrained angle) so motion is **visible and smooth**, not a single jump. All normal commanded moves use one consistent model.

## Goals

- **Smooth path** — prefer **linear interpolation in angle** over time, not a few large discrete steps. Fine-grained steps emerge naturally if the control loop updates often (e.g. every `loop()` or fixed tick)
- **Fixed duration for full travel** — moving across the full useful range (e.g. `REST` → `MAX`) should take a **known time** so SIM/TEST behavior is predictable. Original target: **3 s** for that full span; firmware may use a named constant (e.g. 2000 ms) until aligned.

## Motion model (recommended)

1. On **set animated target** (or mode command that maps to a degree):
   - `start_deg` ← current physical/commanded angle.
   - `target_deg` ← new constrained angle.
   - `t0_ms` ← `millis()`.
2. On each **tick** (main loop or timer):
   - `elapsed` = `now - t0_ms`
   - `u` = `min(1, elapsed / T_ms)` where `T_ms` is duration for **this** move (for a full 0–180° swing, use the agreed full-range duration; for shorter moves, either keep same `T_ms` or scale `T_ms` by `|target - start| / 180` — pick one policy and document it in code).
   - `angle` = `round( start_deg + (target_deg - start_deg) * u )`, then clamp to [0, 180].
   - `servo.write(angle)` when `angle` changes; update tracked current angle.
3. When `u >= 1`, clear “moving” flag.

This yields a **glide** (piecewise linear), not a staircase of 30° every 0.5 s.

## Two servo APIs (contract)

| API | Behavior |
|-----|----------|
| **Animated** | Takes target degree + duration (or uses default full-range duration). Runs interpolation until done or retarget. |
| **Immediate** | `servo.write(deg)` (or equivalent) with **no** time ramp — used where motion must be instant (e.g. **boot to REST**). |

Normal BLE/SIM/TEST **commands** that move the arm should go through the **animated** path unless explicitly documented as immediate.

## Boot

Initial position: move to `REST` **immediately** (no long ramp on power-up).

## Related
- `SERVO` test commands: [COMMANDS.md](COMMANDS.md) (TEST mode).
- SIM cycle uses the same motion primitives as other modes.