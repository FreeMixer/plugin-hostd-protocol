#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# protocol_parse() reads every form of a verb (tests/forms_test.c): each arm puts back what it replaced in a scratch copy
# of the sources, and forms_test built over that copy must fail on each.
#
#   forms-sabotage.sh "<cc>" "<cflags>"
set -eu
cc=$1
cflags=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failed=0

# <what> <the code, as sed matches it> <what it was, or a mutant>
sabotage() {
    what=$1 fixed=$2 mutant=$3
    rm -rf "$work/tree"
    mkdir -p "$work/tree"
    cp -r src include tests "$work/tree/"
    grep -qF "$(printf '%s' "$fixed" | sed 's/\\//g')" "$work/tree/src/protocol.c" || { echo "FAIL $what: the code is not in src/protocol.c"; failed=1; return; }
    sed -i "s/$fixed/$mutant/" "$work/tree/src/protocol.c"
    # shellcheck disable=SC2086
    (cd "$work/tree" && $cc $cflags -o forms_test tests/forms_test.c src/*.c -lpthread -lm) \
        || { echo "FAIL $what: the sabotaged tree does not build"; failed=1; return; }
    if (cd "$work/tree" && ./forms_test) >"$work/log" 2>&1; then
        echo "FAIL $what: forms_test passes"
        failed=1
    else
        echo "ok   $what: forms_test fails ($(grep -m1 '^FAIL' "$work/log" | head -c 120))"
    fi
}

# the first command of the verb decides, whatever its forms, as before 0.2.0
sabotage "the first form of a verb decides" 'if (result >= 0 \&\& typed(\&g_commands\[i\], \&proto))' \
    'if (index = result, typed(\&g_commands[i], \&proto) || 1)'
# the types do not choose between forms of one count
sabotage "the types do not choose" 'if (result >= 0 \&\& typed(\&g_commands\[i\], \&proto))' 'if (result >= 0 \&\& (typed(\&g_commands[i], \&proto) || 1))'

[ "$failed" = 0 ] && echo "forms sabotage ok" || { echo "forms sabotage FAILED"; exit 1; }
