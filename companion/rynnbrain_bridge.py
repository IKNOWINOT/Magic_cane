#!/usr/bin/env python3
"""
Magic Cane – RynnBrain Advisory Bridge
Provides a NON-SAFETY advisory layer that:
  - Processes camera frames for scene understanding
  - Generates navigation hints, object labels, OCR results
  - Sends advisory hints to the cane over BLE

This module is ADVISORY ONLY. The cane firmware makes all safety
decisions locally using its own sensors regardless of what this
module reports.

Copyright 2024 Magic Cane Contributors – Apache-2.0
"""

import asyncio
import logging
import time
from dataclasses import dataclass
from enum import Enum
from typing import List, Optional

from ble_cane_client import AdvisoryHint, CaneBLEClient

logger = logging.getLogger(__name__)


class SceneObjectType(str, Enum):
    """Types of objects the advisory system can detect."""
    OBSTACLE = "obstacle"
    CROSSWALK = "crosswalk"
    TRAFFIC_LIGHT = "traffic_light"
    DOOR = "door"
    STAIRS_UP = "stairs_up"
    STAIRS_DOWN = "stairs_down"
    SIGN = "sign"
    PERSON = "person"
    VEHICLE = "vehicle"
    CURB = "curb"
    UNKNOWN = "unknown"


@dataclass
class SceneObject:
    """A detected object in the scene."""
    object_type: SceneObjectType
    label: str
    bearing_deg: int        # -180 to +180, 0 = straight ahead
    distance_est_m: float   # estimated distance in metres
    confidence: float       # 0.0 to 1.0
    ocr_text: str = ""      # OCR content if object is a sign


@dataclass
class RouteContext:
    """Current navigation context."""
    destination: str = ""
    next_instruction: str = ""
    bearing_to_next: int = 0
    distance_to_next_m: float = 0.0
    on_route: bool = False


class RynnBrainBridge:
    """
    Bridge between RynnBrain perception and the cane's advisory channel.

    This class is intentionally simple and stateless with respect to safety.
    It only produces AdvisoryHint objects; the cane decides whether to
    use them.
    """

    def __init__(self, ble_client: CaneBLEClient):
        self._ble = ble_client
        self._route: Optional[RouteContext] = None
        self._last_scene: List[SceneObject] = []
        self._running = False
        self._advisory_interval = 1.0  # seconds between advisory updates

    @property
    def running(self) -> bool:
        return self._running

    def set_route(self, route: RouteContext):
        """Set the current navigation route (from phone app)."""
        self._route = route
        logger.info(f"Route set: {route.destination}")

    def update_scene(self, objects: List[SceneObject]):
        """Update the current scene analysis (from camera pipeline)."""
        self._last_scene = objects

    async def start(self):
        """Start the advisory loop."""
        self._running = True
        logger.info("RynnBrain advisory bridge started")

        while self._running:
            await self._send_advisories()
            await asyncio.sleep(self._advisory_interval)

    async def stop(self):
        """Stop the advisory loop."""
        self._running = False

    async def _send_advisories(self):
        """Generate and send advisory hints based on current scene + route."""
        if not self._ble.connected:
            return

        # Priority 1: Navigation hints from route context
        if self._route and self._route.on_route:
            hint = AdvisoryHint(
                type="nav_hint",
                bearing=self._route.bearing_to_next,
                label=self._route.next_instruction,
                confidence=0.95,
            )
            await self._ble.send_advisory(hint)
            return

        # Priority 2: Scene objects (highest confidence first)
        if self._last_scene:
            best = max(self._last_scene, key=lambda o: o.confidence)
            if best.confidence >= 0.5:
                hint_type = "scene_label"
                if best.ocr_text:
                    hint_type = "ocr_result"
                    label = f"{best.label}: {best.ocr_text}"
                else:
                    label = best.label

                hint = AdvisoryHint(
                    type=hint_type,
                    bearing=best.bearing_deg,
                    label=label[:63],  # cane firmware label buffer is 64 bytes
                    confidence=best.confidence,
                )
                await self._ble.send_advisory(hint)


class SceneProcessor:
    """
    Camera-based scene analysis pipeline.

    In production this would integrate with a ML model (e.g. YOLO, MobileNet)
    running on the phone GPU. This implementation provides the interface
    and a simulation mode for testing.
    """

    def __init__(self):
        self._running = False

    async def start(self, bridge: RynnBrainBridge):
        """Start processing camera frames and updating the bridge."""
        self._running = True
        logger.info("Scene processor started (simulation mode)")

        while self._running:
            # In production: capture frame → run inference → extract objects
            # For now: empty scene (no false positives)
            bridge.update_scene([])
            await asyncio.sleep(0.5)

    async def stop(self):
        self._running = False

    def process_frame_sync(self, frame_data: bytes) -> List[SceneObject]:
        """
        Synchronously process a single camera frame.
        Returns list of detected SceneObjects.

        In production, this calls the ML inference engine.
        For testing, returns an empty list (safe default).
        """
        # TODO: Integrate ML model inference
        return []
