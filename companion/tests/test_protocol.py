#!/usr/bin/env python3
"""
Magic Cane – Python protocol & bridge unit tests.

Tests the pure-Python BLE protocol parsing, advisory hint generation,
and RynnBrain bridge logic (no hardware or BLE required).

Run with: python -m pytest tests/ -v
"""

import json
import sys
import os

# Add companion root to path so imports work
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from ble_cane_client import BeltPacket, AdvisoryHint, PACKET_HEADER, PACKET_LENGTH
from rynnbrain_bridge import (
    SceneObject, SceneObjectType, RouteContext, RynnBrainBridge, SceneProcessor,
)


# ═══════════════════════════════════════════════════════════════════════
#  BeltPacket Tests
# ═══════════════════════════════════════════════════════════════════════

def _build_packet(seq: int, motors: list, flags: int) -> bytes:
    """Build a valid 12-byte belt packet."""
    buf = bytearray(12)
    buf[0] = PACKET_HEADER
    buf[1] = seq
    for i in range(8):
        buf[2 + i] = motors[i]
    buf[10] = flags
    cs = 0
    for i in range(11):
        cs ^= buf[i]
    buf[11] = cs
    return bytes(buf)


class TestBeltPacket:
    def test_parse_valid_packet(self):
        motors = [10, 20, 30, 40, 50, 60, 70, 80]
        data = _build_packet(1, motors, 0x00)
        pkt = BeltPacket.from_bytes(data)
        assert pkt is not None
        assert pkt.seq == 1
        assert pkt.motors == motors
        assert pkt.emergency_stop is False

    def test_parse_estop(self):
        data = _build_packet(42, [255] * 8, 0x01)
        pkt = BeltPacket.from_bytes(data)
        assert pkt is not None
        assert pkt.emergency_stop is True
        assert all(m == 255 for m in pkt.motors)

    def test_reject_wrong_length(self):
        assert BeltPacket.from_bytes(b"\xCA" * 10) is None

    def test_reject_wrong_header(self):
        data = bytearray(_build_packet(0, [0] * 8, 0))
        data[0] = 0xFF
        assert BeltPacket.from_bytes(bytes(data)) is None

    def test_reject_bad_checksum(self):
        data = bytearray(_build_packet(0, [0] * 8, 0))
        data[11] ^= 0xFF
        assert BeltPacket.from_bytes(bytes(data)) is None

    def test_seq_rollover(self):
        data = _build_packet(255, [0] * 8, 0)
        pkt = BeltPacket.from_bytes(data)
        assert pkt.seq == 255

    def test_mixed_motor_values(self):
        motors = [0, 80, 180, 255, 0, 80, 180, 255]
        data = _build_packet(77, motors, 0x01)
        pkt = BeltPacket.from_bytes(data)
        assert pkt.motors == motors
        assert pkt.emergency_stop is True

    def test_all_zero_motors(self):
        data = _build_packet(0, [0] * 8, 0)
        pkt = BeltPacket.from_bytes(data)
        assert all(m == 0 for m in pkt.motors)
        assert pkt.emergency_stop is False


# ═══════════════════════════════════════════════════════════════════════
#  AdvisoryHint Tests
# ═══════════════════════════════════════════════════════════════════════

class TestAdvisoryHint:
    def test_to_json_roundtrip(self):
        hint = AdvisoryHint(type="nav_hint", bearing=45, label="crosswalk", confidence=0.82)
        j = json.loads(hint.to_json())
        assert j["type"] == "nav_hint"
        assert j["bearing"] == 45
        assert j["label"] == "crosswalk"
        assert j["confidence"] == 0.82

    def test_to_bytes_utf8(self):
        hint = AdvisoryHint(type="scene_label", bearing=-90, label="puerta", confidence=0.7)
        raw = hint.to_bytes()
        assert isinstance(raw, bytes)
        parsed = json.loads(raw.decode("utf-8"))
        assert parsed["label"] == "puerta"

    def test_negative_bearing(self):
        hint = AdvisoryHint(bearing=-180)
        j = json.loads(hint.to_json())
        assert j["bearing"] == -180

    def test_zero_confidence(self):
        hint = AdvisoryHint(confidence=0.0)
        j = json.loads(hint.to_json())
        assert j["confidence"] == 0.0

    def test_max_label_length(self):
        hint = AdvisoryHint(label="a" * 63)
        j = json.loads(hint.to_json())
        assert len(j["label"]) == 63

    def test_all_advisory_types(self):
        for t in ["nav_hint", "scene_label", "ocr_result", "route_update"]:
            hint = AdvisoryHint(type=t)
            j = json.loads(hint.to_json())
            assert j["type"] == t


# ═══════════════════════════════════════════════════════════════════════
#  SceneObject Tests
# ═══════════════════════════════════════════════════════════════════════

class TestSceneObject:
    def test_create_obstacle(self):
        obj = SceneObject(
            object_type=SceneObjectType.OBSTACLE,
            label="wall",
            bearing_deg=0,
            distance_est_m=1.5,
            confidence=0.9,
        )
        assert obj.object_type == SceneObjectType.OBSTACLE
        assert obj.label == "wall"

    def test_ocr_text(self):
        obj = SceneObject(
            object_type=SceneObjectType.SIGN,
            label="street sign",
            bearing_deg=10,
            distance_est_m=3.0,
            confidence=0.85,
            ocr_text="Main Street",
        )
        assert obj.ocr_text == "Main Street"

    def test_all_object_types(self):
        for t in SceneObjectType:
            obj = SceneObject(
                object_type=t, label="test", bearing_deg=0,
                distance_est_m=1.0, confidence=0.5,
            )
            assert obj.object_type == t


# ═══════════════════════════════════════════════════════════════════════
#  RouteContext Tests
# ═══════════════════════════════════════════════════════════════════════

class TestRouteContext:
    def test_default_off_route(self):
        route = RouteContext()
        assert route.on_route is False

    def test_on_route(self):
        route = RouteContext(
            destination="Library",
            next_instruction="Turn left in 20 metres",
            bearing_to_next=-90,
            distance_to_next_m=20.0,
            on_route=True,
        )
        assert route.on_route is True
        assert route.bearing_to_next == -90


# ═══════════════════════════════════════════════════════════════════════
#  SceneProcessor Tests
# ═══════════════════════════════════════════════════════════════════════

class TestSceneProcessor:
    def test_process_empty_frame(self):
        proc = SceneProcessor()
        result = proc.process_frame_sync(b"")
        assert result == []
        assert isinstance(result, list)


# ═══════════════════════════════════════════════════════════════════════
#  Cross-protocol: Python builds packet that matches C++ format
# ═══════════════════════════════════════════════════════════════════════

class TestCrossProtocol:
    def test_python_packet_matches_c_format(self):
        """Verify Python packet builder creates same format as C++ firmware."""
        motors = [0, 80, 180, 255, 0, 80, 180, 255]
        data = _build_packet(77, motors, 0x01)

        # Verify structure
        assert len(data) == 12
        assert data[0] == 0xCA           # header
        assert data[1] == 77             # seq
        assert data[10] == 0x01          # flags (e-stop)

        # Verify checksum
        cs = 0
        for i in range(11):
            cs ^= data[i]
        assert cs == data[11]

        # Verify round-trip
        pkt = BeltPacket.from_bytes(data)
        assert pkt is not None
        assert pkt.motors == motors
        assert pkt.emergency_stop is True

    def test_advisory_under_200_bytes(self):
        """Advisory messages must fit in 200-byte BLE characteristic."""
        hint = AdvisoryHint(
            type="nav_hint",
            bearing=180,
            label="a" * 63,
            confidence=0.99,
        )
        raw = hint.to_bytes()
        assert len(raw) <= 200
