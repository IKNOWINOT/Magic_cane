# ⠍⠁⠛⠊⠉ ⠉⠁⠝⠑ Magic Cane

[![CI](https://github.com/IKNOWINOT/Magic_cane/actions/workflows/ci.yml/badge.svg)](https://github.com/IKNOWINOT/Magic_cane/actions)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
[![Lang](https://img.shields.io/badge/docs-EN%20|%20ES%20|%20FR%20|%20ZH%20|%20AR%20|%20HI%20|%20⠃⠗⠇-green)](docs/i18n/INDEX.md)

**Production-grade firmware and edge-control software for a blind-assist
"computer cane" system with a paired 360° haptic belt.**

Safety-critical stop/go and near-field hazard responses are **deterministic
and local** — they work even if RynnBrain, the phone, cellular connectivity,
or the camera stack fails.

---

## System Architecture

| Device | Role | MCU |
|--------|------|-----|
| 🦯 **Smart Cane** | Primary safety computer | ESP32-S3 |
| 🔔 **360° Haptic Belt** | Directional vibration output (8 motors) | ESP32-S3 |
| 📱 **Phone Companion** | Advisory perception & route context (optional) | Android/iOS |

### Connectivity

| Channel | Type | Purpose |
|---------|------|---------|
| Cane ↔ Belt | BLE | Haptic commands at 10 Hz (safety) |
| Phone → Cane | BLE | Advisory hints (non-safety) |
| Cane → Cloud | **LTE Cat-M1** (SIM7080G) | RynnBrain advisory uplink (non-safety) |

**Supported carriers:** T-Mobile · AT&T · Verizon (multi-carrier nano-SIM)

### Core Design Rules

1. 🛡️ **Cane is the safety authority.** All hazard decisions are local and deterministic — no ML in the safety path.
2. 🔔 **Belt is the haptic output device.** Vibrates directionally based on hazard proximity.
3. 🧠 **RynnBrain is advisory only.** Scene labels, OCR, and route hints never override safety.
4. 🌐 **No cloud dependency in the safety path.** Phone, cellular, or RynnBrain can fail — cane + belt still work.

## Hardware

### Smart Cane Sensors

| Component | Interface | Purpose |
|-----------|-----------|---------|
| 4× VL53L5CX ToF | I²C (mux) | 8×8 zone ranging in 4 quadrants |
| TFmini-S LiDAR | UART1 | Forward long-range (12 m) |
| VL53L1X ToF | I²C (mux) | Downward drop-off detection |
| BNO086 IMU | I²C (mux) | Orientation & step detection |
| SIM7080G Modem | UART2 | LTE Cat-M1/NB-IoT cellular (advisory) |

### 360° Haptic Belt

| Component | Interface | Purpose |
|-----------|-----------|---------|
| 8× ERM Motors | PWM (20 kHz) | 360° directional vibration |
| BNO086 IMU | I²C | Belt orientation |
| LiPo Battery | ADC | Battery monitoring |

📋 Full **[Bill of Materials](docs/bom.md)** (~$265 prototype)

## Quick Start

```bash
# Clone
git clone https://github.com/IKNOWINOT/Magic_cane.git && cd Magic_cane

# Flash cane firmware (PlatformIO)
cd cane_firmware && pio run -e esp32s3 -t upload

# Flash belt firmware
cd ../belt_firmware && pio run -e esp32s3 -t upload

# Run companion (Python)
cd ../companion && pip install -r requirements.txt && python main.py --scan

# Open Web Bluetooth app
open companion/web_app/index.html    # Chrome/Edge
```

## Run Tests (No Hardware Required)

```bash
cd cane_firmware/test && make test     # 84 C++ tests
cd belt_firmware/test && make test     # 19 C++ tests
cd companion && python -m pytest tests/ -v  # 22 Python tests
```

**Total: 125 tests** covering hazard engine, haptic mapper, cellular AT
parsing, belt protocol, battery monitor, and cross-protocol integration.

## Documentation

| Document | Description |
|----------|-------------|
| [Architecture](docs/architecture.md) | System design, safety hierarchy, protocols |
| [BLE Protocol](docs/ble_protocol.md) | Packet formats, UUIDs, connection lifecycle |
| [Setup Guide](docs/setup_guide.md) | Hardware wiring, flashing, SIM card setup |
| [Bill of Materials](docs/bom.md) | Components, quantities, estimated costs |
| [Multilingual Docs](docs/i18n/INDEX.md) | EN · ES · FR · ZH · AR · HI · ⠃⠗⠇ Braille |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Safety-critical changes require
unit tests and review.

## License

[Apache License 2.0](LICENSE)
