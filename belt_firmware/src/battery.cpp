/*
 * 360° Haptic Belt – battery monitor implementation
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#include "battery.h"
#include "belt_config.h"

/* ── Pure function (available in UNIT_TEST builds) ──────────────────── */

uint8_t battery_percent(uint16_t voltage_mv)
{
    const uint16_t full_mv  = static_cast<uint16_t>(BATTERY_FULL_V  * 1000.0f);
    const uint16_t empty_mv = static_cast<uint16_t>(BATTERY_EMPTY_V * 1000.0f);

    if (voltage_mv >= full_mv)  return 100;
    if (voltage_mv <= empty_mv) return 0;

    uint32_t range = full_mv - empty_mv;
    uint32_t above = voltage_mv - empty_mv;
    return static_cast<uint8_t>((above * 100u) / range);
}

/* ── Hardware functions (ESP32 only) ────────────────────────────────── */

#ifndef UNIT_TEST

#include <Arduino.h>

static uint16_t s_last_voltage_mv = 0;

void battery_init()
{
    analogSetAttenuation(ADC_11db);
    analogReadResolution(12);
    Serial.println("[BAT] ADC initialised");
}

uint16_t battery_voltage_mv()
{
    uint32_t raw = analogReadMilliVolts(BATTERY_ADC_PIN);
    s_last_voltage_mv = static_cast<uint16_t>(raw * ADC_DIVIDER_RATIO);
    return s_last_voltage_mv;
}

bool battery_critical()
{
    return battery_percent(s_last_voltage_mv) < 10;
}

#endif /* UNIT_TEST */
