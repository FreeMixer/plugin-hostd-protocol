# SPDX-License-Identifier: GPL-3.0-or-later
# The LV2 isolated tier is omx-clap-host's LV2 adapter, and mod-host runs a plugin only when it is asked for: the daemon's
# own lv2_worker, as protocol/plugin-hostd.json declares it, is omx-clap-host, never mod-host and never auto (which falls
# back to mod-host by itself).
import json, sys

document = json.load(open(sys.argv[1]))
setting = [c for c in document["config"] if c["key"] == "lv2_worker"]
if len(setting) != 1:
    sys.exit("FAIL %s declares lv2_worker %d times" % (sys.argv[1], len(setting)))
if setting[0]["default"] != "omx-clap-host":
    sys.exit("FAIL the default lv2_worker is %r, not omx-clap-host: mod-host runs a plugin only when asked" % setting[0]["default"])
print("ok   the default lv2_worker is omx-clap-host")
