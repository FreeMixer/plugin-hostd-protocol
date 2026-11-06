# SPDX-License-Identifier: GPL-3.0-or-later
# The LV2 isolated tier is omx-clap-host's LV2 adapter, and mod-host runs a plugin only when it is asked for: the daemon's
# own lv2_worker, as protocol/plugin-hostd.json declares it, is omx-clap-host, never mod-host. No word hands a plugin the
# chosen program refuses to another host: auto, which did, is not a value of lv2_worker or lv2_worker_set, and the
# lv2_adapter_refused event never names it.
import json, sys

document = json.load(open(sys.argv[1]))
setting = [c for c in document["config"] if c["key"] == "lv2_worker"]
if len(setting) != 1:
    sys.exit("FAIL %s declares lv2_worker %d times" % (sys.argv[1], len(setting)))
if setting[0]["default"] != "omx-clap-host":
    sys.exit("FAIL the default lv2_worker is %r, not omx-clap-host: mod-host runs a plugin only when asked" % setting[0]["default"])
print("ok   the default lv2_worker is omx-clap-host")

verb = [v for v in document["verbs"] if v["name"] == "lv2_worker_set"]
event = [e for e in document["events"] if e["name"] == "lv2_adapter_refused"]
if len(verb) != 1 or len(event) != 1:
    sys.exit("FAIL %s declares lv2_worker_set %d times and lv2_adapter_refused %d times" % (sys.argv[1], len(verb), len(event)))
if verb[0]["arguments"] != "<uri> <mod-host | omx-clap-host | ->":
    sys.exit("FAIL lv2_worker_set takes %r: the user names mod-host or omx-clap-host, nothing chooses for them" % verb[0]["arguments"])
if event[0]["fields"] != "<uri> <reason> omx-clap-host":
    sys.exit("FAIL lv2_adapter_refused carries %r: a refusal is reported, never handed on" % event[0]["fields"])
for words in (setting[0]["meaning"], [e for e in document["errors"] if e["name"] == "LV2_ADAPTER_REFUSED"][0]["meaning"]):
    if "auto" in words.split() or "auto (" in words:
        sys.exit("FAIL a declaration still offers auto: %r" % words)
print("ok   no lv2 worker hands a refused plugin to another host")
