# Poolantir ESP32 Command Reference (JSON)

All commands use one shared schema:

```json
{"command":"...","type":"...","action":"..."}
```

Notes:
- `action` may be a string or number, depending on the command.

---

## ESP32 -> SERVER

### In-use edge notification
- Trigger: ToF positive edge (`out-of-range -> in-range`) in `SIM` mode.

```json
{"command":"IN-USE","type":"STATE","action":"ENTER"}
```

### Completion notification
- Trigger: ToF negative edge (`in-range -> out-of-range`) in `SIM` mode.
- `duration_s` is the measured time in seconds.

```json
{"command":"COMPLETE","type":"DURATION_S","action":2.00}
```

### Echo acknowledgement
- Trigger: server sends `cmd=ECHO`.

```json
{"command":"ECHO","type":"MESSAGE","action":"hello"}
```

---

## SERVER -> ESP32

### `MODE`
- Accepted in any mode.
- `type` must be `"SET"`.
- Supported `action` values: `"TEST"`, `"SIM"`.

```json
{"command":"MODE","type":"SET","action":"TEST"}
```

```json
{"command":"MODE","type":"SET","action":"SIM"}
```

### `TEST`
- Accepted only in `TEST` mode.

#### LED test

```json
{"command":"TEST","type":"LED","action":"R"}
```

Valid `action` values: `"R"`, `"G"`, `"B"`.

#### SERVO test

```json
{"command":"TEST","type":"SERVO","action":"MAX"}
```

Valid `action` values: `"MAX"`, `"REST"`.

#### SIM test

```json
{"command":"TEST","type":"SIM","action":"RUN"}
```

Behavior:
- Runs static queue `{1,2,1,1}`.
- Each value is treated as hold seconds at servo `MAX`, then servo returns `REST`.

### `USAGE`
- Accepted only in `SIM` mode.
- `type` must be `"DURATION_S"`.
- `action` is required and must be numeric seconds (`> 0`).

```json
{"command":"USAGE","type":"DURATION_S","action":2}
```

```json
{"command":"USAGE","type":"DURATION_S","action":10}
```

### `ECHO`
- Accepted in any mode.
- `type` must be `"MESSAGE"`.
- Returns `{"command":"ECHO","type":"MESSAGE","action":"<same message>"}`.

```json
{"command":"ECHO","type":"MESSAGE","action":"input_field"}
```

---

## Serial parse logs

For every incoming message, the ESP32 prints:

```text
Incoming:
"<raw JSON payload>"
Parsed: <normalized parse summary>
```

Example:

```text
Incoming:
"{\"command\":\"TEST\",\"type\":\"SIM\",\"action\":\"RUN\"}"
Parsed: TEST SIM RUN
```
