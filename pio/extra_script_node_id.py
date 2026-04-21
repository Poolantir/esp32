# Injects POOLANTIR_NODE_ID from the environment into the build so each flash
# can set the id (use scripts/flash_poolantir.py --id <value> or
# POOLANTIR_NODE_ID=<value> pio run ...).
Import("env")

import os

node_id = os.environ.get("POOLANTIR_NODE_ID", "0")
# Escape for a C string literal inside -DPOOLANTIR_NODE_ID="..."
escaped = node_id.replace("\\", "\\\\").replace('"', '\\"')
env.Append(BUILD_FLAGS=['-DPOOLANTIR_NODE_ID=\\"' + escaped + '\\"'])
