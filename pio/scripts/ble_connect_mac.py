#!/usr/bin/env python3
"""Interactive BLE terminal for Poolantir nodes on macOS. Requires: pip install bleak

Retries forever until BLE connects (no overall timeout). After connect, type lines to send;
Ctrl+D or :quit ends the session. Ctrl+C aborts the script anytime.
"""

from __future__ import annotations

import argparse
import asyncio
import sys

from bleak import BleakClient, BleakScanner

CHAR_UUID = "beb5483e-36e1-4688-b7f5-e073f246f7d4"
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"


def _on_notify(_sender, data: bytearray) -> None:
    text = data.decode("utf-8", errors="replace")
    print(f"[notify] {text}", flush=True)


def _start_stdin_queue() -> asyncio.Queue[str | None]:
    """Lines from stdin; queue receives None on EOF."""
    q: asyncio.Queue[str | None] = asyncio.Queue()

    async def _pump() -> None:
        while True:
            line = await asyncio.to_thread(sys.stdin.readline)
            if line == "":
                await q.put(None)
                return
            await q.put(line.rstrip("\r\n"))

    asyncio.create_task(_pump())
    return q


def _print_metadata(args: argparse.Namespace) -> None:
    print("=== Poolantir BLE terminal ===", flush=True)
    if args.address:
        print(f"Connection mode: direct address", flush=True)
        print(f"BLE address:     {args.address}", flush=True)
        print("Target name:     (resolved after connect)", flush=True)
    elif args.id:
        print(f"Connection mode: scan by name", flush=True)
        print(f"Target name:     poolantir-node-{args.id}", flush=True)
        print("BLE address:     (from scan)", flush=True)
    else:
        print("Connection mode: scan for sole poolantir-node-*", flush=True)
        print("Target name:     poolantir-node-* (exactly one in range)", flush=True)
        print("BLE address:     (from scan)", flush=True)
    print(f"Service UUID:    {SERVICE_UUID}", flush=True)
    print(f"Data char UUID:  {CHAR_UUID}", flush=True)
    print("", flush=True)
    print("", flush=True)


def _norm_addr(a: str) -> str:
    return a.replace("-", ":").upper()


async def _find_target(
    args: argparse.Namespace, multiple_warned: list[bool]
) -> tuple[str, str] | None:
    """Return (ble_address, advertising_name) or None if not resolved this round."""

    # Short scan slice per attempt (Bleak requires a duration); script itself has no max retries.
    devices = await BleakScanner.discover(timeout=1.0)

    if args.address:
        want = _norm_addr(args.address)
        for d in devices:
            if d.address and _norm_addr(str(d.address)) == want:
                label = (d.name and str(d.name).strip()) or str(d.address)
                return (str(d.address), label)
        return (args.address, args.address)

    if args.id:
        need = f"poolantir-node-{args.id}"
        candidates = [d for d in devices if d.name and str(d.name) == need]
    else:
        candidates = [d for d in devices if d.name and str(d.name).startswith("poolantir-node-")]
    if not candidates:
        return None
    if len(candidates) > 1 and not args.id:
        if not multiple_warned[0]:
            print(
                "Multiple poolantir devices in range; pass --id <n> or --address to pick one.",
                file=sys.stderr,
                flush=True,
            )
            multiple_warned[0] = True
        return None
    d0 = candidates[0]
    label = (d0.name and str(d0.name).strip()) or str(d0.address)
    return (str(d0.address), label)


async def _interactive_tx(client: BleakClient) -> None:
    await client.start_notify(CHAR_UUID, _on_notify)
    print(
        "Transmission mode: type a message and press Enter to send to the ESP32.\n"
        "  Replies from the device appear as [notify] …\n"
        "  Exit: Ctrl+D or :quit\n",
        flush=True,
    )

    line_q = _start_stdin_queue()
    while True:
        line = await line_q.get()
        if line is None:
            print("[stdin] EOF — closing.", flush=True)
            break
        if line.strip() == ":quit":
            print("[stdin] :quit — closing.", flush=True)
            break
        if not line:
            continue
        payload = line.encode("utf-8")
        await client.write_gatt_char(CHAR_UUID, payload, response=True)
        print(f"[sent] {line}", flush=True)


async def run(args: argparse.Namespace) -> int:
    _print_metadata(args)
    multiple_warned = [False]

    while True:
        try:
            found = await _find_target(args, multiple_warned)
            if found is None:
                print("device not found...", flush=True)
                await asyncio.sleep(10)
                continue

            addr, device_name = found
            async with BleakClient(addr) as client:
                print(f"device connected {device_name}!", flush=True)
                await _interactive_tx(client)

            print("Disconnected.", flush=True)
            return 0

        except KeyboardInterrupt:
            print("\nAborted (Ctrl+C).", flush=True)
            return 130

        except Exception:
            print("device not found...", flush=True)
            await asyncio.sleep(2)


def main() -> int:
    p = argparse.ArgumentParser(
        description="Interactive BLE terminal for a Poolantir ESP32 (macOS). "
        "Retries until connected; then send lines from this terminal to the device."
    )
    p.add_argument("--id", help="Match peripheral name poolantir-node-<id> (recommended if several nodes).")
    p.add_argument("--address", help="Bluetooth address, e.g. AA:BB:CC:DD:EE:FF (skip scan).")
    args = p.parse_args()
    return asyncio.run(run(args))


if __name__ == "__main__":
    raise SystemExit(main())
