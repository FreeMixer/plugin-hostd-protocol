#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# The grammar against the lines it describes, read by an ABNF parser that is not ours (python `abnf`, RFC 5234 and
# RFC 7405):
#   - every command of the scenario corpus (tests/host-scenarios.txt) is a `command`, and every reply it accepts a
#     `reply`;
#   - every line the library answered in the golden transcript (tests/golden/plugin-hostd-protocol.txt) is a `reply`;
#   - every verb of the table, written with a value of each of its types, is its own rule and a `command`; with one
#     word short it is not;
#   - lines that are wrong in a way the grammar names are refused: no verb, a word that is not a verb, a word that is
#     not an int or a float where the format types one;
#   - the float rule takes what param_info writes for a value that is not finite.
#
#   python3 tests/grammar_test.py [<grammar>]       needs the abnf module (pip install abnf)
import json
import os
import re
import sys

try:
    from abnf.parser import Rule as _Rule, ParseError
except ImportError:
    sys.exit("grammar test: no python abnf module (pip install abnf); the grammar is not checked")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GRAMMAR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "grammar", "plugin-hostd-protocol.abnf")
TABLE = os.path.join(ROOT, "protocol", "plugin-hostd-protocol.json")

# the values the scenario runner puts in place of its words
SCENARIO_VALUES = {"$URI": "urn:golden:amp", "$BAD_URI": "urn:golden:missing", "$PARAM": "gain", "$DIR": "/tmp/state",
                   "$PORT_A": "system:capture_1", "$PORT_B": "effect_3:in"}
SAMPLE = {"int": "3", "float": "0.25", "word": '"a word"'}

failed = 0
checked = 0


def fail(what):
    global failed
    print("FAIL grammar: " + what)
    failed += 1


class Rule(_Rule):
    pass


with open(GRAMMAR, encoding="utf-8") as f:
    text = f.read()
# RFC 5234 ends a line with CRLF; the file is written with LF
Rule.load_grammar(text.replace("\r\n", "\n").replace("\n", "\r\n"), strict=False)


def parses(rule, line):
    global checked
    checked += 1
    try:
        Rule(rule).parse_all(line)
        return True
    except ParseError:
        return False


def must(rule, line, where):
    if not parses(rule, line):
        fail(f"{where}: {line!r} is not a {rule}")


def must_not(rule, line, where):
    if parses(rule, line):
        fail(f"{where}: {line!r} is a {rule}, and must not be")


# ---- the scenario corpus ----
scenarios = 0
with open(os.path.join(ROOT, "tests", "host-scenarios.txt"), encoding="utf-8") as f:
    for n, line in enumerate(f, 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        command, _, replies = line.partition(" => ")
        command = re.sub(r"^@\w+ ", "", command)
        for k in sorted(SCENARIO_VALUES, key=len, reverse=True):
            command = command.replace(k, SCENARIO_VALUES[k])
        must("command", command, f"host-scenarios.txt:{n}")
        for r in replies.split(" | "):
            if not r.startswith("$"):
                must("reply", r.strip(), f"host-scenarios.txt:{n}")
        scenarios += 1

# ---- what the library answered ----
answers = set()
with open(os.path.join(ROOT, "tests", "golden", "plugin-hostd-protocol.txt"), encoding="utf-8") as f:
    for line in f:
        if line.startswith("< "):
            # the transcript escapes a backslash as \\ and a byte outside printable ASCII as \xNN
            raw = line[2:].rstrip("\n")
            raw = re.sub(r"\\x([0-9a-f]{2})", lambda m: chr(int(m.group(1), 16)), raw).replace("\\\\", "\\")
            answers.add(raw)
for a in sorted(answers):
    must("reply", a, "golden transcript")

# ---- every verb of the table ----
with open(TABLE, encoding="utf-8") as f:
    table = json.load(f)
forms = 0
for c in table["commands"]:
    rule = "cmd-" + c["verb"].replace("_", "-")
    # the numbers of arguments the word takes in any of its forms: a line short of one form may be another
    taken = {len(f["args"]) for f in c["forms"]}
    tails = [len(f["args"]) for f in c["forms"] if f["tail"]]
    for f in c["forms"]:
        forms += 1
        line = " ".join([c["verb"]] + [SAMPLE[a] for a in f["args"]])
        where = f"the table's {c['verb']} ({f['origin']})"
        must(rule, line, where)
        must("command", line, where)
        if f["tail"]:
            must("command", line + " more words", where + ", with its tail")
        if f["args"]:
            short = len(f["args"]) - 1
            if short not in taken and not any(short >= t for t in tails):
                must_not(rule, " ".join([c["verb"]] + [SAMPLE[a] for a in f["args"][:-1]]), where + ", short")
            for kind, wrong in (("int", "x3"), ("float", "loud")):
                if kind in f["args"]:
                    bad = [SAMPLE[a] for a in f["args"]]
                    bad[f["args"].index(kind)] = wrong
                    # another form of the same length may take a word there
                    if not any(len(g["args"]) == len(f["args"]) and g is not f for g in c["forms"]):
                        must_not(rule, " ".join([c["verb"]] + bad), where + ", a word for an " + kind)
# a word with two forms takes both, and is told apart by the number of its arguments
must("cmd-preset-show", 'preset_show "urn:preset:a"', "mod-host's preset_show <uri>")
must("cmd-preset-show", "preset_show 3 factory:1", "plugin-hostd's preset_show <instance> <key>")
must_not("cmd-preset-show", "preset_show x3 factory:1", "preset_show <instance> <key> with a word for the instance")
for f_ in table["feedback"]:
    must("feedback", " ".join([f_["line"]] + [SAMPLE[a] for a in (f_["args"] or [])]), f"the table's {f_['line']}")
for word in table["refused"]:
    must_not("command", word, f"the refused {word}")

# ---- refusals the grammar names ----
for line in ("", "zzz 3", "add", "remove x", "set_bpm loud", "param_set 3 gain", "bypass 3", "Add urn:golden:amp 3",
             "remove  3", "remove 3 "):
    must_not("command", line, "a wrong line")
for line in ("resp", "resp x", "ok"):
    must_not("reply", line, "a wrong reply")

# ---- the float of #371 ----
for v in ("NaN", "Infinity", "-Infinity", "nan", "inf", "-inf", "0.2500", "-1e-3", "440.125", ".5"):
    must("float", v, "a float")
for v in ("", "-", "1e", "e3", "Infinityx"):
    must_not("float", v, "not a float")

if failed:
    sys.exit(1)
print(f"ok   grammar: {scenarios} scenario lines, {len(answers)} distinct answers of the library, "
      f"{len(table['commands'])} commands ({forms} forms) and {len(table['feedback'])} feedback lines of the table, "
      f"{checked} parses in all")
