/*
 * Magic Cane – cellular modem driver
 *
 * Provides LTE Cat-M1/NB-IoT connectivity via a SIM7080G (or compatible)
 * module over UART.  Supports T-Mobile, AT&T, and Verizon via standard
 * multi-band SIM.  This channel is NON-SAFETY / ADVISORY ONLY — the cane
 * operates with full hazard-detection capability when cellular is offline.
 *
 * The modem enables the cane to reach RynnBrain cloud services for
 * advisory scene understanding, OCR, and route context without requiring
 * a paired phone.
 *
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#pragma once

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

/* ── Modem status ───────────────────────────────────────────────────── */
enum class CellState : uint8_t {
    OFF          = 0,
    INITIALISING = 1,
    SEARCHING    = 2,
    REGISTERED   = 3,   /* registered on network (CS/PS) */
    CONNECTED    = 4,   /* PDP/data context active       */
    ERROR        = 5
};

/* ── Network info (parsed from AT responses) ────────────────────────── */
struct CellStatus {
    CellState state;
    int8_t    rssi_dbm;         /* signal strength, –113…–51 dBm */
    uint8_t   signal_percent;   /* 0–100 mapped from RSSI        */
    char      operator_name[24];
    char      network_type[8];  /* "LTE-M", "NB-IoT", etc.       */
    bool      sim_present;
    bool      data_ready;       /* PDP context is active          */
};

/*
 * Parse an AT+CSQ response into RSSI dBm and signal percentage.
 * Pure function — safe for unit testing on the host.
 *
 * Input: "+CSQ: 18,0" → rssi_dbm = –77, signal_percent = 58
 * Returns false if the string cannot be parsed.
 */
bool cell_parse_csq(const char *response, int8_t *rssi_dbm, uint8_t *signal_percent);

/*
 * Parse an AT+COPS? response to extract the operator name.
 * Pure function — safe for unit testing.
 *
 * Input: "+COPS: 0,0,\"T-Mobile\",7" → operator_name = "T-Mobile"
 * Returns false if the string cannot be parsed.
 */
bool cell_parse_cops(const char *response, char *operator_name, uint8_t max_len);

/*
 * Parse an AT+CREG? or AT+CEREG? response to determine registration status.
 * Pure function — safe for unit testing.
 *
 * Input: "+CEREG: 0,1" → registered = true  (1 or 5 = registered)
 * Returns false if the string cannot be parsed.
 */
bool cell_parse_registration(const char *response, bool *registered);

#ifndef UNIT_TEST

/* ── Hardware API (ESP32-S3 only) ───────────────────────────────────── */

/*
 * Initialise the cellular modem:
 *   - power on via RST pin
 *   - configure UART
 *   - run basic AT handshake
 *   - set APN for multi-carrier SIM
 */
void cell_modem_init();

/*
 * Poll the modem for status updates.  Call from main loop at
 * CELL_STATUS_INTERVAL_MS.  Non-blocking; uses a state machine.
 */
void cell_modem_poll();

/*
 * Get the latest modem status snapshot.
 */
CellStatus cell_modem_status();

/*
 * Send an advisory JSON payload to the RynnBrain cloud endpoint
 * via HTTP POST over the cellular data connection.
 * Returns true if the send was accepted (response may be async).
 */
bool cell_send_advisory_request(const char *json_payload, uint16_t len);

/*
 * Check if a RynnBrain advisory response is available.
 * If true, copies the response JSON into `buf` (up to max_len).
 */
bool cell_recv_advisory_response(char *buf, uint16_t max_len);

/*
 * Returns true if the modem has an active data connection.
 */
bool cell_modem_data_ready();

#endif /* UNIT_TEST */
