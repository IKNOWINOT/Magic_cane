/*
 * Magic Cane – advisory link to RynnBrain companion (ESP32-S3 implementation)
 *
 * NON-SAFETY channel.  Receives JSON-encoded advisory hints over a
 * dedicated BLE GATT characteristic.  The cane never gates a safety
 * decision on data from this link.
 *
 * This file is excluded from host-side unit tests (guarded by UNIT_TEST).
 *
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#ifndef UNIT_TEST

#include "advisory_link.h"
#include "config.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <string.h>

static bool          s_connected    = false;
static AdvisoryHint  s_latest_hint  = {};
static bool          s_hint_ready   = false;

/* Minimal JSON key look-up – avoids pulling in a full JSON library.
 * Only handles flat objects with string/number values.  Good enough
 * for the advisory protocol; a production build may swap in ArduinoJson. */

static bool json_find_string(const char *json, const char *key, char *out, size_t out_len)
{
    const char *k = strstr(json, key);
    if (!k) return false;
    const char *colon = strchr(k, ':');
    if (!colon) return false;
    const char *q1 = strchr(colon, '"');
    if (!q1) return false;
    q1++;
    const char *q2 = strchr(q1, '"');
    if (!q2) return false;
    size_t len = (size_t)(q2 - q1);
    if (len >= out_len) len = out_len - 1;
    memcpy(out, q1, len);
    out[len] = '\0';
    return true;
}

static bool json_find_float(const char *json, const char *key, float *out)
{
    const char *k = strstr(json, key);
    if (!k) return false;
    const char *colon = strchr(k, ':');
    if (!colon) return false;
    *out = strtof(colon + 1, nullptr);
    return true;
}

static bool json_find_int(const char *json, const char *key, int16_t *out)
{
    float f;
    if (!json_find_float(json, key, &f)) return false;
    *out = (int16_t)f;
    return true;
}

/* ── BLE callbacks ──────────────────────────────────────────────────── */

class AdvisoryServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *) override   { s_connected = true;  }
    void onDisconnect(BLEServer *) override { s_connected = false; }
};

class AdvisoryCharCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *c) override {
        std::string val = c->getValue();
        if (val.empty()) return;

        const char *json = val.c_str();
        char type_str[32] = {};
        json_find_string(json, "type", type_str, sizeof(type_str));

        if (strcmp(type_str, "nav_hint") == 0)      s_latest_hint.type = AdvisoryType::NAV_HINT;
        else if (strcmp(type_str, "scene_label") == 0) s_latest_hint.type = AdvisoryType::SCENE_LABEL;
        else if (strcmp(type_str, "ocr_result") == 0)  s_latest_hint.type = AdvisoryType::OCR_RESULT;
        else if (strcmp(type_str, "route_update") == 0) s_latest_hint.type = AdvisoryType::ROUTE_UPDATE;

        json_find_int(json, "bearing", &s_latest_hint.bearing_deg);
        json_find_string(json, "label", s_latest_hint.label, sizeof(s_latest_hint.label));
        json_find_float(json, "confidence", &s_latest_hint.confidence);
        s_latest_hint.received_ms = millis();
        s_hint_ready = true;
    }
};

/* ── Public API ─────────────────────────────────────────────────────── */

void advisory_link_init()
{
    /* Assumes BLEDevice::init() has already been called by belt_link. */
    BLEServer  *server  = BLEDevice::createServer();
    server->setCallbacks(new AdvisoryServerCallbacks());

    BLEService *service = server->createService(ADVISORY_SERVICE_UUID);
    BLECharacteristic *chr = service->createCharacteristic(
        ADVISORY_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    chr->setCallbacks(new AdvisoryCharCallbacks());
    service->start();
}

void advisory_link_poll()
{
    /* BLE writes are handled asynchronously via the callback above.
       This function is a placeholder for future periodic housekeeping. */
}

bool advisory_link_connected()
{
    return s_connected;
}

bool advisory_link_fresh(uint32_t now_ms)
{
    if (!s_hint_ready) return false;
    return (now_ms - s_latest_hint.received_ms) < ADVISORY_STALE_TIMEOUT_MS;
}

bool advisory_get_latest(AdvisoryHint &hint)
{
    if (!s_hint_ready) return false;
    hint = s_latest_hint;
    return true;
}

#endif /* UNIT_TEST */
