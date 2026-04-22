## Connecting from a Mac (terminal)

macOS does not ship a generic “GATT shell” like Linux’s `gatttool`. For a **terminal-based** workflow, use **Python** and **[Bleak](https://github.com/hbldh/bleak)** (cross-platform BLE; works on Apple Silicon and Intel Macs).

Firmware details (for reference):

- **Advertised name:** `poolantir-node-<id>`
- **Service UUID:** `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- **Data characteristic UUID:** `beb5483e-36e1-4688-b7f5-e073f246f7d4` (read / write / notify)

### 1. Prerequisites

- **Bluetooth on** (menu bar or **System Settings → Bluetooth**).
- **Python 3** (`python3 --version`).
- **Bluetooth permission for your terminal:** **System Settings → Privacy & Security → Bluetooth** — enable **Terminal** (or **iTerm**, **VS Code**, etc.), whichever app runs the script.

### 2. Install Bleak

From any directory (user install is fine):

```bash
python3 -m pip install --user bleak
```

Optional: use a venv inside this repo:

```bash
cd /path/to/esp32/pio
python3 -m venv .venv
source .venv/bin/activate
pip install bleak
```

### 3. Run the interactive BLE terminal

From the **`pio`** project directory:

```bash
cd /path/to/esp32/pio
python3 scripts/ble_connect_mac.py --id 3
```

1. The script prints **metadata** (target name/address mode, service and characteristic UUIDs).
2. It **retries until the BLE link is up** — there is **no overall timeout**. While waiting, it prints **`device not found...`** every **2 seconds**. **Ctrl+C** aborts.
3. After connect, **transmission mode**:

- Type a line and press **Enter** to send that UTF-8 text to the ESP32 (`[sent] …` confirms).
- The firmware’s notification ack appears as **`[notify] …`** (e.g. `node 3 received`).
- End the session: **Ctrl+D** or **`:quit`** (disconnects and exits).

Options:

- **`--id 3`** — connect to **`poolantir-node-3`**. Without **`--id`**, the scan must see **exactly one** `poolantir-node-*`; if several are in range, use **`--id`** or **`--address`** (the script keeps retrying and printing **`device not found...`** until resolved).
- **`--address AA:BB:CC:DD:EE:FF`** — skip scanning (address from LightBlue or a previous run).

Example:

```bash
python3 scripts/ble_connect_mac.py --id 1
```

After you see **Transmission mode**, type messages interactively; the ESP32 serial log should show **`[BLE RX]`** / **`[BLE TX]`** for each line.

### 4. Troubleshooting

- **`device not found...` every 2s** — peripheral off, out of range, wrong **`--id`**, or ESP32 still booting (e.g. sensor init). Fix hardware/args; the script will connect when the device is available.
- **Permission errors / no devices** — grant **Bluetooth** to the app running Python (Terminal vs IDE integrated terminal).
- **Multiple nodes in range** — use **`--id`** or **`--address`** so the target is unambiguous.

### 5. Dummy terminal mode (test commands)

If the ESP32 is flashed with the **`poolantir_dummy_terminal`** environment (see `FLASHING_NODES.md`), the BLE terminal supports four test commands:

```
Actions for node N:
  1) Echo          — send a message, ESP32 echoes it back
  2) Move servo    — REST / MAX / specific degree (0-180)
  3) Flash LED     — flash R, G, or B
  4) Simulate user — enqueue pee(1) / poo(2) sequence on ESP32
```

Example payloads sent over BLE:

| Action         | Payload example                |
|----------------|-------------------------------|
| Echo           | `ECHO hello world`            |
| Servo to 90°   | `SERVO 90`                    |
| Servo to rest   | `SERVO REST`                  |
| Flash red LED  | `LED R`                       |
| Simulate users | `SIM 3 {1,2,1}`              |

For **Simulate user**, the ESP32 consumes each element sequentially:
- `1` (pee): servo at max for 2 s, then return to rest, then 3 s pause.
- `2` (poo): servo at max for 4 s, then return to rest, then 3 s pause.

The ESP32 streams timestamped status messages back as BLE notifications, visible in the terminal.

### 6. GUI alternative

If you prefer a graphical client, use **LightBlue** from the Mac App Store: scan → connect to **`poolantir-node-<id>`** → open the service above → enable notifications on the characteristic → write a value.
