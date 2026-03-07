# Running tests and main app

Run all commands from the `pio/` directory (where `platformio.ini` lives).

**If `pio` is not found** in your terminal:

- **VS Code:** Use the PlatformIO sidebar (ant icon) and use **Build**, **Upload**, **Monitor** there. Or open the **PlatformIO CLI** terminal from the status bar so `pio` is on PATH.
- **Install CLI:** `pip install platformio` then use `pio` or `platformio`.

## Main application

Build, upload, and monitor the full Poolantir app (ToF, LEDs, servo):

```bash
pio run -e esp32doit-devkit-v1
pio run -e esp32doit-devkit-v1 -t upload
pio device monitor -e esp32doit-devkit-v1
```

## Tests (each builds from `test/`)

### Bluetooth test (`test/bluetooth_test.cpp`)

```bash
pio run -e bluetooth_test
pio run -e bluetooth_test -t upload
pio device monitor -e bluetooth_test
```

### WiFi test (`test/wifi_test.cpp`)

```bash
pio run -e wifi_test
pio run -e wifi_test -t upload
pio device monitor -e wifi_test
```

### Servo test (`test/servo_test.cpp`)

```bash
pio run -e servo_test
pio run -e servo_test -t upload
pio device monitor -e servo_test
```

### ToF test (`test/tof_test.cpp`)

```bash
pio run -e tof_test
pio run -e tof_test -t upload
pio device monitor -e tof_test
```
