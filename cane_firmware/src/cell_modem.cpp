/*
 * Magic Cane – cellular modem driver (implementation)
 *
 * Drives a SIM7080G (or SIM7600A-H) LTE Cat-M1 / NB-IoT modem over
 * UART2 on the ESP32-S3.  Supports multi-carrier SIM cards for
 * T-Mobile, AT&T, and Verizon.
 *
 * NON-SAFETY: This module is advisory-only.  If the modem fails or has
 * no signal, the cane continues with full local hazard detection.
 *
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

#include "cell_modem.h"
#include <string.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════════════
 *  Pure functions — available in UNIT_TEST builds
 * ═══════════════════════════════════════════════════════════════════════ */

bool cell_parse_csq(const char *response, int8_t *rssi_dbm, uint8_t *signal_percent)
{
    /* Expected: "+CSQ: <rssi>,<ber>" where rssi is 0–31 or 99 */
    const char *p = strstr(response, "+CSQ:");
    if (!p) return false;
    p += 5;
    while (*p == ' ') p++;

    int rssi_raw = atoi(p);
    if (rssi_raw < 0 || rssi_raw == 99) {
        *rssi_dbm = -120;
        *signal_percent = 0;
        return true;    /* valid parse, but no signal */
    }
    if (rssi_raw > 31) rssi_raw = 31;

    /* RSSI mapping: 0 → –113 dBm, 31 → –51 dBm (2 dBm steps) */
    *rssi_dbm = (int8_t)(-113 + rssi_raw * 2);

    /* Percent: 0 at –113, 100 at –51 */
    if (*rssi_dbm <= -113) {
        *signal_percent = 0;
    } else if (*rssi_dbm >= -51) {
        *signal_percent = 100;
    } else {
        *signal_percent = (uint8_t)(((int)(*rssi_dbm) + 113) * 100 / 62);
    }

    return true;
}

bool cell_parse_cops(const char *response, char *operator_name, uint8_t max_len)
{
    /* Expected: +COPS: <mode>,<format>,"<operator>",<AcT> */
    const char *q1 = strchr(response, '"');
    if (!q1) return false;
    q1++;
    const char *q2 = strchr(q1, '"');
    if (!q2) return false;

    uint8_t len = (uint8_t)(q2 - q1);
    if (len >= max_len) len = max_len - 1;
    memcpy(operator_name, q1, len);
    operator_name[len] = '\0';
    return true;
}

bool cell_parse_registration(const char *response, bool *registered)
{
    /* Expected: +CEREG: <n>,<stat>  or  +CREG: <n>,<stat>
     * stat: 1 = registered home, 5 = registered roaming */
    const char *p = strstr(response, "REG:");
    if (!p) return false;

    /* Find the comma separating <n> and <stat> */
    const char *comma = strchr(p, ',');
    if (!comma) return false;
    comma++;
    while (*comma == ' ') comma++;

    int stat = atoi(comma);
    *registered = (stat == 1 || stat == 5);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Hardware implementation — ESP32-S3 only
 * ═══════════════════════════════════════════════════════════════════════ */

#ifndef UNIT_TEST

#include <Arduino.h>

/* ── State machine ──────────────────────────────────────────────────── */
static CellStatus   s_status     = {};
static CellState    s_state      = CellState::OFF;
static uint32_t     s_last_cmd_ms = 0;
static uint8_t      s_init_step  = 0;

/* ── Response buffer ────────────────────────────────────────────────── */
static char s_rx_buf[512];
static uint16_t s_rx_len = 0;

/* ── Advisory response buffer ───────────────────────────────────────── */
static char s_adv_response[256];
static bool s_adv_ready = false;

/* ── Low-level AT command helpers ───────────────────────────────────── */

static void modem_send_at(const char *cmd)
{
    Serial2.print(cmd);
    Serial2.print("\r\n");
    s_last_cmd_ms = millis();
}

static bool modem_read_response(uint32_t timeout_ms)
{
    s_rx_len = 0;
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        while (Serial2.available() && s_rx_len < sizeof(s_rx_buf) - 1) {
            s_rx_buf[s_rx_len++] = Serial2.read();
        }
        if (s_rx_len > 0 && (strstr(s_rx_buf, "OK") || strstr(s_rx_buf, "ERROR"))) {
            s_rx_buf[s_rx_len] = '\0';
            return strstr(s_rx_buf, "OK") != nullptr;
        }
    }
    s_rx_buf[s_rx_len] = '\0';
    return false;
}

/* ── Initialisation sequence ────────────────────────────────────────── */

static const char *INIT_CMDS[] = {
    "AT",                           /* basic handshake */
    "ATE0",                         /* echo off */
    "AT+CPIN?",                     /* check SIM */
    "AT+CFUN=1",                    /* full functionality */
    "AT+CGDCONT=1,\"IP\",\"\"",     /* PDP context – blank APN for auto-detect */
    "AT+CEREG=1",                   /* enable network registration URC */
    "AT+CSQ",                       /* signal quality */
    "AT+COPS?",                     /* operator info */
    "AT+CNACT=0,1",                 /* activate PDP context */
};
static constexpr uint8_t INIT_CMD_COUNT = sizeof(INIT_CMDS) / sizeof(INIT_CMDS[0]);

void cell_modem_init()
{
    /* UART2 to modem */
    Serial2.begin(CELL_BAUD_RATE, SERIAL_8N1, CELL_UART_RX_PIN, CELL_UART_TX_PIN);

    /* Hardware reset pulse */
    pinMode(CELL_RST_PIN, OUTPUT);
    digitalWrite(CELL_RST_PIN, LOW);
    delay(200);
    digitalWrite(CELL_RST_PIN, HIGH);
    delay(3000);    /* wait for modem boot */

    s_state = CellState::INITIALISING;
    s_init_step = 0;
    s_status.state = CellState::INITIALISING;
    s_status.sim_present = false;
    s_status.data_ready = false;

    Serial.println("[CELL] modem reset – starting init sequence");
}

void cell_modem_poll()
{
    uint32_t now = millis();

    switch (s_state) {
    case CellState::OFF:
        break;

    case CellState::INITIALISING:
        if (now - s_last_cmd_ms < CELL_CMD_TIMEOUT_MS) break;

        if (s_init_step < INIT_CMD_COUNT) {
            modem_send_at(INIT_CMDS[s_init_step]);
            if (modem_read_response(2000)) {
                /* Parse specific responses */
                if (strstr(s_rx_buf, "+CPIN: READY")) {
                    s_status.sim_present = true;
                }
                if (strstr(s_rx_buf, "+CSQ:")) {
                    cell_parse_csq(s_rx_buf, &s_status.rssi_dbm, &s_status.signal_percent);
                }
                if (strstr(s_rx_buf, "+COPS:")) {
                    cell_parse_cops(s_rx_buf, s_status.operator_name, sizeof(s_status.operator_name));
                }
                s_init_step++;
            } else {
                Serial.printf("[CELL] init step %u failed, retrying\n", s_init_step);
            }
        } else {
            /* Init complete – check registration */
            s_state = CellState::SEARCHING;
            Serial.printf("[CELL] init complete – SIM: %s, operator: %s, signal: %d%%\n",
                          s_status.sim_present ? "YES" : "NO",
                          s_status.operator_name,
                          s_status.signal_percent);
        }
        break;

    case CellState::SEARCHING:
        if (now - s_last_cmd_ms < CELL_STATUS_INTERVAL_MS) break;
        modem_send_at("AT+CEREG?");
        if (modem_read_response(2000)) {
            bool registered = false;
            if (cell_parse_registration(s_rx_buf, &registered) && registered) {
                s_state = CellState::REGISTERED;
                s_status.state = CellState::REGISTERED;
                Serial.println("[CELL] registered on network");
            }
        }
        /* Also refresh signal strength */
        modem_send_at("AT+CSQ");
        if (modem_read_response(1000)) {
            cell_parse_csq(s_rx_buf, &s_status.rssi_dbm, &s_status.signal_percent);
        }
        break;

    case CellState::REGISTERED:
        /* Try to activate data context */
        if (now - s_last_cmd_ms < CELL_STATUS_INTERVAL_MS) break;
        modem_send_at("AT+CNACT=0,1");
        if (modem_read_response(5000)) {
            s_state = CellState::CONNECTED;
            s_status.state = CellState::CONNECTED;
            s_status.data_ready = true;
            Serial.println("[CELL] data connection active");
        }
        break;

    case CellState::CONNECTED:
        /* Periodic status refresh */
        if (now - s_last_cmd_ms < CELL_STATUS_INTERVAL_MS) break;
        modem_send_at("AT+CSQ");
        if (modem_read_response(1000)) {
            cell_parse_csq(s_rx_buf, &s_status.rssi_dbm, &s_status.signal_percent);
        }
        /* Check for incoming advisory response data */
        while (Serial2.available()) {
            char c = Serial2.read();
            if (s_rx_len < sizeof(s_adv_response) - 1) {
                s_adv_response[s_rx_len++] = c;
                if (c == '\n' || c == '}') {
                    s_adv_response[s_rx_len] = '\0';
                    s_adv_ready = true;
                    s_rx_len = 0;
                }
            }
        }
        break;

    case CellState::ERROR:
        /* Attempt recovery after timeout */
        if (now - s_last_cmd_ms > CELL_RETRY_INTERVAL_MS) {
            s_state = CellState::INITIALISING;
            s_init_step = 0;
            Serial.println("[CELL] attempting recovery...");
        }
        break;
    }

    s_status.state = s_state;
}

CellStatus cell_modem_status()
{
    return s_status;
}

bool cell_send_advisory_request(const char *json_payload, uint16_t len)
{
    if (s_state != CellState::CONNECTED) return false;

    /* HTTP POST via AT+SHCONF / AT+SHREQ (SIM7080G HTTP client) */
    modem_send_at("AT+SHCONF=\"URL\",\"" CELL_RYNNBRAIN_ENDPOINT "\"");
    if (!modem_read_response(2000)) return false;

    modem_send_at("AT+SHCONF=\"BODYLEN\",1024");
    if (!modem_read_response(1000)) return false;

    modem_send_at("AT+SHCONN");
    if (!modem_read_response(5000)) return false;

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+SHBOD=%u,5000", len);
    modem_send_at(cmd);
    delay(100);
    Serial2.write(json_payload, len);

    if (!modem_read_response(5000)) return false;

    modem_send_at("AT+SHREQ=\"/advisory\",1");     /* POST */
    /* Response will arrive asynchronously */
    return true;
}

bool cell_recv_advisory_response(char *buf, uint16_t max_len)
{
    if (!s_adv_ready) return false;
    uint16_t len = strlen(s_adv_response);
    if (len >= max_len) len = max_len - 1;
    memcpy(buf, s_adv_response, len);
    buf[len] = '\0';
    s_adv_ready = false;
    return true;
}

bool cell_modem_data_ready()
{
    return s_state == CellState::CONNECTED && s_status.data_ready;
}

#endif /* UNIT_TEST */
