/*
 * 360° Haptic Belt – shared configuration & constants
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#pragma once

#include <stdint.h>

/* ── Timing ─────────────────────────────────────────────────────────── */
static constexpr uint32_t MOTOR_UPDATE_INTERVAL_MS   = 20;   /* 50 Hz PWM refresh  */
static constexpr uint32_t HEARTBEAT_INTERVAL_MS      = 500;  /* heartbeat to cane   */
static constexpr uint32_t CANE_TIMEOUT_MS            = 2000; /* cane lost if silent */
static constexpr uint32_t WATCHDOG_TIMEOUT_MS        = 5000;
static constexpr uint32_t IMU_POLL_INTERVAL_MS       = 50;   /* 20 Hz */
static constexpr uint32_t BATTERY_CHECK_INTERVAL_MS  = 10000;/* 0.1 Hz */

/* ── Motor configuration ────────────────────────────────────────────── */
static constexpr uint8_t  MOTOR_COUNT       = 8;
static constexpr uint8_t  PWM_RESOLUTION    = 8;     /* 8-bit: 0–255  */
static constexpr uint32_t PWM_FREQUENCY     = 20000; /* 20 kHz – silent */

/* Motor GPIO pins (ESP32-S3) – adjust per PCB layout */
static constexpr uint8_t MOTOR_PINS[MOTOR_COUNT] = {
    4, 5, 6, 7, 15, 16, 17, 18
};

/* Motor layout (overhead, user facing front):
 *        7   0   1
 *      6   user   2
 *        5   4   3
 */

/* ── Packet protocol (received from cane) ───────────────────────────── */
static constexpr uint8_t  PACKET_HEADER  = 0xCA;
static constexpr uint8_t  PACKET_LENGTH  = 12;

/* ── BLE UUIDs (must match cane firmware) ───────────────────────────── */
#define CANE_SERVICE_UUID    "a1b2c3d4-0001-1000-8000-00805f9b34fb"
#define HAPTIC_CHAR_UUID     "a1b2c3d4-0002-1000-8000-00805f9b34fb"

/* ── Battery ────────────────────────────────────────────────────────── */
static constexpr uint8_t  BATTERY_ADC_PIN   = 1;
static constexpr float    BATTERY_FULL_V    = 4.2f;
static constexpr float    BATTERY_EMPTY_V   = 3.3f;
static constexpr float    ADC_DIVIDER_RATIO = 2.0f; /* voltage divider */

/* ── Status LED ─────────────────────────────────────────────────────── */
static constexpr uint8_t  LED_PIN = 2;

/* ── IMU ────────────────────────────────────────────────────────────── */
static constexpr uint8_t  IMU_I2C_SDA = 8;
static constexpr uint8_t  IMU_I2C_SCL = 9;
