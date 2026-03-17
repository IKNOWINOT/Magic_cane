#!/usr/bin/env python3
"""
Magic Cane – Companion Main Entry Point
Runs the phone/edge companion with:
  - BLE connection to Magic Cane
  - RynnBrain advisory bridge (non-safety)
  - Scene processor (camera pipeline)
  - Auto-reconnect and graceful shutdown

Usage:
    python main.py --address XX:XX:XX:XX:XX:XX
    python main.py --scan   (auto-discover cane)

Copyright 2024 Magic Cane Contributors – Apache-2.0
"""

import argparse
import asyncio
import logging
import signal
import sys

from ble_cane_client import CaneBLEClient, BeltPacket
from rynnbrain_bridge import RynnBrainBridge, SceneProcessor

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
)
logger = logging.getLogger("main")


def on_haptic_packet(pkt: BeltPacket):
    """Log haptic packets for debugging / telemetry."""
    motors_str = " ".join(f"{m:3d}" for m in pkt.motors)
    estop = "ESTOP" if pkt.emergency_stop else "     "
    logger.debug(f"Belt seq={pkt.seq:3d} motors=[{motors_str}] {estop}")


async def main(address: str = None, scan: bool = False):
    ble = CaneBLEClient()
    ble.on_haptic_packet(on_haptic_packet)

    # Discover or use provided address
    if scan or not address:
        address = await ble.scan(timeout=15.0)
        if not address:
            logger.error("Could not find MagicCane. Is it powered on?")
            sys.exit(1)

    # Connect
    if not await ble.connect(address):
        logger.error(f"Failed to connect to {address}")
        sys.exit(1)

    # Start advisory pipeline
    bridge = RynnBrainBridge(ble)
    scene = SceneProcessor()

    tasks = [
        asyncio.create_task(bridge.start()),
        asyncio.create_task(scene.start(bridge)),
        asyncio.create_task(ble.run_auto_reconnect(address)),
    ]

    # Graceful shutdown on Ctrl+C
    loop = asyncio.get_event_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, lambda: asyncio.create_task(shutdown(tasks, bridge, scene, ble)))

    logger.info("Companion running. Press Ctrl+C to stop.")
    await asyncio.gather(*tasks, return_exceptions=True)


async def shutdown(tasks, bridge, scene, ble):
    logger.info("Shutting down...")
    await bridge.stop()
    await scene.stop()
    await ble.disconnect()
    for t in tasks:
        t.cancel()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Magic Cane Companion")
    parser.add_argument("--address", "-a", help="BLE address of MagicCane")
    parser.add_argument("--scan", "-s", action="store_true", help="Auto-scan for MagicCane")
    args = parser.parse_args()

    asyncio.run(main(address=args.address, scan=args.scan))
