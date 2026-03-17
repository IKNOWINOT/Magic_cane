# Contributing to Magic Cane

Thank you for your interest in making navigation safer and more accessible
for blind and low-vision users! Contributions of all kinds are welcome.

## Getting Started

1. **Fork** the repository and create a feature branch from `main`.
2. **Clone** your fork locally.
3. Set up the development environment (see below).
4. Make changes, add tests, and ensure all tests pass.
5. Open a Pull Request against `main`.

## Development Environment

### Cane & Belt Firmware (C++ / ESP32-S3)

- **Build system:** [PlatformIO](https://platformio.org/)
- **Host tests:** `g++ -std=c++17` (no hardware required)

```bash
# Run cane firmware host tests
cd cane_firmware/test && make test

# Run belt firmware host tests
cd belt_firmware/test && make test
```

### Companion App (Python)

```bash
cd companion
pip install -r requirements.txt
python -m pytest tests/ -v
```

### Web Bluetooth App

Open `companion/web_app/index.html` in Chrome/Edge. No build step required.

## Coding Standards

### C++ (Firmware)
- C++17 standard
- 4-space indentation
- `snake_case` for functions and variables
- `PascalCase` for types and structs
- All safety-critical functions must be **pure** (no side-effects, no globals)
- No dynamic allocation in the safety path
- Guard hardware code with `#ifndef UNIT_TEST`

### Python (Companion)
- Python 3.9+
- Follow PEP 8
- Type hints encouraged
- Docstrings for public functions

## Safety Rules

**The following rules are non-negotiable:**

1. **No ML models in the safety path.** Hazard detection must remain
   deterministic with fixed thresholds.
2. **No cloud dependencies in the safety path.** The cane must function
   fully offline.
3. **No advisory data may override safety decisions.** RynnBrain hints
   are informational only.
4. **All safety-critical changes require unit tests.**
5. **Emergency stop must always work**, even with degraded sensors.

## Reporting Security Issues

See [SECURITY.md](SECURITY.md).

## Code of Conduct

See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

## License

By contributing, you agree that your contributions will be licensed under
the Apache License 2.0.
