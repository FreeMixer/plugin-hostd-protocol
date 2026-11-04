#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Each fix tests/fixes_test.c holds, taken out again in a scratch copy of the sources: the test built over that copy
# must fail. A fix whose text is not in the source fails the run too, so this list follows the code.
#
#   fixes-sabotage.sh "<cc>" "<cflags>"
set -eu
cc=$1
cflags=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failed=0

# <issue> <file> <the fix, as sed matches it> <what it was>
sabotage() {
    issue=$1 file=$2 fixed=$3 mutant=$4
    rm -rf "$work/tree"
    mkdir -p "$work/tree"
    cp -r src include tests "$work/tree/"
    grep -q "$fixed" "$work/tree/$file" || { echo "FAIL $issue: the fix is not in $file"; failed=1; return; }
    sed -i "s/$fixed/$mutant/" "$work/tree/$file"
    # shellcheck disable=SC2086
    (cd "$work/tree" && $cc $cflags -o fixes_test tests/fixes_test.c src/*.c -lpthread -lm) \
        || { echo "FAIL $issue: the sabotaged tree does not build"; failed=1; return; }
    if (cd "$work/tree" && ./fixes_test) >"$work/log" 2>&1; then
        echo "FAIL $issue: fixes_test passes with the fix taken out of $file"
        failed=1
    else
        echo "ok   $issue: fixes_test fails with the fix taken out ($(grep -m1 -o 'FAIL #[0-9]*.*\|runtime error.*\|ERROR: AddressSanitizer: [a-z-]*' "$work/log" | head -c 120))"
    fi
}

sabotage "#371" src/host-dispatch.c 'if (isnan(value) || isinf(value))' 'if (0)'
sabotage "#372" src/protocol.c ' \&\& (j == 0 || match > 0)' ''
sabotage "#11" src/host-dispatch.c 'if (\*p == 0xc2 \&\& p\[1\] < 0xa0)' 'if (0)'

[ "$failed" = 0 ] && echo "fixes sabotage ok" || { echo "fixes sabotage FAILED"; exit 1; }
