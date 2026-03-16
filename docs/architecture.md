# Magic Cane System Architecture

## Overview

The Magic Cane is a blind-assist navigation system comprising three cooperating
devices. Safety-critical decisions are made entirely on the cane itself so
the system remains functional even when the phone/RynnBrain layer is
unavailable.

## Devices

| Device | Role | MCU |
|--------|------|-----|
| Smart Cane | Primary safety computer | ESP32-S3 |
| 360 Haptic Belt | Directional vibration output | ESP32 |
| Phone Companion | Advisory perception & route context | Android/iOS + Python bridge |

## Safety Hierarchy

```
┌──────────────────────────────────┐
│  RynnBrain / Phone Companion     │  ← Advisory only (non-safety)
│  Scene labeling, OCR, route ctx  │
└──────────┬───────────────────────┘
           │ BLE (advisory channel)
           ▼
┌──────────────────────────────────┐
│  Smart Cane  (ESP32-S3)          │  ← SAFETY AUTHORITY
│  Deterministic hazard engine     │
│  Local sensor fusion             │
│  Belt command generation         │
└──────────┬───────────────────────┘
           │ BLE (haptic channel)
           ▼
┌──────────────────────────────────┐
│  360 Haptic Belt  (ESP32)        │  ← Haptic output device
│  Motor driver array              │
└──────────────────────────────────┘
```

## Core Design Rules

1. **Cane firmware is the safety authority.** All stop/go and near-field
   hazard responses are computed locally on the cane MCU using deterministic
   algorithms—no ML models in the safety path.

2. **Belt is a directional haptic output device.** It receives motor-intensity
   commands from the cane over BLE and drives vibration motors accordingly.

3. **RynnBrain is advisory only.** It may supply semantic scene labels,
   route suggestions, OCR results, and higher-level navigation hints. These
   are displayed/announced but never override local hazard decisions.

4. **No cloud dependency in the safety path.** If the phone, network, or
   RynnBrain service is unavailable the cane and belt continue to operate
   with full hazard-detection capability.

## Cane Sensor Suite

| Sensor | Bus | Purpose |
|--------|-----|---------|
| 4× VL53L5CX ToF | I²C (multiplexed) | Near-field 8×8 zone ranging in 4 quadrants |
| 1× TFmini-S LiDAR | UART | Forward long-range distance |
| 1× VL53L1X downward ToF | I²C | Drop-off / stair detection |
| BNO086 IMU | I²C | Orientation, step detection, tilt |

## Communication Protocol

### Cane → Belt (BLE)

The cane sends a compact binary packet at 10 Hz:

| Byte | Field | Description |
|------|-------|-------------|
| 0 | Header | `0xCA` magic byte |
| 1 | Sequence | Rolling sequence number |
| 2–9 | Motors[0–7] | Intensity 0–255 for each of 8 belt motors |
| 10 | Flags | Bit 0: emergency-stop vibration pattern |
| 11 | Checksum | XOR of bytes 0–10 |

### Phone → Cane (BLE advisory)

JSON-encoded advisory messages (max 200 bytes):

```json
{
  "type": "nav_hint",
  "bearing": 45,
  "label": "crosswalk ahead",
  "confidence": 0.82
}
```

## Graceful Degradation

| Failure | Cane Behaviour | Belt Behaviour |
|---------|---------------|----------------|
| Phone disconnects | Advisory hints stop; hazard engine unaffected | Unchanged |
| RynnBrain timeout | Advisory channel marked stale; ignored | Unchanged |
| Belt disconnects | Cane switches to audio/vibration-on-handle alerts | N/A |
| Single ToF sensor fails | Quadrant marked degraded; remaining sensors active | Reduced haptic in that quadrant |
| IMU fails | Orientation assumed level; sensors still active | Unchanged |
