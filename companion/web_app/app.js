/**
 * Magic Cane – Web Bluetooth Companion App
 *
 * Connects to the MagicCane ESP32-S3 over Web Bluetooth, displays
 * live haptic data, and allows sending advisory hints.
 *
 * Works on Chrome (Android/Desktop), Edge, Opera – any browser
 * supporting the Web Bluetooth API.
 *
 * Copyright 2024 Magic Cane Contributors – Apache-2.0
 */

/* ── BLE UUIDs (must match cane firmware config.h) ──────────────────── */
const CANE_SERVICE_UUID    = "a1b2c3d4-0001-1000-8000-00805f9b34fb";
const HAPTIC_CHAR_UUID     = "a1b2c3d4-0002-1000-8000-00805f9b34fb";
const ADVISORY_SERVICE_UUID = "a1b2c3d4-0003-1000-8000-00805f9b34fb";
const ADVISORY_CHAR_UUID   = "a1b2c3d4-0004-1000-8000-00805f9b34fb";

const PACKET_HEADER = 0xCA;
const PACKET_LENGTH = 12;
const MOTOR_COUNT   = 8;

const DIRECTION_NAMES = [
    "Front", "Front-Right", "Right", "Rear-Right",
    "Rear", "Rear-Left", "Left", "Front-Left"
];

/* ── State ──────────────────────────────────────────────────────────── */
let bleDevice = null;
let hapticChar = null;
let advisoryChar = null;
let connected = false;

/* ── DOM References ─────────────────────────────────────────────────── */
const statusIndicator = document.getElementById("status-indicator");
const statusText      = document.getElementById("status-text");
const btnConnect      = document.getElementById("btn-connect");
const btnDisconnect   = document.getElementById("btn-disconnect");
const btnSendHint     = document.getElementById("btn-send-hint");
const btnClearLog     = document.getElementById("btn-clear-log");
const estopBanner     = document.getElementById("estop-banner");
const logContainer    = document.getElementById("log-container");
const advisoryForm    = document.getElementById("advisory-form");
const confidenceInput = document.getElementById("hint-confidence");
const confidenceDisp  = document.getElementById("confidence-display");

/* ── Utility ────────────────────────────────────────────────────────── */

function log(msg) {
    const entry = document.createElement("p");
    entry.className = "log-entry";
    const time = new Date().toLocaleTimeString();
    entry.textContent = `[${time}] ${msg}`;
    logContainer.prepend(entry);
    /* Keep log manageable */
    while (logContainer.children.length > 200) {
        logContainer.removeChild(logContainer.lastChild);
    }
}

function setStatus(text, level) {
    statusText.textContent = text;
    statusIndicator.className = `dot dot-${level}`;
}

function validatePacket(data) {
    if (data.byteLength !== PACKET_LENGTH) return false;
    if (data.getUint8(0) !== PACKET_HEADER) return false;
    let cs = 0;
    for (let i = 0; i < PACKET_LENGTH - 1; i++) {
        cs ^= data.getUint8(i);
    }
    return cs === data.getUint8(PACKET_LENGTH - 1);
}

function intensityToLevel(v) {
    if (v === 0)   return "clear";
    if (v <= 80)   return "warn";
    if (v <= 180)  return "near";
    return "stop";
}

/* ── Belt Display Update ────────────────────────────────────────────── */

function updateBeltDisplay(motors, estop) {
    for (let i = 0; i < MOTOR_COUNT; i++) {
        const el = document.getElementById(`motor-${i}`);
        if (!el) continue;
        const level = intensityToLevel(motors[i]);
        el.className = `motor motor-${level}`;
        el.textContent = motors[i];
        el.setAttribute("aria-label", `${DIRECTION_NAMES[i]}: ${level} (${motors[i]})`);
    }
    if (estop) {
        estopBanner.classList.remove("hidden");
        estopBanner.setAttribute("aria-hidden", "false");
    } else {
        estopBanner.classList.add("hidden");
        estopBanner.setAttribute("aria-hidden", "true");
    }
}

/* ── BLE Connection ─────────────────────────────────────────────────── */

async function connectBLE() {
    if (!navigator.bluetooth) {
        log("Web Bluetooth not supported in this browser.");
        alert("Web Bluetooth is required. Use Chrome, Edge, or Opera.");
        return;
    }

    try {
        setStatus("Scanning...", "scanning");
        log("Scanning for MagicCane...");

        bleDevice = await navigator.bluetooth.requestDevice({
            filters: [{ services: [CANE_SERVICE_UUID] }],
            optionalServices: [ADVISORY_SERVICE_UUID],
        });

        bleDevice.addEventListener("gattserverdisconnected", onDisconnected);

        setStatus("Connecting...", "scanning");
        const server = await bleDevice.gatt.connect();

        /* Haptic service */
        const hapticService = await server.getPrimaryService(CANE_SERVICE_UUID);
        hapticChar = await hapticService.getCharacteristic(HAPTIC_CHAR_UUID);
        await hapticChar.startNotifications();
        hapticChar.addEventListener("characteristicvaluechanged", onHapticNotification);

        /* Advisory service (optional) */
        try {
            const advService = await server.getPrimaryService(ADVISORY_SERVICE_UUID);
            advisoryChar = await advService.getCharacteristic(ADVISORY_CHAR_UUID);
        } catch (_e) {
            log("Advisory service not available – hint sending disabled.");
            advisoryChar = null;
        }

        connected = true;
        setStatus("Connected", "connected");
        btnConnect.disabled = true;
        btnDisconnect.disabled = false;
        btnSendHint.disabled = !advisoryChar;
        log(`Connected to ${bleDevice.name || "MagicCane"}`);

    } catch (err) {
        setStatus("Disconnected", "disconnected");
        log(`Connection error: ${err.message}`);
    }
}

function onDisconnected() {
    connected = false;
    hapticChar = null;
    advisoryChar = null;
    setStatus("Disconnected", "disconnected");
    btnConnect.disabled = false;
    btnDisconnect.disabled = true;
    btnSendHint.disabled = true;
    log("Disconnected from MagicCane.");
    updateBeltDisplay(new Array(8).fill(0), false);
}

async function disconnectBLE() {
    if (bleDevice && bleDevice.gatt.connected) {
        bleDevice.gatt.disconnect();
    }
}

/* ── Haptic Notification Handler ────────────────────────────────────── */

function onHapticNotification(event) {
    const data = event.target.value;
    if (!validatePacket(data)) {
        log("Invalid packet received (checksum mismatch).");
        return;
    }

    const seq = data.getUint8(1);
    const motors = [];
    for (let i = 0; i < MOTOR_COUNT; i++) {
        motors.push(data.getUint8(2 + i));
    }
    const flags = data.getUint8(10);
    const estop = (flags & 0x01) !== 0;

    updateBeltDisplay(motors, estop);

    if (estop) {
        log(`⚠️ EMERGENCY STOP – seq=${seq}`);
    }
}

/* ── Advisory Hint Sender ───────────────────────────────────────────── */

async function sendAdvisory(e) {
    e.preventDefault();
    if (!advisoryChar) return;

    const hint = {
        type: document.getElementById("hint-type").value,
        bearing: parseInt(document.getElementById("hint-bearing").value, 10),
        label: document.getElementById("hint-label").value,
        confidence: parseInt(confidenceInput.value, 10) / 100,
    };

    try {
        const json = JSON.stringify(hint);
        const encoder = new TextEncoder();
        await advisoryChar.writeValue(encoder.encode(json));
        log(`Advisory sent: "${hint.label}" bearing=${hint.bearing}° conf=${hint.confidence}`);
    } catch (err) {
        log(`Advisory send failed: ${err.message}`);
    }
}

/* ── Event Listeners ────────────────────────────────────────────────── */

btnConnect.addEventListener("click", connectBLE);
btnDisconnect.addEventListener("click", disconnectBLE);
advisoryForm.addEventListener("submit", sendAdvisory);
btnClearLog.addEventListener("click", () => { logContainer.innerHTML = ""; });
confidenceInput.addEventListener("input", () => {
    confidenceDisp.textContent = `${confidenceInput.value}%`;
});
