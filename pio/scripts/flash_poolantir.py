# AI-ASSISTED
# Matt Krueger
# April 2026

#!/usr/bin/env python3
"""Flash poolantir firmware; pass --id to set the BLE device suffix (stored in NVS on boot).

Example:
  python3 scripts/flash_poolantir.py --id 1
  python3 scripts/flash_poolantir.py --id 1 --dummy --monitor
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys


def _find_pio() -> str:
    found = shutil.which("pio")
    if found:
        return found
    home_bin = os.path.expanduser("~/.platformio/penv/bin/pio")
    if os.path.isfile(home_bin):
        return home_bin
    return "pio"


def main() -> int:
    parser = argparse.ArgumentParser(description="Flash poolantir with POOLANTIR_NODE_ID set.")
    parser.add_argument(
        "--id",
        default=os.environ.get("POOLANTIR_NODE_ID", "0"),
        help="Node id suffix; advertised name is poolantir-node-<id> (default: 0 or env).",
    )
    parser.add_argument(
        "--dummy",
        action="store_true",
        help="Flash the dummy terminal firmware (poolantir_dummy_terminal) instead of production.",
    )
    parser.add_argument(
        "--monitor",
        action="store_true",
        help="Open the serial monitor after uploading.",
    )
    args = parser.parse_args()

    env = os.environ.copy()
    env["POOLANTIR_NODE_ID"] = args.id

    pio = _find_pio()
    pio_env = "poolantir_dummy_terminal" if args.dummy else "poolantir_simulation"
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    pio_ini = os.path.join(project_root, "platformio.ini")
    if not os.path.isfile(pio_ini):
        print(f"Could not find platformio.ini at: {pio_ini}", file=sys.stderr)
        return 1

    cmd = [pio, "run", "-e", pio_env, "-t", "upload"]
    if args.monitor:
        cmd += ["-t", "monitor"]

    return subprocess.call(cmd, env=env, cwd=project_root)


if __name__ == "__main__":
    sys.exit(main())
