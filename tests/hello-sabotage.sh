#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# hello and request tags (tests/hello_test.c): each arm breaks one rule in a scratch copy of the sources, and hello_test
# built over that copy must fail on each.
#
#   hello-sabotage.sh "<cc>" "<cflags>"
set -eu
cc=$1
cflags=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failed=0

# <what> <file> <the code, as sed matches it> <a mutant>
sabotage() {
    what=$1 file=$2 fixed=$3 mutant=$4
    rm -rf "$work/tree"
    mkdir -p "$work/tree"
    cp -r src include tests "$work/tree/"
    grep -qF "$(printf '%s' "$fixed" | sed 's/\\//g')" "$work/tree/$file" || { echo "FAIL $what: the code is not in $file"; failed=1; return; }
    sed -i "s/$fixed/$mutant/" "$work/tree/$file"
    # shellcheck disable=SC2086
    (cd "$work/tree" && $cc $cflags -o hello_test tests/hello_test.c src/*.c -lpthread -lm) \
        || { echo "FAIL $what: the sabotaged tree does not build"; failed=1; return; }
    if (cd "$work/tree" && ./hello_test) >"$work/log" 2>&1; then
        echo "FAIL $what: hello_test passes"
        failed=1
    else
        echo "ok   $what: hello_test fails ($(grep -m1 '^FAIL' "$work/log" | head -c 120))"
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

[ "$failed" = 0 ] && echo "hello sabotage ok" || { echo "hello sabotage FAILED"; exit 1; }
