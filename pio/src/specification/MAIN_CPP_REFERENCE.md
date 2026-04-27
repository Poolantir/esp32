# Firmware reference — `main.cpp` entry point

This document describes the **poolantir_simulation** firmware as orchestrated from [`src/main.cpp`](../main.cpp). Behavior is split across modules in `src/`; `main.cpp` owns global **mode**, **BLE JSON dispatch**, and **setup/loop** wiring.

---

## Libraries and purposes

| Source | Role |
|--------|------|
| **Arduino / ESP32** (`Arduino.h`) | Core framework: `setup`, `loop`, `Serial`, `String`, GPIO, `millis`, etc. |
| **ArduinoJson** (`ArduinoJson.h`) | Parse incoming BLE JSON and build outbound JSON (MODE ack, ECHO, FLASH ack, SIM COMPLETE). |
| **ESP-IDF / Arduino BLE** (via `ble.cpp`) | GATT server, notify, advertising, characteristic write callbacks. Included with `espressif32` + Arduino framework. |
| **Wire** (`Wire.h`, in `tof.cpp`) | I²C for VL53L0X. |
| **VL53L0X** (Pololu, `lib_deps`) | Time-of-flight ranging. |
| **ESP32Servo** (`lib_deps`) | PWM servo control on ESP32. |
| **ESP32PWM** (bundled with ESP32Servo) | Timer allocation for servo PWM. |
| **`<mutex>`, `<deque>`** (in `ble.cpp`) | Thread-safe BLE RX queue and JSON reassembly buffer. |
| **Project headers** | `config.h` (pins, defaults, `Mode`), `servo.h`, `led.h`, `tof.h`, `ble.h`, `test_mode.h`, `sim_mode.h`, `clock.h` (millis timers). |

**PlatformIO** (`platformio.ini`, env `poolantir_simulation`): `pololu/VL53L0X`, `madhephaestus/ESP32Servo`, `bblanchon/ArduinoJson`. Node ID comes from build flag `POOLANTIR_NODE_ID` (see `extra_script_node_id.py`).

---

## Pinout and peripheral mapping

Defined in [`src/config.h`](../config.h).

| Symbol | GPIO | Peripheral |
|--------|------|--------------|
| `PIN_SERVO` | **14** | Pan servo (PWM via ESP32Servo) |
| `LED_R` | **25** | Red LED (active HIGH) |
| `LED_G` | **26** | Green LED |
| `LED_B` | **27** | Blue LED |
| `TOF_SDA` | **21** | VL53L0X I²C data |
| `TOF_SCL` | **22** | VL53L0X I²C clock |

**Defaults** (tunable at runtime via `FLASH`; see [COMMANDS.md](COMMANDS.md)):

- ToF “in range” threshold: `DEFAULT_IN_RANGE_MM` = 60 mm (clamped in firmware when set).
- Servo ramp duration: `DEFAULT_SERVO_RAMP_MS` = 2000 ms.
- Servo angles: `SERVO_REST_DEG` = 0°, `SERVO_MAX_DEG` = 180°.

---

## Starting states

### After `setup()` (boot order)

1. `Serial.begin(115200)`
2. `ledInit()` — all LEDs off
3. `tofInit()` — I²C + VL53L0X init (hangs with red LED if init fails — see `tof.cpp`)
4. `servoInit()` — attach servo, **immediate** move to REST (0°)
5. `delay(500)` — short settle
6. `enterTestMode()` — **default mode** = TEST (see below)
7. `bleInit()` — BLE GATT service/characteristic, start advertising

**Global in `main.cpp`:** `gMode = MODE_TEST` before `setup` runs; `enterTestMode()` matches that.

### TEST mode entry (`enterTestMode()`)

- Servo: **immediate** REST
- LED: **blue**
- ToF: **continuous** ranging on (for LED coupling and queue test)
- Test queue state cleared

### SIM mode entry (`enterSimMode()`), when `MODE` / `SET` / `SIM` applied

- Servo: **immediate** REST
- LED: **green**
- ToF: **continuous** ranging **off**
- SIM FSM idle; pause cleared

---

## Operation flow

### High-level architecture

```text
setup: HAL init → enterTestMode → BLE start
loop:  drain BLE RX → JSON dispatch → mode tick → servoTick → BLE advertising watchdog
```

`main.cpp` does **not** implement hardware drivers; it calls `test_mode` / `sim_mode` / HAL modules.

### `loop()` sequence (every ~5 ms)

1. **BLE RX** — While `bleHasRxMessage()`, `blePopRxMessage()` → `handleCommand(raw)` (one JSON object per message; fragmentation reassembled in `ble.cpp`).
2. **Mode tick** — If `gMode == MODE_TEST` → `testModeTick()`; else → `simModeTick()`.
3. **`servoTick()`** — Advances animated servo moves (both TEST and SIM use this).
4. **`bleEnsureAdvertising()`** — If disconnected, periodically restart advertising.

### Command dispatch (`handleCommand`)

Rough order of checks (see `main.cpp` for exact rules):

| `command` | When accepted | Notes |
|-----------|----------------|--------|
| `MODE` | Always | `action`: `TEST` or `SIM` → `setMode` → `enterTestMode` / `enterSimMode` → **TX** MODE ack JSON. |
| `ECHO` | Always | `type` must be `MESSAGE`; echoes `action` string. |
| `FLASH` | Always | `IN_RANGE` / `SERVO_RAMP`; **TX** FLASH ack with clamped applied value. |
| `TEST` | Only `MODE_TEST` | `LED`, `SERVO`, `QUEUE` (see [TEST_MODE.md](TEST_MODE.md), [COMMANDS.md](COMMANDS.md)). |
| `SIM` | Only `MODE_SIM` | `NEW` (user + duration), `CONTROL` (`PAUSE` / `PLAY`). |

Unknown commands logged and ignored. Invalid JSON logged.

### TEST vs SIM (where logic lives)

- **TEST** — `test_mode.cpp`: ToF ↔ LED (blue idle, red in range; queue test uses green base), LED flash test, servo test moves, queue state machine.
- **SIM** — `sim_mode.cpp`: user cycle (animate to MAX → hold → snap REST → COMPLETE), pause/play freeze/resume.

### Outbound JSON (from `main.cpp` helpers)

- MODE ack, ECHO ack, FLASH ack shapes built here and sent via `bleSendMessage()`.
- SIM COMPLETE built in `sim_mode.cpp`.

### BLE identity

Device name: `poolantir-node-{POOLANTIR_NODE_ID}`. Service and characteristic UUIDs suffix the same node id string (see `ble.cpp`).

---

## Related specification files

| File | Contents |
|------|----------|
| [COMMANDS.md](COMMANDS.md) | Central server ↔ ESP32 JSON protocol |
| [TEST_MODE.md](TEST_MODE.md) | TEST behavior |
| [SIMULATION_MODE.md](SIMULATION_MODE.md) | SIM behavior |
| [ANIMATION.md](ANIMATION.md) | Servo immediate vs animated motion |
| [main.md](main.md) | End-to-end firmware flow overview |

---

## File map (firmware sources)

| File | Responsibility |
|------|----------------|
| `main.cpp` | Mode variable, command router, TX helpers for MODE/ECHO/FLASH, `setup` / `loop` |
| `config.h` | Pins, defaults, `Mode` enum |
| `ble.cpp` | BLE stack, RX queue, JSON reassembly, notify TX |
| `servo.cpp` | Immediate / animated servo, pause/resume animation, ramp ms |
| `led.cpp` | RGB GPIO, solid colors, blocking flash for LED test |
| `tof.cpp` | VL53L0X, continuous mode, in-range threshold |
| `test_mode.cpp` | TEST tick + commands |
| `sim_mode.cpp` | SIM FSM, NEW user, pause/play, COMPLETE |
| `clock.h` | Millis-based `ClockTimer` |
