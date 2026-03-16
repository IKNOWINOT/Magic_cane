/*
 * Magic Cane – main application (ESP32-S3)
 *
 * Runs the deterministic safety loop:
 *   1. Poll sensors
 *   2. Evaluate hazards (local, deterministic)
 *   3. Build haptic packet and send to belt
 *   4. Optionally overlay advisory hints (non-safety)
 *   5. Pet the watchdog
 *
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#ifndef UNIT_TEST

#include <Arduino.h>
#include "config.h"
#include "sensor_manager.h"
#include "hazard_engine.h"
#include "haptic_mapper.h"
#include "belt_link.h"
#include "advisory_link.h"

#include <esp_task_wdt.h>

/* ── State ──────────────────────────────────────────────────────────── */
static SensorSnapshot s_snap   = {};
static HazardResult   s_hazard = {};
static uint8_t        s_seq    = 0;
static uint32_t       s_last_sensor_ms = 0;
static uint32_t       s_last_belt_ms   = 0;

/* ── Status LED (optional, active-low on GPIO 2) ────────────────────── */
static constexpr int LED_PIN = 2;

static void status_led_update()
{
    if (s_hazard.emergency_stop) {
        /* Fast blink on emergency */
        digitalWrite(LED_PIN, (millis() / 100) & 1);
    } else if (!belt_link_connected()) {
        /* Slow blink when belt disconnected */
        digitalWrite(LED_PIN, (millis() / 500) & 1);
    } else {
        digitalWrite(LED_PIN, LOW);     /* solid on */
    }
}

/* ── Arduino entry points ───────────────────────────────────────────── */

void setup()
{
    Serial.begin(115200);
    Serial.println("\n=== Magic Cane v0.1 ===");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    sensors_init();
    belt_link_init();
    advisory_link_init();

    /* Enable hardware watchdog – resets MCU if loop stalls */
    esp_task_wdt_init(WATCHDOG_TIMEOUT_MS / 1000, true);
    esp_task_wdt_add(nullptr);

    Serial.println("[MAIN] setup complete – entering safety loop");
}

void loop()
{
    uint32_t now = millis();

    /* ── 1. Poll sensors at SENSOR_POLL_INTERVAL_MS ─────────────────── */
    if (now - s_last_sensor_ms >= SENSOR_POLL_INTERVAL_MS) {
        s_last_sensor_ms = now;
        sensors_poll(s_snap);

        /* ── 2. Deterministic hazard evaluation ─────────────────────── */
        s_hazard = hazard_evaluate(s_snap);
    }

    /* ── 3. Send belt packet at BELT_TX_INTERVAL_MS ─────────────────── */
    if (now - s_last_belt_ms >= BELT_TX_INTERVAL_MS) {
        s_last_belt_ms = now;

        BeltPacket pkt = haptic_build_packet(s_hazard, s_seq++);
        belt_link_send(pkt);
    }

    /* ── 4. Check advisory channel (non-safety) ─────────────────────── */
    advisory_link_poll();
    if (advisory_link_connected() && advisory_link_fresh(now)) {
        AdvisoryHint hint;
        if (advisory_get_latest(hint)) {
            /* Log to serial for debugging; in production this would
               drive a bone-conduction speaker or secondary haptic cue. */
            Serial.printf("[ADV] %s bearing=%d conf=%.2f\n",
                          hint.label, hint.bearing_deg, hint.confidence);
        }
    }

    /* ── 5. Graceful degradation checks ─────────────────────────────── */
    if (!belt_link_connected()) {
        /* Belt lost – could activate on-handle vibration motor or buzzer.
           For now we log and let the status LED indicate the issue. */
    }

    /* ── 6. Status LED & watchdog ───────────────────────────────────── */
    status_led_update();
    esp_task_wdt_reset();
}

#endif /* UNIT_TEST */
