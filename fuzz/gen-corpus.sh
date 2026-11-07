#!/bin/sh
# Writes the seed corpora into $1 (default fuzz/corpus): one file per seed, named by what it is. Seeds come
# from tests/host-scenarios.txt and from the verbs of the protocol, never from a network capture.
set -eu
out=${1:-fuzz/corpus}
scen=tests/host-scenarios.txt

for h in protocol dispatch scenario session; do
    rm -rf "$out/$h"
    mkdir -p "$out/$h"
done

# every command of the scenario file, with its $NAMEs given a value; a line of a session (hello, and a line that
# follows it, guarded by @hello) means something only after its hello, so the session seed below carries those
n=0
sed -e '/^hello /d' -e '/^@hello /d' -e 's/^@[^ ]* //' -e 's/ *=>.*$//' -e '/^#/d' -e '/^$/d' \
    -e 's#\$URI#http://example.org/plugin#g' -e 's#\$BAD_URI#urn:no:such:plugin#g' \
    -e 's#\$PARAM#gain#g' -e 's#\$DIR#/tmp/state#g' -e 's#\$PORT_A#system:capture_1#g' \
    -e 's#\$PORT_B#system:playback_1#g' "$scen" | sort -u | while IFS= read -r cmd; do
    n=$((n + 1))
    printf '%s' "$cmd" > "$out/protocol/scenario-$n"
    printf '\001%s' "$cmd" > "$out/dispatch/scenario-$n"
done

# the verbs the scenario file does not carry, and the shapes a parser has to refuse
i=0
while IFS= read -r cmd; do
    i=$((i + 1))
    printf '%s' "$cmd" > "$out/protocol/extra-$i"
    printf '\002%s' "$cmd" > "$out/dispatch/extra-$i"
done <<'SEEDS'
cpu_load
quit
monitor_output 3 gain
track_info 3 "Lead Vocal" #FF8800 bus
track_info 3 x - master
remote_pages 3
remote_page_get 3 0
param_info 3 cutoff
connect system:capture_1 system:playback_1
disconnect system:capture_1 system:playback_1
param_set 3 gain
param_set 3 gain 1 2
add
add urn:x
remove
remove 3 4
unknown_verb 1 2 3
patch_set 3 urn:p "a b c"
state_save
%i %s %f
param_get 99999999999999999999 gain
param_set -1 gain nan
zzz %s 5
add urn:x 5 ... c
SEEDS

# an add of a bare clap: uri of high bytes, a control byte, a CR LF and a NUL: the dispatcher's model read such an
# add's uri differently from phd_add_uri, and check_value aborted (fixed in 0.4.2); kept byte for byte
printf '\002add clap:ur\371\371_m\371\371\371\371\371/72 \371& #FF8800\02301  8 \371\371\371\371\371\371\371\371\012\015\012\000\371\371\371371 \371' > "$out/dispatch/add-clap-high-bytes"

# a verb whose reply carries a value, with a salt that makes the backend succeed
printf '\006param_get 3 gain' > "$out/dispatch/param-get"

# quotes join words (the example of doc/protocol.md for track_info), and names of UTF-8: valid ones of two, three and
# four bytes, and an overlong form, a surrogate, a code point past U+10FFFF, a lead byte with no tail and the C1 control
# characters U+0080 and U+009F, each refused
printf 'connect "a b" c' > "$out/protocol/quoted-words"
printf '\002track_info 0 "Kick In" #FF8000' > "$out/dispatch/track-info-quoted"
printf '\002track_info 0 "Voz \303\251 \342\202\254 \360\237\216\270" #FF8000' > "$out/dispatch/track-info-utf8"
printf '\002track_info 0 \301\277 #FF8000' > "$out/dispatch/track-info-overlong"
printf '\002track_info 0 \340\200\257 #FF8000' > "$out/dispatch/track-info-overlong3"
printf '\002track_info 0 \355\240\200 #FF8000' > "$out/dispatch/track-info-surrogate"
printf '\002track_info 0 \364\220\200\200 #FF8000' > "$out/dispatch/track-info-beyond"
printf '\002track_info 0 Kick\303 #FF8000' > "$out/dispatch/track-info-no-tail"
printf '\002track_info 0 \302\200 #FF8000' > "$out/dispatch/track-info-c1"
printf '\002track_info 0 a\302\237b #FF8000' > "$out/dispatch/track-info-c1-inside"

# param_info of a port whose maximum, minimum or default is not finite (the backend's symbols inf, -inf and nan)
printf '\002param_info 3 inf' > "$out/dispatch/param-info-inf"
printf '\002param_info 3 -inf' > "$out/dispatch/param-info-neg-inf"
printf '\002param_info 3 nan' > "$out/dispatch/param-info-nan"

# the sessions of fuzz_session (fuzz/fuzz_session.c): one command a line, run in order on one connection
sed -n -e '/^hello /p' -e 's/^@hello //p' "$scen" | sed -e 's/ *=>.*$//' -e 's#\$PARAM#gain#g' \
    > "$out/session/host-scenarios"
printf 'hello 1 tags\n@a param_get 3 gain\n@a.b-c_d frobnicate 1\n@ remove 3\n@x\nhello 1\n@a param_get 3 gain\n' \
    > "$out/session/tags"
printf 'param_get 3 gain\n@a param_get 3 gain\nhello 1 tags tags zzz\n@a@b remove 3\nhello 2\nhello -1 tags\n' \
    > "$out/session/mixed"
printf 'refuse -3\nhello 1 errors\nrefuse -3\nrefuse -2\nrefuse -3 x\nrefuse -902\nhello 1 tags errors\n@a refuse -103\nrefuse 0\nhello 0\n' \
    > "$out/session/errors"
printf 'emit 1\nhello 1 seq\nemit 2\nemit 3\nhello 1 seq tags\nemit 4\n@a emit 5\nhello 1 seq\nemit 6\nhello 1\nemit 7\n' \
    > "$out/session/seq"
printf 'param_get_many 3 gain cutoff\nparam_get_many 3 gain xbad gain\nparam_get_many 9 gain\nhello 1 errors\nparam_get_many 9 gain\nparam_get_many 3 a "b c" ddd\nparam_get_many 3\n' \
    > "$out/session/many"
printf 'param_set_many 3 a 0.5 bb 0.25\nparam_get_many 3 a bb\nparam_set_many 3 a 0.1 xbad 0.2 bb 0.3\nparam_get_many 3 a bb\nparam_set_many 3 a 0.1 bb 2000\nparam_set_many 9 a 1\nparam_set_many 3 a notanumber\nparam_set_many 3 a 1 bb\n' \
    > "$out/session/setmany"

# the scenario file reader: the real file, and its edges
cp "$scen" "$out/scenario/host-scenarios"
printf 'add $URI 3 => resp 3\n@state_save remove 3 => resp 0\n' > "$out/scenario/guarded"
printf 'no arrow here\n' > "$out/scenario/no-arrow"
printf '@guard_only => x\n' > "$out/scenario/guard-no-command"
printf 'cmd $URI $UNKNOWN $EMPTY => $PORT_A | resp 0\n' > "$out/scenario/vars"
printf '# comment\n\n   \n' > "$out/scenario/blank"
printf 'a => b\n\000' > "$out/scenario/nul"

