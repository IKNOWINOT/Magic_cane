/*
 * 360° Haptic Belt – BLE peripheral
 * Connects to the Magic Cane as a GATT client and receives haptic packets.
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#pragma once

#include "belt_config.h"
#include "motor_driver.h"
#include <stdint.h>
#include <stdbool.h>

/* Parsed haptic command from the cane */
struct HapticCommand {
    uint8_t  seq;
    uint8_t  motors[MOTOR_COUNT];
    bool     emergency_stop;
    uint32_t received_ms;
};

/*
 * Initialise BLE and start scanning for the MagicCane service.
 */
void ble_peripheral_init();

/*
 * Poll for new data from BLE. Call from main loop.
 * Returns true if a new valid haptic command was received.
 */
bool ble_peripheral_poll(HapticCommand &cmd);

/*
 * Check if connected to the cane.
 */
bool ble_peripheral_connected();

/*
 * Send heartbeat/ACK byte back to cane.
 */
void ble_peripheral_send_ack();

/*
 * Get millisecond timestamp of last valid packet from cane.
 */
uint32_t ble_peripheral_last_rx_ms();

/*
 * Validate a raw packet buffer (header + checksum).
 * Pure function, safe for unit testing.
 */
bool belt_validate_packet(const uint8_t *buf, uint8_t len);

/*
 * Parse a validated packet buffer into a HapticCommand.
 * Pure function, safe for unit testing.
 */
HapticCommand belt_parse_packet(const uint8_t *buf, uint32_t now_ms);
