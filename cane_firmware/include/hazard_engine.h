/*
 * Magic Cane – deterministic hazard-detection engine
 * This is the SAFETY-CRITICAL module. It uses only local sensor data and
 * fixed thresholds – no ML, no cloud, no advisory input.
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#pragma once

#include "config.h"
#include "sensor_manager.h"
#include <stdint.h>

/* ── Per-quadrant hazard result ─────────────────────────────────────── */
struct QuadrantHazard {
    HazardLevel level;
    uint16_t    closest_mm;
};

/* ── Full hazard assessment produced each cycle ─────────────────────── */
struct HazardResult {
    QuadrantHazard quadrants[QUAD_COUNT];
    HazardLevel    forward_level;
    uint16_t       forward_mm;
    bool           drop_detected;
    bool           emergency_stop;   /* true if ANY zone is STOP or drop */
};

/*
 * Evaluate the sensor snapshot and produce a deterministic hazard result.
 * This function is pure: no side-effects, no global state, no heap
 * allocation – making it straightforward to unit-test on the host.
 */
HazardResult hazard_evaluate(const SensorSnapshot &snap);
