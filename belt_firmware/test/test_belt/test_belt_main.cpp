/*
 * 360° Haptic Belt – Native unit tests
 *
 * Tests the pure (non-hardware) functions:
 *   - belt_validate_packet()
 *   - belt_parse_packet()
 *   - battery_percent()
 *
 * Compile & run on host:
 *   g++ -std=c++17 -DUNIT_TEST -I ../include -o test_runner \
 *       test_belt_main.cpp ../src/ble_peripheral.cpp ../src/battery.cpp \
 *       && ./test_runner
 *
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#include "ble_peripheral.h"
#include "battery.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static int g_total = 0;
static int g_pass  = 0;
static int g_fail  = 0;

#define TEST(name)                                                          \
    do {                                                                    \
        g_total++;                                                          \
        printf("  ");                                                       \
    } while (0)

#define ASSERT(cond, name)                                                  \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("FAIL  %s   (%s:%d)\n", name, __FILE__, __LINE__);       \
            g_fail++;                                                       \
            return;                                                         \
        }                                                                   \
        printf("PASS  %s\n", name);                                         \
        g_pass++;                                                           \
    } while (0)

/* ── Helper: build a valid packet ───────────────────────────────────── */

static void build_packet(uint8_t *buf, uint8_t seq, const uint8_t motors[8],
                          uint8_t flags)
{
    buf[0] = 0xCA;
    buf[1] = seq;
    for (int i = 0; i < 8; i++) buf[2 + i] = motors[i];
    buf[10] = flags;
    uint8_t cs = 0;
    for (int i = 0; i < 11; i++) cs ^= buf[i];
    buf[11] = cs;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Packet Validation Tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_validate_good_packet()
{
    TEST("validate_good_packet");
    uint8_t motors[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    uint8_t buf[12];
    build_packet(buf, 1, motors, 0x00);
    ASSERT(belt_validate_packet(buf, 12), "validate_good_packet");
}

static void test_validate_bad_length()
{
    TEST("validate_bad_length");
    uint8_t buf[10] = {};
    ASSERT(!belt_validate_packet(buf, 10), "validate_bad_length");
}

static void test_validate_bad_header()
{
    TEST("validate_bad_header");
    uint8_t motors[8] = {};
    uint8_t buf[12];
    build_packet(buf, 0, motors, 0);
    buf[0] = 0xFF;  /* corrupt header */
    ASSERT(!belt_validate_packet(buf, 12), "validate_bad_header");
}

static void test_validate_bad_checksum()
{
    TEST("validate_bad_checksum");
    uint8_t motors[8] = {};
    uint8_t buf[12];
    build_packet(buf, 0, motors, 0);
    buf[11] ^= 0xFF;  /* corrupt checksum */
    ASSERT(!belt_validate_packet(buf, 12), "validate_bad_checksum");
}

static void test_validate_estop_flag()
{
    TEST("validate_estop_flag");
    uint8_t motors[8] = {255, 255, 255, 255, 255, 255, 255, 255};
    uint8_t buf[12];
    build_packet(buf, 42, motors, 0x01);
    ASSERT(belt_validate_packet(buf, 12), "validate_estop_flag");
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Packet Parse Tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_parse_seq()
{
    TEST("parse_seq");
    uint8_t motors[8] = {};
    uint8_t buf[12];
    build_packet(buf, 99, motors, 0);
    HapticCommand cmd = belt_parse_packet(buf, 1000);
    ASSERT(cmd.seq == 99, "parse_seq");
}

static void test_parse_motors()
{
    TEST("parse_motors");
    uint8_t motors[8] = {11, 22, 33, 44, 55, 66, 77, 88};
    uint8_t buf[12];
    build_packet(buf, 0, motors, 0);
    HapticCommand cmd = belt_parse_packet(buf, 500);
    bool ok = true;
    for (int i = 0; i < 8; i++) {
        if (cmd.motors[i] != motors[i]) ok = false;
    }
    ASSERT(ok, "parse_motors");
}

static void test_parse_estop_true()
{
    TEST("parse_estop_true");
    uint8_t motors[8] = {};
    uint8_t buf[12];
    build_packet(buf, 0, motors, 0x01);
    HapticCommand cmd = belt_parse_packet(buf, 0);
    ASSERT(cmd.emergency_stop == true, "parse_estop_true");
}

static void test_parse_estop_false()
{
    TEST("parse_estop_false");
    uint8_t motors[8] = {};
    uint8_t buf[12];
    build_packet(buf, 0, motors, 0x00);
    HapticCommand cmd = belt_parse_packet(buf, 0);
    ASSERT(cmd.emergency_stop == false, "parse_estop_false");
}

static void test_parse_timestamp()
{
    TEST("parse_timestamp");
    uint8_t motors[8] = {};
    uint8_t buf[12];
    build_packet(buf, 0, motors, 0);
    HapticCommand cmd = belt_parse_packet(buf, 12345);
    ASSERT(cmd.received_ms == 12345, "parse_timestamp");
}

static void test_parse_all_motors_max()
{
    TEST("parse_all_motors_max");
    uint8_t motors[8] = {255, 255, 255, 255, 255, 255, 255, 255};
    uint8_t buf[12];
    build_packet(buf, 200, motors, 0x01);
    HapticCommand cmd = belt_parse_packet(buf, 0);
    bool all_max = true;
    for (int i = 0; i < 8; i++) {
        if (cmd.motors[i] != 255) all_max = false;
    }
    ASSERT(all_max && cmd.emergency_stop, "parse_all_motors_max");
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Battery Percent Tests
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_battery_full()
{
    TEST("battery_full");
    ASSERT(battery_percent(4200) == 100, "battery_full");
}

static void test_battery_empty()
{
    TEST("battery_empty");
    ASSERT(battery_percent(3300) == 0, "battery_empty");
}

static void test_battery_half()
{
    TEST("battery_half");
    /* midpoint = 3750 → ~50% */
    uint8_t pct = battery_percent(3750);
    ASSERT(pct >= 45 && pct <= 55, "battery_half");
}

static void test_battery_over_full()
{
    TEST("battery_over_full");
    ASSERT(battery_percent(4500) == 100, "battery_over_full");
}

static void test_battery_under_empty()
{
    TEST("battery_under_empty");
    ASSERT(battery_percent(2000) == 0, "battery_under_empty");
}

static void test_battery_critical_threshold()
{
    TEST("battery_critical_threshold");
    /* 10% of 900mV range = 90mV → 3390mV → should be ~10% */
    uint8_t pct = battery_percent(3390);
    ASSERT(pct == 10, "battery_critical_threshold");
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Round-trip: build → validate → parse
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_roundtrip()
{
    TEST("roundtrip");
    uint8_t motors[8] = {0, 80, 180, 255, 0, 80, 180, 255};
    uint8_t buf[12];
    build_packet(buf, 77, motors, 0x01);

    ASSERT(belt_validate_packet(buf, 12), "roundtrip (validate)");
    /* need a second test macro call, just do inline */
    HapticCommand cmd = belt_parse_packet(buf, 9999);
    bool ok = (cmd.seq == 77 && cmd.emergency_stop && cmd.received_ms == 9999);
    for (int i = 0; i < 8; i++) {
        if (cmd.motors[i] != motors[i]) ok = false;
    }
    /* report via printf since we already used the macro */
    if (ok) {
        printf("  PASS  roundtrip_parse\n");
        g_total++; g_pass++;
    } else {
        printf("  FAIL  roundtrip_parse\n");
        g_total++; g_fail++;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Main
 * ═══════════════════════════════════════════════════════════════════════ */

int main()
{
    printf("\n=== 360° Haptic Belt – Native Tests ===\n\n");

    printf("--- Packet Validation ---\n");
    test_validate_good_packet();
    test_validate_bad_length();
    test_validate_bad_header();
    test_validate_bad_checksum();
    test_validate_estop_flag();

    printf("\n--- Packet Parsing ---\n");
    test_parse_seq();
    test_parse_motors();
    test_parse_estop_true();
    test_parse_estop_false();
    test_parse_timestamp();
    test_parse_all_motors_max();

    printf("\n--- Battery Percentage ---\n");
    test_battery_full();
    test_battery_empty();
    test_battery_half();
    test_battery_over_full();
    test_battery_under_empty();
    test_battery_critical_threshold();

    printf("\n--- Round-trip ---\n");
    test_roundtrip();

    printf("\n========================================\n");
    printf("  Tests run:    %d\n", g_total);
    printf("  Tests passed: %d\n", g_pass);
    printf("  Tests failed: %d\n", g_fail);
    printf("========================================\n");

    return g_fail > 0 ? 1 : 0;
}
