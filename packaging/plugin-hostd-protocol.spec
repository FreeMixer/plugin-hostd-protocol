Name: plugin-hostd-protocol
Version: 0.4.0
Release: 1%{?dist}
License: GPL-3.0-or-later
Summary: The plugin-hostd protocol library, a superset of mod-host's text protocol
URL: https://github.com/FreeMixer/plugin-hostd-protocol

Source0: %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires: gcc
BuildRequires: make
BuildRequires: pkgconfig
BuildRequires: binutils
# make test: the splitter, framing and fix tests run under AddressSanitizer and UndefinedBehaviorSanitizer, and the
# golden comparison is a python script
BuildRequires: libasan
BuildRequires: libubsan
BuildRequires: python3
# the declared protocol: tools/protocol-gen holds protocol.h's parameter scales to omx-contract's C headers, and
# install-contract writes the contract directory's package with omx-contract's script
BuildRequires: (omx-contract-devel >= 3.9.0 with omx-contract-devel < 4)

%description
libplugin-hostd-protocol is the socket server, line protocol and command
dispatch that plugin-hostd and omx-clap-host answer their controllers with:
mod-host's text protocol, so any mod-host client works unchanged. A host
registers its plugin backend and the library listens on the command and
feedback ports, parses each command and sends the reply.
Its documentation is on openmixer.org.

%package devel
Summary: Build a host that answers the plugin-hostd protocol
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
Headers, the pkg-config file, the scenario corpus and its runner of
libplugin-hostd-protocol. The headers have the names and the API of
libmod-host-protocol's, so a host built against mod-host-protocol changes only
the package name it asks pkg-config for.
Its documentation is on openmixer.org.

It also carries the declared protocol: plugin-hostd/protocol.h and the headers
it includes, the one verb table, the grammar of a line in ABNF, and the
protocol as JSON, each named by a pkg-config variable.

%package contract
Summary: The plugin-hostd protocol's verbs in omx-contract's contract directory
BuildArch: noarch
Requires: (omx-contract >= 3.2.0 with omx-contract < 4)
# plugin-hostd before 0.16.0 declared the same items in its own package of the contract directory
Conflicts: plugin-hostd-contract < 0.16.0

%description contract
data/verbs.json of plugin-hostd-protocol, the verbs and reply codes
plugin-hostd adds to mod-host's protocol, as the package plugin-hostd-protocol
of omx-contract's contract directory, under
share/omx-contract/contract.d/3/plugin-hostd-protocol.
Its documentation is on openmixer.org.

%prep
%autosetup -p1

%build
%set_build_flags
%make_build

%install
%make_install PREFIX=%{_prefix} LIBDIR=%{_libdir} INCLUDEDIR=%{_includedir} DATADIR=%{_datadir}
# data/verbs.json, as the package plugin-hostd-protocol of omx-contract's contract directory
%make_build install-contract DESTDIR=%{buildroot} PREFIX=%{_prefix}

%check
make test

%files
%license COPYING
%doc README.md
%{_libdir}/libplugin-hostd-protocol.so.0
%{_libdir}/libplugin-hostd-protocol.so.%{version}
# the declared protocol a program reads at run time: the verb table, the grammar, the protocol's JSON and mod-host's
%dir %{_datadir}/plugin-hostd-protocol
%{_datadir}/plugin-hostd-protocol/plugin-hostd-protocol.json
%{_datadir}/plugin-hostd-protocol/plugin-hostd-protocol.abnf
%{_datadir}/plugin-hostd-protocol/plugin-hostd.json
%{_datadir}/plugin-hostd-protocol/mod-host.json

%files devel
%license COPYING
%doc README.md
%{_libdir}/libplugin-hostd-protocol.so
%{_libdir}/pkgconfig/plugin-hostd-protocol.pc
%{_includedir}/plugin-hostd-protocol/
%{_datadir}/plugin-hostd-protocol/host-scenarios.txt
%{_datadir}/plugin-hostd-protocol/host_scenarios.c
%{_datadir}/plugin-hostd-protocol/protocol.md
%{_datadir}/plugin-hostd-protocol/plugin-hostd.schema.json
%{_datadir}/plugin-hostd-protocol/protocol-gen.c

%files contract
%license COPYING
%doc README.md
%{_datadir}/omx-contract/contract.d/3/plugin-hostd-protocol/

%changelog
* Fri Nov 06 2026 Pau Aliagas <linuxnow@gmail.com> - 0.4.0-1
- lv2_worker auto is no longer a word of the protocol: a plugin the chosen LV2
  worker refuses is answered with its code and never handed to another host.
  lv2_worker and lv2_worker_set take mod-host or omx-clap-host, and
  lv2_adapter_refused names omx-clap-host only
- pin_accept of a plugin whose pin carries verdict=refused (pins_dir or
  pin_file) is the user's choice to load it anyway, alone in a worker of its
  own; pin_clear takes it back; -516 is no longer its answer
- plugin_db_vocabulary.h is the render of plugin-db 0.2.0's words
* Wed Nov 04 2026 Pau Aliagas <linuxnow@gmail.com> - 0.3.0-1
- hello: the protocol version and the extensions a host grants; a client that
  sends no hello, or whose hello is refused, is answered as mod-host answers it
- Request tags, error words and event sequence numbers, each an extension a
  session asks for in its hello
- param_get_many and param_set_many: several values of one instance in one line,
  param_set_many all or nothing
- The default lv2_worker is omx-clap-host; mod-host runs an LV2 plugin only when
  named (#13)
- worker_list and plan: the core field reads -, a worker pinned to the whole
  audio set (#15)
- A track_info name is held to UTF-8 without C1 controls (#11); a word "..."
  where a pattern ends in "..." is an argument (#12)
- Fuzz harnesses with records, sabotage arms and a coverage floor run in CI
- The packages install their licence and a short README and no other markdown
- protocol_add_hello() and host_scenario_set_host() are the exported symbols added
- The words of a pin in protocol.h are the ones plugin-db declares
  (plugin_db_vocabulary.h, rendered from plugin-db 0.1.0's package)
- Dependencies carry a floor and a ceiling: omx-contract >= 3.9.0 and < 4 to
  build, >= 3.2.0 and < 4 to install the contract package

* Tue Nov 03 2026 Pau Aliagas <linuxnow@gmail.com> - 0.2.0-1
- protocol_parse() reads every form registered for a verb and runs the first whose
  arguments fit in number and type, so both forms of preset_show dispatch through the
  library (#5)
- The whole declared protocol moves here from plugin-hostd: plugin-hostd/protocol.h
  and the headers omx-contract renders for it, data/verbs.json,
  protocol/plugin-hostd.json and its schema, protocol/mod-host.json, and the
  generators and checks that keep them current
- One verb table, protocol/plugin-hostd-protocol.json: every command, each once,
  with its origin, its argument types, its reply and its refusals; a word of ours
  that changes a mod-host word is refused
- The grammar of a line in ABNF, generated from the table, and doc/protocol.md,
  generated from the table and the declarations; the scenario corpus and every
  reply of the golden transcript are held to the grammar
- New package plugin-hostd-protocol-contract: data/verbs.json as the package
  plugin-hostd-protocol of omx-contract's contract directory
- The library package installs the table, the grammar and the JSON of the
  protocol and of mod-host's, which programs read at run time; the devel package
  installs the headers under plugin-hostd/, the schema and the generator's
  source; pkg-config variables name each
- The library's exported symbols and types are those of 0.1.0

* Mon Nov 02 2026 Pau Aliagas <linuxnow@gmail.com> - 0.1.0-1
- First release: mod-host's socket server, line protocol and command dispatch
  (FreeMixer/mod-host 013ac7d, the code of mod-host-protocol 0.10.6-11.3) as a
  library of its own, the same API under the name plugin-hostd-protocol
- param_info writes NaN, Infinity and -Infinity for a value that is not finite,
  where it wrote past a stack array (plugin-hostd#371)
- a command whose first word is not a verb's no longer runs that verb's
  callback (plugin-hostd#372)
