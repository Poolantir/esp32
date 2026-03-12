# Poolantir ESP32 Bathroom Sensing Module

## Iteration 1:
This code is for the ESP32 sensing module. The sensing module is used for 3 purposes:
1. Determine and display when the user is within the "pissing range". This uses the ToF sensor and LED connected to GPIO 4. 
2. Display when WiFi is used (LED connected to GPIO 16) and when Bluetooth is used (LED Connected to GPIO 17)
3. Control the servo motor to simulate user "pissing". This should move the servo a certain number of degrees forward to its final position within the "pissing range". Then, pause momentarily within the "pissing range", then return to original position.

---

## PlatformIO (run from `pio/`)

### Main application
Build, upload, and monitor the full Poolantir app (ToF, LEDs, servo):

```bash
cd pio
pio run -e esp32doit-devkit-v1
pio run -e esp32doit-devkit-v1 -t upload
pio device monitor -e esp32doit-devkit-v1
```

### Tests (each builds from `pio/test/`)

**Bluetooth test** (`test/bluetooth_test.cpp`):
```bash
cd pio
pio run -e bluetooth_test
pio run -e bluetooth_test -t upload
pio device monitor -e bluetooth_test
```

**WiFi test** (`test/wifi_test.cpp`):
```bash
cd pio
pio run -e wifi_test
pio run -e wifi_test -t upload
pio device monitor -e wifi_test
```

**Servo test** (`test/servo_test.cpp`):
```bash
cd pio
pio run -e servo_test
pio run -e servo_test -t upload
pio device monitor -e servo_test
```

**ToF test** (`test/tof_test.cpp`):
```bash
cd pio
pio run -e tof_test
pio run -e tof_test -t upload
pio device monitor -e tof_test
``` 
