#!/usr/bin/env python3
"""Flash poolantir firmware; pass --id to set the BLE device suffix (stored in NVS on boot).

Example:
  python3 scripts/flash_poolantir.py --id kitchen-1
  python3 scripts/flash_poolantir.py --id 7 -- pio run -e poolantir_simulation -t upload -t monitor
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description="Flash poolantir with POOLANTIR_NODE_ID set.")
    parser.add_argument(
        "--id",
        default=os.environ.get("POOLANTIR_NODE_ID", "0"),
        help="Node id suffix; advertised name is poolantir-node-<id> (default: 0 or env).",
    )
    parser.add_argument(
        "pio_args",
        nargs=argparse.REMAINDER,
        help="Extra args after '--' are passed to pio (default: run -e poolantir_simulation -t upload).",
    )
    args = parser.parse_args()

    env = os.environ.copy()
    env["POOLANTIR_NODE_ID"] = args.id

    remainder = args.pio_args
    if remainder and remainder[0] == "--":
        remainder = remainder[1:]

    if remainder:
        cmd = ["pio", *remainder]
    else:
        cmd = ["pio", "run", "-e", "poolantir_simulation", "-t", "upload"]

    return subprocess.call(cmd, env=env)


if __name__ == "__main__":
    sys.exit(main())
