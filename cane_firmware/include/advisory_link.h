/*
 * Magic Cane – advisory link to RynnBrain companion
 * This channel is NON-SAFETY. It carries semantic hints only.
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#pragma once

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

/* Parsed advisory hint from the companion */
struct AdvisoryHint {
    AdvisoryType type;
    int16_t      bearing_deg;    /* -180 to +180, 0 = straight ahead */
    char         label[64];
    float        confidence;     /* 0.0 – 1.0 */
    uint32_t     received_ms;    /* timestamp when received */
};

#ifndef UNIT_TEST
void  advisory_link_init();
void  advisory_link_poll();
bool  advisory_link_connected();
bool  advisory_link_fresh(uint32_t now_ms);
bool  advisory_get_latest(AdvisoryHint &hint);
#endif
