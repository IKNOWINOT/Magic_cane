Build production-style firmware and edge-control software for a blind-assist “computer cane” system with a paired 360 haptic belt, using RynnBrain only as a non-safety advisory perception/planning layer. Safety-critical stop/go and near-field hazard responses must remain deterministic and local even if RynnBrain, phone connectivity, or the camera stack fails.

System architecture:
The system has three devices:
1. Smart Cane (primary safety computer)
2. 360 Haptic Belt (directional vibration peripheral)
3. Optional phone/head camera companion running RynnBrain-assisted perception and route/context features

Core design rule:
- Cane firmware is the safety authority.
- Belt is the directional haptic output device.
- RynnBrain is advisory only for semantic scene understanding, route context, object labeling, OCR, and higher-level navigation suggestions.
- No cloud dependency is allowed in the safety path.
- If RynnBrain or phone disconnects, cane and belt must still function safely.

Target hardware:
Cane:
- ESP32-S3 main MCU
- BNO086 or equivalent IMU
- 4x VL53L5CX ToF sensors in 4 quadrants around upper shaft
- 1x forward TFmini-S or equivalent eye-safe LiDAR
- 1x downward ToF sensor near tip
