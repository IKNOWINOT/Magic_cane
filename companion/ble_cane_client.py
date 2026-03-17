#!/usr/bin/env python3
"""
Magic Cane – BLE Cane Client
Connects to the MagicCane BLE device and provides:
  - Advisory hint transmission (non-safety)
  - Sensor telemetry reception for logging / display
  - Connection state management with auto-reconnect

Copyright 2024 Magic Cane Contributors – Apache-2.0
"""

import asyncio
import json
import logging
import struct
from dataclasses import dataclass, field
from typing import Callable, Optional

try:
    from bleak import BleakClient, BleakScanner
    HAS_BLEAK = True
except ImportError:
    HAS_BLEAK = False

logger = logging.getLogger(__name__)

# BLE UUIDs – must match cane firmware config.h
CANE_SERVICE_UUID = "a1b2c3d4-0001-1000-8000-00805f9b34fb"
HAPTIC_CHAR_UUID = "a1b2c3d4-0002-1000-8000-00805f9b34fb"
ADVISORY_SERVICE_UUID = "a1b2c3d4-0003-1000-8000-00805f9b34fb"
ADVISORY_CHAR_UUID = "a1b2c3d4-0004-1000-8000-00805f9b34fb"

# Belt packet format
PACKET_HEADER = 0xCA
PACKET_LENGTH = 12


@dataclass
class BeltPacket:
    """Parsed haptic belt packet from the cane."""
    seq: int = 0
    motors: list = field(default_factory=lambda: [0] * 8)
    flags: int = 0
    emergency_stop: bool = False

    @staticmethod
    def from_bytes(data: bytes) -> Optional["BeltPacket"]:
        """Parse a raw 12-byte belt packet. Returns None if invalid."""
        if len(data) != PACKET_LENGTH:
            return None
        if data[0] != PACKET_HEADER:
            return None
        # Verify XOR checksum
        cs = 0
        for b in data[:PACKET_LENGTH - 1]:
            cs ^= b
        if cs != data[PACKET_LENGTH - 1]:
            return None
        pkt = BeltPacket()
        pkt.seq = data[1]
        pkt.motors = list(data[2:10])
        pkt.flags = data[10]
        pkt.emergency_stop = bool(pkt.flags & 0x01)
        return pkt


@dataclass
class AdvisoryHint:
    """Advisory hint to send to the cane (non-safety)."""
    type: str = "nav_hint"
    bearing: int = 0
    label: str = ""
    confidence: float = 0.0

    def to_json(self) -> str:
        return json.dumps({
            "type": self.type,
            "bearing": self.bearing,
            "label": self.label,
            "confidence": round(self.confidence, 2),
        })

    def to_bytes(self) -> bytes:
        return self.to_json().encode("utf-8")


class CaneBLEClient:
    """Async BLE client that connects to the Magic Cane."""

    def __init__(self):
        self._client: Optional[BleakClient] = None
        self._connected = False
        self._on_haptic: Optional[Callable] = None
        self._reconnect_delay = 2.0

    @property
    def connected(self) -> bool:
        return self._connected and self._client is not None

    def on_haptic_packet(self, callback: Callable[[BeltPacket], None]):
        """Register a callback for incoming haptic packets (for monitoring)."""
        self._on_haptic = callback

    async def scan(self, timeout: float = 10.0) -> Optional[str]:
        """Scan for MagicCane device. Returns address or None."""
        if not HAS_BLEAK:
            logger.error("bleak not installed – BLE unavailable")
            return None

        logger.info("Scanning for MagicCane...")
        devices = await BleakScanner.discover(timeout=timeout)
        for d in devices:
            if d.name and "MagicCane" in d.name:
                logger.info(f"Found MagicCane at {d.address}")
                return d.address
        logger.warning("MagicCane not found")
        return None

    async def connect(self, address: str) -> bool:
        """Connect to the MagicCane at the given BLE address."""
        if not HAS_BLEAK:
            return False
        try:
            self._client = BleakClient(address, disconnected_callback=self._on_disconnect)
            await self._client.connect()
            self._connected = True
            logger.info(f"Connected to MagicCane at {address}")

            # Subscribe to haptic notifications (for monitoring)
            try:
                await self._client.start_notify(HAPTIC_CHAR_UUID, self._haptic_notification)
            except Exception:
                logger.debug("Haptic notifications not available (normal if belt-only)")

            return True
        except Exception as e:
            logger.error(f"Connection failed: {e}")
            self._connected = False
            return False

    async def disconnect(self):
        """Disconnect from the cane."""
        if self._client:
            await self._client.disconnect()
        self._connected = False

    async def send_advisory(self, hint: AdvisoryHint) -> bool:
        """Send an advisory hint to the cane (non-safety)."""
        if not self.connected:
            return False
        try:
            await self._client.write_gatt_char(ADVISORY_CHAR_UUID, hint.to_bytes())
            logger.debug(f"Advisory sent: {hint.label}")
            return True
        except Exception as e:
            logger.error(f"Advisory send failed: {e}")
            return False

    async def run_auto_reconnect(self, address: str):
        """Continuously reconnect if connection drops."""
        while True:
            if not self.connected:
                logger.info("Attempting reconnect...")
                await self.connect(address)
            await asyncio.sleep(self._reconnect_delay)

    def _haptic_notification(self, _sender, data: bytearray):
        """BLE notification callback for haptic characteristic."""
        pkt = BeltPacket.from_bytes(bytes(data))
        if pkt and self._on_haptic:
            self._on_haptic(pkt)

    def _on_disconnect(self, _client):
        self._connected = False
        logger.warning("Disconnected from MagicCane")
