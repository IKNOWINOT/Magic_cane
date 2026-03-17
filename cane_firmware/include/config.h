/*
 * Magic Cane – shared configuration & constants
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#pragma once

#include <stdint.h>

/* ── Timing ─────────────────────────────────────────────────────────── */
static constexpr uint32_t SENSOR_POLL_INTERVAL_MS   = 50;   /* 20 Hz */
static constexpr uint32_t BELT_TX_INTERVAL_MS       = 100;  /* 10 Hz */
static constexpr uint32_t ADVISORY_STALE_TIMEOUT_MS = 3000; /* 3 s   */
static constexpr uint32_t BELT_HEARTBEAT_TIMEOUT_MS = 2000; /* 2 s   */
static constexpr uint32_t WATCHDOG_TIMEOUT_MS       = 5000; /* 5 s   */

/* ── Sensor thresholds (millimetres) ────────────────────────────────── */
static constexpr uint16_t HAZARD_STOP_DISTANCE_MM      = 400;   /* < 0.4 m → emergency stop */
static constexpr uint16_t HAZARD_NEAR_DISTANCE_MM      = 1200;  /* < 1.2 m → strong alert   */
static constexpr uint16_t HAZARD_WARN_DISTANCE_MM      = 2500;  /* < 2.5 m → mild alert      */
static constexpr uint16_t DROP_OFF_THRESHOLD_MM         = 300;   /* downward ToF: > 0.3 m = drop */
static constexpr uint16_t LIDAR_MAX_RANGE_MM            = 12000; /* TFmini-S max range        */
static constexpr uint16_t TOF_SENSOR_INVALID            = 0xFFFF;

/* ── Belt motor layout ──────────────────────────────────────────────── */
static constexpr uint8_t  BELT_MOTOR_COUNT     = 8;
static constexpr uint8_t  BELT_PACKET_HEADER   = 0xCA;
static constexpr uint8_t  BELT_PACKET_LENGTH   = 12;

/* ── Quadrant mapping ───────────────────────────────────────────────── */
enum Quadrant : uint8_t {
    QUAD_FRONT = 0,
    QUAD_RIGHT = 1,
    QUAD_REAR  = 2,
    QUAD_LEFT  = 3,
    QUAD_COUNT = 4
};

/* ── Hazard severity levels ─────────────────────────────────────────── */
enum class HazardLevel : uint8_t {
    NONE    = 0,
    WARN    = 1,   /* object detected at moderate range */
    NEAR    = 2,   /* object close – strong vibration   */
    STOP    = 3,   /* imminent collision – emergency     */
    DROPOFF = 4    /* downward sensor detected a drop    */
};

/* ── Advisory message types ─────────────────────────────────────────── */
enum class AdvisoryType : uint8_t {
    NAV_HINT     = 0,
    SCENE_LABEL  = 1,
    OCR_RESULT   = 2,
    ROUTE_UPDATE = 3
};

/* ── BLE UUIDs ──────────────────────────────────────────────────────── */
#define CANE_SERVICE_UUID        "a1b2c3d4-0001-1000-8000-00805f9b34fb"
#define HAPTIC_CHAR_UUID         "a1b2c3d4-0002-1000-8000-00805f9b34fb"
#define ADVISORY_SERVICE_UUID    "a1b2c3d4-0003-1000-8000-00805f9b34fb"
#define ADVISORY_CHAR_UUID       "a1b2c3d4-0004-1000-8000-00805f9b34fb"

/* ── Cellular modem (SIM7080G – LTE Cat-M1/NB-IoT) ─────────────────── */
/* Supports T-Mobile, AT&T, Verizon via multi-band nano-SIM.            */
/* NON-SAFETY: advisory channel only — cane works fully without cell.   */
static constexpr uint32_t CELL_STATUS_INTERVAL_MS   = 10000; /* 10 s  */
static constexpr uint32_t CELL_CMD_TIMEOUT_MS        = 3000; /* 3 s   */
static constexpr uint32_t CELL_RETRY_INTERVAL_MS     = 30000;/* 30 s  */
static constexpr uint32_t CELL_BAUD_RATE             = 115200;

/* UART2 pins to cellular modem */
static constexpr uint8_t  CELL_UART_RX_PIN = 44;
static constexpr uint8_t  CELL_UART_TX_PIN = 43;
static constexpr uint8_t  CELL_RST_PIN     = 38;  /* modem hardware reset */
static constexpr uint8_t  CELL_PWR_PIN     = 39;  /* modem power key      */

/* RynnBrain cloud advisory endpoint (non-safety) */
#define CELL_RYNNBRAIN_ENDPOINT  "https://api.rynnbrain.local/v1"
