# Setup Guide

## Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| [PlatformIO](https://platformio.org/) | ≥ 6.0 | Firmware build & upload |
| Python | ≥ 3.9 | Companion app |
| Chrome / Edge | Latest | Web Bluetooth app |
| USB-C cable | — | Flashing & charging |

## 1. Clone the Repository

```bash
git clone https://github.com/IKNOWINOT/Magic_cane.git
cd Magic_cane
```

## 2. Flash the Cane Firmware

```bash
cd cane_firmware
pio run -e esp32s3 -t upload
pio device monitor     # view serial output
```

### Pin Connections (Cane)

| Component | Interface | Pins |
|-----------|-----------|------|
| TCA9548A I²C Mux | I²C | SDA=default, SCL=default |
| VL53L5CX ×4 | Mux Ch 0–3 | via TCA9548A |
| VL53L1X (down) | Mux Ch 4 | via TCA9548A |
| BNO086 IMU | Mux Ch 5 | via TCA9548A |
| TFmini-S LiDAR | UART1 | RX=GPIO18, TX=GPIO17 |
| SIM7080G Cellular | UART2 | RX=GPIO44, TX=GPIO43, RST=GPIO38, PWR=GPIO39 |
| Nano-SIM Slot | — | On SIM7080G module |
| Status LED | GPIO | GPIO2 |

### SIM Card Setup

Insert a nano-SIM with an active data plan from **T-Mobile**, **AT&T**,
or **Verizon** into the SIM7080G module's SIM slot. The modem auto-detects
the carrier APN. On power-up the cane will:

1. Reset the modem via GPIO38
2. Run AT handshake and detect SIM
3. Register on the best available LTE Cat-M1 or NB-IoT network
4. Activate a data context for RynnBrain advisory queries

If no SIM is inserted or signal is unavailable, the cane operates
normally with local hazard detection only.

## 3. Flash the Belt Firmware

```bash
cd belt_firmware
pio run -e esp32s3 -t upload
pio device monitor
```

### Pin Connections (Belt)

| Component | Interface | Pins |
|-----------|-----------|------|
| Motor 0 (Front) | PWM | GPIO4 |
| Motor 1 (Front-R) | PWM | GPIO5 |
| Motor 2 (Right) | PWM | GPIO6 |
| Motor 3 (Rear-R) | PWM | GPIO7 |
| Motor 4 (Rear) | PWM | GPIO15 |
| Motor 5 (Rear-L) | PWM | GPIO16 |
| Motor 6 (Left) | PWM | GPIO17 |
| Motor 7 (Front-L) | PWM | GPIO18 |
| IMU (BNO086) | I²C | SDA=GPIO8, SCL=GPIO9 |
| Battery ADC | ADC | GPIO1 |
| Status LED | GPIO | GPIO2 |

## 4. Run the Python Companion

```bash
cd companion
pip install -r requirements.txt
python main.py --scan     # auto-discover cane
```

## 5. Open the Web Bluetooth App

1. Open `companion/web_app/index.html` in Chrome or Edge.
2. Click **Connect to Cane**.
3. Select "MagicCane" from the Bluetooth pairing dialog.
4. The live 360° hazard map will display motor intensities in real time.

## 6. Run Tests (No Hardware Required)

```bash
# C++ host tests (cane)
cd cane_firmware/test && make test

# C++ host tests (belt)
cd belt_firmware/test && make test

# Python tests
cd companion && python -m pytest tests/ -v
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Cane not advertising | Check serial output; reset ESP32-S3 |
| Belt not connecting | Ensure cane is powered on and advertising |
| Web BLE "not supported" | Use Chrome/Edge; enable `chrome://flags/#enable-web-bluetooth` |
| Python BLE fails | Install `bleak`: `pip install bleak` |
| Tests fail to compile | Ensure `g++ -std=c++17` is available |
