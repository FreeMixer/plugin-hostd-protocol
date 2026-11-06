#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# The grammar against the lines it describes, read by an ABNF parser that is not ours (python `abnf`, RFC 5234 and
# RFC 7405):
#   - every command of the scenario corpus (tests/host-scenarios.txt) is a `request` (a command, tagged or not), and
#     every reply it accepts an `answer`;
#   - every line the library answered in the golden transcript (tests/golden/plugin-hostd-protocol.txt) is an `answer`;
#   - hello and its reply, tags and tagged lines take the shapes the grammar gives them, and no other;
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
        fail(f"{where}: {line!r} is not {'an' if rule[0] in 'aeiou' else 'a'} {rule}")


def must_not(rule, line, where):
    if parses(rule, line):
        fail(f"{where}: {line!r} is {'an' if rule[0] in 'aeiou' else 'a'} {rule}, and must not be")


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
        # a line of a session may carry a tag; one whose first word starts with "@" but is no tag is not a request
        if re.match(r"@[^ ]* ", command) and not parses("tag", command.split(" ", 1)[0]):
            must_not("request", command, f"host-scenarios.txt:{n}")
        else:
            must("request", command, f"host-scenarios.txt:{n}")
        for r in replies.split(" | "):
            if not r.startswith("$"):
                must("answer", r.strip(), f"host-scenarios.txt:{n}")
        scenarios += 1

# ---- what the library answered ----
def table_version():
    with open(TABLE, encoding="utf-8") as f:
        return json.load(f)["version"]


hello_answers = error_answers = 0
answers = set()
with open(os.path.join(ROOT, "tests", "golden", "plugin-hostd-protocol.txt"), encoding="utf-8") as f:
    for line in f:
        if line.startswith("< "):
            # the transcript escapes a backslash as \\ and a byte outside printable ASCII as \xNN
            raw = line[2:].rstrip("\n")
            raw = re.sub(r"\\x([0-9a-f]{2})", lambda m: chr(int(m.group(1), 16)), raw).replace("\\\\", "\\")
            answers.add(raw)
for a in sorted(answers):
    must("answer", a, "golden transcript")
    if a.startswith("resp 0 ") and a.split()[2:3] == [str(table_version())] and all(
            w in [e["word"] for e in json.load(open(TABLE, encoding="utf-8"))["extensions"]] for w in a.split()[3:]):
        hello_answers += 1

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

# ---- hello and request tags ----
version = table["version"]
words = [e["word"] for e in table["extensions"]]
must("cmd-hello", f"hello {version}", "hello with no extension")
must("cmd-hello", f"hello {version} " + " ".join(words) + " no_such_extension", "hello with an unknown extension")
must_not("cmd-hello", "hello", "hello with no version")
must_not("cmd-hello", "hello x", "hello with a word for the version")
must("hello-reply", f"resp 0 {version}", "hello's reply granting nothing")
must("hello-reply", f"resp 0 {version} " + " ".join(words), "hello's reply granting every extension")
must("reply", f"resp 0 {version} " + " ".join(words), "hello's reply is a reply")
must_not("hello-reply", f"resp 0 {version} no_such_extension", "hello's reply granting what no table has")
must_not("hello-reply", f"resp 0 {version} " + " ".join(words + words), "hello's reply granting one twice")
for line in ("@a7 param_get 3 gain", "@t.2-x_Y param_get 3 gain", "param_get 3 gain"):
    must("request", line, "a request")
for line in ("@ param_get 3 gain", "@a@b param_get 3 gain", "@a7  param_get 3 gain", "@a7"):
    must_not("request", line, "not a request")
for line in ("@a7 resp 0 0.2500", "@a8 few arguments", "resp -3"):
    must("answer", line, "an answer")
for line in ("@ resp 0", "@a7resp 0"):
    must_not("answer", line, "not an answer")
fb0 = " ".join([table["feedback"][0]["line"]] + [SAMPLE[a] for a in (table["feedback"][0]["args"] or [])])
must_not("feedback", "@a7 " + fb0, "a tagged feedback line")
must_not("event", "@a7 " + fb0, "a tagged event")
for n in ("1", "17", "4294967296"):
    must("event", f"#{n} " + fb0, "a numbered feedback line")
must("event", fb0, "a feedback line without a number")
for n in ("#0", "#", "#x", "#-1", "#01"):
    must_not("event", f"{n} " + fb0, "not a numbered feedback line")
must_not("request", "#1 " + fb0.split(" ")[0], "a numbered request")

codes = table["reply_codes"]
for c in codes:
    must("error-reply", f"resp {c['resp']} {c['code']}", "a refusal with its error word")
    must("reply", f"resp {c['resp']} {c['code']}", "a refusal with its error word is a reply")
must_not("error-reply", f"resp {codes[0]['resp']} {codes[1]['code']}", "a refusal with another number's word")
must_not("error-reply", f"resp {codes[0]['resp']}", "a refusal without its word")
for a in answers:
    words_ = a.split(" ")
    if len(words_) == 3 and words_[0] == "resp" and words_[2].startswith("reply."):
        must("error-reply", a, "golden transcript, a refusal with its error word")
        error_answers += 1
if error_answers == 0:
    fail("the golden transcript holds no refusal with its error word")

# ---- the curve verbs' words (core.abnf, curve-binding) ----
# the role and shape words are omx-dsp's: the grammar's alternatives, the words omx_curve.h holds, one for one
def header_words(table_name):
    import subprocess
    try:
        flags = subprocess.run(["pkg-config", "--cflags-only-I", "omxdsp"], capture_output=True, text=True,
                               check=True).stdout.split()
    except (OSError, subprocess.CalledProcessError):
        fail("no omxdsp for pkg-config: the curve words cannot be held to omx_curve.h")
        return []
    for d in [f[2:] for f in flags] + ["/usr/include"]:
        h = os.path.join(d, "omxdsp", "omx_curve.h")
        if os.path.exists(h):
            text = open(h, encoding="utf-8").read()
            m = re.search(table_name + r"\[[A-Z_]+\] = \{([^}]*)\}", text)
            if m:
                return re.findall(r'"([^"]*)"', m.group(1))
    fail(f"omx_curve.h declares no {table_name}")
    return []


def grammar_words(rule):
    m = re.search(r"^" + rule + r" += ((?:.*\n)(?:  +.*\n)*)", text, re.M)
    return re.findall(r'"([^"]*)"', m.group(1)) if m else []


for rule, table_name in (("curve-role", "OMX_CURVE_ROLE_WORDS"), ("curve-shape", "OMX_CURVE_SHAPE_WORDS")):
    have, want = grammar_words(rule), header_words(table_name)
    if sorted(have) != sorted(want) or not want:
        fail(f"{rule} is {have}, omx_curve.h's {table_name} is {want}")
for line in ("clear", "transfer_curve dynamics threshold=thr:dbfs ratio=ratio:ratio knee=k:db makeup=g:db",
             "eq_response eq band_freq.0=f_0:hz band_gain.0=g_0:coef band_q.0=w_0:oct band_type.0=ft_0@off|bell|lowshelf|highshelf",
             "eq_response eq band_type.31=t-31@-|bell band_freq.31=f.31:khz@notch hpf_on=:bypass",
             "transfer_curve gate threshold=t:db range=r:db knee_start=a:db knee_end=b:db ratio=q:coef"):
    must("curve-binding", line, "a curve binding")
    must("cmd-curve-bind", "curve_bind 3 0 " + line, "a curve_bind line")
for line in ("", "clear threshold=t:db", "bar_graph dynamics threshold=t:db", "transfer_curve dynamics",
             "transfer_curve dynamics threshold", "transfer_curve dynamics attack=a:ms", "eq_response eq band_freq.x=f:hz",
             "eq_response eq band_freq.=f:hz", "transfer_curve dynamics threshold=", "transfer_curve dynamics threshold=t:",
             "eq_response eq band_type.0=t@bell|wobble", "eq_response eq band_type.0=t@bell||notch",
             "transfer_curve dynamics threshold=t/x:db", "transfer_curve Dynamics threshold=t:db"):
    must_not("curve-binding", line, "not a curve binding")
curve_replies = 0
for a in answers:
    if re.match(r"resp 0 \d+ \S+,", a):
        must("curve-points-reply", a, "golden transcript, curve_points' reply")
        curve_replies += 1
if curve_replies == 0:
    fail("the golden transcript holds no curve_points reply")
for line in ("resp 0 1 20,0.0", "resp 0 2 20.00,1", "resp 0 0", "resp 0 3 x,0.00"):
    must_not("curve-points-reply", line, "not a curve_points reply")
with open(os.path.join(ROOT, "tests", "golden", "plugin-hostd-protocol.txt"), encoding="utf-8") as f:
    golden = f.read().split("\n")
for i, line in enumerate(golden[:-1]):
    if line.startswith("> curve_bind ") and golden[i + 1] == "< resp 0":
        must("curve-binding", line[2:].split(" ", 3)[3], "golden transcript, a curve_bind answered resp 0")

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
if hello_answers == 0:
    fail("the golden transcript holds no answer to hello")
print(f"ok   grammar: {scenarios} scenario lines, {len(answers)} distinct answers of the library ({hello_answers} to hello, "
      f"{error_answers} with an error word), "
      f"{len(table['commands'])} commands ({forms} forms) and {len(table['feedback'])} feedback lines of the table, "
      f"{checked} parses in all")
