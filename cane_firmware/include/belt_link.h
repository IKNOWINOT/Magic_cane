/*
 * Magic Cane – BLE link to haptic belt
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#pragma once

#include "haptic_mapper.h"
#include <stdbool.h>

#ifndef UNIT_TEST
void     belt_link_init();
void     belt_link_send(const BeltPacket &pkt);
bool     belt_link_connected();
uint32_t belt_link_last_ack_ms();
#endif
