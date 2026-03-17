# ⠍⠁⠛⠊⠉ ⠉⠁⠝⠑ Magic Cane — Canne Intelligente

[English](../../README.md) | [Español](README_es.md) | **Français** | [中文](README_zh.md) | [العربية](README_ar.md) | [हिन्दी](README_hi.md) | [⠃⠗⠇ Braille](README_braille.md)

## Vue d'Ensemble

Magic Cane est un système de navigation assistée pour les personnes aveugles
et malvoyantes. Il comprend trois dispositifs :

1. **Canne Intelligente** — Ordinateur de sécurité principal (ESP32-S3)
2. **Ceinture Haptique 360°** — Vibrations directionnelles pour la navigation
3. **Téléphone Compagnon** — Perception assistée par RynnBrain (optionnel)

## Principes de Conception

- 🛡️ **La canne est l'autorité de sécurité.** Toutes les décisions
  d'arrêt/avance sont prises localement avec des algorithmes déterministes.
- 🔔 **La ceinture est le dispositif de sortie haptique.** Elle reçoit
  des commandes de la canne et vibre dans la direction du danger.
- 🧠 **RynnBrain est uniquement consultatif.** Il fournit des étiquettes
  de scène, de l'OCR et des suggestions de route, mais ne remplace JAMAIS
  les décisions de sécurité.
- 🌐 **Aucune dépendance cloud dans le chemin de sécurité.** Si le
  téléphone se déconnecte, la canne et la ceinture continuent de fonctionner.

## Capteurs de la Canne

| Capteur | Fonction |
|---------|----------|
| 4× VL53L5CX ToF | Détection d'obstacles dans 4 quadrants (8×8 zones) |
| 1× TFmini-S LiDAR | Distance frontale longue portée |
| 1× VL53L1X ToF (bas) | Détection de marches et de dénivellations |
| BNO086 IMU | Orientation, détection de pas, inclinaison |

## Niveaux de Danger

| Niveau | Distance | Réponse |
|--------|----------|---------|
| AUCUN | ≥ 2,5 m | Pas de vibration |
| ATTENTION | 1,2 – 2,5 m | Vibration légère |
| PROCHE | 0,4 – 1,2 m | Vibration forte |
| ARRÊT | < 0,4 m | Arrêt d'urgence |
| CHUTE | Dénivellation détectée | Toutes les vibrations au maximum |

## Démarrage

Consultez le [Guide d'Installation](../setup_guide.md) et la
[Liste des Matériaux](../bom.md).

## Licence

Apache License 2.0 — voir [LICENSE](../../LICENSE).
