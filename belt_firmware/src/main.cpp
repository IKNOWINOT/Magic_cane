/*
 * 360° Haptic Belt – main application (ESP32-S3)
 *
 * Receives haptic commands from the Magic Cane over BLE and drives
 * 8 vibration motors to provide 360° directional feedback.
 *
 * Safety behaviour:
 *   - If cane connection is lost, motors ramp down gracefully.
 *   - Emergency-stop flag triggers full-body attention pulse.
 *   - Watchdog ensures the loop never stalls.
 *
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#ifndef UNIT_TEST

#include <Arduino.h>
#include <esp_task_wdt.h>

#include "belt_config.h"
#include "motor_driver.h"
#include "ble_peripheral.h"
#include "battery.h"

/* ── State ──────────────────────────────────────────────────────────── */
static MotorState    s_motors     = {};
static HapticCommand s_last_cmd   = {};
static uint32_t      s_last_motor_ms   = 0;
static uint32_t      s_last_battery_ms = 0;
static bool          s_cane_lost       = false;

/* ── Ramp-down on connection loss ───────────────────────────────────── */
static void graceful_ramp_down()
{
    for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
        if (s_motors.intensity[i] > 4) {
            s_motors.intensity[i] -= 4;     /* smooth decay */
        } else {
            s_motors.intensity[i] = 0;
        }
    }
    motor_driver_set(s_motors);
}

/* ── Status LED ─────────────────────────────────────────────────────── */
static void status_led_update(uint32_t now)
{
    if (s_cane_lost) {
        /* Fast blink – connection lost */
        digitalWrite(LED_PIN, (now / 150) & 1);
    } else if (s_last_cmd.emergency_stop) {
        /* Strobe – emergency */
        digitalWrite(LED_PIN, (now / 75) & 1);
    } else if (!ble_peripheral_connected()) {
        /* Slow blink – scanning */
        digitalWrite(LED_PIN, (now / 500) & 1);
    } else {
        /* Solid – normal operation */
        digitalWrite(LED_PIN, LOW);
    }
}

/* ── Arduino entry points ───────────────────────────────────────────── */

void setup()
{
    Serial.begin(115200);
    Serial.println("\n=== Magic Belt v0.1 ===");

    pinMode(LED_PIN, OUTPUT);

    motor_driver_init();
    battery_init();
    ble_peripheral_init();

    /* Power-on self-test */
    motor_driver_self_test();

    /* Hardware watchdog */
    esp_task_wdt_init(WATCHDOG_TIMEOUT_MS / 1000, true);
    esp_task_wdt_add(nullptr);

    Serial.println("[MAIN] setup complete – awaiting cane connection");
}

void loop()
{
    uint32_t now = millis();

    /* ── 1. Receive haptic commands from cane ───────────────────────── */
    HapticCommand cmd;
    if (ble_peripheral_poll(cmd)) {
        s_last_cmd = cmd;
        s_cane_lost = false;

        /* Apply motor intensities */
        for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
            s_motors.intensity[i] = cmd.motors[i];
        }

        /* Emergency-stop override */
        if (cmd.emergency_stop) {
            motor_driver_emergency_pulse();
        } else {
            motor_driver_set(s_motors);
        }

        /* Send ACK to cane */
        ble_peripheral_send_ack();
    }

    /* ── 2. Check for cane timeout ──────────────────────────────────── */
    if (ble_peripheral_connected()) {
        uint32_t last_rx = ble_peripheral_last_rx_ms();
        if (last_rx > 0 && (now - last_rx) > CANE_TIMEOUT_MS) {
            if (!s_cane_lost) {
                Serial.println("[MAIN] WARNING: cane connection timeout");
                s_cane_lost = true;
            }
            graceful_ramp_down();
        }
    } else {
        if (!s_cane_lost && s_last_cmd.received_ms > 0) {
            Serial.println("[MAIN] WARNING: cane disconnected");
            s_cane_lost = true;
        }
        graceful_ramp_down();
    }

    /* ── 3. Battery monitoring ──────────────────────────────────────── */
    if (now - s_last_battery_ms >= BATTERY_CHECK_INTERVAL_MS) {
        s_last_battery_ms = now;
        uint16_t mv = battery_voltage_mv();
        uint8_t  pct = battery_percent(mv);
        Serial.printf("[BAT] %u mV (%u%%)\n", mv, pct);

        if (battery_critical()) {
            /* Low battery: pulse motor 0 briefly as haptic warning */
            MotorState warn = {};
            warn.intensity[0] = 128;
            motor_driver_set(warn);
            delay(100);
            motor_driver_set(s_motors);
        }
    }

    /* ── 4. Status LED & watchdog ───────────────────────────────────── */
    status_led_update(now);
    esp_task_wdt_reset();

    /* Yield to BLE stack */
    delay(1);
}

#endif /* UNIT_TEST */
