# Changelog

All notable changes to the Magic Cane project will be documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.1.0] – 2024-12-01

### Added
- **Cane firmware** (ESP32-S3): deterministic hazard engine, haptic mapper,
  sensor manager, BLE belt link, advisory link, watchdog, graceful degradation.
- **Belt firmware** (ESP32-S3): 8-motor PWM driver, BLE peripheral,
  battery monitor, self-test, graceful ramp-down on cane disconnect.
- **Companion app** (Python): BLE cane client, RynnBrain advisory bridge,
  scene processor scaffold, auto-reconnect.
- **Web Bluetooth app**: smartphone pairing, live 360° hazard display,
  advisory hint sender, accessible high-contrast UI with Braille.
- **Native unit tests**: 67+ tests for hazard engine, haptic mapper,
  integration pipeline (host-compiled, no hardware required).
- **Belt protocol tests**: packet validation, parse, battery percentage.
- **Python protocol tests**: BeltPacket, AdvisoryHint, round-trip validation.
- **CI/CD**: GitHub Actions pipeline for C++ tests, Python tests, linting.
- **Documentation**: architecture spec, BLE protocol reference, setup guide,
  bill of materials, multilingual README (EN/ES/FR/ZH/AR/HI), Braille guide.
- **Professional repo infrastructure**: CONTRIBUTING, CODE_OF_CONDUCT,
  SECURITY policy, issue/PR templates, clang-format, .gitignore.
