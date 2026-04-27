# ESP32 Firmware — Overall Flow

## Boot Sequence
1. Initialize LEDs, ToF sensor, and servo motor (servo goes to REST immediately).
2. Enter **TEST** mode (the default on power-up): LED turns BLUE, ToF begins continuous readings.
3. Initialize BLE and begin advertising as `poolantir-node-{NODE_ID}`.

## Main Loop
Each iteration of the main loop performs the following in order:
1. **Drain BLE RX queue** — deserialize every complete JSON message and route it through the command dispatcher.
2. **Mode tick** — run the active mode's state machine (TEST or SIM).
3. **Servo tick** — advance any in-progress animated servo move by one interpolation step.
4. **BLE advertising watchdog** — if disconnected, periodically restart advertising.

## Command Flow
All commands arrive as JSON over BLE. The dispatcher accepts the following top-level `command` values:

| Command | Mode restriction | Description |
|---------|-----------------|-------------|
| `MODE`  | any | Switch between TEST and SIM. ESP32 applies the transition and sends an ACK. |
| `ECHO`  | any | Loopback test — echoes the `action` value back to the central server. |
| `FLASH` | any | Set runtime-tunable parameters (`IN_RANGE` distance, `SERVO_RAMP` duration). |
| `TEST`  | TEST only | Trigger LED flash, servo move, or queue test. |
| `SIM`   | SIM only  | Send a new user for simulated usage cycle. |

See [COMMANDS.md](COMMANDS.md) for exact JSON shapes.

## Mode Transitions
On every `MODE SET` command, the ESP32:
1. Resets all mode-specific state (cancels any active cycle or queue).
2. Applies the target mode's starting configuration (LED color, servo position, ToF policy).
3. Sends a `MODE ACK` message to the central server.

Re-sending the current mode is valid and acts as a soft reset.

## TEST Mode
See [TEST_MODE.md](TEST_MODE.md) for detailed specification.

- LED is BLUE (idle) or RED (ToF detects an object in range).
- During a QUEUE test, the base LED is GREEN instead of BLUE, but in-range still overrides to RED.
- Servo commands use the **animated** path.

## SIM Mode
See [SIMULATION_MODE.md](SIMULATION_MODE.md) for detailed specification.

- ToF is disabled; LED tracks the servo animation cycle.
- On receiving a new user: animate servo to MAX → LED RED → hold for duration → snap servo to REST (immediate) → LED GREEN → send `SIM COMPLETE`.
- The ESP32 processes one user at a time; additional `SIM NEW` messages while busy are rejected.

## Servo Motion
See [ANIMATION.md](ANIMATION.md) for the motion model.

Two APIs exist:
- **Animated**: linear interpolation over a configurable ramp duration (default 2000 ms, adjustable via `FLASH SERVO_RAMP`).
- **Immediate**: instant `servo.write()` with no ramp — used for boot-to-REST and SIM end-of-cycle snap.

## Runtime-Tunable Parameters
The `FLASH` command allows the testbench to adjust:
- **IN_RANGE** — ToF distance threshold in mm (clamped 20–2000, default 60).
- **SERVO_RAMP** — animated move duration in ms (clamped 200–10000, default 2000).

Values take effect immediately and persist until power cycle.
