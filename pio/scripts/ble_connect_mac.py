#!/usr/bin/env python3
"""Multi-node BLE terminal for Poolantir on macOS.

Scans for all 6 poolantir nodes, connects to every one found, then enters an
interactive loop: pick a node, choose an action (echo, servo, LED, simulate
user), build the matching prefixed payload, and send it.  The script displays
timestamped logs and streams any BLE notifications from the ESP32 in real time.

The script never exits on its own -- only 'quit' or Ctrl+C will end it.
Type 'scan' at the node prompt to rescan and reconnect dropped nodes.

Requires: pip install bleak
"""

from __future__ import annotations

import asyncio
import time

from bleak import BleakClient, BleakScanner


NODE_COUNT = 6

ACTION_MENU = """\
  1) Echo          — send a message, ESP32 echoes it back
  2) Move servo    — REST / MAX / specific degree
  3) Flash LED     — flash R, G, or B
  4) Simulate user — enqueue pee(1) / poo(2) sequence on ESP32"""


def _ts() -> str:
    return time.strftime("[%H:%M:%S]")


def service_uuid(node: int) -> str:
    return f"4fafc201-1fb5-459e-8fcc-c5c9c33191a{node}"


def char_uuid(node: int) -> str:
    return f"beb5483e-36e1-4688-b7f5-e073f246f7b{node}"


def _is_connected(clients: dict[int, BleakClient], node: int) -> bool:
    return node in clients and clients[node].is_connected


async def _scan_and_connect(
    clients: dict[int, BleakClient],
    ack_event: dict[int, asyncio.Event],
    ack_text: dict[int, str],
) -> None:
    """Scan for nodes and connect to any that aren't already connected."""
    print(f"{_ts()} Scanning for poolantir nodes (10s)...", flush=True)
    devices = await BleakScanner.discover(
        timeout=10.0,
        return_adv=True,
    )

    print(f"  discovered {len(devices)} device(s):", flush=True)
    for addr, (dev, adv) in devices.items():
        uuids = [str(u) for u in adv.service_uuids] if adv.service_uuids else []
        print(f"    {dev.name or '(no name)'} [{addr}] uuids={uuids}", flush=True)

    for node in range(1, NODE_COUNT + 1):
        if _is_connected(clients, node):
            continue

        target_name = f"poolantir-node-{node}"
        target_svc = service_uuid(node)
        addr = None
        for dev_addr, (dev, adv) in devices.items():
            name_match = dev.name and str(dev.name) == target_name
            svc_uuids = [str(u) for u in adv.service_uuids] if adv.service_uuids else []
            uuid_match = target_svc in svc_uuids
            if name_match or uuid_match:
                addr = str(dev_addr)
                break

        if addr is None:
            print(f"  node {node}: not found", flush=True)
            continue

        try:
            client = BleakClient(addr)
            await client.connect()
            clients[node] = client

            evt = asyncio.Event()
            ack_event[node] = evt
            ack_text[node] = ""

            def _make_handler(n: int, e: asyncio.Event) -> callable:
                def _handler(_sender, data: bytearray) -> None:
                    text = data.decode("utf-8", errors="replace")
                    ack_text[n] = text
                    print(f"{_ts()} [notify] node {n}: {text}", flush=True)
                    e.set()
                return _handler

            await client.start_notify(char_uuid(node), _make_handler(node, evt))
            print(f"  node {node}: connected ({addr})", flush=True)
        except Exception as e:
            print(f"  node {node}: connect failed -- {e}", flush=True)


def _print_status(clients: dict[int, BleakClient]) -> None:
    print(f"\n{_ts()} --- Node Status ---", flush=True)
    for node in range(1, NODE_COUNT + 1):
        status = "connected" if _is_connected(clients, node) else "disconnected"
        print(f"  node {node} - {status}", flush=True)
    print(flush=True)


async def _prompt(text: str) -> str:
    return await asyncio.to_thread(input, text)


# ── Payload builders ─────────────────────────────────────────────

async def _build_echo_payload() -> str | None:
    msg = await _prompt("  Enter message to echo: ")
    msg = msg.strip()
    if not msg or msg.lower() in ("quit", "q"):
        return None
    return f"ECHO {msg}"


async def _build_servo_payload() -> str | None:
    print("  Options: REST, MAX, or a degree (0-180)")
    val = await _prompt("  Servo position: ")
    val = val.strip()
    if not val or val.lower() in ("quit", "q"):
        return None
    upper = val.upper()
    if upper in ("REST", "MAX"):
        return f"SERVO {upper}"
    try:
        deg = int(val)
        if deg < 0 or deg > 180:
            print("  Degree must be 0-180.", flush=True)
            return None
        return f"SERVO {deg}"
    except ValueError:
        print("  Invalid input.", flush=True)
        return None


async def _build_led_payload() -> str | None:
    print("  Options: R (red), G (green), B (blue)")
    color = await _prompt("  LED color: ")
    color = color.strip().upper()
    if color in ("QUIT", "Q"):
        return None
    if color not in ("R", "G", "B"):
        print("  Invalid color. Use R, G, or B.", flush=True)
        return None
    return f"LED {color}"


async def _build_sim_payload() -> str | None:
    print("  Enter a comma-separated list of 1 (pee) and 2 (poo).")
    print("  Example: 1,2,1,1,2")
    raw = await _prompt("  Sequence: ")
    raw = raw.strip()
    if not raw or raw.lower() in ("quit", "q"):
        return None
    try:
        elems = [int(x.strip()) for x in raw.split(",") if x.strip()]
    except ValueError:
        print("  All elements must be integers.", flush=True)
        return None
    if not elems:
        print("  Empty list.", flush=True)
        return None
    for v in elems:
        if v not in (1, 2):
            print(f"  Invalid element {v}. Only 1 (pee) or 2 (poo) allowed.", flush=True)
            return None
    body = ",".join(str(v) for v in elems)
    return f"SIM {len(elems)} {{{body}}}"


ACTION_BUILDERS = {
    "1": ("Echo", _build_echo_payload),
    "2": ("Move servo", _build_servo_payload),
    "3": ("Flash LED", _build_led_payload),
    "4": ("Simulate user", _build_sim_payload),
}


# ── Send helper ──────────────────────────────────────────────────

async def _send_and_wait(
    clients: dict[int, BleakClient],
    ack_event: dict[int, asyncio.Event],
    ack_text: dict[int, str],
    node: int,
    payload: str,
    timeout: float = 10.0,
    wait_until: str | None = None,
) -> None:
    """Send a payload and wait for a BLE notification.

    If *wait_until* is set, keep waiting for successive notifications until one
    contains the substring (used by SIM to wait for the full simulation to
    finish before returning to the menu).
    """
    print(f"{_ts()} [send] node {node} <- {payload}", flush=True)
    ack_event[node].clear()

    try:
        await clients[node].write_gatt_char(
            char_uuid(node), payload.encode("utf-8"), response=True
        )
    except Exception as e:
        print(f"{_ts()} [error] write failed: {e}", flush=True)
        return

    try:
        await asyncio.wait_for(ack_event[node].wait(), timeout=timeout)
    except asyncio.TimeoutError:
        print(f"{_ts()} [warn] no ack within {timeout:.0f}s", flush=True)
        return

    if wait_until is None:
        return

    while wait_until not in ack_text.get(node, ""):
        ack_event[node].clear()
        try:
            await asyncio.wait_for(ack_event[node].wait(), timeout=timeout)
        except asyncio.TimeoutError:
            print(f"{_ts()} [warn] timed out waiting for simulation to finish", flush=True)
            return


# ── Main loop ────────────────────────────────────────────────────

async def run() -> int:
    print("=== Poolantir Multi-Node BLE Terminal ===", flush=True)
    print("Type 'quit' to exit, 'scan' to rescan for nodes.\n", flush=True)

    clients: dict[int, BleakClient] = {}
    ack_event: dict[int, asyncio.Event] = {}
    ack_text: dict[int, str] = {}

    await _scan_and_connect(clients, ack_event, ack_text)

    try:
        while True:
            _print_status(clients)

            sel = await _prompt("Select a node to message (1-6): ")
            cmd = sel.strip().lower()

            if cmd in ("quit", "q"):
                break
            if cmd == "scan":
                await _scan_and_connect(clients, ack_event, ack_text)
                continue

            try:
                node = int(cmd)
            except ValueError:
                print("Enter a valid node number.\n", flush=True)
                continue

            if not _is_connected(clients, node):
                print(f"Node {node} is not connected. Type 'scan' to retry.\n", flush=True)
                continue

            # Action menu
            print(f"\nActions for node {node}:")
            print(ACTION_MENU)
            action = await _prompt("  Choose action (1-4): ")
            action = action.strip()

            if action.lower() in ("quit", "q"):
                break

            builder = ACTION_BUILDERS.get(action)
            if builder is None:
                print("  Invalid action.\n", flush=True)
                continue

            action_name, build_fn = builder
            payload = await build_fn()
            if payload is None:
                continue

            sim_done = "returning to idle" if action == "4" else None
            await _send_and_wait(
                clients, ack_event, ack_text, node, payload,
                wait_until=sim_done,
            )

    except (KeyboardInterrupt, EOFError):
        print("\nExiting.", flush=True)

    for client in clients.values():
        try:
            await client.disconnect()
        except Exception:
            pass
    print("Disconnected from all nodes.", flush=True)
    return 0


def main() -> int:
    return asyncio.run(run())


if __name__ == "__main__":
    raise SystemExit(main())
