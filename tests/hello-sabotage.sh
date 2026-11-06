#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# hello and request tags (tests/hello_test.c), and the scenario runner's reading of "not found" (tests/runner_test.c):
# each arm breaks one rule in a scratch copy of the sources, and the test built over that copy must fail on each.
#
#   hello-sabotage.sh "<cc>" "<cflags>"
set -eu
cc=$1
cflags=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failed=0

# <what> <file> <the code, as sed matches it> <a mutant> [<test>, hello_test by default]
sabotage() {
    what=$1 file=$2 fixed=$3 mutant=$4 prog=${5:-hello_test}
    rm -rf "$work/tree"
    mkdir -p "$work/tree"
    cp -r src include tests "$work/tree/"
    grep -qF "$(printf '%s' "$fixed" | sed 's/\\//g')" "$work/tree/$file" || { echo "FAIL $what: the code is not in $file"; failed=1; return; }
    sed -i "s/$fixed/$mutant/" "$work/tree/$file"
    # shellcheck disable=SC2086
    (cd "$work/tree" && $cc $cflags -o "$prog" "tests/$prog.c" src/*.c -lpthread -lm) \
        || { echo "FAIL $what: the sabotaged tree does not build"; failed=1; return; }
    if (cd "$work/tree" && "./$prog") >"$work/log" 2>&1; then
        echo "FAIL $what: $prog passes"
        failed=1
    else
        echo "ok   $what: $prog fails ($(grep -m1 '^FAIL' "$work/log" | head -c 120))"
    fi
}

sabotage "a new connection keeps the grant" src/socket.c 'if (socket_session_start) socket_session_start();' ''
sabotage "a tag is read without the grant" src/protocol.c \
    "if (data\[0\] == '@' \&\& granted(OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_TAGS))" "if (data[0] == '@' \&\& (granted(OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_TAGS) || 1))"
sabotage "the answer drops the tag" src/protocol.c 'line = MALLOC(tag_size + 1 + size);' 'line = NULL;'
sabotage "hello grants what it was not asked for" src/protocol.c 'want\[i\] = 0;' 'want[i] = 1;'
sabotage "a refused hello drops the grant" src/protocol.c \
    'protocol_response_int(ERR_INVALID_OPERATION, proto);' 'memset(g_granted, 0, sizeof(g_granted)); protocol_response_int(ERR_INVALID_OPERATION, proto);'
sabotage "a tag takes an @" src/protocol.c "c == '.';" "c == '.' || c == '@';"
sabotage "an error word without the grant" src/protocol.c \
    'if (granted(OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_ERRORS))' 'if (granted(OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_ERRORS) || 1)'
sabotage "an error word after a reply with values" src/protocol.c 'if (strcmp(bare, proto->response) != 0)' 'if (0)'
sabotage "a feedback line numbered without the grant" src/protocol.c \
    'if (!granted(OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_SEQ))' 'if (0)'
sabotage "a hello that grants seq does not start again at 1" src/protocol.c \
    '(void)__atomic_exchange_n(\&g_seq, 0, __ATOMIC_SEQ_CST);' ''
sabotage "param_set_many sets before it checks" src/host-dispatch.c 'if (checked < SUCCESS)' 'if (0)'
sabotage "param_set_many sets nothing back" src/host-dispatch.c \
    'g_backend->param_set(instance, proto->list\[2 + 2 \* j\], held\[j\]);' ';'
sabotage "a required verb not found is a verb not served" src/host-scenario.c 'if (level == VERB_REQUIRED)' 'if (0)' \
    runner_test
sabotage "the host named is not read" src/host-scenario.c '(!g_host || strcmp(hosts\[h\], g_host) == 0)' '(!g_host || hosts[h] != NULL)' runner_test
sabotage "any not found is a verb not served" src/host-scenario.c \
    'level = strcmp(reply, "not found") == 0 ? verb_level(verb) : VERB_UNDECLARED;' \
    'level = strcmp(reply, "not found") == 0 ? ((void)verb_level(verb), VERB_NOT_REQUIRED) : VERB_UNDECLARED;' runner_test

# the curve verbs (tests/curve_test.c)
sabotage "remove keeps the instance's curve bindings" src/host-dispatch.c 'host_curve_forget(atoi(proto->list\[1\]));' '(void)proto;' curve_test
sabotage "a line of another family keeps the old terms" src/host-curve.c '        b->nterms = 0;' '        (void)b;' curve_test
sabotage "a refused term keeps the line's other terms" src/host-curve.c '        if (code < 0)' '        if (code < 0 \&\& i == 0)' curve_test
sabotage "a band_type's value is not read" src/host-curve.c 't->shapes\[(uint32_t)c\]' 't->shapes[0]' curve_test
sabotage "the instance's rate is not read" src/host-curve.c 'rq.sample_rate = rate;' 'rq.sample_rate = 48000.0;' curve_test
sabotage "a slope's enumeration is not read" src/host-curve.c 'rqt\[i\].value = t->slopes\[(uint32_t)c\], rqt\[i\].unit = OMX_CURVE_UNITS_DB;' 'rqt[i].unit = OMX_CURVE_UNITS_DB;' curve_test
sabotage "a value that is not finite answered -902" src/host-curve.c 'case OMX_CURVE_VALUE_NOT_FINITE: return OMX_REPLY_PLUGIN_HOSTD_CURVE_VALUE_NOT_FINITE_RESP;' '' curve_test
sabotage "y written to 0.1 dB" src/host-curve.c ' %s,%.2f' ' %s,%.1f' curve_test
sabotage "a host without the hook answers not found" src/host-dispatch.c ' OMX_VERB_CURVE_BIND_FMT, OMX_VERB_CURVE_POINTS_FMT,' '' curve_test

[ "$failed" = 0 ] && echo "hello sabotage ok" || { echo "hello sabotage FAILED"; exit 1; }
