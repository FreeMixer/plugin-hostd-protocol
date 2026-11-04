#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# fuzz/corpus-abnf/: seeds generated from the grammar (grammar/plugin-hostd-protocol.abnf), read by an ABNF parser that
# is not ours (python `abnf`), so every rule the grammar has gives the harnesses lines of its own shape:
#
#   protocol/abnf-<rule>-<n>   a line of each command rule (cmd-*), and of request (a tag before a command)
#   dispatch/abnf-<rule>-<n>   the same lines, after a salt byte (fuzz_dispatch's first byte)
#   session/abnf-<n>           a hello that asks for every extension, then requests
#   scenario/abnf-<n>          scenario lines, a request and the answer it accepts
#   GRAMMAR.sha256             the SHA-256 of the grammar the seeds were written from
#
# The walk is seeded, so the same grammar always gives the same seeds. `make fuzz-abnf-corpus` writes them;
# `make check-abnf-corpus` fails when the grammar changed since (the SHA-256), and `--check` (make test-abnf-corpus)
# writes them again into a scratch directory and compares, file by file.
#
#   tools/abnf-corpus.py <dir>            write the seeds under <dir>
#   tools/abnf-corpus.py --check <dir>    exit 1 when <dir> is not what this writes
import filecmp
import hashlib
import os
import random
import sys
import tempfile

try:
    from abnf.parser import Rule as _Rule, Alternation, Concatenation, Repetition, Option, Literal
except ImportError:
    sys.exit("abnf-corpus: no python abnf module (pip install abnf)")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GRAMMAR = os.path.join(ROOT, "grammar", "plugin-hostd-protocol.abnf")
SEED = 20261104
PER_RULE = 2
SESSIONS = 6
SESSION_LINES = 12
SCENARIOS = 4
SCENARIO_LINES = 10
# a repetition with no upper bound takes at most this many more than its least
SPREAD = 2


class Rule(_Rule):
    pass


def text():
    with open(GRAMMAR, encoding="utf-8") as f:
        return f.read()


def load():
    Rule.load_grammar(text().replace("\r\n", "\n").replace("\n", "\r\n"), strict=False)


def char_in(rng, lo, hi):
    lo, hi = ord(lo), ord(hi)
    # a printable ASCII character where the range has one, else one of the first few above its low end
    if lo <= 0x7E:
        return chr(rng.randint(lo, min(hi, 0x7E)))
    return chr(rng.randint(lo, min(hi, lo + 0x3FF)))


def gen(node, rng, depth=0):
    if depth > 60:
        raise RecursionError("the grammar recurses past the walk's depth")
    if isinstance(node, Rule) or isinstance(node, _Rule):
        return gen(node.definition, rng, depth + 1)
    if isinstance(node, Alternation):
        return gen(rng.choice(node.parsers), rng, depth + 1)
    if isinstance(node, Concatenation):
        return "".join(gen(p, rng, depth + 1) for p in node.parsers)
    if isinstance(node, Option):
        return gen(node.alternation, rng, depth + 1) if rng.random() < 0.5 else ""
    if isinstance(node, Repetition):
        least = node.repeat.min or 0
        most = node.repeat.max if node.repeat.max is not None else least + SPREAD
        return "".join(gen(node.element, rng, depth + 1) for _ in range(rng.randint(least, most)))
    if isinstance(node, Literal):
        if isinstance(node.value, tuple):
            return char_in(rng, *node.value)
        return node.value
    raise TypeError(f"abnf-corpus: no walk for {type(node).__name__}")


def line(rule, rng):
    return gen(Rule(rule), rng)


def write(out):
    load()
    rng = random.Random(SEED)
    # every command rule, by its name where the grammar defines it
    names = sorted({l.split()[0] for l in text().splitlines() if l.startswith("cmd-")})
    files = {}
    for rule in names + ["request"]:
        for n in range(1, PER_RULE + 1):
            l = line(rule, rng)
            files[f"protocol/abnf-{rule}-{n}"] = l.encode()
            files[f"dispatch/abnf-{rule}-{n}"] = bytes([n]) + l.encode()
    for n in range(1, SESSIONS + 1):
        lines = ["hello 1 " + " ".join(sorted({line("extension", rng) for _ in range(8)}))]
        lines += [line("request", rng) for _ in range(SESSION_LINES)]
        files[f"session/abnf-{n}"] = "\n".join(lines).encode() + b"\n"
    for n in range(1, SCENARIOS + 1):
        lines = [f"{line('request', rng)} => {line('answer', rng)}" for _ in range(SCENARIO_LINES)]
        files[f"scenario/abnf-{n}"] = "\n".join(lines).encode() + b"\n"
    files["GRAMMAR.sha256"] = (hashlib.sha256(text().encode()).hexdigest() + "  grammar/plugin-hostd-protocol.abnf\n").encode()
    for rel, data in files.items():
        path = os.path.join(out, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(data)
    return set(files)


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "--check":
        have = sys.argv[2]
        with tempfile.TemporaryDirectory() as tmp:
            want = write(tmp)
            got = {os.path.relpath(os.path.join(d, f), have) for d, _, fs in os.walk(have) for f in fs}
            stale = sorted(got ^ want) + sorted(f for f in want & got
                                                 if not filecmp.cmp(os.path.join(tmp, f), os.path.join(have, f), False))
        if stale:
            sys.exit(f"{have} is not what tools/abnf-corpus.py writes ({', '.join(stale[:5])}): run make fuzz-abnf-corpus")
        print(f"{have}: {len(want) - 1} seeds, as the grammar gives them")
        return
    if len(sys.argv) != 2:
        sys.exit(f"usage: {sys.argv[0]} [--check] <dir>")
    out = sys.argv[1]
    for d, _, fs in os.walk(out):
        for f in fs:
            os.unlink(os.path.join(d, f))
    print(f"{out}: {len(write(out)) - 1} seeds")


if __name__ == "__main__":
    main()
