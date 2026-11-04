#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# The checks of the generated files, against a scratch copy of the tree with one thing changed in each arm: a generated
# file edited by hand, or a declaration it is written from changed without writing it again. check-generated or
# check-protocol-doc must fail on each, and the copy left as it is must pass, so a check that cannot fail is caught.
# Two arms hold tools/protocol-table.py's rule that a word of ours never changes a mod-host word: a new word of ours
# that takes as many arguments as mod-host's and other types is refused, and so is a second form of a word (preset_show)
# changed so that a line of mod-host's form is a line of it too. There is no list of exceptions to put either in.
#
#   sh tests/generated-sabotage.sh          (make test-generated-sabotage)
set -eu
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failed=0
MAKE=${MAKE:-make}

fresh() {
    rm -rf "$work/tree"
    mkdir -p "$work/tree"
    cp -r Makefile VERSION README.md include src data protocol grammar doc tools tests packaging "$work/tree/"
}

checks() {
    (cd "$work/tree" && $MAKE -s check-generated check-protocol-doc) >"$work/log" 2>&1
}

# <name> <file> <text, as sed matches it> <what it becomes>
arm() {
    name=$1 file=$2 from=$3 to=$4
    fresh
    grep -q "$from" "$work/tree/$file" || { echo "FAIL $name: \"$from\" is not in $file"; failed=1; return; }
    sed -i "0,/$from/s//$to/" "$work/tree/$file"
    if checks; then
        echo "FAIL $name: the checks pass with $file changed"
        failed=1
    else
        echo "ok   $name: red ($(grep -v 'current$' "$work/log" | grep -m1 -i 'differ\|not what\|FAIL\|error\|region' | head -c 110))"
    fi
}

# <name> <file> <python that edits the parsed JSON d> <the refusal, as grep matches it>
table_arm() {
    name=$1 file=$2 edit=$3 refusal=$4
    fresh
    python3 - "$work/tree/$file" "$edit" <<'PY'
import json, sys
path, edit = sys.argv[1], sys.argv[2]
d = json.load(open(path))
exec(edit)
json.dump(d, open(path, "w"), indent=2)
PY
    if (cd "$work/tree" && python3 tools/protocol-table.py) >"$work/log" 2>&1; then
        echo "FAIL $name: tools/protocol-table.py writes the table"
        failed=1
    elif grep -q "$refusal" "$work/log"; then
        echo "ok   $name: refused ($(head -c 110 "$work/log"))"
    else
        echo "FAIL $name: refused, but not for this: $(head -c 200 "$work/log")"
        failed=1
    fi
}

fresh
if checks; then echo "ok   control: the copy as it is passes"; else echo "FAIL control: the copy as it is fails"; cat "$work/log"; failed=1; fi

arm "plugin-hostd.json by hand" protocol/plugin-hostd.json '"worker_list"' '"worker_lists"'
arm "protocol.h not generated" include/plugin-hostd/protocol.h '"an LV2 plugin.s worker' '"an LV2 worker'
arm "mod-host.h not generated" include/mod-host.h '"remove %i"' '"destroy %i"'
arm "mod-host.json by hand" protocol/mod-host.json '"cpu_load"' '"cpu_loads"'
arm "verbs.json not generated" data/verbs.json '"resp 0 <load_key> <name>"' '"resp 0 <name>"'
arm "the table by hand" protocol/plugin-hostd-protocol.json '"origin": "omx-clap-core"' '"origin": "mod-host"'
arm "the grammar by hand" grammar/plugin-hostd-protocol.abnf '%s"bypass" SP int SP int' '%s"bypass" SP int'
arm "the grammar's core not generated" grammar/core.abnf 'int              = \["-"\] 1\*DIGIT' 'int              = 1*DIGIT'
arm "the README region by hand" README.md '| `bypass` | mod-ui | required' '| `bypass` | mod-ui | optional'
arm "protocol.md by hand" doc/protocol.md 'is a superset of' 'extends'
arm "the evidence not generated" doc/evidence/mod-ui-verbs.tsv 'connect' 'konnect'
arm "the error words by hand" src/reply-words.h '"reply.host.instance-non-exists"' '"reply.host.no-instance"'
arm "a reply code not generated" data/verbs.json '"reply.plugin-hostd.no-such-worker"' '"reply.plugin-hostd.no-worker"'
arm "mod-host's page not generated" doc/mod-host-commands.md 'toggle effect processing' 'toggle processing'

table_arm "a word of ours that changes mod-host's" data/verbs.json \
    'd["PLUGIN_HOSTD_VERBS"]["verbs"]["bypassAll"] = {"verb": "bypass", "format": "bypass %s %s", "reply": "resp 0", "implementedBy": {"plugin-hostd": "required"}, "doc": "a word of ours"}' \
    "changes mod-host's bypass"
table_arm "a second form a mod-host line is a line of" data/verbs.json \
    'v = d["PLUGIN_HOSTD_VERBS"]["verbs"]["presetShow"]; v["format"] = "preset_show %i"' \
    "changes mod-host's preset_show"
table_arm "one refusal number, two codes" data/verbs.json \
    'd["PLUGIN_HOSTD_REPLY_CODES"]["codes"]["noSuchWorker"]["resp"] = -3' \
    "is declared as more than one code"

[ "$failed" = 0 ] && echo "generated sabotage ok" || { echo "generated sabotage FAILED"; exit 1; }
