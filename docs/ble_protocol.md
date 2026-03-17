# BLE Protocol Reference

## Overview

The Magic Cane system uses Bluetooth Low Energy (BLE) for all device
communication. There are two independent BLE channels:

1. **Haptic Channel** — Cane → Belt (safety-critical, binary)
2. **Advisory Channel** — Phone → Cane (non-safety, JSON)

## Service & Characteristic UUIDs

| Service | UUID | Direction |
|---------|------|-----------|
| Cane Haptic Service | `a1b2c3d4-0001-1000-8000-00805f9b34fb` | Cane → Belt |
| Haptic Characteristic | `a1b2c3d4-0002-1000-8000-00805f9b34fb` | Notify + Write |
| Advisory Service | `a1b2c3d4-0003-1000-8000-00805f9b34fb` | Phone → Cane |
| Advisory Characteristic | `a1b2c3d4-0004-1000-8000-00805f9b34fb` | Write |

## Haptic Packet Format (Cane → Belt)

Sent at 10 Hz via BLE Notify. Total: **12 bytes**, fixed length.

```
Byte  Field       Description
──────────────────────────────────────────
 0    Header      0xCA (magic byte)
 1    Sequence    Rolling counter 0–255
 2    Motor[0]    Front intensity 0–255
 3    Motor[1]    Front-Right intensity
 4    Motor[2]    Right intensity
 5    Motor[3]    Rear-Right intensity
 6    Motor[4]    Rear intensity
 7    Motor[5]    Rear-Left intensity
 8    Motor[6]    Left intensity
 9    Motor[7]    Front-Left intensity
10    Flags       Bit 0: emergency stop
11    Checksum    XOR of bytes 0–10
```

### Motor Layout (overhead view)

```
        7   0   1
      6   USER   2
        5   4   3
```

### Intensity Mapping

| Hazard Level | Intensity | Distance (mm) |
|-------------|-----------|---------------|
| NONE | 0 | ≥ 2500 |
| WARN | 80 | 1200–2499 |
| NEAR | 180 | 400–1199 |
| STOP | 255 | < 400 |

### Checksum Calculation

```
checksum = 0
for i in range(11):
    checksum ^= packet[i]
```

### Belt ACK

Belt writes a single byte `0x01` to the Haptic Characteristic to
acknowledge receipt.

## Advisory Protocol (Phone → Cane)

JSON-encoded UTF-8 string written to the Advisory Characteristic.
Maximum 200 bytes. **Non-safety — never overrides hazard decisions.**

### Message Format

```json
{
    "type": "nav_hint",
    "bearing": 45,
    "label": "crosswalk ahead",
    "confidence": 0.82
}
```

### Message Types

| Type | Description |
|------|-------------|
| `nav_hint` | Turn-by-turn navigation instruction |
| `scene_label` | Object/scene identification |
| `ocr_result` | Text detected by camera OCR |
| `route_update` | Route status change |

### Fields

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `type` | string | see above | Message category |
| `bearing` | int | -180 to +180 | Degrees from forward, 0 = ahead |
| `label` | string | max 63 chars | Human-readable description |
| `confidence` | float | 0.0 – 1.0 | Model confidence score |

## Connection Lifecycle

```
1. Cane powers on → starts BLE advertising as "MagicCane"
2. Belt scans → finds CANE_SERVICE_UUID → connects → subscribes to Notify
3. Phone scans → finds CANE_SERVICE_UUID → connects → writes to Advisory
4. Cane sends haptic packets at 10 Hz
5. Belt ACKs each packet
6. If belt disconnects → cane falls back to on-handle alerts
7. If phone disconnects → advisory hints stop; safety unaffected
```
