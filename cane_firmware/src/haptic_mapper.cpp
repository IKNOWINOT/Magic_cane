/*
 * Magic Cane – haptic mapper (implementation)
 *
 * Converts a HazardResult into an 8-motor belt packet.
 * Pure function, no hardware dependencies – unit-testable on host.
 *
 * Motor arrangement (overhead view, user facing "front"):
 *
 *        7   0   1
 *      6    user    2
 *        5   4   3
 *
 * Quadrant → motor mapping:
 *   FRONT (quad 0) → motors 7, 0, 1
 *   RIGHT (quad 1) → motors 1, 2, 3
 *   REAR  (quad 2) → motors 3, 4, 5
 *   LEFT  (quad 3) → motors 5, 6, 7
 *
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#include "haptic_mapper.h"
#include <string.h>

/* Intensity for each hazard level */
static constexpr uint8_t INTENSITY_NONE = 0;
static constexpr uint8_t INTENSITY_WARN = 80;
static constexpr uint8_t INTENSITY_NEAR = 180;
static constexpr uint8_t INTENSITY_STOP = 255;

/* Which motors each quadrant contributes to (3 motors per quadrant) */
static constexpr uint8_t QUAD_MOTORS[QUAD_COUNT][3] = {
    {7, 0, 1},  /* FRONT */
    {1, 2, 3},  /* RIGHT */
    {3, 4, 5},  /* REAR  */
    {5, 6, 7},  /* LEFT  */
};

static uint8_t level_to_intensity(HazardLevel lv)
{
    switch (lv) {
        case HazardLevel::STOP:    return INTENSITY_STOP;
        case HazardLevel::NEAR:    return INTENSITY_NEAR;
        case HazardLevel::WARN:    return INTENSITY_WARN;
        case HazardLevel::DROPOFF: return INTENSITY_STOP;
        default:                   return INTENSITY_NONE;
    }
}

/* Take the maximum of two uint8_t values */
static inline uint8_t max_u8(uint8_t a, uint8_t b) { return a > b ? a : b; }

/* ── Public API ─────────────────────────────────────────────────────── */

BeltPacket haptic_build_packet(const HazardResult &haz, uint8_t seq)
{
    BeltPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.header = BELT_PACKET_HEADER;
    pkt.seq    = seq;

    /* Map each quadrant's hazard level to its three belt motors.
     * When quadrants share a motor (e.g. motor 1 is shared by FRONT and
     * RIGHT) we take the maximum intensity so the strongest warning wins. */
    for (uint8_t q = 0; q < QUAD_COUNT; ++q) {
        uint8_t intensity = level_to_intensity(haz.quadrants[q].level);
        for (uint8_t m = 0; m < 3; ++m) {
            uint8_t idx = QUAD_MOTORS[q][m];
            pkt.motors[idx] = max_u8(pkt.motors[idx], intensity);
        }
    }

    /* Forward LiDAR overrides front-centre motor */
    uint8_t fwd_intensity = level_to_intensity(haz.forward_level);
    pkt.motors[0] = max_u8(pkt.motors[0], fwd_intensity);

    /* Drop-off drives all motors at full intensity */
    if (haz.drop_detected) {
        for (uint8_t i = 0; i < BELT_MOTOR_COUNT; ++i) {
            pkt.motors[i] = INTENSITY_STOP;
        }
    }

    /* Emergency-stop flag */
    pkt.flags = haz.emergency_stop ? 0x01 : 0x00;

    /* Checksum: XOR of bytes 0..10 */
    pkt.checksum = belt_packet_checksum(pkt);

    return pkt;
}

uint8_t belt_packet_checksum(const BeltPacket &pkt)
{
    const uint8_t *raw = reinterpret_cast<const uint8_t *>(&pkt);
    uint8_t cs = 0;
    for (uint8_t i = 0; i < BELT_PACKET_LENGTH - 1; ++i) {
        cs ^= raw[i];
    }
    return cs;
}

bool belt_packet_valid(const uint8_t *buf, uint8_t len)
{
    if (len != BELT_PACKET_LENGTH) return false;
    if (buf[0] != BELT_PACKET_HEADER) return false;

    uint8_t cs = 0;
    for (uint8_t i = 0; i < BELT_PACKET_LENGTH - 1; ++i) {
        cs ^= buf[i];
    }
    return cs == buf[BELT_PACKET_LENGTH - 1];
}
