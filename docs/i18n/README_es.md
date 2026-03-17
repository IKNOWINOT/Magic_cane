# ⠍⠁⠛⠊⠉ ⠉⠁⠝⠑ Magic Cane — Bastón Inteligente

[English](../../README.md) | **Español** | [Français](README_fr.md) | [中文](README_zh.md) | [العربية](README_ar.md) | [हिन्दी](README_hi.md) | [⠃⠗⠇ Braille](README_braille.md)

## Descripción General

Magic Cane es un sistema de navegación asistida para personas ciegas y con
baja visión. Está compuesto por tres dispositivos que trabajan juntos:

1. **Bastón Inteligente** — Computadora de seguridad principal (ESP32-S3)
2. **Cinturón Háptico 360°** — Vibración direccional para navegación
3. **Teléfono Acompañante** — Percepción asistida por RynnBrain (opcional)

## Principios de Diseño

- 🛡️ **El bastón es la autoridad de seguridad.** Todas las decisiones de
  parar/avanzar se toman localmente con algoritmos determinísticos.
- 🔔 **El cinturón es el dispositivo de salida háptica.** Recibe comandos
  del bastón y vibra en la dirección del peligro.
- 🧠 **RynnBrain es solo consultivo.** Proporciona etiquetas de escena,
  OCR, y sugerencias de ruta, pero NUNCA anula las decisiones de seguridad.
- 🌐 **Sin dependencia de la nube en la ruta de seguridad.** Si el teléfono
  se desconecta, el bastón y cinturón siguen funcionando.

## Sensores del Bastón

| Sensor | Función |
|--------|---------|
| 4× VL53L5CX ToF | Detección de obstáculos en 4 cuadrantes (8×8 zonas) |
| 1× TFmini-S LiDAR | Distancia frontal de largo alcance |
| 1× VL53L1X ToF (abajo) | Detección de escalones y desniveles |
| BNO086 IMU | Orientación, detección de pasos, inclinación |

## Niveles de Peligro

| Nivel | Distancia | Respuesta |
|-------|-----------|-----------|
| NINGUNO | ≥ 2.5 m | Sin vibración |
| ADVERTENCIA | 1.2 – 2.5 m | Vibración suave |
| CERCA | 0.4 – 1.2 m | Vibración fuerte |
| PARADA | < 0.4 m | Parada de emergencia |
| DESNIVEL | Caída detectada | Todas las vibraciones al máximo |

## Comenzar

Consulte la [Guía de Configuración](../setup_guide.md) y la
[Lista de Materiales](../bom.md).

## Licencia

Apache License 2.0 — ver [LICENSE](../../LICENSE).
