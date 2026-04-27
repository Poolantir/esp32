# Poolantir Node Device
Platform IO (VSCODE extension) is used to program and flash the ESP32 nodes for the Poolantir Simulation. 

Poolantir Simulation code is within [src/main.cpp](/src/main.cpp)

### Flashing Nodes
To flash a Poolantir node, copy one of the following commands. 
This writes to the ESP32's persistent storage so that on power-up, the node's BLE name is unique.

Run these commands from the `pio` directory.

#### Choosing a firmware environment

- **`poolantir_simulation`** — production firmware with ToF sensor + BLE array protocol.
- **`poolantir_dummy_terminal`** — test firmware that accepts prefixed commands (`ECHO`, `SERVO`, `LED`, `SIM`) over BLE. Use this for interactive hardware testing and simulation queue testing via `scripts/ble_connect_mac.py`.

To flash the dummy terminal variant instead, pass `--dummy` to the flash script.

#### Production firmware (`poolantir_simulation`)

##### Node 1:
```bash
python3 scripts/flash_poolantir.py --id 1
```

##### Node 2:
```bash
python3 scripts/flash_poolantir.py --id 2
```

##### Node 3:
```bash
python3 scripts/flash_poolantir.py --id 3
```

##### Node 4:
```bash
python3 scripts/flash_poolantir.py --id 4
```

##### Node 5:
```bash
python3 scripts/flash_poolantir.py --id 5
```

##### Node 6:
```bash
python3 scripts/flash_poolantir.py --id 6
```

#### Dummy terminal firmware (`poolantir_dummy_terminal`)

These commands flash the dummy terminal firmware and open the serial monitor so you can observe timestamped simulation logs.

##### Node 1:
```bash
python3 scripts/flash_poolantir.py --id 1 --dummy --monitor
```

##### Node 2:
```bash
python3 scripts/flash_poolantir.py --id 2 --dummy --monitor
```

##### Node 3:
```bash
python3 scripts/flash_poolantir.py --id 3 --dummy --monitor
```

##### Node 4:
```bash
python3 scripts/flash_poolantir.py --id 4 --dummy --monitor
```

##### Node 5:
```bash
python3 scripts/flash_poolantir.py --id 5 --dummy --monitor
```

##### Node 6:
```bash
python3 scripts/flash_poolantir.py --id 6 --dummy --monitor
```
