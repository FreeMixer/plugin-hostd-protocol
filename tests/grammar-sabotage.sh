#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# tests/grammar_test.py against a scratch copy of the grammar with one rule broken in each arm: it must fail on each,
# so a grammar test that holds nothing is caught.
#
#   sh tests/grammar-sabotage.sh          (make test-grammar)
set -eu
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failed=0
PYTHON=${PYTHON:-python3}

# <name> <text, as sed matches it> <what it becomes>
arm() {
    name=$1 from=$2 to=$3
    cp grammar/plugin-hostd-protocol.abnf "$work/g.abnf"
    grep -q "$from" "$work/g.abnf" || { echo "FAIL $name: \"$from\" is not in the grammar"; failed=1; return; }
    sed -i "0,\\#$from#s##$to#" "$work/g.abnf"
    if $PYTHON tests/grammar_test.py "$work/g.abnf" >"$work/log" 2>&1; then
        echo "FAIL $name: the grammar test passes"
        failed=1
    else
        echo "ok   $name: red ($(head -n1 "$work/log" | head -c 110))"
    fi
}

arm "an int is letters" 'int              = \["-"\] 1\*DIGIT' 'int              = 1*ALPHA'
arm "no non-finite float" 'non-finite       = "nan" / "infinity" / "inf"' 'non-finite       = "nan"'
arm "a verb one word short" '%s"param_get" SP int SP word' '%s"param_get" SP int'
arm "a reply is not resp" 'resp-line        = %s"resp"' 'resp-line        = %s"rsp"'
arm "a quoted word holds no space" 'quoted-char      = %x20-21' 'quoted-char      = %x21'
arm "no parse refusal" '%s"few arguments" / ' ''
arm "no tagged request" 'request          = \[tag SP\] command' 'request          = command'
arm "a tag takes an @" 'tag-char         = ALPHA' 'tag-char         = "@" / ALPHA'
arm "hello grants one twice" 'hello-reply      = %s"resp" SP %s"0" SP int' 'hello-reply      = %s"resp" SP %s"0" SP int *(SP extension)'
arm "an error word of another number" 'error-reply      = %s"resp -1 ' 'error-reply      = %s"resp -3 '
arm "a feedback line numbered 0" '%x31-39 \*DIGIT' '%x30-39 *DIGIT'
arm "a curve role omx-dsp does not have" '%s"knee_end"' '%s"knee_end" / %s"attack"'
arm "a curve binding of no term" 'curve-family 1\*(SP curve-term)' 'curve-family *(SP curve-term)'
arm "a curve y to 0.1 dB" 'curve-y          = \["-"\] 1\*DIGIT "." 2DIGIT' 'curve-y          = ["-"] 1*DIGIT "." 1*DIGIT'

[ "$failed" = 0 ] && echo "grammar sabotage ok" || { echo "grammar sabotage FAILED"; exit 1; }
