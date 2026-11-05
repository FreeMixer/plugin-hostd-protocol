#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# check-rendered against a scratch copy of the tree with one rendered header changed, and with data/verbs.json changed
# and the header not rendered again: it must fail on each, and pass on the copy as it is.
#
#   sh tests/rendered-sabotage.sh          (make check-protocol-tools)
set -eu
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failed=0
MAKE=${MAKE:-make}

fresh() {
    rm -rf "$work/tree"
    mkdir -p "$work/tree"
    cp -r Makefile VERSION include data "$work/tree/"
}

check() {
    (cd "$work/tree" && $MAKE -s check-rendered) >"$work/log" 2>&1
}

# <name> <file> <text, as sed matches it> <what it becomes>
arm() {
    name=$1 file=$2 from=$3 to=$4
    fresh
    grep -q "$from" "$work/tree/$file" || { echo "FAIL $name: \"$from\" is not in $file"; failed=1; return; }
    sed -i "0,/$from/s//$to/" "$work/tree/$file"
    if check; then
        echo "FAIL $name: check-rendered passes with $file changed"
        failed=1
    else
        echo "ok   $name: red ($(grep -v 'current$' "$work/log" | head -n1 | head -c 110))"
    fi
}

fresh
if check; then echo "ok   control: the copy as it is passes"; else echo "FAIL control: the copy as it is fails"; cat "$work/log"; failed=1; fi
arm "plugin-hostd's verbs header by hand" include/plugin-hostd/plugin_hostd_protocol_host_verbs.h '"chain_get %i"' '"chain_get %s"'
arm "the reply codes header by hand" include/plugin-hostd/plugin_hostd_protocol_vocabulary.h '(-522)' '(-599)'
arm "omx-clap-core's verbs header by hand" include/plugin-hostd/omx_clap_core_host_verbs.h '"note_on %i %i %i %i"' '"note_on %i %i %i"'
arm "plugin-db's words header by hand" include/plugin-hostd/plugin_db_vocabulary.h '"isolated"' '"alone"'
arm "verbs.json not rendered" data/verbs.json '"format": "chain_get %i"' '"format": "chain_get %i %i"'

[ "$failed" = 0 ] && echo "rendered sabotage ok" || { echo "rendered sabotage FAILED"; exit 1; }
