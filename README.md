# poolantir-esp32-sensing-module

## Iteration 1:
This code is for the ESP32 sensing module. The sensing module is used for 3 purposes:
1. Determine and display when the user is within the "pissing range". This uses the ToF sensor and LED connected to GPIO 4. 
2. Display when WiFi is used (LED connected to GPIO 16) and when Bluetooth is used (LED Connected to GPIO 17)
3. Control the servo motor to simulate user "pissing". This should move the servo a certain number of degrees forward to its final position within the "pissing range". Then, pause momentarily within the "pissing range", then return to original position. 