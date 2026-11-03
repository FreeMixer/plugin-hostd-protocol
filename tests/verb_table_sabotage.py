# SPDX-License-Identifier: GPL-3.0-or-later
# tests/verb_table.py against a scratch copy of include/ with one row of protocol.h broken in each arm: it must fail on
# each, and pass on the copy as it is, so a verb table check that holds nothing is caught.
#
#   python3 tests/verb_table_sabotage.py          (make test-verb-table)
import os, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join("include", "plugin-hostd", "protocol.h")

# (what is broken, the text, its replacement)
ARMS = [
    ("a row of the host verb table is dropped",
     '    X("cpu_load", MOD_UI, REQUIRED, REQUIRED, REQUIRED, REQUIRED) \\\n', ""),
    ("a verb is added to the daemon with no row in the host verb table",
     '    X(CHAIN_CLEAR, OMX_VERB_CHAIN_CLEAR, "<chain>", OMX_VERB_CHAIN_CLEAR_REPLY, \\\n',
     '    X(SABOTAGE, "verb_table_sabotage", "", "resp 0", "a verb with no row") \\\n'
     '    X(CHAIN_CLEAR, OMX_VERB_CHAIN_CLEAR, "<chain>", OMX_VERB_CHAIN_CLEAR_REPLY, \\\n'),
    ("a verb the daemon answers itself is absent from a profile",
     "    X(OMX_VERB_CHAIN, DAEMON, REQUIRED, REQUIRED, REQUIRED, REQUIRED) \\\n",
     "    X(OMX_VERB_CHAIN, DAEMON, REQUIRED, REQUIRED, ABSENT, REQUIRED) \\\n"),
    ("a verb mod-host never had is required of a profile",
     '    X("set_midi_program_change_pedalboard_bank_channel", MOD_UI, ABSENT, ABSENT, ABSENT, ABSENT) \\\n',
     '    X("set_midi_program_change_pedalboard_bank_channel", MOD_UI, ABSENT, REQUIRED, ABSENT, ABSENT) \\\n'),
]


def run(tree):
    return subprocess.run([sys.executable, os.path.join(ROOT, "tests", "verb_table.py"), tree], capture_output=True,
                          text=True)


failed = 0
work = tempfile.mkdtemp(prefix="verb-table-sabotage.")
try:
    tree = os.path.join(work, "t")
    os.mkdir(tree)
    shutil.copytree(os.path.join(ROOT, "include"), os.path.join(tree, "include"))
    if run(tree).returncode != 0:
        print("FAIL control: tests/verb_table.py fails on the copy as it is")
        failed += 1
    original = open(os.path.join(ROOT, HEADER)).read()
    for what, text, repl in ARMS:
        if original.count(text) != 1:
            print("FAIL %s: the text to break occurs %d times in %s" % (what, original.count(text), HEADER))
            failed += 1
            continue
        with open(os.path.join(tree, HEADER), "w") as f:
            f.write(original.replace(text, repl))
        r = run(tree)
        if r.returncode == 0:
            print("FAIL %s: tests/verb_table.py passes" % what)
            failed += 1
        else:
            print("ok   %s: red (%s)" % (what, r.stdout.strip().splitlines()[0][:100]))
    with open(os.path.join(tree, HEADER), "w") as f:
        f.write(original)
finally:
    shutil.rmtree(work, ignore_errors=True)
print("verb table sabotage %s" % ("FAILED" if failed else "ok"))
sys.exit(1 if failed else 0)
