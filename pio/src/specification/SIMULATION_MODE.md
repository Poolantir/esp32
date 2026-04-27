# SIMULATION Mode
The ESP32 will move into simulation mode when following json command is sent from the central server:

```
# JSON
{"command":"MODE","type":"SET","action":"SIM"}
```

### Starting Configuration:
- ToF: OFF - Stop Reading the ToF
- Servo: REST - return the servo to the resting position
- LED: GREEN - Green indicates that the urinal is open

### Operation
The ESP32 will be able to operate upon the commands listed within [COMMANDS.md](COMMANDS.md)

During the SIMULATION mode, the ToF and LED will be decoupled:
1. ToF does not serve any purpose during the simulation
2. The LED is changed at the completion of the animation of the servo:
    - when a new user is added, after the servo moves from REST to MAX, the LED should turn RED
    - when the duration of the user is completed, and the servo moves from MAX to REST, the LED should turn GREEN.
3. Only once the starting configuration is correctly set, does the ESP32 send the COMPLETE message


### Flow
0. LED starts Green, Servo in rest, ToF unattached
1. esp32 receives new user with duration
2. esp32 moves servo into MAX position (animation applied) -> Turn LED RED
3. esp32 holds the servo in MAX position for the duration specified within the new user message
4. (duration expires)
5. esp32 moves the servo to REST position (immediately) -> Turn LED Green
7. esp32 sends COMPLETE message to central server, attaching the id of the user worked on.
8. esp32 resets all configurations

1-8 run continously while in SIMULATION mode