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

PC_LIBDIR = $(patsubst $(PREFIX)/%,$${prefix}/%,$(LIBDIR))
PC_INCLUDEDIR = $(patsubst $(PREFIX)/%,$${prefix}/%,$(INCLUDEDIR))
PC_DATADIR = $(patsubst $(PREFIX)/%,$${prefix}/%,$(DATADIR))

all: $(LIB_SO)

$(OBJ): $(HEADERS)

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
	sed -e 's,@PREFIX@,$(PREFIX),' -e 's,@LIBDIR@,$(PC_LIBDIR),' -e 's,@INCLUDEDIR@,$(PC_INCLUDEDIR),' \
	    -e 's,@DATADIR@,$(PC_DATADIR),' -e 's,@VERSION@,$(VERSION),' $(NAME).pc.in > $(DESTDIR)$(LIBDIR)/pkgconfig/$(NAME).pc
	chmod 644 $(DESTDIR)$(LIBDIR)/pkgconfig/$(NAME).pc

# ---------------------------------------------------------------------------------------------------------------------
# tests: none needs JACK, a plugin or a network beyond 127.0.0.1

TEST_CFLAGS = $(filter-out -O3,$(CFLAGS)) -O1 -g -Werror
# the sources are compiled into the sanitized tests as they are; a distribution's _FORTIFY_SOURCE flags their unchecked
# write() calls, which mod-host leaves unchecked too
SAN = -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Wno-error=unused-result
TEST_PROGS = tests/protocol_test tests/protocol_commands_test tests/host_scenarios tests/golden-run tests/fixes_test \
             tests/socket-test tests/bind-test tests/utils-test

test: test-protocol test-protocol-commands test-utils test-socket test-bind test-golden test-golden-modhost \
      test-fixes test-fixes-sabotage test-install test-abi version-check

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

# the fixes of plugin-hostd#371 and #372 (tests/fixes_test.c), against the sources with AddressSanitizer and
# UndefinedBehaviorSanitizer
test-fixes: tests/fixes_test
	./tests/fixes_test

tests/fixes_test: tests/fixes_test.c $(SRC) $(HEADERS)
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

# VERSION, the spec's Version and the newest debian/changelog entry say the same
version-check:
	@spec=$$(sed -n 's/^Version: *//p' packaging/$(NAME).spec); \
	deb=$$(sed -n '1s/^[^(]*(\([^-)]*\).*/\1/p' debian/changelog); \
	test "$$spec" = "$(VERSION)" && test "$$deb" = "$(VERSION)" \
	    || { echo "VERSION $(VERSION), spec $$spec, debian/changelog $$deb"; exit 1; }; \
	echo "version $(VERSION) in VERSION, the spec and debian/changelog"

# ---------------------------------------------------------------------------------------------------------------------
# libFuzzer harnesses (fuzz/README.md), built with clang: `make fuzz`, `make fuzz-run FUZZ_TIME=60`
FUZZ_CC        ?= clang
FUZZ_BUILD     ?= build/fuzz
FUZZ_TIME      ?= 60
FUZZ_HARNESSES := protocol dispatch scenario
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

clean:
	rm -rf src/*.o $(LIB_SO) $(LIB_SONAME) $(LIB_FILE) $(TEST_PROGS) build

.PHONY: all install test test-protocol test-protocol-commands test-utils test-socket test-bind test-golden \
        test-golden-modhost golden-update golden-modhost test-fixes test-fixes-sabotage test-install test-abi \
        abi-symbols abi-stage abi-baseline abi-check version-check fuzz fuzz-run fuzz-corpus clean
