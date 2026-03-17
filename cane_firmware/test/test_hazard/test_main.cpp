/*
 * Magic Cane – Host-side unit tests for hazard_engine and haptic_mapper.
 *
 * Compile & run:
 *   g++ -std=c++17 -DUNIT_TEST -I cane_firmware/include \
 *       -o test_runner \
 *       cane_firmware/test/test_hazard/test_main.cpp \
 *       cane_firmware/src/hazard_engine.cpp \
 *       cane_firmware/src/haptic_mapper.cpp \
 *   && ./test_runner
 *
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#include "hazard_engine.h"
#include "haptic_mapper.h"
#include "cell_modem.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

/* ── Minimal test framework ─────────────────────────────────────────── */

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::printf("  FAIL  %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return false;                                                   \
        }                                                                   \
    } while (0)

#define TEST_ASSERT_EQ(a, b)                                               \
    do {                                                                   \
        auto _a = (a); auto _b = (b);                                      \
        if (_a != _b) {                                                    \
            std::printf("  FAIL  %s:%d: %s == %s  (%d != %d)\n",          \
                        __FILE__, __LINE__, #a, #b,                        \
                        static_cast<int>(_a), static_cast<int>(_b));       \
            return false;                                                   \
        }                                                                   \
    } while (0)

#define RUN_TEST(fn)                                                       \
    do {                                                                   \
        ++g_tests_run;                                                     \
        if (fn()) {                                                        \
            ++g_tests_passed;                                              \
            std::printf("  PASS  %s\n", #fn);                             \
        } else {                                                           \
            ++g_tests_failed;                                              \
        }                                                                   \
    } while (0)

/* ── Helpers ────────────────────────────────────────────────────────── */

static SensorSnapshot make_clear_snapshot()
{
    SensorSnapshot snap{};
    for (uint8_t q = 0; q < QUAD_COUNT; ++q) {
        snap.quadrants[q].min_distance_mm = LIDAR_MAX_RANGE_MM;
        snap.quadrants[q].avg_distance_mm = LIDAR_MAX_RANGE_MM;
        snap.quadrants[q].valid           = true;
    }
    snap.forward_lidar_mm    = LIDAR_MAX_RANGE_MM;
    snap.forward_lidar_valid = true;
    snap.downward_tof_mm     = 50;   /* ground close → no drop */
    snap.downward_tof_valid  = true;
    snap.pitch_deg           = 0.0f;
    snap.roll_deg            = 0.0f;
    snap.yaw_deg             = 0.0f;
    snap.imu_valid           = true;
    snap.timestamp_ms        = 1000;
    return snap;
}

/* ===================================================================
 * SECTION 1: Hazard Engine Tests
 * =================================================================== */

/* ── 1.1  Distance classification ──────────────────────────────────── */

static bool test_hazard_clear_scene()
{
    auto snap   = make_clear_snapshot();
    auto result = hazard_evaluate(snap);

    for (uint8_t q = 0; q < QUAD_COUNT; ++q) {
        TEST_ASSERT_EQ(result.quadrants[q].level, HazardLevel::NONE);
    }
    TEST_ASSERT_EQ(result.forward_level, HazardLevel::NONE);
    TEST_ASSERT(!result.drop_detected);
    TEST_ASSERT(!result.emergency_stop);
    return true;
}

static bool test_hazard_warn_level()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_FRONT].min_distance_mm = 2000;  /* < 2500 → WARN */
    auto result = hazard_evaluate(snap);

    TEST_ASSERT_EQ(result.quadrants[QUAD_FRONT].level, HazardLevel::WARN);
    TEST_ASSERT(!result.emergency_stop);
    return true;
}

static bool test_hazard_near_level()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_RIGHT].min_distance_mm = 800;   /* < 1200 → NEAR */
    auto result = hazard_evaluate(snap);

    TEST_ASSERT_EQ(result.quadrants[QUAD_RIGHT].level, HazardLevel::NEAR);
    TEST_ASSERT(!result.emergency_stop);
    return true;
}

static bool test_hazard_stop_level()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_LEFT].min_distance_mm = 200;    /* < 400 → STOP */
    auto result = hazard_evaluate(snap);

    TEST_ASSERT_EQ(result.quadrants[QUAD_LEFT].level, HazardLevel::STOP);
    TEST_ASSERT(result.emergency_stop);
    return true;
}

/* ── 1.2  Boundary conditions (exactly at threshold) ───────────────── */

static bool test_hazard_boundary_stop_at_400()
{
    /* distance == 400  →  NOT STOP (thresholds use strict <) */
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_FRONT].min_distance_mm = HAZARD_STOP_DISTANCE_MM;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT_EQ(result.quadrants[QUAD_FRONT].level, HazardLevel::NEAR);
    TEST_ASSERT(!result.emergency_stop);
    return true;
}

static bool test_hazard_boundary_stop_at_399()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_FRONT].min_distance_mm = HAZARD_STOP_DISTANCE_MM - 1;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT_EQ(result.quadrants[QUAD_FRONT].level, HazardLevel::STOP);
    TEST_ASSERT(result.emergency_stop);
    return true;
}

static bool test_hazard_boundary_near_at_1200()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_RIGHT].min_distance_mm = HAZARD_NEAR_DISTANCE_MM;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT_EQ(result.quadrants[QUAD_RIGHT].level, HazardLevel::WARN);
    return true;
}

static bool test_hazard_boundary_near_at_1199()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_RIGHT].min_distance_mm = HAZARD_NEAR_DISTANCE_MM - 1;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT_EQ(result.quadrants[QUAD_RIGHT].level, HazardLevel::NEAR);
    return true;
}

static bool test_hazard_boundary_warn_at_2500()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_REAR].min_distance_mm = HAZARD_WARN_DISTANCE_MM;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT_EQ(result.quadrants[QUAD_REAR].level, HazardLevel::NONE);
    return true;
}

static bool test_hazard_boundary_warn_at_2499()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_REAR].min_distance_mm = HAZARD_WARN_DISTANCE_MM - 1;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT_EQ(result.quadrants[QUAD_REAR].level, HazardLevel::WARN);
    return true;
}

static bool test_hazard_zero_distance()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_FRONT].min_distance_mm = 0;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT_EQ(result.quadrants[QUAD_FRONT].level, HazardLevel::STOP);
    TEST_ASSERT(result.emergency_stop);
    return true;
}

/* ── 1.3  Emergency stop from each quadrant ────────────────────────── */

static bool test_hazard_estop_front_quadrant()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_FRONT].min_distance_mm = 100;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT(result.emergency_stop);
    TEST_ASSERT_EQ(result.quadrants[QUAD_FRONT].level, HazardLevel::STOP);
    return true;
}

static bool test_hazard_estop_right_quadrant()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_RIGHT].min_distance_mm = 100;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT(result.emergency_stop);
    return true;
}

static bool test_hazard_estop_rear_quadrant()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_REAR].min_distance_mm = 100;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT(result.emergency_stop);
    return true;
}

static bool test_hazard_estop_left_quadrant()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_LEFT].min_distance_mm = 100;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT(result.emergency_stop);
    return true;
}

/* ── 1.4  Forward LiDAR ───────────────────────────────────────────── */

static bool test_hazard_forward_lidar_stop()
{
    auto snap = make_clear_snapshot();
    snap.forward_lidar_mm = 200;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT_EQ(result.forward_level, HazardLevel::STOP);
    TEST_ASSERT(result.emergency_stop);
    return true;
}

static bool test_hazard_forward_lidar_near()
{
    auto snap = make_clear_snapshot();
    snap.forward_lidar_mm = 800;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT_EQ(result.forward_level, HazardLevel::NEAR);
    TEST_ASSERT(!result.emergency_stop);
    return true;
}

static bool test_hazard_forward_lidar_warn()
{
    auto snap = make_clear_snapshot();
    snap.forward_lidar_mm = 2000;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT_EQ(result.forward_level, HazardLevel::WARN);
    TEST_ASSERT(!result.emergency_stop);
    return true;
}

static bool test_hazard_forward_lidar_none()
{
    auto snap = make_clear_snapshot();
    snap.forward_lidar_mm = 5000;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT_EQ(result.forward_level, HazardLevel::NONE);
    return true;
}

static bool test_hazard_forward_lidar_invalid()
{
    auto snap = make_clear_snapshot();
    snap.forward_lidar_valid = false;
    snap.forward_lidar_mm    = 100;  /* would be STOP if valid */
    auto result = hazard_evaluate(snap);
    TEST_ASSERT_EQ(result.forward_level, HazardLevel::NONE);
    TEST_ASSERT(!result.emergency_stop);
    return true;
}

/* ── 1.5  Drop-off detection ───────────────────────────────────────── */

static bool test_hazard_dropoff_detected()
{
    auto snap = make_clear_snapshot();
    snap.downward_tof_mm = 400;   /* > 300 → drop */
    auto result = hazard_evaluate(snap);
    TEST_ASSERT(result.drop_detected);
    TEST_ASSERT(result.emergency_stop);
    return true;
}

static bool test_hazard_dropoff_at_boundary_300()
{
    auto snap = make_clear_snapshot();
    snap.downward_tof_mm = DROP_OFF_THRESHOLD_MM;  /* == 300 → NOT a drop (strict >) */
    auto result = hazard_evaluate(snap);
    TEST_ASSERT(!result.drop_detected);
    TEST_ASSERT(!result.emergency_stop);
    return true;
}

static bool test_hazard_dropoff_at_boundary_301()
{
    auto snap = make_clear_snapshot();
    snap.downward_tof_mm = DROP_OFF_THRESHOLD_MM + 1;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT(result.drop_detected);
    TEST_ASSERT(result.emergency_stop);
    return true;
}

static bool test_hazard_dropoff_invalid_sensor()
{
    auto snap = make_clear_snapshot();
    snap.downward_tof_valid = false;
    snap.downward_tof_mm    = 1000; /* would be a drop if valid */
    auto result = hazard_evaluate(snap);
    TEST_ASSERT(!result.drop_detected);
    return true;
}

static bool test_hazard_dropoff_sensor_invalid_marker()
{
    auto snap = make_clear_snapshot();
    snap.downward_tof_mm = TOF_SENSOR_INVALID;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT(!result.drop_detected);
    return true;
}

/* ── 1.6  Invalid sensor handling ──────────────────────────────────── */

static bool test_hazard_invalid_quadrant_sensor()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_FRONT].valid           = false;
    snap.quadrants[QUAD_FRONT].min_distance_mm = 100; /* would be STOP */
    auto result = hazard_evaluate(snap);
    TEST_ASSERT_EQ(result.quadrants[QUAD_FRONT].level, HazardLevel::NONE);
    TEST_ASSERT(!result.emergency_stop);
    return true;
}

static bool test_hazard_quadrant_invalid_marker()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_RIGHT].min_distance_mm = TOF_SENSOR_INVALID;
    auto result = hazard_evaluate(snap);
    TEST_ASSERT_EQ(result.quadrants[QUAD_RIGHT].level, HazardLevel::NONE);
    return true;
}

static bool test_hazard_all_sensors_invalid()
{
    SensorSnapshot snap{};
    for (uint8_t q = 0; q < QUAD_COUNT; ++q) {
        snap.quadrants[q].valid           = false;
        snap.quadrants[q].min_distance_mm = TOF_SENSOR_INVALID;
    }
    snap.forward_lidar_valid = false;
    snap.forward_lidar_mm    = TOF_SENSOR_INVALID;
    snap.downward_tof_valid  = false;
    snap.downward_tof_mm     = TOF_SENSOR_INVALID;

    auto result = hazard_evaluate(snap);

    for (uint8_t q = 0; q < QUAD_COUNT; ++q) {
        TEST_ASSERT_EQ(result.quadrants[q].level, HazardLevel::NONE);
    }
    TEST_ASSERT_EQ(result.forward_level, HazardLevel::NONE);
    TEST_ASSERT(!result.drop_detected);
    TEST_ASSERT(!result.emergency_stop);
    return true;
}

/* ── 1.7  Multiple simultaneous hazards ────────────────────────────── */

static bool test_hazard_multiple_quadrants()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_FRONT].min_distance_mm = 2000; /* WARN */
    snap.quadrants[QUAD_RIGHT].min_distance_mm = 800;  /* NEAR */
    snap.quadrants[QUAD_LEFT].min_distance_mm  = 300;  /* STOP */
    auto result = hazard_evaluate(snap);

    TEST_ASSERT_EQ(result.quadrants[QUAD_FRONT].level, HazardLevel::WARN);
    TEST_ASSERT_EQ(result.quadrants[QUAD_RIGHT].level, HazardLevel::NEAR);
    TEST_ASSERT_EQ(result.quadrants[QUAD_LEFT].level,  HazardLevel::STOP);
    TEST_ASSERT_EQ(result.quadrants[QUAD_REAR].level,  HazardLevel::NONE);
    TEST_ASSERT(result.emergency_stop);
    return true;
}

static bool test_hazard_combined_forward_and_quadrant()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_FRONT].min_distance_mm = 900;  /* NEAR */
    snap.forward_lidar_mm                      = 350;  /* STOP */
    auto result = hazard_evaluate(snap);

    TEST_ASSERT_EQ(result.quadrants[QUAD_FRONT].level, HazardLevel::NEAR);
    TEST_ASSERT_EQ(result.forward_level, HazardLevel::STOP);
    TEST_ASSERT(result.emergency_stop);
    return true;
}

static bool test_hazard_closest_mm_passthrough()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_FRONT].min_distance_mm = 1234;
    snap.forward_lidar_mm                      = 5678;
    auto result = hazard_evaluate(snap);

    TEST_ASSERT_EQ(result.quadrants[QUAD_FRONT].closest_mm, 1234);
    TEST_ASSERT_EQ(result.forward_mm, 5678);
    return true;
}

/* ===================================================================
 * SECTION 2: Haptic Mapper Tests
 * =================================================================== */

/* ── 2.1  Intensity levels ─────────────────────────────────────────── */

static bool test_haptic_intensity_none()
{
    HazardResult haz{};
    auto pkt = haptic_build_packet(haz, 0);
    for (uint8_t i = 0; i < BELT_MOTOR_COUNT; ++i) {
        TEST_ASSERT_EQ(pkt.motors[i], 0);
    }
    return true;
}

static bool test_haptic_intensity_warn()
{
    HazardResult haz{};
    haz.quadrants[QUAD_FRONT].level = HazardLevel::WARN;
    auto pkt = haptic_build_packet(haz, 0);
    /* FRONT → motors 7, 0, 1 should be 80 */
    TEST_ASSERT_EQ(pkt.motors[7], 80);
    TEST_ASSERT_EQ(pkt.motors[0], 80);
    TEST_ASSERT_EQ(pkt.motors[1], 80);
    return true;
}

static bool test_haptic_intensity_near()
{
    HazardResult haz{};
    haz.quadrants[QUAD_RIGHT].level = HazardLevel::NEAR;
    auto pkt = haptic_build_packet(haz, 0);
    /* RIGHT → motors 1, 2, 3 should be 180 */
    TEST_ASSERT_EQ(pkt.motors[1], 180);
    TEST_ASSERT_EQ(pkt.motors[2], 180);
    TEST_ASSERT_EQ(pkt.motors[3], 180);
    return true;
}

static bool test_haptic_intensity_stop()
{
    HazardResult haz{};
    haz.quadrants[QUAD_REAR].level = HazardLevel::STOP;
    auto pkt = haptic_build_packet(haz, 0);
    /* REAR → motors 3, 4, 5 should be 255 */
    TEST_ASSERT_EQ(pkt.motors[3], 255);
    TEST_ASSERT_EQ(pkt.motors[4], 255);
    TEST_ASSERT_EQ(pkt.motors[5], 255);
    return true;
}

/* ── 2.2  Motor mapping for each quadrant ──────────────────────────── */

static bool test_haptic_front_motors()
{
    HazardResult haz{};
    haz.quadrants[QUAD_FRONT].level = HazardLevel::NEAR;
    auto pkt = haptic_build_packet(haz, 0);
    TEST_ASSERT_EQ(pkt.motors[7], 180);
    TEST_ASSERT_EQ(pkt.motors[0], 180);
    TEST_ASSERT_EQ(pkt.motors[1], 180);
    /* Non-front motors (that are not shared) should be 0 */
    TEST_ASSERT_EQ(pkt.motors[2], 0);
    TEST_ASSERT_EQ(pkt.motors[3], 0);
    TEST_ASSERT_EQ(pkt.motors[4], 0);
    TEST_ASSERT_EQ(pkt.motors[5], 0);
    TEST_ASSERT_EQ(pkt.motors[6], 0);
    return true;
}

static bool test_haptic_right_motors()
{
    HazardResult haz{};
    haz.quadrants[QUAD_RIGHT].level = HazardLevel::NEAR;
    auto pkt = haptic_build_packet(haz, 0);
    TEST_ASSERT_EQ(pkt.motors[1], 180);
    TEST_ASSERT_EQ(pkt.motors[2], 180);
    TEST_ASSERT_EQ(pkt.motors[3], 180);
    TEST_ASSERT_EQ(pkt.motors[0], 0);
    TEST_ASSERT_EQ(pkt.motors[4], 0);
    TEST_ASSERT_EQ(pkt.motors[5], 0);
    TEST_ASSERT_EQ(pkt.motors[6], 0);
    TEST_ASSERT_EQ(pkt.motors[7], 0);
    return true;
}

static bool test_haptic_rear_motors()
{
    HazardResult haz{};
    haz.quadrants[QUAD_REAR].level = HazardLevel::NEAR;
    auto pkt = haptic_build_packet(haz, 0);
    TEST_ASSERT_EQ(pkt.motors[3], 180);
    TEST_ASSERT_EQ(pkt.motors[4], 180);
    TEST_ASSERT_EQ(pkt.motors[5], 180);
    TEST_ASSERT_EQ(pkt.motors[0], 0);
    TEST_ASSERT_EQ(pkt.motors[1], 0);
    TEST_ASSERT_EQ(pkt.motors[2], 0);
    TEST_ASSERT_EQ(pkt.motors[6], 0);
    TEST_ASSERT_EQ(pkt.motors[7], 0);
    return true;
}

static bool test_haptic_left_motors()
{
    HazardResult haz{};
    haz.quadrants[QUAD_LEFT].level = HazardLevel::NEAR;
    auto pkt = haptic_build_packet(haz, 0);
    TEST_ASSERT_EQ(pkt.motors[5], 180);
    TEST_ASSERT_EQ(pkt.motors[6], 180);
    TEST_ASSERT_EQ(pkt.motors[7], 180);
    TEST_ASSERT_EQ(pkt.motors[0], 0);
    TEST_ASSERT_EQ(pkt.motors[1], 0);
    TEST_ASSERT_EQ(pkt.motors[2], 0);
    TEST_ASSERT_EQ(pkt.motors[3], 0);
    TEST_ASSERT_EQ(pkt.motors[4], 0);
    return true;
}

/* ── 2.3  Overlapping quadrant motors (max wins) ───────────────────── */

static bool test_haptic_overlap_front_right()
{
    HazardResult haz{};
    haz.quadrants[QUAD_FRONT].level = HazardLevel::WARN;   /* 80  */
    haz.quadrants[QUAD_RIGHT].level = HazardLevel::NEAR;   /* 180 */
    auto pkt = haptic_build_packet(haz, 0);

    /* Motor 1 shared: max(80, 180) = 180 */
    TEST_ASSERT_EQ(pkt.motors[1], 180);
    /* Motor 7,0 only from FRONT = 80 */
    TEST_ASSERT_EQ(pkt.motors[7], 80);
    TEST_ASSERT_EQ(pkt.motors[0], 80);
    /* Motor 2,3 only from RIGHT = 180 */
    TEST_ASSERT_EQ(pkt.motors[2], 180);
    TEST_ASSERT_EQ(pkt.motors[3], 180);
    return true;
}

static bool test_haptic_overlap_right_rear()
{
    HazardResult haz{};
    haz.quadrants[QUAD_RIGHT].level = HazardLevel::WARN;   /* 80  */
    haz.quadrants[QUAD_REAR].level  = HazardLevel::STOP;   /* 255 */
    auto pkt = haptic_build_packet(haz, 0);

    /* Motor 3 shared: max(80, 255) = 255 */
    TEST_ASSERT_EQ(pkt.motors[3], 255);
    TEST_ASSERT_EQ(pkt.motors[1], 80);
    TEST_ASSERT_EQ(pkt.motors[2], 80);
    TEST_ASSERT_EQ(pkt.motors[4], 255);
    TEST_ASSERT_EQ(pkt.motors[5], 255);
    return true;
}

static bool test_haptic_overlap_rear_left()
{
    HazardResult haz{};
    haz.quadrants[QUAD_REAR].level = HazardLevel::NEAR;    /* 180 */
    haz.quadrants[QUAD_LEFT].level = HazardLevel::WARN;    /* 80  */
    auto pkt = haptic_build_packet(haz, 0);

    /* Motor 5 shared: max(180, 80) = 180 */
    TEST_ASSERT_EQ(pkt.motors[5], 180);
    TEST_ASSERT_EQ(pkt.motors[3], 180);
    TEST_ASSERT_EQ(pkt.motors[4], 180);
    TEST_ASSERT_EQ(pkt.motors[6], 80);
    TEST_ASSERT_EQ(pkt.motors[7], 80);
    return true;
}

static bool test_haptic_overlap_left_front()
{
    HazardResult haz{};
    haz.quadrants[QUAD_LEFT].level  = HazardLevel::NEAR;   /* 180 */
    haz.quadrants[QUAD_FRONT].level = HazardLevel::STOP;   /* 255 */
    auto pkt = haptic_build_packet(haz, 0);

    /* Motor 7 shared: max(180, 255) = 255 */
    TEST_ASSERT_EQ(pkt.motors[7], 255);
    TEST_ASSERT_EQ(pkt.motors[0], 255);
    TEST_ASSERT_EQ(pkt.motors[1], 255);
    TEST_ASSERT_EQ(pkt.motors[5], 180);
    TEST_ASSERT_EQ(pkt.motors[6], 180);
    return true;
}

static bool test_haptic_all_quadrants_active()
{
    HazardResult haz{};
    haz.quadrants[QUAD_FRONT].level = HazardLevel::WARN;   /* 80  */
    haz.quadrants[QUAD_RIGHT].level = HazardLevel::NEAR;   /* 180 */
    haz.quadrants[QUAD_REAR].level  = HazardLevel::STOP;   /* 255 */
    haz.quadrants[QUAD_LEFT].level  = HazardLevel::WARN;   /* 80  */
    auto pkt = haptic_build_packet(haz, 0);

    /* Motor 0: FRONT only = 80 */
    TEST_ASSERT_EQ(pkt.motors[0], 80);
    /* Motor 1: max(FRONT=80, RIGHT=180) = 180 */
    TEST_ASSERT_EQ(pkt.motors[1], 180);
    /* Motor 2: RIGHT only = 180 */
    TEST_ASSERT_EQ(pkt.motors[2], 180);
    /* Motor 3: max(RIGHT=180, REAR=255) = 255 */
    TEST_ASSERT_EQ(pkt.motors[3], 255);
    /* Motor 4: REAR only = 255 */
    TEST_ASSERT_EQ(pkt.motors[4], 255);
    /* Motor 5: max(REAR=255, LEFT=80) = 255 */
    TEST_ASSERT_EQ(pkt.motors[5], 255);
    /* Motor 6: LEFT only = 80 */
    TEST_ASSERT_EQ(pkt.motors[6], 80);
    /* Motor 7: max(LEFT=80, FRONT=80) = 80 */
    TEST_ASSERT_EQ(pkt.motors[7], 80);
    return true;
}

/* ── 2.4  Forward LiDAR override on motor 0 ───────────────────────── */

static bool test_haptic_forward_lidar_overrides_motor0()
{
    HazardResult haz{};
    haz.quadrants[QUAD_FRONT].level = HazardLevel::WARN;  /* motor 0 = 80 */
    haz.forward_level               = HazardLevel::STOP;  /* motor 0 → max(80,255) = 255 */
    auto pkt = haptic_build_packet(haz, 0);

    TEST_ASSERT_EQ(pkt.motors[0], 255);
    /* motors 7 and 1 should still reflect FRONT quadrant only */
    TEST_ASSERT_EQ(pkt.motors[7], 80);
    TEST_ASSERT_EQ(pkt.motors[1], 80);
    return true;
}

static bool test_haptic_forward_lidar_weaker_than_quadrant()
{
    HazardResult haz{};
    haz.quadrants[QUAD_FRONT].level = HazardLevel::STOP;  /* motor 0 = 255 */
    haz.forward_level               = HazardLevel::WARN;  /* motor 0 → max(255,80) = 255 */
    auto pkt = haptic_build_packet(haz, 0);

    TEST_ASSERT_EQ(pkt.motors[0], 255);
    return true;
}

/* ── 2.5  Drop-off drives all motors ───────────────────────────────── */

static bool test_haptic_dropoff_all_motors()
{
    HazardResult haz{};
    haz.drop_detected = true;
    auto pkt = haptic_build_packet(haz, 0);

    for (uint8_t i = 0; i < BELT_MOTOR_COUNT; ++i) {
        TEST_ASSERT_EQ(pkt.motors[i], 255);
    }
    return true;
}

static bool test_haptic_dropoff_overrides_lower_hazards()
{
    HazardResult haz{};
    haz.quadrants[QUAD_FRONT].level = HazardLevel::WARN;
    haz.drop_detected               = true;
    auto pkt = haptic_build_packet(haz, 0);

    for (uint8_t i = 0; i < BELT_MOTOR_COUNT; ++i) {
        TEST_ASSERT_EQ(pkt.motors[i], 255);
    }
    return true;
}

/* ── 2.6  Emergency-stop flag ──────────────────────────────────────── */

static bool test_haptic_estop_flag_set()
{
    HazardResult haz{};
    haz.emergency_stop = true;
    auto pkt = haptic_build_packet(haz, 0);
    TEST_ASSERT_EQ(pkt.flags & 0x01, 0x01);
    return true;
}

static bool test_haptic_estop_flag_clear()
{
    HazardResult haz{};
    haz.emergency_stop = false;
    auto pkt = haptic_build_packet(haz, 0);
    TEST_ASSERT_EQ(pkt.flags & 0x01, 0x00);
    return true;
}

/* ── 2.7  Packet header ───────────────────────────────────────────── */

static bool test_haptic_packet_header()
{
    HazardResult haz{};
    auto pkt = haptic_build_packet(haz, 0);
    TEST_ASSERT_EQ(pkt.header, BELT_PACKET_HEADER);
    TEST_ASSERT_EQ(pkt.header, 0xCA);
    return true;
}

/* ── 2.8  Sequence number passthrough ──────────────────────────────── */

static bool test_haptic_seq_passthrough()
{
    HazardResult haz{};
    for (uint8_t s : {0, 1, 42, 127, 255}) {
        auto pkt = haptic_build_packet(haz, s);
        TEST_ASSERT_EQ(pkt.seq, s);
    }
    return true;
}

/* ── 2.9  Checksum validation ──────────────────────────────────────── */

static bool test_haptic_checksum_valid()
{
    HazardResult haz{};
    haz.quadrants[QUAD_FRONT].level = HazardLevel::NEAR;
    haz.emergency_stop              = true;
    auto pkt = haptic_build_packet(haz, 42);

    /* Recompute checksum independently */
    const uint8_t *raw = reinterpret_cast<const uint8_t *>(&pkt);
    uint8_t cs = 0;
    for (uint8_t i = 0; i < BELT_PACKET_LENGTH - 1; ++i) {
        cs ^= raw[i];
    }
    TEST_ASSERT_EQ(pkt.checksum, cs);
    return true;
}

static bool test_haptic_checksum_function()
{
    HazardResult haz{};
    auto pkt = haptic_build_packet(haz, 7);
    uint8_t expected = belt_packet_checksum(pkt);
    TEST_ASSERT_EQ(pkt.checksum, expected);
    return true;
}

/* ── 2.10  belt_packet_valid ───────────────────────────────────────── */

static bool test_haptic_packet_valid_good()
{
    HazardResult haz{};
    haz.quadrants[QUAD_LEFT].level = HazardLevel::STOP;
    haz.emergency_stop             = true;
    auto pkt = haptic_build_packet(haz, 99);

    const uint8_t *buf = reinterpret_cast<const uint8_t *>(&pkt);
    TEST_ASSERT(belt_packet_valid(buf, BELT_PACKET_LENGTH));
    return true;
}

static bool test_haptic_packet_valid_bad_length()
{
    uint8_t buf[BELT_PACKET_LENGTH] = {};
    buf[0] = BELT_PACKET_HEADER;
    TEST_ASSERT(!belt_packet_valid(buf, BELT_PACKET_LENGTH - 1));
    TEST_ASSERT(!belt_packet_valid(buf, BELT_PACKET_LENGTH + 1));
    TEST_ASSERT(!belt_packet_valid(buf, 0));
    return true;
}

static bool test_haptic_packet_valid_bad_header()
{
    HazardResult haz{};
    auto pkt = haptic_build_packet(haz, 0);
    pkt.header = 0x00;
    /* Recompute checksum with corrupted header */
    pkt.checksum = belt_packet_checksum(pkt);
    const uint8_t *buf = reinterpret_cast<const uint8_t *>(&pkt);
    TEST_ASSERT(!belt_packet_valid(buf, BELT_PACKET_LENGTH));
    return true;
}

static bool test_haptic_packet_valid_bad_checksum()
{
    HazardResult haz{};
    auto pkt = haptic_build_packet(haz, 0);
    /* Corrupt the checksum */
    uint8_t raw[BELT_PACKET_LENGTH];
    std::memcpy(raw, &pkt, BELT_PACKET_LENGTH);
    raw[BELT_PACKET_LENGTH - 1] ^= 0xFF;
    TEST_ASSERT(!belt_packet_valid(raw, BELT_PACKET_LENGTH));
    return true;
}

static bool test_haptic_packet_valid_corrupted_motor()
{
    HazardResult haz{};
    auto pkt = haptic_build_packet(haz, 0);
    uint8_t raw[BELT_PACKET_LENGTH];
    std::memcpy(raw, &pkt, BELT_PACKET_LENGTH);
    raw[4] ^= 0x01; /* flip a motor byte without updating checksum */
    TEST_ASSERT(!belt_packet_valid(raw, BELT_PACKET_LENGTH));
    return true;
}

/* ── 2.11  BeltPacket struct size ──────────────────────────────────── */

static bool test_haptic_packet_size()
{
    TEST_ASSERT_EQ(sizeof(BeltPacket), static_cast<size_t>(BELT_PACKET_LENGTH));
    return true;
}

/* ===================================================================
 * SECTION 3: Integration Tests – Sensor → Hazard → Haptic Pipeline
 * =================================================================== */

static bool test_integration_clear_scene()
{
    auto snap   = make_clear_snapshot();
    auto haz    = hazard_evaluate(snap);
    auto pkt    = haptic_build_packet(haz, 1);

    for (uint8_t i = 0; i < BELT_MOTOR_COUNT; ++i) {
        TEST_ASSERT_EQ(pkt.motors[i], 0);
    }
    TEST_ASSERT_EQ(pkt.flags & 0x01, 0x00);
    TEST_ASSERT(belt_packet_valid(
        reinterpret_cast<const uint8_t *>(&pkt), BELT_PACKET_LENGTH));
    return true;
}

static bool test_integration_all_sensors_invalid()
{
    SensorSnapshot snap{};
    for (uint8_t q = 0; q < QUAD_COUNT; ++q) {
        snap.quadrants[q].valid           = false;
        snap.quadrants[q].min_distance_mm = TOF_SENSOR_INVALID;
    }
    snap.forward_lidar_valid = false;
    snap.forward_lidar_mm    = TOF_SENSOR_INVALID;
    snap.downward_tof_valid  = false;
    snap.downward_tof_mm     = TOF_SENSOR_INVALID;

    auto haz = hazard_evaluate(snap);
    auto pkt = haptic_build_packet(haz, 0);

    /* Graceful degradation: no false hazards, all motors silent */
    for (uint8_t i = 0; i < BELT_MOTOR_COUNT; ++i) {
        TEST_ASSERT_EQ(pkt.motors[i], 0);
    }
    TEST_ASSERT_EQ(pkt.flags & 0x01, 0x00);
    TEST_ASSERT(!haz.emergency_stop);
    TEST_ASSERT(!haz.drop_detected);
    return true;
}

static bool test_integration_front_obstacle_stop()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_FRONT].min_distance_mm = 200;
    auto haz = hazard_evaluate(snap);
    auto pkt = haptic_build_packet(haz, 5);

    /* Motors 7, 0, 1 should be at STOP intensity (255) */
    TEST_ASSERT_EQ(pkt.motors[7], 255);
    TEST_ASSERT_EQ(pkt.motors[0], 255);
    TEST_ASSERT_EQ(pkt.motors[1], 255);
    /* Emergency stop flag set */
    TEST_ASSERT_EQ(pkt.flags & 0x01, 0x01);
    TEST_ASSERT(belt_packet_valid(
        reinterpret_cast<const uint8_t *>(&pkt), BELT_PACKET_LENGTH));
    return true;
}

static bool test_integration_dropoff_propagates()
{
    auto snap = make_clear_snapshot();
    snap.downward_tof_mm = 500;  /* drop */
    auto haz = hazard_evaluate(snap);
    auto pkt = haptic_build_packet(haz, 10);

    TEST_ASSERT(haz.drop_detected);
    TEST_ASSERT(haz.emergency_stop);
    for (uint8_t i = 0; i < BELT_MOTOR_COUNT; ++i) {
        TEST_ASSERT_EQ(pkt.motors[i], 255);
    }
    TEST_ASSERT_EQ(pkt.flags & 0x01, 0x01);
    return true;
}

static bool test_integration_forward_lidar_pipeline()
{
    auto snap = make_clear_snapshot();
    snap.forward_lidar_mm = 300;  /* STOP */
    auto haz = hazard_evaluate(snap);
    auto pkt = haptic_build_packet(haz, 20);

    TEST_ASSERT_EQ(haz.forward_level, HazardLevel::STOP);
    TEST_ASSERT(haz.emergency_stop);
    TEST_ASSERT_EQ(pkt.motors[0], 255);
    TEST_ASSERT_EQ(pkt.flags & 0x01, 0x01);
    return true;
}

static bool test_integration_multi_hazard_pipeline()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_FRONT].min_distance_mm = 2000;  /* WARN  */
    snap.quadrants[QUAD_RIGHT].min_distance_mm = 800;   /* NEAR  */
    snap.quadrants[QUAD_REAR].min_distance_mm  = 200;   /* STOP  */
    snap.forward_lidar_mm                      = 1000;  /* NEAR  */

    auto haz = hazard_evaluate(snap);
    auto pkt = haptic_build_packet(haz, 33);

    /* Check hazard levels */
    TEST_ASSERT_EQ(haz.quadrants[QUAD_FRONT].level, HazardLevel::WARN);
    TEST_ASSERT_EQ(haz.quadrants[QUAD_RIGHT].level, HazardLevel::NEAR);
    TEST_ASSERT_EQ(haz.quadrants[QUAD_REAR].level,  HazardLevel::STOP);
    TEST_ASSERT_EQ(haz.forward_level, HazardLevel::NEAR);
    TEST_ASSERT(haz.emergency_stop);

    /* Check motor intensities */
    /* Motor 0: max(FRONT_WARN=80, FWD_NEAR=180) = 180 */
    TEST_ASSERT_EQ(pkt.motors[0], 180);
    /* Motor 1: max(FRONT_WARN=80, RIGHT_NEAR=180) = 180 */
    TEST_ASSERT_EQ(pkt.motors[1], 180);
    /* Motor 2: RIGHT_NEAR = 180 */
    TEST_ASSERT_EQ(pkt.motors[2], 180);
    /* Motor 3: max(RIGHT_NEAR=180, REAR_STOP=255) = 255 */
    TEST_ASSERT_EQ(pkt.motors[3], 255);
    /* Motor 4: REAR_STOP = 255 */
    TEST_ASSERT_EQ(pkt.motors[4], 255);
    /* Motor 5: REAR_STOP = 255 */
    TEST_ASSERT_EQ(pkt.motors[5], 255);
    /* Motor 6: 0 (LEFT is clear) */
    TEST_ASSERT_EQ(pkt.motors[6], 0);
    /* Motor 7: FRONT_WARN = 80 */
    TEST_ASSERT_EQ(pkt.motors[7], 80);

    /* Packet is valid */
    TEST_ASSERT(belt_packet_valid(
        reinterpret_cast<const uint8_t *>(&pkt), BELT_PACKET_LENGTH));
    return true;
}

static bool test_integration_estop_propagates_to_belt()
{
    auto snap = make_clear_snapshot();
    snap.quadrants[QUAD_LEFT].min_distance_mm = 50;
    auto haz = hazard_evaluate(snap);

    TEST_ASSERT(haz.emergency_stop);

    auto pkt = haptic_build_packet(haz, 0);
    TEST_ASSERT_EQ(pkt.flags & 0x01, 0x01);
    return true;
}

/* ===================================================================
 * Cellular Modem AT Parser Tests
 * =================================================================== */

static bool test_cell_csq_good_signal()
{
    int8_t rssi; uint8_t pct;
    TEST_ASSERT(cell_parse_csq("+CSQ: 18,0", &rssi, &pct));
    TEST_ASSERT(rssi == -77);       /* -113 + 18*2 = -77 */
    TEST_ASSERT(pct >= 55 && pct <= 60);    /* ~58% */
    return true;
}

static bool test_cell_csq_no_signal()
{
    int8_t rssi; uint8_t pct;
    TEST_ASSERT(cell_parse_csq("+CSQ: 99,99", &rssi, &pct));
    TEST_ASSERT(rssi == -120);
    TEST_ASSERT(pct == 0);
    return true;
}

static bool test_cell_csq_max_signal()
{
    int8_t rssi; uint8_t pct;
    TEST_ASSERT(cell_parse_csq("+CSQ: 31,0", &rssi, &pct));
    TEST_ASSERT(rssi == -51);       /* -113 + 31*2 = -51 */
    TEST_ASSERT(pct == 100);
    return true;
}

static bool test_cell_csq_unknown_99()
{
    int8_t rssi; uint8_t pct;
    TEST_ASSERT(cell_parse_csq("+CSQ: 99,0", &rssi, &pct));
    TEST_ASSERT(pct == 0);
    return true;
}

static bool test_cell_csq_invalid_string()
{
    int8_t rssi; uint8_t pct;
    TEST_ASSERT(!cell_parse_csq("GARBAGE", &rssi, &pct));
    return true;
}

static bool test_cell_csq_zero()
{
    int8_t rssi; uint8_t pct;
    TEST_ASSERT(cell_parse_csq("+CSQ: 0,0", &rssi, &pct));
    TEST_ASSERT(rssi == -113);
    TEST_ASSERT(pct == 0);
    return true;
}

static bool test_cell_cops_tmobile()
{
    char name[24];
    TEST_ASSERT(cell_parse_cops("+COPS: 0,0,\"T-Mobile\",7", name, sizeof(name)));
    TEST_ASSERT(strcmp(name, "T-Mobile") == 0);
    return true;
}

static bool test_cell_cops_att()
{
    char name[24];
    TEST_ASSERT(cell_parse_cops("+COPS: 0,0,\"AT&T\",7", name, sizeof(name)));
    TEST_ASSERT(strcmp(name, "AT&T") == 0);
    return true;
}

static bool test_cell_cops_verizon()
{
    char name[24];
    TEST_ASSERT(cell_parse_cops("+COPS: 0,0,\"Verizon\",7", name, sizeof(name)));
    TEST_ASSERT(strcmp(name, "Verizon") == 0);
    return true;
}

static bool test_cell_cops_no_quotes()
{
    char name[24];
    TEST_ASSERT(!cell_parse_cops("+COPS: 0,0,NoQuotes,7", name, sizeof(name)));
    return true;
}

static bool test_cell_cops_long_name()
{
    char name[8];   /* intentionally short buffer */
    TEST_ASSERT(cell_parse_cops("+COPS: 0,0,\"VeryLongOperatorName\",7", name, sizeof(name)));
    TEST_ASSERT(strlen(name) == 7);     /* truncated to max_len - 1 */
    return true;
}

static bool test_cell_reg_home()
{
    bool reg;
    TEST_ASSERT(cell_parse_registration("+CREG: 0,1", &reg));
    TEST_ASSERT(reg == true);
    return true;
}

static bool test_cell_reg_roaming()
{
    bool reg;
    TEST_ASSERT(cell_parse_registration("+CREG: 0,5", &reg));
    TEST_ASSERT(reg == true);
    return true;
}

static bool test_cell_reg_searching()
{
    bool reg;
    TEST_ASSERT(cell_parse_registration("+CREG: 0,2", &reg));
    TEST_ASSERT(reg == false);
    return true;
}

static bool test_cell_reg_denied()
{
    bool reg;
    TEST_ASSERT(cell_parse_registration("+CREG: 0,3", &reg));
    TEST_ASSERT(reg == false);
    return true;
}

static bool test_cell_reg_cereg()
{
    bool reg;
    TEST_ASSERT(cell_parse_registration("+CEREG: 0,1", &reg));
    TEST_ASSERT(reg == true);
    return true;
}

static bool test_cell_reg_invalid()
{
    bool reg;
    TEST_ASSERT(!cell_parse_registration("GARBAGE", &reg));
    return true;
}

/* ===================================================================
 * main – run all tests
 * =================================================================== */

int main()
{
    std::printf("=== Magic Cane Unit Tests ===\n\n");

    /* ── Hazard Engine ──────────────────────────────────────────────── */
    std::printf("--- Hazard Engine: Distance Classification ---\n");
    RUN_TEST(test_hazard_clear_scene);
    RUN_TEST(test_hazard_warn_level);
    RUN_TEST(test_hazard_near_level);
    RUN_TEST(test_hazard_stop_level);
    RUN_TEST(test_hazard_zero_distance);

    std::printf("\n--- Hazard Engine: Boundary Conditions ---\n");
    RUN_TEST(test_hazard_boundary_stop_at_400);
    RUN_TEST(test_hazard_boundary_stop_at_399);
    RUN_TEST(test_hazard_boundary_near_at_1200);
    RUN_TEST(test_hazard_boundary_near_at_1199);
    RUN_TEST(test_hazard_boundary_warn_at_2500);
    RUN_TEST(test_hazard_boundary_warn_at_2499);

    std::printf("\n--- Hazard Engine: Emergency Stop ---\n");
    RUN_TEST(test_hazard_estop_front_quadrant);
    RUN_TEST(test_hazard_estop_right_quadrant);
    RUN_TEST(test_hazard_estop_rear_quadrant);
    RUN_TEST(test_hazard_estop_left_quadrant);

    std::printf("\n--- Hazard Engine: Forward LiDAR ---\n");
    RUN_TEST(test_hazard_forward_lidar_stop);
    RUN_TEST(test_hazard_forward_lidar_near);
    RUN_TEST(test_hazard_forward_lidar_warn);
    RUN_TEST(test_hazard_forward_lidar_none);
    RUN_TEST(test_hazard_forward_lidar_invalid);

    std::printf("\n--- Hazard Engine: Drop-off Detection ---\n");
    RUN_TEST(test_hazard_dropoff_detected);
    RUN_TEST(test_hazard_dropoff_at_boundary_300);
    RUN_TEST(test_hazard_dropoff_at_boundary_301);
    RUN_TEST(test_hazard_dropoff_invalid_sensor);
    RUN_TEST(test_hazard_dropoff_sensor_invalid_marker);

    std::printf("\n--- Hazard Engine: Invalid Sensors ---\n");
    RUN_TEST(test_hazard_invalid_quadrant_sensor);
    RUN_TEST(test_hazard_quadrant_invalid_marker);
    RUN_TEST(test_hazard_all_sensors_invalid);

    std::printf("\n--- Hazard Engine: Multiple/Combined Hazards ---\n");
    RUN_TEST(test_hazard_multiple_quadrants);
    RUN_TEST(test_hazard_combined_forward_and_quadrant);
    RUN_TEST(test_hazard_closest_mm_passthrough);

    /* ── Haptic Mapper ──────────────────────────────────────────────── */
    std::printf("\n--- Haptic Mapper: Intensity Levels ---\n");
    RUN_TEST(test_haptic_intensity_none);
    RUN_TEST(test_haptic_intensity_warn);
    RUN_TEST(test_haptic_intensity_near);
    RUN_TEST(test_haptic_intensity_stop);

    std::printf("\n--- Haptic Mapper: Motor Mapping ---\n");
    RUN_TEST(test_haptic_front_motors);
    RUN_TEST(test_haptic_right_motors);
    RUN_TEST(test_haptic_rear_motors);
    RUN_TEST(test_haptic_left_motors);

    std::printf("\n--- Haptic Mapper: Overlapping Motors ---\n");
    RUN_TEST(test_haptic_overlap_front_right);
    RUN_TEST(test_haptic_overlap_right_rear);
    RUN_TEST(test_haptic_overlap_rear_left);
    RUN_TEST(test_haptic_overlap_left_front);
    RUN_TEST(test_haptic_all_quadrants_active);

    std::printf("\n--- Haptic Mapper: Forward LiDAR Override ---\n");
    RUN_TEST(test_haptic_forward_lidar_overrides_motor0);
    RUN_TEST(test_haptic_forward_lidar_weaker_than_quadrant);

    std::printf("\n--- Haptic Mapper: Drop-off ---\n");
    RUN_TEST(test_haptic_dropoff_all_motors);
    RUN_TEST(test_haptic_dropoff_overrides_lower_hazards);

    std::printf("\n--- Haptic Mapper: Emergency Stop Flag ---\n");
    RUN_TEST(test_haptic_estop_flag_set);
    RUN_TEST(test_haptic_estop_flag_clear);

    std::printf("\n--- Haptic Mapper: Packet Format ---\n");
    RUN_TEST(test_haptic_packet_header);
    RUN_TEST(test_haptic_seq_passthrough);
    RUN_TEST(test_haptic_packet_size);

    std::printf("\n--- Haptic Mapper: Checksum ---\n");
    RUN_TEST(test_haptic_checksum_valid);
    RUN_TEST(test_haptic_checksum_function);

    std::printf("\n--- Haptic Mapper: Packet Validation ---\n");
    RUN_TEST(test_haptic_packet_valid_good);
    RUN_TEST(test_haptic_packet_valid_bad_length);
    RUN_TEST(test_haptic_packet_valid_bad_header);
    RUN_TEST(test_haptic_packet_valid_bad_checksum);
    RUN_TEST(test_haptic_packet_valid_corrupted_motor);

    /* ── Integration Tests ──────────────────────────────────────────── */
    std::printf("\n--- Integration: Sensor → Hazard → Haptic ---\n");
    RUN_TEST(test_integration_clear_scene);
    RUN_TEST(test_integration_all_sensors_invalid);
    RUN_TEST(test_integration_front_obstacle_stop);
    RUN_TEST(test_integration_dropoff_propagates);
    RUN_TEST(test_integration_forward_lidar_pipeline);
    RUN_TEST(test_integration_multi_hazard_pipeline);
    RUN_TEST(test_integration_estop_propagates_to_belt);

    /* ── Cellular Modem AT Parser Tests ────────────────────────────── */
    std::printf("\n--- Cellular Modem: CSQ Parsing ---\n");
    RUN_TEST(test_cell_csq_good_signal);
    RUN_TEST(test_cell_csq_no_signal);
    RUN_TEST(test_cell_csq_max_signal);
    RUN_TEST(test_cell_csq_unknown_99);
    RUN_TEST(test_cell_csq_invalid_string);
    RUN_TEST(test_cell_csq_zero);

    std::printf("\n--- Cellular Modem: COPS Parsing ---\n");
    RUN_TEST(test_cell_cops_tmobile);
    RUN_TEST(test_cell_cops_att);
    RUN_TEST(test_cell_cops_verizon);
    RUN_TEST(test_cell_cops_no_quotes);
    RUN_TEST(test_cell_cops_long_name);

    std::printf("\n--- Cellular Modem: Registration Parsing ---\n");
    RUN_TEST(test_cell_reg_home);
    RUN_TEST(test_cell_reg_roaming);
    RUN_TEST(test_cell_reg_searching);
    RUN_TEST(test_cell_reg_denied);
    RUN_TEST(test_cell_reg_cereg);
    RUN_TEST(test_cell_reg_invalid);

    /* ── Summary ────────────────────────────────────────────────────── */
    std::printf("\n========================================\n");
    std::printf("  Tests run:    %d\n", g_tests_run);
    std::printf("  Tests passed: %d\n", g_tests_passed);
    std::printf("  Tests failed: %d\n", g_tests_failed);
    std::printf("========================================\n");

    return g_tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
