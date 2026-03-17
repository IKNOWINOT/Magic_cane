/*
 * 360° Haptic Belt – battery monitor
 * Reads battery voltage via ADC and computes percentage.
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#pragma once

#include <stdint.h>

/*
 * Initialise ADC for battery monitoring.
 */
void battery_init();

/*
 * Read battery voltage in millivolts.
 */
uint16_t battery_voltage_mv();

/*
 * Compute battery percentage (0–100).
 * Pure function, safe for unit testing.
 */
uint8_t battery_percent(uint16_t voltage_mv);

/*
 * Returns true if battery is critically low (< 10%).
 */
bool battery_critical();
