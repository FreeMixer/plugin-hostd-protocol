Name: plugin-hostd-protocol
Version: 0.1.0
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

%description
libplugin-hostd-protocol is the socket server, line protocol and command
dispatch that plugin-hostd and omx-clap-host answer their controllers with:
mod-host's text protocol, so any mod-host client works unchanged. A host
registers its plugin backend and the library listens on the command and
feedback ports, parses each command and sends the reply.

%package devel
Summary: Build a host that answers the plugin-hostd protocol
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
Headers, the pkg-config file, the scenario corpus and its runner of
libplugin-hostd-protocol. The headers have the names and the API of
libmod-host-protocol's, so a host built against mod-host-protocol changes only
the package name it asks pkg-config for.

%prep
%autosetup -p1

%build
%set_build_flags
%make_build

%install
%make_install PREFIX=%{_prefix} LIBDIR=%{_libdir} INCLUDEDIR=%{_includedir} DATADIR=%{_datadir}

%check
make test

%files
%license COPYING
%doc README.md
%{_libdir}/libplugin-hostd-protocol.so.0
%{_libdir}/libplugin-hostd-protocol.so.%{version}

%files devel
%{_libdir}/libplugin-hostd-protocol.so
%{_libdir}/pkgconfig/plugin-hostd-protocol.pc
%{_includedir}/plugin-hostd-protocol/
%{_datadir}/plugin-hostd-protocol/

%changelog
* Mon Nov 02 2026 Pau Aliagas <linuxnow@gmail.com> - 0.1.0-1
- First release: mod-host's socket server, line protocol and command dispatch
  (FreeMixer/mod-host 013ac7d, the code of mod-host-protocol 0.10.6-11.3) as a
  library of its own, the same API under the name plugin-hostd-protocol
- param_info writes NaN, Infinity and -Infinity for a value that is not finite,
  where it wrote past a stack array (plugin-hostd#371)
- a command whose first word is not a verb's no longer runs that verb's
  callback (plugin-hostd#372)
