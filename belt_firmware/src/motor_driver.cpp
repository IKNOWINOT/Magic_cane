/*
 * 360° Haptic Belt – motor driver implementation
 * Uses ESP32-S3 LEDC PWM peripheral to drive 8 vibration motors.
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#ifndef UNIT_TEST

#include "motor_driver.h"
#include <Arduino.h>

void motor_driver_init()
{
    for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
        ledcAttach(MOTOR_PINS[i], PWM_FREQUENCY, PWM_RESOLUTION);
        ledcWrite(MOTOR_PINS[i], 0);
    }
    Serial.println("[MOTOR] init complete – 8 channels at 20 kHz");
}

void motor_driver_set(const MotorState &state)
{
    for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
        ledcWrite(MOTOR_PINS[i], state.intensity[i]);
    }
}

void motor_driver_emergency_pulse()
{
    /* Full intensity burst on all motors */
    for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
        ledcWrite(MOTOR_PINS[i], 255);
    }
}

void motor_driver_stop()
{
    for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
        ledcWrite(MOTOR_PINS[i], 0);
    }
}

bool motor_driver_self_test()
{
    Serial.println("[MOTOR] self-test: pulsing each motor...");
    for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
        ledcWrite(MOTOR_PINS[i], 128);
        delay(100);
        ledcWrite(MOTOR_PINS[i], 0);
        delay(50);
    }
    Serial.println("[MOTOR] self-test complete");
    return true;    /* Basic check – production would verify back-EMF */
}

#endif /* UNIT_TEST */
