/*
 * Magic Cane – haptic mapper
 * Translates HazardResult into an 8-motor intensity array for the belt.
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#pragma once

#include "config.h"
#include "hazard_engine.h"
#include <stdint.h>

/* Belt packet sent over BLE at BELT_TX_INTERVAL_MS */
struct BeltPacket {
    uint8_t header;                         /* BELT_PACKET_HEADER */
    uint8_t seq;                            /* rolling counter    */
    uint8_t motors[BELT_MOTOR_COUNT];       /* 0–255 intensity    */
    uint8_t flags;                          /* bit 0 = e-stop     */
    uint8_t checksum;                       /* XOR of bytes 0–10  */
};

/*
 * Build a belt packet from the current hazard result.
 * Pure function, safe to unit-test on the host.
 *
 * Motor layout (looking down at belt, front = 0):
 *   Motor 0 – front-centre
 *   Motor 1 – front-right
 *   Motor 2 – right
 *   Motor 3 – rear-right
 *   Motor 4 – rear
 *   Motor 5 – rear-left
 *   Motor 6 – left
 *   Motor 7 – front-left
 */
BeltPacket haptic_build_packet(const HazardResult &haz, uint8_t seq);

/*
 * Compute the checksum for a belt packet (XOR of bytes 0 through 10).
 */
uint8_t belt_packet_checksum(const BeltPacket &pkt);

/*
 * Validate an incoming belt packet (header + checksum).
 */
bool belt_packet_valid(const uint8_t *buf, uint8_t len);
