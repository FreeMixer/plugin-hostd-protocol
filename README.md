# plugin-hostd-protocol

The plugin-hostd protocol, a superset of mod-host's text protocol: any mod-host client works unchanged.

`libplugin-hostd-protocol` is the socket server, the line protocol and the command dispatch that plugin-hostd and
omx-clap-host answer their controllers with. A host registers a backend (`host_backend_t`) and the library does the
rest: it listens on the command and feedback ports, splits the input into NUL-terminated commands, parses them against
the command table and answers `resp <status> [value]`, as mod-host does. It also holds the whole declared protocol:
the verbs plugin-hostd and omx-clap-host add, their replies and refusals, the events, and the one verb table and the
grammar written from them.

## Install

Fedora 44:

```
sudo dnf install https://www.openmixer.org/rpm/openmixer-release.noarch.rpm
sudo dnf install plugin-hostd-protocol-devel
```

Debian bookworm and trixie (also Raspberry Pi OS and Zynthian):

```
curl -fsSLO https://www.openmixer.org/deb/openmixer-release.deb && sudo apt install ./openmixer-release.deb
sudo apt install libplugin-hostd-protocol-dev
```

## Start

```
pkg-config --cflags --libs plugin-hostd-protocol
```

The headers keep the names and the API of `libmod-host-protocol` (`protocol.h`, `socket.h`, `host-dispatch.h`, ...),
so a program built against `mod-host-protocol` changes only the package name it asks pkg-config for.

## Documentation

<https://www.openmixer.org/projects/plugin-hostd-protocol>, and in this repository: the declared protocol, the build
and its tests, what differs from mod-host and the credit ([doc/overview.md](doc/overview.md)); the commands and the
error codes ([doc/protocol.md](doc/protocol.md)).

## Licence

GPL-3.0-or-later, as mod-host: [COPYING](COPYING). The library's code is mod-host's, by Filipe Coelho, MOD Audio UG
and the mod-host contributors.
