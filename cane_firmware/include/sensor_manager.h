/*
 * Magic Cane – sensor manager
 * Abstracts all cane sensors behind a unified polling interface.
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#pragma once

#include "config.h"
#include <stdint.h>

/* ── Per-quadrant VL53L5CX summary ──────────────────────────────────── */
struct QuadrantReading {
    uint16_t min_distance_mm;   /* closest object in 8×8 zone grid  */
    uint16_t avg_distance_mm;   /* average distance across all zones */
    bool     valid;             /* false if sensor comms failed      */
};

/* ── Aggregated sensor snapshot taken once per poll cycle ────────────── */
struct SensorSnapshot {
    QuadrantReading quadrants[QUAD_COUNT];
    uint16_t        forward_lidar_mm;   /* TFmini-S reading            */
    bool            forward_lidar_valid;
    uint16_t        downward_tof_mm;    /* downward ToF for drops      */
    bool            downward_tof_valid;
    float           pitch_deg;          /* IMU pitch in degrees        */
    float           roll_deg;           /* IMU roll  in degrees        */
    float           yaw_deg;            /* IMU yaw   in degrees        */
    bool            imu_valid;
    uint32_t        timestamp_ms;
};

#ifndef UNIT_TEST
/* Hardware sensor initialisation & polling (ESP32 only) */
void sensors_init();
void sensors_poll(SensorSnapshot &snap);
#endif
