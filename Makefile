# SPDX-License-Identifier: GPL-3.0-or-later
# compiler
CC ?= gcc
PKG_CONFIG ?= pkg-config

# the one version of the library: VERSION holds it, and `make version-check` holds the spec's Version and the newest
# debian/changelog entry to it
VERSION := $(strip $(shell cat VERSION))

# the library: soname libplugin-hostd-protocol.so.<major>, the file <major>.<minor>.<patch>. While the version is 0.x
# the soname is bumped on any ABI change, a field appended to host_backend_t included.
NAME = plugin-hostd-protocol
MAJOR = 0
LIB_SO = lib$(NAME).so
LIB_SONAME = $(LIB_SO).$(MAJOR)
LIB_FILE = $(LIB_SO).$(VERSION)

# install paths
PREFIX = /usr/local
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include
DATADIR = $(PREFIX)/share

# the flags mod-host builds this code with, so the library behaves as libmod-host-protocol does
CFLAGS += -O3 -Wall -Wextra -std=gnu99 -fPIC -D_GNU_SOURCE -pthread
CFLAGS += -Wno-deprecated-declarations
CFLAGS += -Werror=implicit-function-declaration -Werror=return-type
ifeq ($(DEBUG), 1)
   CFLAGS += -O0 -g -DDEBUG
endif
INCS = -Iinclude
LDFLAGS += -Wl,--no-undefined

SRC = src/socket.c src/protocol.c src/utils.c src/host-dispatch.c src/host-scenario.c
OBJ = $(SRC:.c=.o)
# the public headers, installed under <includedir>/plugin-hostd-protocol: the names libmod-host-protocol installs
# under <includedir>/mod-host, so a consumer changes only the package it asks pkg-config for
HEADERS = include/host-backend.h include/host-dispatch.h include/host-errors.h include/mod-host.h \
          include/host-scenario.h include/protocol.h include/socket.h include/utils.h

# the declared protocol (doc/protocol.md): plugin-hostd's protocol.h and the headers it includes, rendered by
# omx-contract from data/verbs.json and from omx-clap-core's and plugin-db's packages of the contract directory; installed under
# <includedir>/plugin-hostd-protocol/plugin-hostd/, so a consumer includes <plugin-hostd/protocol.h> with this package's
# Cflags. src/protocol.c reads the protocol version, hello's line and the extensions hello grants from them.
RENDERED = include/plugin-hostd/plugin_hostd_protocol_host_verbs.h include/plugin-hostd/plugin_hostd_protocol_vocabulary.h \
           include/plugin-hostd/omx_clap_core_host_verbs.h include/plugin-hostd/omx_clap_core_vocabulary.h \
           include/plugin-hostd/plugin_db_vocabulary.h
PROTOCOL_HEADERS = include/plugin-hostd/protocol.h include/plugin-hostd/search_paths.h $(RENDERED)

PC_LIBDIR = $(patsubst $(PREFIX)/%,$${prefix}/%,$(LIBDIR))
PC_INCLUDEDIR = $(patsubst $(PREFIX)/%,$${prefix}/%,$(INCLUDEDIR))
PC_DATADIR = $(patsubst $(PREFIX)/%,$${prefix}/%,$(DATADIR))

all: $(LIB_SO)

$(OBJ): $(HEADERS) $(PROTOCOL_HEADERS) src/session.h src/reply-words.h

src/%.o: src/%.c
	$(CC) $(INCS) $(CFLAGS) -c -o $@ $<

$(LIB_FILE): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -shared -Wl,-soname,$(LIB_SONAME) -lpthread -lm -o $@

$(LIB_SONAME): $(LIB_FILE)
	ln -sf $(LIB_FILE) $@

$(LIB_SO): $(LIB_SONAME)
	ln -sf $(LIB_SONAME) $@

install: $(LIB_FILE)
	install -d $(DESTDIR)$(LIBDIR)/pkgconfig $(DESTDIR)$(INCLUDEDIR)/$(NAME) $(DESTDIR)$(DATADIR)/$(NAME)
	install -m 755 $(LIB_FILE) $(DESTDIR)$(LIBDIR)/
	ln -sf $(LIB_FILE) $(DESTDIR)$(LIBDIR)/$(LIB_SONAME)
	ln -sf $(LIB_SONAME) $(DESTDIR)$(LIBDIR)/$(LIB_SO)
	install -m 644 $(HEADERS) $(DESTDIR)$(INCLUDEDIR)/$(NAME)/
	install -m 644 tests/host-scenarios.txt tests/host_scenarios.c doc/protocol.md $(DESTDIR)$(DATADIR)/$(NAME)/
	install -d $(DESTDIR)$(INCLUDEDIR)/$(NAME)/plugin-hostd
	install -m 644 $(PROTOCOL_HEADERS) $(DESTDIR)$(INCLUDEDIR)/$(NAME)/plugin-hostd/
	install -m 644 protocol/plugin-hostd-protocol.json $(DESTDIR)$(DATADIR)/$(NAME)/plugin-hostd-protocol.json
	install -m 644 grammar/plugin-hostd-protocol.abnf $(DESTDIR)$(DATADIR)/$(NAME)/plugin-hostd-protocol.abnf
	install -m 644 protocol/plugin-hostd.json $(DESTDIR)$(DATADIR)/$(NAME)/plugin-hostd.json
	install -m 644 protocol/plugin-hostd.schema.json $(DESTDIR)$(DATADIR)/$(NAME)/plugin-hostd.schema.json
	install -m 644 protocol/mod-host.json $(DESTDIR)$(DATADIR)/$(NAME)/mod-host.json
	install -m 644 tools/protocol-gen.c $(DESTDIR)$(DATADIR)/$(NAME)/protocol-gen.c
	sed -e 's,@PREFIX@,$(PREFIX),' -e 's,@LIBDIR@,$(PC_LIBDIR),' -e 's,@INCLUDEDIR@,$(PC_INCLUDEDIR),' \
	    -e 's,@DATADIR@,$(PC_DATADIR),' -e 's,@VERSION@,$(VERSION),' $(NAME).pc.in > $(DESTDIR)$(LIBDIR)/pkgconfig/$(NAME).pc
	chmod 644 $(DESTDIR)$(LIBDIR)/pkgconfig/$(NAME).pc

# ---------------------------------------------------------------------------------------------------------------------
# tests: none needs JACK, a plugin or a network beyond 127.0.0.1

TEST_CFLAGS = $(filter-out -O3,$(CFLAGS)) -O1 -g -Werror
# the sources are compiled into the sanitized tests as they are; a distribution's _FORTIFY_SOURCE flags their unchecked
# write() calls, which mod-host leaves unchecked too
SAN = -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Wno-error=unused-result
TEST_PROGS = tests/protocol_test tests/protocol_commands_test tests/host_scenarios tests/golden-run tests/fixes_test tests/forms_test \
             tests/hello_test tests/runner_test \
             tests/socket-test tests/bind-test tests/utils-test

test: test-protocol test-protocol-commands test-utils test-socket test-bind test-golden test-golden-modhost \
      test-fixes test-fixes-sabotage test-forms test-forms-sabotage test-hello test-runner test-hello-sabotage test-install test-abi version-check \
      check-generated check-protocol-doc test-verb-table test-mod-host-json test-generated-sabotage test-default-lv2-worker

# the protocol, the dispatcher and the scenario corpus over a fake backend, as mod-host's test-protocol runs them
test-protocol: tests/protocol_test
	./tests/protocol_test

test-protocol-commands: tests/protocol_commands_test
	./tests/protocol_commands_test

tests/protocol_test tests/protocol_commands_test tests/host_scenarios: tests/%: tests/%.c $(OBJ)
	$(CC) $(INCS) $(TEST_CFLAGS) -o $@ $< $(OBJ) -lpthread -lm

# the input splitter, the NUL framing of the socket and the default bind address, with AddressSanitizer
test-utils: tests/utils-test
	./tests/utils-test
test-socket: tests/socket-test
	./tests/socket-test
test-bind: tests/bind-test
	./tests/bind-test

tests/socket-test tests/bind-test: tests/%: tests/%.c src/socket.c include/socket.h
	$(CC) $< -std=gnu99 -D_GNU_SOURCE -O1 -g -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer -Iinclude -lpthread -o $@
tests/utils-test: tests/utils-test.c src/utils.c include/utils.h
	$(CC) $< -std=gnu99 -D_GNU_SOURCE -O1 -g -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer -Iinclude -lm -o $@

# The golden transcript (tests/golden.c): the scenario corpus and both fuzz seed corpora over a real socket, each
# command answered nine ways. test-golden holds the library to tests/golden/plugin-hostd-protocol.txt byte for byte;
# test-golden-modhost holds that file to tests/golden/mod-host-protocol.txt, what libmod-host-protocol 0.10.6-11.3
# answered: every exchange the same, except the ones tests/golden/deviations.txt names, which must differ.
GOLDEN_INPUTS = tests/host-scenarios.txt $(sort $(wildcard fuzz/corpus/protocol/*)) \
                $(addprefix dispatch:,$(sort $(wildcard fuzz/corpus/dispatch/*)))
GOLDEN_PORT ?= 15570

tests/golden-run: tests/golden.c $(LIB_SO)
	$(CC) $(INCS) $(TEST_CFLAGS) -o $@ $< -L. -l$(NAME) -Wl,-rpath,$(CURDIR) -lpthread -lm

test-golden: tests/golden-run
	@set -e; tmp=$$(mktemp); trap 'rm -f $$tmp' EXIT; \
	LC_ALL=C GOLDEN_PORT=$(GOLDEN_PORT) ./tests/golden-run $(GOLDEN_INPUTS) > $$tmp; \
	cmp $$tmp tests/golden/plugin-hostd-protocol.txt || { diff -u tests/golden/plugin-hostd-protocol.txt $$tmp | head -40; exit 1; }; \
	echo "golden: $$(grep -c '^#' $$tmp) exchanges, byte-identical to tests/golden/plugin-hostd-protocol.txt"

test-golden-modhost:
	python3 tests/golden-compare.py tests/golden/mod-host-protocol.txt tests/golden/plugin-hostd-protocol.txt \
	    tests/golden/deviations.txt

# rewrite our transcript after a deliberate change (and name each changed exchange in deviations.txt)
golden-update: tests/golden-run
	LC_ALL=C GOLDEN_PORT=$(GOLDEN_PORT) ./tests/golden-run $(GOLDEN_INPUTS) > tests/golden/plugin-hostd-protocol.txt

# mod-host's transcript, from the libmod-host-protocol that pkg-config finds (MODHOST_PREFIX=<its install prefix>),
# over the inputs MODHOST_INPUTS names (default: this tree's)
MODHOST_PREFIX ?=
MODHOST_INPUTS ?= $(GOLDEN_INPUTS)
golden-modhost:
	@set -e; tmp=$$(mktemp -d); trap 'rm -rf $$tmp' EXIT; \
	export PKG_CONFIG_PATH=$(if $(MODHOST_PREFIX),$(MODHOST_PREFIX)/lib/pkgconfig:)$$PKG_CONFIG_PATH; \
	$(CC) $$($(PKG_CONFIG) --cflags mod-host-protocol) $(TEST_CFLAGS) -o $$tmp/golden tests/golden.c \
	    $$($(PKG_CONFIG) --libs mod-host-protocol) $(if $(MODHOST_PREFIX),-Wl$(comma)-rpath$(comma)$(MODHOST_PREFIX)/lib) -lpthread -lm; \
	ldd $$tmp/golden | grep 'libmod-host-protocol'; \
	LC_ALL=C GOLDEN_PORT=$(GOLDEN_PORT) $$tmp/golden $(MODHOST_INPUTS) > tests/golden/mod-host-protocol.txt
comma := ,

# the fixes of plugin-hostd#371 and #372, and of #11 and #12 (tests/fixes_test.c), against the sources with
# AddressSanitizer and UndefinedBehaviorSanitizer
test-fixes: tests/fixes_test
	./tests/fixes_test

tests/fixes_test: tests/fixes_test.c $(SRC) $(HEADERS)
	$(CC) $(INCS) $(TEST_CFLAGS) $(SAN) -o $@ $< $(SRC) -lpthread -lm

# a verb of more than one form (the verb table's preset_show) runs the form its arguments fit (tests/forms_test.c), with
# AddressSanitizer and UndefinedBehaviorSanitizer; and the first-form behaviour put back must make it fail
test-forms: tests/forms_test
	./tests/forms_test

tests/forms_test: tests/forms_test.c $(SRC) $(HEADERS)
	$(CC) $(INCS) $(TEST_CFLAGS) $(SAN) -o $@ $< $(SRC) -lpthread -lm

test-forms-sabotage:
	sh tests/forms-sabotage.sh "$(CC)" "$(INCS) $(TEST_CFLAGS) $(SAN)"

# hello and request tags (tests/hello_test.c), over the library's own server and its feedback port, with
# AddressSanitizer and UndefinedBehaviorSanitizer: a client that never sends hello is answered as before
HELLO_PORT ?= 15590
test-hello: tests/hello_test
	HELLO_PORT=$(HELLO_PORT) ./tests/hello_test

# the scenario runner reads "not found" by the host's levels (tests/runner_test.c): a verb not required of the host is
# one it does not serve, a required one fails; test-hello-sabotage breaks this too
test-runner: tests/runner_test
	./tests/runner_test

tests/runner_test: tests/runner_test.c $(SRC) $(HEADERS) $(PROTOCOL_HEADERS) src/session.h
	$(CC) $(INCS) $(TEST_CFLAGS) $(SAN) -o $@ $< $(SRC) -lpthread -lm

test-hello-sabotage:
	HELLO_PORT=$(HELLO_PORT) sh tests/hello-sabotage.sh "$(CC)" "$(INCS) $(TEST_CFLAGS) $(SAN)"

tests/hello_test: tests/hello_test.c $(SRC) $(HEADERS) $(PROTOCOL_HEADERS) src/session.h
	$(CC) $(INCS) $(TEST_CFLAGS) $(SAN) -o $@ $< $(SRC) -lpthread -lm

# each fix taken out again in a scratch copy of the sources: fixes_test must fail on each
test-fixes-sabotage:
	sh tests/fixes-sabotage.sh "$(CC)" "$(INCS) $(TEST_CFLAGS) $(SAN)"

# install as a consumer meets it: links, soname, headers standing alone, the corpus, the .pc file, a consumer built
# from pkg-config alone, and mod-host's framing, command-table and bind checks against the installed library
test-install: $(LIB_FILE)
	sh tests/install-check.sh

# the exported symbols: the ones libmod-host-protocol.so.0 exports, no more and no fewer (abi/$(LIB_SONAME).symbols);
# with abidw and abidiff installed, abi-check compares the types as well against abi/$(LIB_SONAME).abi
ABI_SYMBOLS = abi/$(LIB_SONAME).symbols
ABI_BASELINE = abi/$(LIB_SONAME).abi
test-abi: $(LIB_FILE)
	@set -e; tmp=$$(mktemp); trap 'rm -f $$tmp' EXIT; \
	nm -D --defined-only $(LIB_FILE) | awk '$$2 ~ /^[TDBRVW]$$/ { print $$3 }' | LC_ALL=C sort > $$tmp; \
	diff -u $(ABI_SYMBOLS) $$tmp; echo "abi: $$(wc -l < $$tmp) exported symbols as $(ABI_SYMBOLS) lists"

abi-symbols: $(LIB_FILE)
	nm -D --defined-only $(LIB_FILE) | awk '$$2 ~ /^[TDBRVW]$$/ { print $$3 }' | LC_ALL=C sort > $(ABI_SYMBOLS)

abi-stage: $(LIB_FILE)
	rm -rf build/abi
	$(MAKE) install DESTDIR=$(CURDIR)/build/abi PREFIX=/usr LIBDIR=/usr/lib

abi-baseline: abi-stage
	abidw --headers-dir build/abi/usr/include/$(NAME) --out-file $(ABI_BASELINE) build/abi/usr/lib/$(LIB_FILE)

abi-check: abi-stage
	abidiff --headers-dir2 build/abi/usr/include/$(NAME) $(ABI_BASELINE) build/abi/usr/lib/$(LIB_FILE)

# ---------------------------------------------------------------------------------------------------------------------
# the declared protocol: the verb table, the JSON of protocol.h, mod-host's commands, the grammar and the protocol page,
# each written from the declarations by a tool of tools/, never by hand. `make gen` writes them, `make check-generated`
# and `make check-protocol-doc` fail on any drift.
#
#   protocol/plugin-hostd.json            tools/protocol-gen json, compiled against include/plugin-hostd/protocol.h
#   protocol/mod-host.json                tools/mod-host-json.py, from include/mod-host.h, protocol.h, host-errors.h
#   protocol/plugin-hostd-protocol.json   tools/protocol-table.py, the one table merged from the three above and
#                                         data/verbs.json; it refuses a word of ours that changes a mod-host word
#   grammar/plugin-hostd-protocol.abnf    tools/abnf-gen.py, grammar/core.abnf and a rule per word of the table
#   fuzz/corpus-abnf/                     tools/abnf-corpus.py, seeds from the grammar (check-generated reads the
#                                         SHA-256 of the grammar they were written from)
#   src/reply-words.h                     tools/reply-words.py, the error word of each refusal number of the table
#   doc/overview.md                       the region host-verbs, tools/protocol-gen splice
#   doc/protocol.md                       tools/protocol-doc.py, from all of the above
#
# protocol-gen holds the parameter scales of protocol.h to omx-contract's PARAM_SCALES, so it builds against
# omx-contract's C headers (omx-contract-devel, libomx-contract-dev; CONTRACT_CFLAGS=-I<dir> for headers that are not
# installed).
CONTRACT_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags omx-contract 2>/dev/null)
GEN = build/protocol-gen

$(GEN): tools/protocol-gen.c $(PROTOCOL_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) -Wall -Wextra -Werror -std=gnu99 -Iinclude $(CONTRACT_CFLAGS) -o $@ tools/protocol-gen.c

gen: $(GEN)
	$(GEN) json > protocol/plugin-hostd.json.new && mv protocol/plugin-hostd.json.new protocol/plugin-hostd.json
	python3 tools/mod-host-json.py include > protocol/mod-host.json.new && mv protocol/mod-host.json.new protocol/mod-host.json
	python3 tools/protocol-table.py > protocol/plugin-hostd-protocol.json.new && \
	    mv protocol/plugin-hostd-protocol.json.new protocol/plugin-hostd-protocol.json
	python3 tools/abnf-gen.py > grammar/plugin-hostd-protocol.abnf.new && \
	    mv grammar/plugin-hostd-protocol.abnf.new grammar/plugin-hostd-protocol.abnf
	python3 tools/reply-words.py > src/reply-words.h.new && mv src/reply-words.h.new src/reply-words.h
	$(GEN) splice doc/overview.md > doc/overview.md.new && mv doc/overview.md.new doc/overview.md
	$(MAKE) --no-print-directory protocol-doc

check-generated: $(GEN)
	@set -e; tmp=$$(mktemp -d); trap 'rm -rf $$tmp' EXIT; \
	$(GEN) json > $$tmp/json; cmp $$tmp/json protocol/plugin-hostd.json; \
	python3 tools/mod-host-json.py include > $$tmp/mod-host.json; cmp $$tmp/mod-host.json protocol/mod-host.json; \
	python3 tools/protocol-table.py --check; \
	python3 tools/abnf-gen.py --check; \
	python3 tools/reply-words.py --check; \
	$(GEN) splice doc/overview.md > $$tmp/readme; cmp $$tmp/readme doc/overview.md; \
	sha256sum -c --quiet $(ABNF_CORPUS)/GRAMMAR.sha256 || { echo "$(ABNF_CORPUS) is not written from this grammar: run make fuzz-abnf-corpus"; exit 1; }; \
	$(GEN) regions > $$tmp/regions; test -s $$tmp/regions; \
	while read r; do grep -q "BEGIN GENERATED protocol:$$r\( .*\)\?$$" doc/overview.md || { echo "region $$r is in no file"; exit 1; }; done < $$tmp/regions; \
	echo "generated files are current"

protocol-doc:
	python3 tools/protocol-doc.py > doc/protocol.md.tmp && mv doc/protocol.md.tmp doc/protocol.md

check-protocol-doc:
	python3 tools/protocol-doc.py --check

# the host verb table of protocol.h has a row for every verb a controller sends, no more and no fewer; and each of its
# rules broken in a scratch copy of the header turns it red
test-verb-table:
	python3 tests/verb_table.py
	python3 tests/verb_table_sabotage.py

# the default lv2_worker is omx-clap-host: mod-host runs an LV2 plugin only when lv2_worker or lv2_worker_set names it
test-default-lv2-worker:
	python3 tests/default_lv2_worker.py protocol/plugin-hostd.json

# mod-host.json follows its headers: a moved verb, code or message in a scratch copy moves the JSON
test-mod-host-json:
	python3 tests/mod_host_json.py include

# each generated file and each declaration it is written from, changed in a scratch copy of the tree: the checks above
# must fail on each (tests/generated-sabotage.sh)
test-generated-sabotage:
	sh tests/generated-sabotage.sh

# The checks that need tools a package build does not: run by CI (.github/workflows/ci.yml, job protocol).
#   check-rendered   the four rendered headers are what omx-contract renders from data/verbs.json (staged as the
#                    package plugin-hostd-protocol of the contract directory) and from omx-clap-core's installed package;
#                    test-rendered-sabotage holds it to failing
#   check-contract-dir  that staged package validates with the installed contract directory
#   check-schema     protocol/plugin-hostd.json against its schema (python3-jsonschema)
#   test-grammar     the grammar against the scenario corpus, the golden transcript and the table (python abnf)
OMX_CONTRACT ?= omx-contract
CONTRACT_SCHEMA_MAJOR = 3
CONTRACTDIR = $(or $(shell $(PKG_CONFIG) --variable=contractdir omx-contract 2>/dev/null),$(DATADIR)/omx-contract/contract.d)
INSTALL_CONTRACT_DIR = $(firstword $(wildcard $(shell $(PKG_CONFIG) --variable=datadir omx-contract 2>/dev/null)/install-contract-dir.sh) $(wildcard $(OMX_CONTRACT_DIR)/tools/install-contract-dir.sh))

check-protocol-tools: check-rendered test-rendered-sabotage check-schema test-grammar check-contract-dir test-abnf-corpus

# data/verbs.json as the package plugin-hostd-protocol of omx-contract's contract directory,
# <contractdir>/<schema major>/plugin-hostd-protocol/, with the MANIFEST.json omx-contract's script writes
install-contract:
	@test -n "$(INSTALL_CONTRACT_DIR)" || { echo "install-contract: no install-contract-dir.sh: install omx-contract-devel, or set OMX_CONTRACT_DIR to a checkout" >&2; exit 1; }
	sh $(INSTALL_CONTRACT_DIR) $(NAME) $(VERSION) $(CONTRACT_SCHEMA_MAJOR) data $(DESTDIR)$(CONTRACTDIR) data

# the staged package validates together with the installed contract directory; an absent omx-contract is red
check-contract-dir:
	@command -v $(OMX_CONTRACT) >/dev/null || { echo "$@: no $(OMX_CONTRACT) command: install omx-contract" >&2; exit 1; }
	@set -e; tmp=$$(mktemp -d); trap 'rm -rf $$tmp' EXIT; \
	$(MAKE) --no-print-directory install-contract DESTDIR=$$tmp >/dev/null; \
	OMX_CONTRACT_PATH=$$tmp$(CONTRACTDIR)$(if $(OMX_CONTRACT_PATH),:$(OMX_CONTRACT_PATH)):$(CONTRACTDIR) $(OMX_CONTRACT) validate --installed

# check-rendered must fail on a rendered header changed by hand and on verbs.json not rendered again
test-rendered-sabotage:
	sh tests/rendered-sabotage.sh

render-verbs check-rendered:
	@command -v $(OMX_CONTRACT) >/dev/null || { echo "$@: no $(OMX_CONTRACT) command: install omx-contract" >&2; exit 1; }
	@test -n "$(INSTALL_CONTRACT_DIR)" || { echo "$@: no install-contract-dir.sh: install omx-contract-devel, or set OMX_CONTRACT_DIR to a checkout" >&2; exit 1; }
	@set -e; tmp=$$(mktemp -d); trap 'rm -rf $$tmp' EXIT; \
	sh $(INSTALL_CONTRACT_DIR) $(NAME) $(VERSION) $(CONTRACT_SCHEMA_MAJOR) data $$tmp data >/dev/null; \
	OMX_CONTRACT_PATH=$$tmp$(if $(OMX_CONTRACT_PATH),:$(OMX_CONTRACT_PATH)):$(CONTRACTDIR) \
	  $(OMX_CONTRACT) render --target c --package $(NAME) --installed --out include/plugin-hostd $(if $(filter check-rendered,$@),--check)
	@set -e; tmp=$$(mktemp -d); trap 'rm -rf $$tmp' EXIT; \
	OMX_CONTRACT_PATH=$(if $(OMX_CONTRACT_PATH),$(OMX_CONTRACT_PATH):)$(CONTRACTDIR) \
	  $(OMX_CONTRACT) render --target c --package omx-clap-core --installed --out $$tmp; \
	for f in omx_clap_core_host_verbs.h omx_clap_core_vocabulary.h; do \
	  $(if $(filter check-rendered,$@),cmp -s $$tmp/$$f include/plugin-hostd/$$f || { echo "include/plugin-hostd/$$f is not the render of omx-clap-core's package" >&2; exit 1; },cp $$tmp/$$f include/plugin-hostd/$$f); \
	done
	@set -e; tmp=$$(mktemp -d); trap 'rm -rf $$tmp' EXIT; \
	OMX_CONTRACT_PATH=$(if $(OMX_CONTRACT_PATH),$(OMX_CONTRACT_PATH):)$(CONTRACTDIR) \
	  $(OMX_CONTRACT) render --target c --package plugin-db --installed --out $$tmp; \
	$(if $(filter check-rendered,$@),cmp -s $$tmp/plugin_db_vocabulary.h include/plugin-hostd/plugin_db_vocabulary.h || { echo "include/plugin-hostd/plugin_db_vocabulary.h is not the render of plugin-db's package" >&2; exit 1; },cp $$tmp/plugin_db_vocabulary.h include/plugin-hostd/plugin_db_vocabulary.h); \
	echo "the rendered headers are current"

check-schema:
	python3 tests/protocol_schema.py protocol/plugin-hostd.json protocol/plugin-hostd.schema.json

PYTHON ?= python3
test-grammar:
	$(PYTHON) tests/grammar_test.py
	PYTHON=$(PYTHON) sh tests/grammar-sabotage.sh

# VERSION, the spec's Version and the newest debian/changelog entry say the same (the shared DEB workflow
# stamps an untagged build 0.1.0~git<sha>; that suffix is not compared)
version-check:
	@spec=$$(sed -n 's/^Version: *//p' packaging/$(NAME).spec); \
	deb=$$(sed -n '1s/^[^(]*(\([^-)]*\).*/\1/p' debian/changelog); \
	deb=$${deb%%~git*}; \
	test "$$spec" = "$(VERSION)" && test "$$deb" = "$(VERSION)" \
	    || { echo "VERSION $(VERSION), spec $$spec, debian/changelog $$deb"; exit 1; }; \
	echo "version $(VERSION) in VERSION, the spec and debian/changelog"

# ---------------------------------------------------------------------------------------------------------------------
# libFuzzer harnesses (fuzz/README.md), built with clang: `make fuzz`, `make fuzz-run FUZZ_TIME=60`
FUZZ_CC        ?= clang
FUZZ_BUILD     ?= build/fuzz
FUZZ_TIME      ?= 60
FUZZ_HARNESSES := protocol dispatch scenario session
FUZZ_BINS      := $(FUZZ_HARNESSES:%=$(FUZZ_BUILD)/fuzz_%)
FUZZ_CFLAGS    := -O1 -g -std=gnu99 -D_GNU_SOURCE -fno-omit-frame-pointer -fsanitize=fuzzer,address,undefined \
                  -fno-sanitize-recover=undefined
FUZZ_SRCS      := src/protocol.c src/utils.c fuzz/socket_fuzz.c src/host-dispatch.c src/host-scenario.c

$(FUZZ_BUILD)/fuzz_%: fuzz/fuzz_%.c fuzz/fuzz.h $(FUZZ_SRCS)
	@mkdir -p $(FUZZ_BUILD)
	$(FUZZ_CC) $(FUZZ_CFLAGS) -Iinclude -o $@ $< $(FUZZ_SRCS) -lpthread -lm

fuzz: $(FUZZ_BINS)

fuzz-run: fuzz
	FUZZ_BUILD=$(FUZZ_BUILD) FUZZ_TIME=$(FUZZ_TIME) sh fuzz/run.sh $(FUZZ_HARNESSES)

fuzz-corpus:
	sh fuzz/gen-corpus.sh fuzz/corpus

# the seeds generated from the grammar (tools/abnf-corpus.py, python's abnf module): fuzz-run adds them to each
# harness's corpus. check-generated fails when the grammar changed since they were written; test-abnf-corpus (in
# check-protocol-tools) writes them again and compares
ABNF_CORPUS = fuzz/corpus-abnf
fuzz-abnf-corpus:
	$(PYTHON) tools/abnf-corpus.py $(ABNF_CORPUS)

test-abnf-corpus:
	$(PYTHON) tools/abnf-corpus.py --check $(ABNF_CORPUS)

# each fuzz/sabotage/ patch is a one-line bug the check_<property> it names must catch, and only that one
fuzz-sabotage:
	FUZZ_BUILD=$(FUZZ_BUILD) MAKE="$(MAKE)" sh fuzz/sabotage.sh

# every check's comment names a one-line change and the seed on which that check aborts, or why it is not a property;
# each change is applied alone to the plain harness (fuzz/records.sh)
fuzz-records:
	FUZZ_BUILD=$(FUZZ_BUILD) FUZZ_CC=$(FUZZ_CC) sh fuzz/records.sh

# the coverage witness: the same harnesses without sanitizers, instrumented for llvm-cov, each run once over its seed
# corpus (-runs=0 executes the corpus and nothing else), held to fuzz/coverage-floor.txt (fuzz/coverage.sh)
FUZZ_COV_BUILD := $(FUZZ_BUILD)/cov
FUZZ_COV_FLAGS := -O0 -g -std=gnu99 -D_GNU_SOURCE -fprofile-instr-generate -fcoverage-mapping
LLVM_COV       ?= llvm-cov
LLVM_PROFDATA  ?= llvm-profdata

$(FUZZ_COV_BUILD)/fuzz_%: fuzz/fuzz_%.c fuzz/fuzz.h $(FUZZ_SRCS)
	@mkdir -p $(FUZZ_COV_BUILD)
	$(FUZZ_CC) $(FUZZ_COV_FLAGS) -fsanitize=fuzzer -Iinclude -o $@ $< $(FUZZ_SRCS) -lpthread -lm

fuzz-coverage: $(FUZZ_HARNESSES:%=$(FUZZ_COV_BUILD)/fuzz_%)
	@set -e; for h in $(FUZZ_HARNESSES); do \
	  rm -f $(FUZZ_COV_BUILD)/$$h.profraw; \
	  LLVM_PROFILE_FILE=$(FUZZ_COV_BUILD)/$$h.profraw $(FUZZ_COV_BUILD)/fuzz_$$h -runs=0 fuzz/corpus/$$h/ \
	      >$(FUZZ_COV_BUILD)/$$h.log 2>&1 || \
	    { cat $(FUZZ_COV_BUILD)/$$h.log; echo "fuzz-coverage: fuzz_$$h failed on its own corpus"; exit 1; }; \
	  $(LLVM_PROFDATA) merge -sparse $(FUZZ_COV_BUILD)/$$h.profraw -o $(FUZZ_COV_BUILD)/$$h.profdata; \
	done
	@LLVM_COV=$(LLVM_COV) sh fuzz/coverage.sh $(FUZZ_COV_BUILD) $(FUZZ_HARNESSES)

clean:
	rm -rf src/*.o $(LIB_SO) $(LIB_SONAME) $(LIB_FILE) $(TEST_PROGS) build

.PHONY: gen check-generated protocol-doc check-protocol-doc test-verb-table test-mod-host-json test-generated-sabotage test-default-lv2-worker \
        check-protocol-tools install-contract check-contract-dir render-verbs check-rendered test-rendered-sabotage \
        check-schema test-grammar
.PHONY: all install test test-protocol test-protocol-commands test-utils test-socket test-bind test-golden \
        test-golden-modhost golden-update golden-modhost test-fixes test-fixes-sabotage test-forms test-forms-sabotage test-hello test-runner test-hello-sabotage test-install test-abi \
        abi-symbols abi-stage abi-baseline abi-check version-check fuzz fuzz-run fuzz-corpus fuzz-abnf-corpus test-abnf-corpus fuzz-sabotage fuzz-records fuzz-coverage clean
