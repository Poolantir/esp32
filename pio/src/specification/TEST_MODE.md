# Test Mode
Upon connecting the ESP32 to power, it should begin in the TEST mode

Additionally, the internal ESP32 mode will be changed when the following JSON message is sent via the central server.
```
# JSOn
{"command":"MODE","type":"SET","action":"TEST"}
```

### Starting Configuration:
- ToF: OFF - do not read any values from the ToF Sensor
- Servo: REST - return the servo to the resting position
- LED: BLUE - Blue indicates testing mode

### Operation
The ESP32 will be able to operate upon the commands listed within [COMMANDS.md](COMMANDS.md)

During the TESTING mode, the ToF and LED WILL be coupled:
1. ToF shows when an object is in range of the ToF sensor
2. Whenever an object is within the range of the ToF, the LED will turn RED - this holds for ANY operation performed during testing mode

### Flow:
0. LED starts BLUE, Servo in rest, ToF attached
1. When ToF in range, turn LED RED
2. Await future commands from central server to test and configure values.