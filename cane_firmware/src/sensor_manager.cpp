/*
 * Magic Cane – sensor manager (ESP32-S3 implementation)
 *
 * Initialises and polls all physical sensors on the cane.
 * This file is excluded from host-side unit tests (guarded by UNIT_TEST).
 *
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#ifndef UNIT_TEST

#include "sensor_manager.h"
#include <Arduino.h>
#include <Wire.h>

/* ── TCA9548A I²C multiplexer ───────────────────────────────────────── */
static constexpr uint8_t MUX_ADDR = 0x70;

static void mux_select(uint8_t channel)
{
    Wire.beginTransmission(MUX_ADDR);
    Wire.write(1 << channel);
    Wire.endTransmission();
}

/* ── VL53L5CX stub addresses (via multiplexer channels 0–3) ────────── */
static constexpr uint8_t VL53L5CX_CHANNELS[QUAD_COUNT] = {0, 1, 2, 3};

/* ── Downward VL53L1X on mux channel 4 ─────────────────────────────── */
static constexpr uint8_t DOWN_TOF_CHANNEL = 4;

/* ── BNO086 on mux channel 5 ───────────────────────────────────────── */
static constexpr uint8_t IMU_CHANNEL = 5;

/* ── TFmini-S on UART1 ─────────────────────────────────────────────── */
static constexpr int TFMINI_RX_PIN = 18;
static constexpr int TFMINI_TX_PIN = 17;

/* ── Forward declarations for low-level sensor I/O ──────────────────── */
static bool     vl53l5cx_read(uint8_t channel, QuadrantReading &qr);
static bool     vl53l1x_read_downward(uint16_t &distance_mm);
static bool     tfmini_read(uint16_t &distance_mm);
static bool     bno086_read(float &pitch, float &roll, float &yaw);

/* ── Initialisation ─────────────────────────────────────────────────── */

void sensors_init()
{
    Wire.begin();
    Wire.setClock(400000);      /* 400 kHz I²C fast-mode */

    Serial1.begin(115200, SERIAL_8N1, TFMINI_RX_PIN, TFMINI_TX_PIN);

    /* TODO: send configuration frames to each VL53L5CX via mux */
    /* TODO: initialise BNO086 and enable rotation vector report */
    /* TODO: initialise VL53L1X downward sensor */

    Serial.println("[SENSOR] sensor_init complete");
}

/* ── Poll all sensors into snapshot ─────────────────────────────────── */

void sensors_poll(SensorSnapshot &snap)
{
    snap.timestamp_ms = millis();

    /* Quadrant ToF sensors */
    for (uint8_t q = 0; q < QUAD_COUNT; ++q) {
        snap.quadrants[q].valid =
            vl53l5cx_read(VL53L5CX_CHANNELS[q], snap.quadrants[q]);
    }

    /* Forward LiDAR */
    snap.forward_lidar_valid = tfmini_read(snap.forward_lidar_mm);

    /* Downward ToF */
    snap.downward_tof_valid = vl53l1x_read_downward(snap.downward_tof_mm);

    /* IMU */
    snap.imu_valid = bno086_read(snap.pitch_deg, snap.roll_deg, snap.yaw_deg);
}

/* ── Low-level sensor stubs (to be completed with real driver code) ── */

static bool vl53l5cx_read(uint8_t channel, QuadrantReading &qr)
{
    mux_select(channel);
    /* TODO: read 8×8 ranging result from VL53L5CX and compute
       min/average distances.  Return false on I²C error. */
    qr.min_distance_mm = TOF_SENSOR_INVALID;
    qr.avg_distance_mm = TOF_SENSOR_INVALID;
    return false;   /* stub – sensor driver not yet integrated */
}

static bool vl53l1x_read_downward(uint16_t &distance_mm)
{
    mux_select(DOWN_TOF_CHANNEL);
    /* TODO: single-shot read from VL53L1X.  Return false on error. */
    distance_mm = TOF_SENSOR_INVALID;
    return false;   /* stub */
}

static bool tfmini_read(uint16_t &distance_mm)
{
    /* TFmini-S sends 9-byte frames at 100 Hz on UART1.
       Frame: 0x59 0x59 [Dist_L] [Dist_H] [Str_L] [Str_H] [Temp_L] [Temp_H] [Checksum] */
    if (Serial1.available() < 9) {
        return false;
    }

    uint8_t buf[9];
    /* Sync to header bytes */
    while (Serial1.available() >= 9) {
        if (Serial1.peek() != 0x59) {
            Serial1.read();     /* discard until sync */
            continue;
        }
        Serial1.readBytes(buf, 9);
        if (buf[0] == 0x59 && buf[1] == 0x59) {
            uint8_t cs = 0;
            for (int i = 0; i < 8; ++i) cs += buf[i];
            if (cs == buf[8]) {
                distance_mm = static_cast<uint16_t>(buf[2]) |
                              (static_cast<uint16_t>(buf[3]) << 8);
                /* Convert cm → mm (TFmini reports in cm) */
                distance_mm *= 10;
                return true;
            }
        }
    }
    return false;
}

static bool bno086_read(float &pitch, float &roll, float &yaw)
{
    mux_select(IMU_CHANNEL);
    /* TODO: read rotation vector from BNO086 and convert to Euler.
       Return false on error. */
    pitch = 0.0f;
    roll  = 0.0f;
    yaw   = 0.0f;
    return false;   /* stub */
}

#endif /* UNIT_TEST */
