/*
 * Magic Cane – BLE link to haptic belt (ESP32-S3 implementation)
 *
 * Uses BLE GATT server with a custom service to send haptic packets
 * to the belt and receive heartbeat/ACK responses.
 *
 * This file is excluded from host-side unit tests (guarded by UNIT_TEST).
 *
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#ifndef UNIT_TEST

#include "belt_link.h"
#include "config.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

static BLECharacteristic *s_haptic_char = nullptr;
static bool               s_connected   = false;
static uint32_t           s_last_ack_ms = 0;

/* ── BLE server callbacks ───────────────────────────────────────────── */

class BeltServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer * /*srv*/) override   { s_connected = true;  }
    void onDisconnect(BLEServer * /*srv*/) override { s_connected = false; }
};

class HapticCharCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic * /*c*/) override {
        /* Belt sends a 1-byte ACK (0x01) when it processes a packet */
        s_last_ack_ms = millis();
    }
};

/* ── Public API ─────────────────────────────────────────────────────── */

void belt_link_init()
{
    BLEDevice::init("MagicCane");
    BLEServer  *server  = BLEDevice::createServer();
    server->setCallbacks(new BeltServerCallbacks());

    BLEService *service = server->createService(CANE_SERVICE_UUID);

    s_haptic_char = service->createCharacteristic(
        HAPTIC_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ   |
        BLECharacteristic::PROPERTY_WRITE  |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    s_haptic_char->addDescriptor(new BLE2902());
    s_haptic_char->setCallbacks(new HapticCharCallbacks());

    service->start();

    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(CANE_SERVICE_UUID);
    adv->setScanResponse(true);
    BLEDevice::startAdvertising();

    Serial.println("[BELT_LINK] BLE advertising started");
}

void belt_link_send(const BeltPacket &pkt)
{
    if (!s_connected || !s_haptic_char) return;

    s_haptic_char->setValue(
        const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(&pkt)),
        BELT_PACKET_LENGTH
    );
    s_haptic_char->notify();
}

bool belt_link_connected()
{
    return s_connected;
}

uint32_t belt_link_last_ack_ms()
{
    return s_last_ack_ms;
}

#endif /* UNIT_TEST */
