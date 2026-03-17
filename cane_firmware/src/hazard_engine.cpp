/*
 * Magic Cane – deterministic hazard-detection engine (implementation)
 *
 * SAFETY-CRITICAL: This module must remain free of dynamic allocation,
 * floating-point in the hot path, and external dependencies beyond the
 * sensor snapshot struct.  All thresholds are compile-time constants
 * defined in config.h.
 *
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#include "hazard_engine.h"

/* ── Internal helpers ───────────────────────────────────────────────── */

static HazardLevel classify_distance(uint16_t distance_mm, bool valid)
{
    if (!valid || distance_mm == TOF_SENSOR_INVALID) {
        return HazardLevel::NONE;          /* sensor failed → do not assert hazard */
    }
    if (distance_mm < HAZARD_STOP_DISTANCE_MM) {
        return HazardLevel::STOP;
    }
    if (distance_mm < HAZARD_NEAR_DISTANCE_MM) {
        return HazardLevel::NEAR;
    }
    if (distance_mm < HAZARD_WARN_DISTANCE_MM) {
        return HazardLevel::WARN;
    }
    return HazardLevel::NONE;
}

/* ── Public API ─────────────────────────────────────────────────────── */

HazardResult hazard_evaluate(const SensorSnapshot &snap)
{
    HazardResult result{};
    result.emergency_stop = false;
    result.drop_detected  = false;

    /* ── Quadrant ToF sensors ───────────────────────────────────────── */
    for (uint8_t q = 0; q < QUAD_COUNT; ++q) {
        const auto &qr = snap.quadrants[q];
        result.quadrants[q].closest_mm = qr.min_distance_mm;
        result.quadrants[q].level      = classify_distance(qr.min_distance_mm, qr.valid);

        if (result.quadrants[q].level == HazardLevel::STOP) {
            result.emergency_stop = true;
        }
    }

    /* ── Forward LiDAR ──────────────────────────────────────────────── */
    result.forward_mm    = snap.forward_lidar_mm;
    result.forward_level = classify_distance(snap.forward_lidar_mm,
                                             snap.forward_lidar_valid);

    if (result.forward_level == HazardLevel::STOP) {
        result.emergency_stop = true;
    }

    /* ── Downward ToF – drop/stair detection ────────────────────────── */
    if (snap.downward_tof_valid &&
        snap.downward_tof_mm != TOF_SENSOR_INVALID &&
        snap.downward_tof_mm > DROP_OFF_THRESHOLD_MM) {
        result.drop_detected  = true;
        result.emergency_stop = true;
    }

    return result;
}
