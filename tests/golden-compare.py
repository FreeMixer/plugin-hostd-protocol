#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Holds this library's golden transcript to mod-host's: every exchange of mod-host's transcript is in ours, the same
# reply and the same backend calls, except the exchanges deviations.txt names (by input, every answer of it), each of
# which must differ, so the list never names a fix that is not there.
#
#   golden-compare.py <mod-host transcript> <our transcript> <deviations.txt>
import sys


def blocks(path):
    out = {}
    with open(path, "rb") as f:
        lines = f.read().split(b"\n")
    for i in range(0, len(lines) - 1, 4):
        head = lines[i].decode()
        assert head.startswith("# "), "%s:%d: not a block header" % (path, i + 1)
        out[head[2:]] = b"\n".join(lines[i + 1:i + 4])
    return out


theirs, ours = blocks(sys.argv[1]), blocks(sys.argv[2])
with open(sys.argv[3]) as f:
    deviations = {}
    for line in f:
        line = line.split("#", 1)[0].strip()
        if line:
            name, issue = line.rsplit(None, 1)
            deviations[name] = issue

failed = 0
same = differ = 0
for key, block in theirs.items():
    name = key.rsplit(" answer ", 1)[0]
    if key not in ours:
        print("missing from ours: %s" % key)
        failed = 1
    elif name in deviations:
        differ += ours[key] != block
    elif ours[key] != block:
        print("differs from mod-host's: %s\n  mod-host: %r\n  ours:     %r" % (key, block, ours[key]))
        failed = 1
    else:
        same += 1
for name, issue in deviations.items():
    keys = [k for k in theirs if k.rsplit(" answer ", 1)[0] == name]
    if not keys:
        print("deviations.txt names %s (%s), which mod-host's transcript does not have" % (name, issue))
        failed = 1
    elif all(ours[k] == theirs[k] for k in keys if k in ours):
        print("deviations.txt names %s (%s), but ours answers it as mod-host does" % (name, issue))
        failed = 1
added = len(set(ours) - set(theirs))
print("golden vs mod-host-protocol: %d exchanges identical, %d differ as deviations.txt says (%s), %d only ours"
      % (same, differ, ", ".join(sorted(set(deviations.values()))) or "none", added))
sys.exit(failed)
