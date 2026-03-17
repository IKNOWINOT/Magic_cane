/*
 * 360° Haptic Belt – motor driver
 * Controls 8 ERM/LRA vibration motors via PWM on ESP32-S3.
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#pragma once

#include "belt_config.h"
#include <stdint.h>

/* Motor intensity array – one byte per motor, 0 = off, 255 = max */
struct MotorState {
    uint8_t intensity[MOTOR_COUNT];
};

/*
 * Initialise all motor PWM channels.
 */
void motor_driver_init();

/*
 * Set all motor intensities from state array.
 * Call at MOTOR_UPDATE_INTERVAL_MS for smooth transitions.
 */
void motor_driver_set(const MotorState &state);

/*
 * Emergency: all motors to max for an attention-grabbing burst.
 */
void motor_driver_emergency_pulse();

/*
 * All motors off immediately.
 */
void motor_driver_stop();

/*
 * Power-on self-test: briefly vibrate each motor sequentially.
 * Returns true if all channels respond (basic check).
 */
bool motor_driver_self_test();
