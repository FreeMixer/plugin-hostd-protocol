# plugin-hostd-protocol

The plugin-hostd protocol, a superset of mod-host's text protocol: any mod-host client works unchanged.

`libplugin-hostd-protocol` is the socket server, the line protocol and the command dispatch that plugin-hostd and
omx-clap-host answer their controllers with. A host registers a backend (`host_backend_t`) and the library does the
rest: it listens on the command and feedback ports, splits the input into NUL-terminated commands, parses them against
the command table and answers `resp <status> [value]`, as mod-host does.

Version 0.1 is exactly mod-host's protocol: the same commands, the same replies, byte for byte, with two bugs fixed
(below). The verbs plugin-hostd adds stay in plugin-hostd for now; they move here with the 0.2 protocol.

## Use it

```
pkg-config --cflags --libs plugin-hostd-protocol
```

The headers are installed under `<includedir>/plugin-hostd-protocol/` with the names and the API of
`libmod-host-protocol` (`protocol.h`, `socket.h`, `host-dispatch.h`, `host-backend.h`, `host-errors.h`,
`host-scenario.h`, `mod-host.h`, `utils.h`), so a program built against `mod-host-protocol` changes only the package
name it asks pkg-config for. The scenario corpus every host is tested with, the source of its runner and the protocol
page are installed too: `pkg-config --variable=scenarios` (`runner`, `protocoldoc`) `plugin-hostd-protocol`.

The commands and the error codes: [doc/protocol.md](doc/protocol.md).

Packages: `plugin-hostd-protocol` and `plugin-hostd-protocol-devel` (RPM), `libplugin-hostd-protocol0` and
`libplugin-hostd-protocol-dev` (DEB).

## Build and test

```
make
make test
make install PREFIX=/usr LIBDIR=/usr/lib64
```

`make test` needs gcc with AddressSanitizer and UndefinedBehaviorSanitizer (libasan, libubsan) and python3. It runs:

- the protocol, dispatcher and scenario tests over a fake backend (`test-protocol`, `test-protocol-commands`), and the
  input splitter, NUL framing and bind address tests (`test-utils`, `test-socket`, `test-bind`);
- the golden test (`test-golden`): the scenario corpus and both fuzz seed corpora sent over a real socket, each command
  answered nine ways by a recording backend, the transcript held byte for byte to `tests/golden/plugin-hostd-protocol.txt`;
- the comparison with mod-host (`test-golden-modhost`): every exchange of `tests/golden/mod-host-protocol.txt`, the
  same transcript taken from libmod-host-protocol 0.10.6-11.3, is the same in ours, except the ones
  `tests/golden/deviations.txt` names, which must differ. `make golden-modhost MODHOST_PREFIX=<dir>` takes it again
  from a libmod-host-protocol installed there;
- the fixes below (`test-fixes`), and each fix taken out again, which must fail (`test-fixes-sabotage`);
- the installed library as a consumer meets it (`test-install`), the exported symbols (`test-abi`, the list of
  libmod-host-protocol.so.0) and the version (`version-check`).

`make fuzz` and `make fuzz-run` build and run the libFuzzer harnesses with clang ([fuzz/README.md](fuzz/README.md)).

## What differs from mod-host

- `format_number()` writes NaN, Infinity and -Infinity, where it wrote past a stack array: `param_info` of a port whose
  range or default is not finite (plugin-hostd#371).
- `protocol_parse()` runs the callback of the command the message names, and no other: `zzz %s 5` is not found, where
  it ran `add` (plugin-hostd#372).

## Credit

This code is mod-host's ([mod-audio/mod-host](https://github.com/mod-audio/mod-host)), by Filipe Coelho, MOD Audio UG
and the mod-host contributors, taken from FreeMixer's fork
([FreeMixer/mod-host](https://github.com/FreeMixer/mod-host), branch `freemixer`, 013ac7d): upstream 12335c2 with the
pull requests that made it a library (#81, #82, #98, #100, #101, #104 to #109, #112, #113, #116 and the bind address of
FreeMixer/mod-host#12). The files that come from it keep mod-host's copyright and licence lines.

## Licence

GPL-3.0-or-later, as mod-host: [COPYING](COPYING).
