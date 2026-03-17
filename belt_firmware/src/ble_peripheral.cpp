/*
 * 360° Haptic Belt – BLE peripheral implementation
 * Connects as a GATT client to the MagicCane BLE server and receives
 * haptic command packets on the HAPTIC_CHAR_UUID characteristic.
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#include "ble_peripheral.h"
#include <string.h>

/* ── Pure functions (available in UNIT_TEST builds too) ──────────────── */

bool belt_validate_packet(const uint8_t *buf, uint8_t len)
{
    if (len != PACKET_LENGTH) return false;
    if (buf[0] != PACKET_HEADER) return false;

    uint8_t cs = 0;
    for (uint8_t i = 0; i < PACKET_LENGTH - 1; ++i) {
        cs ^= buf[i];
    }
    return cs == buf[PACKET_LENGTH - 1];
}

HapticCommand belt_parse_packet(const uint8_t *buf, uint32_t now_ms)
{
    HapticCommand cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.seq = buf[1];
    for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
        cmd.motors[i] = buf[2 + i];
    }
    cmd.emergency_stop = (buf[10] & 0x01) != 0;
    cmd.received_ms = now_ms;

    return cmd;
}

/* ── Hardware BLE code (ESP32 only) ─────────────────────────────────── */

#ifndef UNIT_TEST

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

static BLERemoteCharacteristic *s_haptic_char = nullptr;
static BLEClient              *s_client       = nullptr;
static bool                    s_connected    = false;
static uint32_t                s_last_rx_ms   = 0;

/* Circular buffer for received commands */
static constexpr uint8_t CMD_BUF_SIZE = 4;
static HapticCommand s_cmd_buf[CMD_BUF_SIZE];
static volatile uint8_t s_cmd_write = 0;
static volatile uint8_t s_cmd_read  = 0;

/* ── Notification callback ──────────────────────────────────────────── */

static void notify_callback(BLERemoteCharacteristic * /*chr*/,
                             uint8_t *data, size_t length, bool /*isNotify*/)
{
    if (!belt_validate_packet(data, static_cast<uint8_t>(length))) return;

    HapticCommand cmd = belt_parse_packet(data, millis());
    s_last_rx_ms = cmd.received_ms;

    uint8_t next = (s_cmd_write + 1) % CMD_BUF_SIZE;
    if (next != s_cmd_read) {      /* drop if buffer full */
        s_cmd_buf[s_cmd_write] = cmd;
        s_cmd_write = next;
    }
}

/* ── Scan callback ──────────────────────────────────────────────────── */

class CaneAdvertisedCallback : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice dev) override {
        if (dev.haveServiceUUID() && dev.isAdvertisingService(BLEUUID(CANE_SERVICE_UUID))) {
            BLEDevice::getScan()->stop();

            s_client = BLEDevice::createClient();
            if (!s_client->connect(&dev)) {
                Serial.println("[BLE] connection failed");
                return;
            }

            BLERemoteService *svc = s_client->getService(CANE_SERVICE_UUID);
            if (!svc) {
                Serial.println("[BLE] service not found");
                s_client->disconnect();
                return;
            }

            s_haptic_char = svc->getCharacteristic(HAPTIC_CHAR_UUID);
            if (!s_haptic_char) {
                Serial.println("[BLE] characteristic not found");
                s_client->disconnect();
                return;
            }

            if (s_haptic_char->canNotify()) {
                s_haptic_char->registerForNotify(notify_callback);
            }

            s_connected = true;
            Serial.println("[BLE] connected to MagicCane");
        }
    }
};

/* ── Public API ─────────────────────────────────────────────────────── */

void ble_peripheral_init()
{
    BLEDevice::init("MagicBelt");

    BLEScan *scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new CaneAdvertisedCallback());
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
    scan->start(0, false);      /* scan indefinitely in background */

    Serial.println("[BLE] scanning for MagicCane...");
}

bool ble_peripheral_poll(HapticCommand &cmd)
{
    /* Re-scan if disconnected */
    if (!s_connected && s_client) {
        if (!s_client->isConnected()) {
            s_connected = false;
            BLEDevice::getScan()->start(0, false);
        }
    }

    if (s_cmd_read == s_cmd_write) return false;    /* nothing new */

    cmd = s_cmd_buf[s_cmd_read];
    s_cmd_read = (s_cmd_read + 1) % CMD_BUF_SIZE;
    return true;
}

bool ble_peripheral_connected()
{
    return s_connected;
}

void ble_peripheral_send_ack()
{
    if (!s_connected || !s_haptic_char) return;
    uint8_t ack = 0x01;
    s_haptic_char->writeValue(&ack, 1);
}

uint32_t ble_peripheral_last_rx_ms()
{
    return s_last_rx_ms;
}

#endif /* UNIT_TEST */
