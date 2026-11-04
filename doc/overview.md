# plugin-hostd-protocol: overview

The plugin-hostd protocol, a superset of mod-host's text protocol: any mod-host client works unchanged.

`libplugin-hostd-protocol` is the socket server, the line protocol and the command dispatch that plugin-hostd and
omx-clap-host answer their controllers with. A host registers a backend (`host_backend_t`) and the library does the
rest: it listens on the command and feedback ports, splits the input into NUL-terminated commands, parses them against
the command table and answers `resp <status> [value]`, as mod-host does.

The library answers mod-host's commands as mod-host does, byte for byte, with two bugs fixed (below). Since 0.2 it also
holds the whole declared protocol: the verbs plugin-hostd and omx-clap-host add, their replies and refusals, the
events, and the one verb table and the grammar written from them (below and [doc/protocol.md](protocol.md)).

## Use it

```
pkg-config --cflags --libs plugin-hostd-protocol
```

The headers are installed under `<includedir>/plugin-hostd-protocol/` with the names and the API of
`libmod-host-protocol` (`protocol.h`, `socket.h`, `host-dispatch.h`, `host-backend.h`, `host-errors.h`,
`host-scenario.h`, `mod-host.h`, `utils.h`), so a program built against `mod-host-protocol` changes only the package
name it asks pkg-config for. The scenario corpus every host is tested with, the source of its runner and the protocol
page are installed too: `pkg-config --variable=scenarios` (`runner`, `protocoldoc`) `plugin-hostd-protocol`.

The commands and the error codes: [doc/protocol.md](protocol.md).

## The declared protocol

What plugin-hostd declares of the protocol is installed with the development package, so no consumer keeps a copy:

- `<plugin-hostd/protocol.h>` (under `<includedir>/plugin-hostd-protocol/`, found with this package's Cflags): the
  verbs, the placement, the reply codes, the events, the host verb table and the settings, with the headers omx-contract
  renders from `data/verbs.json` (`plugin_hostd_protocol_host_verbs.h`, `plugin_hostd_protocol_vocabulary.h`) and from
  omx-clap-core's package of the contract directory (`omx_clap_core_host_verbs.h`, `omx_clap_core_vocabulary.h`);
- the one verb table, `plugin-hostd-protocol.json`: every command, each once, with its origin (mod-host, plugin-hostd or
  omx-clap-core), its argument types, its reply and its refusals, and every feedback line
  (`pkg-config --variable=table plugin-hostd-protocol`);
- the grammar of a line, in ABNF, generated from that table (`--variable=grammar`);
- `plugin-hostd.json`, what `protocol.h` declares as JSON, and its schema (`--variable=protocoljson`,
  `--variable=protocolschema`), and `mod-host.json`, mod-host's commands and codes (`--variable=modhostjson`);
- the source of the generator of `plugin-hostd.json` (`--variable=protocolgen`), for a consumer that checks its own
  pages against a changed copy of the headers;
- `data/verbs.json`, as the package plugin-hostd-protocol of omx-contract's contract directory
  (`plugin-hostd-protocol-contract`).

Every one is written from the declarations by a tool of `tools/`, and `make check-generated` fails when one is not what
its tool writes. A word added to the protocol never changes a mod-host word: `tools/protocol-table.py` refuses to write
the table when a verb of another origin neither extends mod-host's command of the same word nor is a second form of it
told apart by its number of arguments (`preset_show <uri>` is mod-host's, `preset_show <instance> <key>` plugin-hostd's).
There is no list of exceptions.

What each host profile owes each verb a controller sends:

<!-- BEGIN GENERATED protocol:host-verbs -->
| verb | sent by | lv2-worker | clap-worker | console | zynthian |
|---|---|---|---|---|---|
| `add` | mod-ui | required | required | required | required |
| `remove` | mod-ui | required | required | required | required |
| `bypass` | mod-ui | required | required | required | required |
| `connect` | mod-ui | required | required | required | required |
| `param_set` | mod-ui | required | required | required | required |
| `state_load` | mod-ui | required | required | required | required |
| `state_save` | mod-ui | required | required | required | required |
| `state_tmpdir` | mod-ui | required | required | required | required |
| `feature_enable` | mod-ui | required | required | required | required |
| `bundle_add` | mod-ui | required | required | required | required |
| `bundle_remove` | mod-ui | required | required | required | required |
| `preset_load` | mod-ui | required | required | required | required |
| `preset_save` | mod-ui | required | required | required | required |
| `preset_show` | mod-ui | required | required | required | required |
| `transport` | mod-ui | required | required | required | required |
| `transport_sync` | mod-ui | required | required | required | required |
| `cpu_load` | mod-ui | required | required | required | required |
| `midi_learn` | mod-ui | optional | optional | optional | optional |
| `midi_map` | mod-ui | optional | optional | optional | optional |
| `midi_unmap` | mod-ui | optional | optional | optional | optional |
| `cv_map` | mod-ui | optional | optional | optional | optional |
| `cv_unmap` | mod-ui | optional | optional | optional | optional |
| `monitor_midi_program` | mod-ui | optional | optional | optional | optional |
| `monitor_output` | mod-ui | optional | optional | optional | optional |
| `output_data_ready` | mod-ui | optional | optional | optional | optional |
| `patch_get` | mod-ui | optional | optional | optional | optional |
| `patch_set` | mod-ui | optional | optional | optional | optional |
| `cc_map` | mod-ui | absent | absent | absent | absent |
| `cc_value_set` | mod-ui | absent | absent | absent | absent |
| `cc_unmap` | mod-ui | absent | absent | absent | absent |
| `hmi_map` | mod-ui | absent | absent | absent | absent |
| `hmi_unmap` | mod-ui | absent | absent | absent | absent |
| `show_external_ui` | mod-ui | absent | absent | absent | absent |
| `set_midi_program_change_pedalboard_bank_channel` | mod-ui | absent | absent | absent | absent |
| `set_midi_program_change_pedalboard_snapshot_channel` | mod-ui | absent | absent | absent | absent |
| `disconnect` | mod-host | required | required | required | required |
| `set_bpm` | mod-host | required | required | required | required |
| `set_bpb` | mod-host | required | required | required | required |
| `quit` | mod-host | required | required | required | required |
| `param_get` | mod-host | optional | optional | optional | optional |
| `param_monitor` | mod-host | optional | optional | optional | optional |
| `licensee` | mod-host | optional | optional | optional | optional |
| `monitor` | mod-host | optional | optional | optional | optional |
| `load` | mod-host | optional | optional | optional | optional |
| `save` | mod-host | optional | optional | optional | optional |
| `help` | mod-host | optional | optional | optional | optional |
| `track_info` | mod-host | optional | optional | optional | optional |
| `remote_pages` | mod-host | optional | optional | optional | optional |
| `remote_page_get` | mod-host | optional | optional | optional | optional |
| `param_info` | mod-host | optional | optional | optional | optional |
| `preset_list` | daemon | optional | optional | optional | optional |
| `preset_get` | daemon | optional | optional | optional | optional |
| `note_input` | daemon | optional | optional | optional | optional |
| `note_on` | daemon | optional | optional | optional | optional |
| `note_off` | daemon | optional | optional | optional | optional |
| `all_notes_off` | daemon | optional | optional | optional | optional |
| `worker_list` | daemon | required | required | required | required |
| `instance_info` | daemon | required | required | required | required |
| `supervisor_reset` | daemon | required | required | required | required |
| `quarantine_clear` | daemon | required | required | required | required |
| `policy_set` | daemon | required | required | required | required |
| `pool_config` | daemon | required | required | required | required |
| `worker_env` | daemon | required | required | required | required |
| `pin_set` | daemon | required | required | required | required |
| `pin_clear` | daemon | required | required | required | required |
| `pin_accept` | daemon | required | required | required | required |
| `pin_reload` | daemon | required | required | required | required |
| `pin_changed` | daemon | required | required | required | required |
| `lv2_worker_set` | daemon | required | required | required | required |
| `chain` | daemon | required | required | required | required |
| `chain_clear` | daemon | required | required | required | required |
| `chain_get` | daemon | required | required | required | required |
| `plan` | daemon | required | required | required | required |
| `plan_get` | daemon | required | required | required | required |
| `plan_clear` | daemon | required | required | required | required |
| `descriptor_get` | daemon | required | required | required | required |
| `panel_get` | daemon | required | required | required | required |
| `history_get` | daemon | required | required | required | required |
| `history_clear` | daemon | required | required | required | required |
| `estimate` | daemon | required | required | required | required |

The profiles: `lv2-worker`, an LV2 plugin's worker: mod-host, or omx-clap-host's LV2 adapter; `clap-worker`, a CLAP plugin's worker, omx-clap-host; `console`, the openmixer console as a controller of the daemon; `zynthian`, Zynthian's mod-ui and its plugind engine as a controller of the daemon. `required` is answered as its grammar says; `optional` is answered, or refused with -902, and never misread; `absent` is refused on purpose, and no client of the profile may need it.
<!-- END GENERATED protocol:host-verbs -->

Packages: `plugin-hostd-protocol`, `plugin-hostd-protocol-devel` and `plugin-hostd-protocol-contract` (RPM),
`libplugin-hostd-protocol0`, `libplugin-hostd-protocol-dev` and `plugin-hostd-protocol-contract` (DEB). The library
package carries the table, the grammar and the JSON documents a program reads at run time; the development package the
headers, the schema, the scenario corpus and its runner, the protocol page and the generator's source.

## Build and test

```
make
make test
make install PREFIX=/usr LIBDIR=/usr/lib64
```

`make test` needs gcc with AddressSanitizer and UndefinedBehaviorSanitizer (libasan, libubsan), python3 and
omx-contract's C headers (omx-contract-devel, libomx-contract-dev). It runs:

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
  libmod-host-protocol.so.0) and the version (`version-check`);
- the declared protocol: every generated file is what its tool writes from the declarations (`check-generated`,
  `check-protocol-doc`; `make gen` writes them), the host verb table covers every verb (`test-verb-table`),
  `mod-host.json` follows its headers (`test-mod-host-json`), and each generated file edited by hand, or each
  declaration changed without writing them again, turns those checks red (`test-generated-sabotage`), and so does each
  rule of the host verb table broken in a scratch copy of the header.

`make check-protocol-tools` runs what needs tools a package build does not carry, as CI does: the rendered headers
against the installed omx-contract and omx-clap-core-contract (`check-rendered`, and `test-rendered-sabotage`), the
staged contract package (`check-contract-dir`), the JSON against its schema (`check-schema`, python3-jsonschema), and
the grammar against the scenario corpus, the golden transcript and the table (`test-grammar`, python's `abnf` module,
`PYTHON=<an interpreter that has it>`), each with its sabotage.

`make fuzz` and `make fuzz-run` build and run the libFuzzer harnesses with clang ([fuzz/README.md](../fuzz/README.md)).

## What differs from mod-host

- `format_number()` writes NaN, Infinity and -Infinity, where it wrote past a stack array: `param_info` of a port whose
  range or default is not finite (plugin-hostd#371).
- `protocol_parse()` runs the callback of the command the message names, and no other: `zzz %s 5` is not found, where
  it ran `add` (plugin-hostd#372).
- `protocol_parse()` reads a word `...` at the `...` of a pattern as one of the words the `...` takes: `add urn:x 5 ... c`
  runs `add`, where it answered `many arguments` (#12).
- A verb may have more than one form, told apart by its number of arguments (`preset_show <uri>` is mod-host's,
  `preset_show <instance> <key>` plugin-hostd's): `protocol_parse()` reads every command registered for the verb and
  runs the first whose arguments fit, in number and then in types (`%i` a whole number, `%f` a number), where it read
  only the first one registered. A line no form takes is refused as the first form refuses it, so a verb of one form
  answers as mod-host answers it (`make test-forms`, and `test-forms-sabotage` puts the first-form reading back).
- A host that calls `protocol_add_hello()` answers `hello`, which names the protocol version and grants extensions:
  request tags (a line may start with `@<tag>`, and its answer then carries the tag) and error words (a refusal's
  number is followed by its declared code) ([doc/protocol.md](protocol.md), Sessions). A client that never sends
  `hello` is answered as mod-host answers it, and a new connection starts with nothing granted (`make test-hello`, and
  `test-hello-sabotage` breaks each rule).
- The scenario runner reads a declared verb a host answers `not found` by what that host owes it
  (`host_scenario_set_host()`, `HOST=<host>` for `tests/host_scenarios.c`): one not required of it is a verb it does
  not serve, as `resp -902` is, so the lines after `hello` are skipped against mod-host; a required one fails
  (`make test-runner`).

## Credit

The library's code (`src/`, the headers at the top of `include/`) is mod-host's
([mod-audio/mod-host](https://github.com/mod-audio/mod-host)), by Filipe Coelho, MOD Audio UG and the mod-host
contributors, taken from FreeMixer's fork ([FreeMixer/mod-host](https://github.com/FreeMixer/mod-host), branch
`freemixer`, 013ac7d): upstream 12335c2 with the pull requests that made it a library (#81, #82, #98, #100, #101, #104
to #109, #112, #113, #116 and the bind address of FreeMixer/mod-host#12). The files that come from it keep mod-host's
copyright and licence lines. The declared protocol (`include/plugin-hostd/`, `data/`, `protocol/`, `grammar/`,
`tools/`) came from plugin-hostd.

## Licence

GPL-3.0-or-later, as mod-host: [COPYING](../COPYING).
