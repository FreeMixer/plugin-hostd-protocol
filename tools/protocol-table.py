#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# protocol/plugin-hostd-protocol.json, the one verb table of the protocol: every word a controller or the daemon sends
# and every line the feedback port carries, each once, with where it comes from (its origin), its argument types, its
# reply and its refusals. It is merged from the declarations this tree holds and checks, and from nothing else:
#
#   protocol/mod-host.json        mod-host's commands and feedback, written from include/mod-host.h (check-generated)
#   protocol/plugin-hostd.json    what include/plugin-hostd/protocol.h declares (check-generated)
#   data/verbs.json               the verbs plugin-hostd adds, the protocol's own (hello) and the extensions hello
#                                 grants, as omx-contract declares them (check-rendered)
#   include/plugin-hostd/omx_clap_core_host_verbs.h   omx-clap-core's verbs, as omx-contract renders them
#
# A word of ours never shadows or changes a mod-host word. A verb of another origin that has a mod-host word either
# extends mod-host's command (takes mod-host's arguments, in its types, as its own first arguments, and more only where
# mod-host's command takes a tail, `...`), or is a second form of the word told apart from mod-host's by its number of
# arguments: no line mod-host's form takes is a line of the other, so a mod-host client's line is always answered as
# mod-host answers it (preset_show: `preset_show <uri>` is mod-host's, `preset_show <instance> <key>` plugin-hostd's).
# Any other is a conflict, and the table is not written; there is no list of exceptions.
#
#   tools/protocol-table.py            prints protocol/plugin-hostd-protocol.json
#   tools/protocol-table.py --check    exits 1 when protocol/plugin-hostd-protocol.json is not what it prints
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = "protocol/plugin-hostd-protocol.json"
SHARED_H = "include/plugin-hostd/omx_clap_core_host_verbs.h"

TYPES = {"%i": "int", "%s": "word", "%f": "float"}


def fail(msg):
    sys.exit(f"protocol-table: {msg}")


def read(rel):
    try:
        with open(os.path.join(ROOT, rel), encoding="utf-8") as f:
            return f.read()
    except OSError as e:
        fail(f"{rel}: {e.strerror}")


def load(rel):
    try:
        return json.loads(read(rel))
    except ValueError as e:
        fail(f"{rel}: {e}")


def counts(form):
    """the numbers of arguments a form takes: (least, most), most None for a tail"""
    return len(form["args"]), (None if form["tail"] else len(form["args"]))


def disjoint(a, b):
    """no number of arguments is taken by both forms"""
    (alo, ahi), (blo, bhi) = counts(a), counts(b)
    return (ahi is not None and ahi < blo) or (bhi is not None and bhi < alo)


def parse_format(fmt, where):
    """a format line (`verb %i %s ...`) as (verb, [types], tail)"""
    words = fmt.split()
    if not words:
        fail(f"{where}: an empty format")
    verb, args = words[0], words[1:]
    tail = bool(args) and args[-1] == "..."
    if tail:
        args = args[:-1]
    for a in args:
        if a not in TYPES:
            fail(f'{where}: "{fmt}": the word "{a}" is not %i, %s, %f or a final ...')
    return verb, [TYPES[a] for a in args], tail


def shared_verbs():
    """omx-clap-core's verbs as rendered: word -> {format, reply, codes, hosts, item}"""
    text = read(SHARED_H)
    hosts = re.search(r"#define OMX_HOST_VERB_HOSTS_INIT \{ (.*) \}", text)
    if not hosts:
        fail(f"{SHARED_H}: no OMX_HOST_VERB_HOSTS_INIT")
    hosts = re.findall(r'"([^"]+)"', hosts.group(1))
    macro = dict(re.findall(r'#define OMX_VERB_([A-Z0-9_]+) "((?:[^"\\]|\\.)*)"', text))
    item, doc = {}, {}
    for m in re.finditer(r"/\* (OMX_[A-Z_]+)\.(\w+) \(data/verbs\.json\): (.*?) \*/", text, re.S):
        key = re.sub(r"([A-Z])", r"_\1", m.group(2)).upper()
        item[key], doc[key] = m.group(1), " ".join(m.group(3).split())
    out = {}
    for m in re.finditer(r"\{ OMX_VERB_([A-Z0-9_]+), OMX_VERB_\1_FMT, OMX_VERB_\1_REPLY, \{ ([A-Z_, ]+) \} \}", text):
        key = m.group(1)
        levels = [l.strip().replace("OMX_HOST_VERB_", "").lower() for l in m.group(2).split(",")]
        codes = re.search(rf"#define OMX_VERB_{key}_CODES_INIT \{{ ([-0-9, ]*) \}}", text)
        out[macro[key]] = {
            "format": macro.get(f"{key}_FMT", ""),
            "reply": macro.get(f"{key}_REPLY", "").replace('\\"', '"'),
            "codes": [int(c) for c in codes.group(1).split(",")] if codes else [],
            "hosts": dict(zip(hosts, levels)),
            "item": item.get(key, ""),
            "doc": doc.get(key, ""),
        }
    if not out:
        fail(f"{SHARED_H}: no verb table")
    return out


def table():
    mod = load("protocol/mod-host.json")
    phd = load("protocol/plugin-hostd.json")
    verbs = load("data/verbs.json")
    shared = shared_verbs()

    vocab = read("include/plugin-hostd/omx_clap_core_vocabulary.h")
    host_codes = {}
    for m in re.finditer(r'#define OMX_REPLY_HOST_([A-Z0-9_]+) "[^"]+"(?: ///< .*)?\n#define OMX_REPLY_HOST_\1_RESP \((-?\d+)\)',
                         vocab):
        parts = m.group(1).lower().split("_")
        host_codes[parts[0] + "".join(x.capitalize() for x in parts[1:])] = int(m.group(2))
    phd_codes = {k: v["resp"] for k, v in verbs["PLUGIN_HOSTD_REPLY_CODES"]["codes"].items()}

    def resolve(ref):
        group, name = ref.split(".", 1)
        known = {"PLUGIN_HOSTD_REPLY_CODES": phd_codes, "HOST_REPLY_CODES": host_codes}.get(group)
        if known is None or name not in known:
            fail(f"data/verbs.json: the code {ref} is declared nowhere")
        return known[name]

    declared = {v["verb"]: (k, v) for k, v in verbs["PLUGIN_HOSTD_VERBS"]["verbs"].items()}
    meaning = {v["name"]: v for v in phd["verbs"] + phd["instance_verbs"]}
    host_rows = {r["name"]: r for r in phd["host_verbs"]}

    commands = {}
    for c in mod["commands"]:
        commands[c["verb"]] = {
            "verb": c["verb"], "origin": "mod-host", "declared": f"include/mod-host.h {c['name']}",
            "forms": [{"args": [TYPES[a] for a in c["args"]], "tail": c["optional_tail"], "origin": "mod-host"}],
            "reply": None, "codes": [], "sent_by": "controller", "extended_by": [],
        }

    def ours(word, origin, where, fmt, reply, codes, doc):
        verb, args, tail = parse_format(fmt, where)
        if verb != word:
            fail(f"{where}: the format names {verb}, not {word}")
        said = (meaning.get(word, {}).get("meaning", "") + " " + doc).lower()
        sent_by = "daemon" if "sent by the daemon" in said or "plugin-hostd sends it" in said \
            else "controller"
        return {"verb": word, "origin": origin, "declared": where,
                "forms": [{"args": args, "tail": tail, "origin": origin}], "reply": reply, "codes": codes,
                "sent_by": sent_by, "extended_by": []}

    entries = []
    for word, (key, v) in declared.items():
        entries.append(ours(word, "plugin-hostd", f"data/verbs.json PLUGIN_HOSTD_VERBS.{key}", v["format"], v["reply"],
                            [resolve(c["ref"]) for c in v.get("codes", [])], v.get("doc", "")))
    # the verbs of the protocol itself (hello), which the library answers for the host that registers them
    for key, v in verbs["PLUGIN_HOSTD_PROTOCOL_VERBS"]["verbs"].items():
        if v["verb"] in declared:
            fail(f"{v['verb']}: declared by both PLUGIN_HOSTD_VERBS and PLUGIN_HOSTD_PROTOCOL_VERBS")
        entries.append(ours(v["verb"], "plugin-hostd-protocol", f"data/verbs.json PLUGIN_HOSTD_PROTOCOL_VERBS.{key}",
                            v["format"], v["reply"], [resolve(c["ref"]) for c in v.get("codes", [])], v.get("doc", "")))
    for word, v in shared.items():
        entries.append(ours(word, "omx-clap-core", f"omx-clap-core data/verbs.json {v['item']}", v["format"],
                            v["reply"], v["codes"], v["doc"]))
    # a verb protocol.h declares that neither declaration holds is mod-host's (add) or a fault
    for word in meaning:
        if word not in commands and word not in declared and word not in shared:
            fail(f"protocol/plugin-hostd.json: {word} is declared by protocol.h alone; declare it in data/verbs.json")

    for e in entries:
        word = e["verb"]
        base = commands.get(word)
        if base is None:
            commands[word] = e
            continue
        mine = e["forms"][0]
        if base["origin"] != "mod-host":
            if base["forms"][0]["args"] != mine["args"] or base["forms"][0]["tail"] != mine["tail"]:
                fail(f"{word}: declared twice with other arguments, by {base['declared']} and {e['declared']}")
            base["extended_by"].append(e["origin"])
            continue
        theirs = base["forms"][0]
        n = len(theirs["args"])
        extends = mine["args"][:n] == theirs["args"] and (theirs["tail"] or (len(mine["args"]) == n and not mine["tail"]))
        if extends:
            if e["origin"] not in base["extended_by"]:
                base["extended_by"].append(e["origin"])
            base["reply"] = base["reply"] or e["reply"]
            base["codes"] = base["codes"] or e["codes"]
        elif disjoint(theirs, mine):
            # a second form, told apart by its number of arguments: its reply and refusals are its own
            base["forms"].append(dict(mine, declared=e["declared"], reply=e["reply"], codes=e["codes"]))
        else:
            fail(f"{word}: a word of ours changes mod-host's {word}: mod-host's takes {json.dumps(theirs)}, "
                 f"{e['declared']} takes {json.dumps(mine)}, which neither extends it nor is told apart from it by its "
                 "number of arguments; a word added to the protocol never changes a mod-host word")
    # add: plugin-hostd's placement is a word of mod-host's own tail, declared in protocol.h
    if "add" in meaning and "add" in commands and "plugin-hostd" not in commands["add"]["extended_by"]:
        commands["add"]["extended_by"].append("plugin-hostd")

    feedback = {}
    for f in mod["feedback"]:
        feedback[f["verb"]] = {"line": f["verb"], "origin": "mod-host", "args": [TYPES[a] for a in f["args"]],
                               "tail": f["optional_tail"], "fields": None}
    for ev in phd["events"]:
        if ev["name"] in feedback:
            feedback[ev["name"]]["fields"] = ev["fields"]
            continue
        feedback[ev["name"]] = {"line": ev["name"], "origin": "plugin-hostd", "args": None, "tail": True,
                                "fields": ev["fields"]}

    for word, e in commands.items():
        row = host_rows.get(word)
        e["levels"] = row["levels"] if row else None
        e["extended_by"] = sorted(e["extended_by"])
    mod_ui = [r["name"] for r in phd["host_verbs"] if r["name"] not in commands]

    return {
        "protocol": "plugin-hostd-protocol",
        "version": phd["version"],
        "superset_of": "mod-host",
        "sources": ["protocol/mod-host.json", "protocol/plugin-hostd.json", "data/verbs.json", SHARED_H],
        "commands": sorted(commands.values(), key=lambda e: (e["origin"] != "mod-host", e["origin"], e["verb"])),
        "refused": mod_ui,
        "feedback": sorted(feedback.values(), key=lambda e: (e["origin"] != "mod-host", e["line"])),
        "protocol_errors": mod["protocol_errors"],
        # what hello can grant, in the order its reply names them
        "extensions": [{"word": w["word"], "doc": w.get("doc", "")}
                       for w in verbs["PLUGIN_HOSTD_PROTOCOL_EXTENSIONS"]["words"].values()],
    }


def dump(doc):
    """one object per line inside each list, so a diff of the table reads verb by verb"""
    out = ["{"]
    keys = list(doc)
    for i, k in enumerate(keys):
        comma = "," if i < len(keys) - 1 else ""
        v = doc[k]
        if isinstance(v, list) and v and isinstance(v[0], dict):
            out.append(f"  {json.dumps(k)}: [")
            out += [f"    {json.dumps(x, ensure_ascii=False)}" + ("," if j < len(v) - 1 else "") for j, x in enumerate(v)]
            out.append(f"  ]{comma}")
        else:
            out.append(f"  {json.dumps(k)}: {json.dumps(v, ensure_ascii=False)}{comma}")
    out.append("}")
    return "\n".join(out) + "\n"


def main():
    text = dump(table())
    if len(sys.argv) > 1 and sys.argv[1] == "--check":
        try:
            have = read(OUT)
        except SystemExit:
            have = None
        if have != text:
            sys.exit(f"{OUT} is not what tools/protocol-table.py writes: run make gen")
        print(f"{OUT} is current")
        return
    if len(sys.argv) > 1:
        fail(f"usage: {sys.argv[0]} [--check]")
    sys.stdout.write(text)


if __name__ == "__main__":
    main()
