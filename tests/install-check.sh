#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Checks `make install` the way a consumer meets it: the library, its soname link and development link, the headers,
# the scenario corpus and plugin-hostd-protocol.pc, installed with no source tree in reach. A program built from
# `pkg-config --cflags --libs plugin-hostd-protocol` alone must compile against every header, link and run; the
# framing (mod-audio/mod-host#113), command-table (mod-audio/mod-host#116) and bind-address checks run against the
# installed library. A DESTDIR install must leave DESTDIR out of the .pc file.
# Run from the top of the tree after `make`: sh tests/install-check.sh
set -eu

CC=${CC:-cc}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
fail() { echo "install: $*" >&2; exit 1; }
version=$(cat VERSION)
name=plugin-hostd-protocol

# a plain install under a prefix of its own
prefix=$work/prefix
make -s install PREFIX="$prefix" LIBDIR="$prefix/lib" INCLUDEDIR="$prefix/include" DATADIR="$prefix/share" >/dev/null
lib=$prefix/lib
[ -f "$lib/lib$name.so.$version" ] && [ ! -L "$lib/lib$name.so.$version" ] || fail "no lib$name.so.$version"
[ "$(readlink "$lib/lib$name.so.0")" = "lib$name.so.$version" ] || fail "soname link is wrong"
[ "$(readlink "$lib/lib$name.so")" = "lib$name.so.0" ] || fail "development link is wrong"
readelf -d "$lib/lib$name.so.$version" | grep -q "SONAME.*\[lib$name\.so\.0\]" \
    || fail "the library does not carry the soname lib$name.so.0"
for f in host-scenarios.txt host_scenarios.c protocol.md; do
    [ -f "$prefix/share/$name/$f" ] || fail "no $f under share/$name"
done

export PKG_CONFIG_PATH="$lib/pkgconfig"
cflags=$(pkg-config --cflags $name) || fail "pkg-config does not find $name"
libs=$(pkg-config --libs $name)
[ "$(pkg-config --modversion $name)" = "$version" ] || fail "pkg-config says $(pkg-config --modversion $name)"
[ "$(pkg-config --variable=scenarios $name)" = "$prefix/share/$name/host-scenarios.txt" ] \
    || fail "the scenarios variable does not name the installed corpus"
[ "$(pkg-config --variable=runner $name)" = "$prefix/share/$name/host_scenarios.c" ] || fail "the runner variable is wrong"
[ "$(pkg-config --variable=protocoldoc $name)" = "$prefix/share/$name/protocol.md" ] || fail "the protocoldoc variable is wrong"

# every installed header stands alone, and every header the build lists is installed
for h in host-backend.h host-dispatch.h host-errors.h mod-host.h host-scenario.h protocol.h socket.h utils.h; do
    [ -f "$prefix/include/$name/$h" ] || fail "$h is not installed"
done
for h in "$prefix/include/$name"/*.h; do
    printf '#include <%s>\nint main(void) { return 0; }\n' "$(basename "$h")" > "$work/one.c"
    # shellcheck disable=SC2086
    $CC -std=gnu99 -Wall -Wextra -Werror $cflags -fsyntax-only "$work/one.c" || fail "$(basename "$h") does not compile alone"
done

# a consumer built from pkg-config alone links the library and runs from it
cat > "$work/consumer.c" <<'C'
#include <socket.h>
#include <protocol.h>
#include <host-dispatch.h>
#include <stdio.h>
#include <string.h>
static void quit(proto_t *proto) { protocol_response_int(0, proto); }
int main(void)
{
    const char *msg;
    protocol_add_command("quit", quit);
    msg = protocol_error_message(-1);
    if (msg == NULL || msg[0] == '\0')
        return 1;
    socket_set_idle_interval(10);
    protocol_remove_commands();
    printf("ok: %s\n", msg);
    return 0;
}
C
# shellcheck disable=SC2086
$CC -std=gnu99 -Wall -Wextra -Werror $cflags -o "$work/consumer" "$work/consumer.c" $libs \
    || fail "a consumer does not build from pkg-config alone"
readelf -d "$work/consumer" | grep -q "NEEDED.*\[lib$name\.so\.0\]" || fail "the consumer does not need the soname"
LD_LIBRARY_PATH="$lib" "$work/consumer" >/dev/null || fail "the consumer does not run"

# the scenario runner builds from what is installed alone
# shellcheck disable=SC2086
$CC -std=gnu99 -D_GNU_SOURCE -Wall -Wextra -Werror -o "$work/host_scenarios" "$(pkg-config --variable=runner $name)" $cflags $libs \
    || fail "the installed scenario runner does not build against the installed library"

# the framing, the 128-command table and the default bind address, against the installed library
for t in framing-test commands-test bind-check; do
    # shellcheck disable=SC2086
    $CC -std=gnu99 -D_GNU_SOURCE -Wall -Wextra -Werror -o "$work/$t" "tests/$t.c" $cflags $libs -lpthread \
        || fail "tests/$t.c does not build against the installed library"
    LD_LIBRARY_PATH="$lib" timeout 60 "$work/$t" >"$work/$t.log" 2>&1 || { cat "$work/$t.log"; fail "$t failed"; }
done

# a staged install, as the packages do it: DESTDIR stays out of the .pc file
stage=$work/stage
make -s install PREFIX=/usr LIBDIR=/usr/lib64 INCLUDEDIR=/usr/include DATADIR=/usr/share DESTDIR="$stage" >/dev/null
pc=$stage/usr/lib64/pkgconfig/$name.pc
[ -f "$pc" ] || fail "no .pc under DESTDIR/LIBDIR/pkgconfig"
grep -q "$stage" "$pc" && fail "the .pc file names DESTDIR"
grep -qx 'prefix=/usr' "$pc" || fail "the staged .pc does not say prefix=/usr"
grep -qx 'libdir=${prefix}/lib64' "$pc" || fail "the staged .pc does not follow LIBDIR"

echo "install: library, links, headers, corpus, runner, protocol page, $name.pc, framing, 128 commands and bind address check out"
