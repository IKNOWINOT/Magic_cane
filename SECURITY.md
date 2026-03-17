# Security Policy

## Reporting a Vulnerability

If you discover a security vulnerability in the Magic Cane system —
especially one that could affect the safety-critical path — please
report it responsibly.

**Email:** Open a private security advisory on GitHub via the
repository's **Security** tab.

**Do NOT** open a public issue for security vulnerabilities.

## Scope

### Safety-Critical (High Priority)
- Hazard detection engine bypass or false negatives
- Emergency stop failure
- Belt motor control malfunction
- BLE packet injection that could override safety decisions

### Advisory (Lower Priority)
- RynnBrain bridge vulnerabilities
- Companion app authentication
- Web Bluetooth app XSS or data leaks

## Supported Versions

| Version | Supported |
|---------|-----------|
| main    | ✅        |
| < v1.0  | Development – best effort |

## Response Timeline

- **Acknowledgement:** within 48 hours
- **Initial assessment:** within 1 week
- **Fix for safety-critical issues:** within 2 weeks
