#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2026 Hachi
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version. See COPYING.
"""Check the UI message files (data/cfg/*.msg) against the code.

gpSP loads messages by position and refuses to start if a file has the wrong
number of entries. Message strings are also used as printf formats, so a
%s where the code passes a number crashes the PSP. This script checks:

  1. every .msg file has exactly one entry per name in src/message.h, and the
     "# MSG_..." label above each entry matches the name at that position;
  2. the printf codes of each message fit the arguments the code passes;
  3. no line is longer than the loader's line buffer (load_msgcfg reads
     lines with fgets into 256 bytes, so a longer line is split and the
     entries shift), and the file ends with a newline (the loader drops
     a last line without one).

Usage: tools/check_messages.py [REPO_ROOT]   (exit status 1 on problems)
"""
import glob
import os
import re
import sys

# Labels in the b225 message files that differ from the names in message.h.
LABEL_ALIASES = {
    "MSG_DATE_FMT_0": "MSG_MENU_DATE_FMT_0",
    "MSG_DATE_FMT_1": "MSG_MENU_DATE_FMT_1",
    "MSG_DATE_FMT_2": "MSG_MENU_DATE_FMT_2",
    "MSG_FS_RAMDOM": "MSG_FS_RANDOM",
    "MSG_SS_FMT_1": "MSG_SS_FMT_0",
    "MSG_SS_FMT_2": "MSG_SS_FMT_1",
    "MSG_SS_FMT_3": "MSG_SS_FMT_2",
    "MSG_LOAD_STATE_No": "MSG_LOAD_STATE_NUM",
    "MSG_SAVE_STATE_No": "MSG_SAVE_STATE_NUM",
}

# Size of the line buffer in load_msgcfg (src/gui.c).
MAX_LINE_BYTES = 256

# Argument types passed by code that formats messages directly
# (src/gui.c get_timestamp_string, print_status, cheat menu; src/memory.c).
DATE = ("d", "d", "d", "s", "d", "d", "d", "d")
FIXED_FORMATS = {
    "MSG_CHEAT_MENU_NON_LOAD": ("d",),
    "MSG_CHEAT_MENU_0": ("d", "s"),
    "MSG_MENU_BATTERY": ("d", "d"),
    "MSG_MENU_BATTERY_TIME": ("d", "d"),
    "MSG_MENU_BATTERY_CHARGING": (),
    "MSG_LOAD_STATE_NUM": ("d",),
    "MSG_SAVE_STATE_NUM": ("d",),
    "MSG_STATE_MENU_MEMORY_LABEL": (),
    "MSG_STATE_MENU_MEMORY_NONE": (),
    "MSG_DELETE_STATE_NUM": ("d",),
    "MSG_SS_FMT_0": DATE,
    # The screenshot file name formats put the weekday first.
    "MSG_SS_FMT_1": ("s", "d", "d", "d", "d", "d", "d", "d"),
    "MSG_SS_FMT_2": ("s", "d", "d", "d", "d", "d", "d", "d"),
}
for base in ("MSG_MENU_DATE_FMT_", "MSG_STATE_MENU_DATE_FMT_",
             "MSG_STATE_MENU_DATE_NONE_"):
    for i in range(3):
        FIXED_FORMATS[base + str(i)] = DATE

MENU_MACROS = re.compile(
    r"(STRING_SELECTION(?:_ACTION)?_OPTION|NUMERIC_SELECTION(?:_ACTION|_HIDE)?"
    r"_OPTION|GAMEPAD_CONFIG_OPTION|ANALOG_CONFIG_OPTION|ACTION_OPTION|"
    r"SUBMENU_OPTION)\(([^;]*?)\)\s*,?\s*(?:/\*.*?\*/)?\s*$", re.M)


def enum_names(root):
    text = open(os.path.join(root, "src", "message.h"), encoding="utf-8").read()
    body = text[text.index("enum MSG"):text.index("MSG_END")]
    return re.findall(r"^\s*(MSG_\w+),", body, re.M)


def read_messages(path):
    """Return a list of (label, text) per entry, in file order."""
    raw = open(path, "rb").read().decode("utf-8").lstrip("﻿")
    entries, label, current = [], None, None
    for line in raw.split("\n"):
        line = line.rstrip("\r")
        if line.startswith("!"):
            if current is None:
                current = [label, []]
                entries.append(current)
            current[1].append(line[1:])
        else:
            current = None
            m = re.match(r"# (MSG_\w+)", line)
            if m:
                label = LABEL_ALIASES.get(m.group(1), m.group(1))
    return [(lab, "\n".join(parts)) for lab, parts in entries]


def printf_codes(text):
    return tuple(c[-1] for c in
                 re.findall(r"%[-+ #0-9.]*[a-zA-Z]", text.replace("%%", "")))


def expected_formats(root):
    """Map message name -> set of acceptable argument type tuples."""
    gui = open(os.path.join(root, "src", "gui.c"), encoding="utf-8").read()
    expect = {}
    for m in MENU_MACROS.finditer(gui):
        kind, args = m.group(1), m.group(2)
        names = re.findall(r"msg\[(MSG_\w+)\]", args)
        if not names:
            continue
        if kind.startswith("STRING") or "CONFIG" in kind:
            types = ("s",)
        elif kind.startswith("NUMERIC"):
            types = ("d",)
        else:
            types = ()
        expect.setdefault(names[0], set()).add(types)
        for help_name in names[1:]:          # help texts are printed verbatim
            expect.setdefault(help_name, set()).add(())
    for name, types in FIXED_FORMATS.items():
        expect.setdefault(name, set()).add(types)
    return expect


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..")
    names = enum_names(root)
    expect = expected_formats(root)
    problems = 0
    files = sorted(glob.glob(os.path.join(root, "data", "cfg", "*.msg")))
    for path in files:
        lang = os.path.basename(path)
        for number, line in enumerate(open(path, "rb").read().split(b"\n"), 1):
            # fgets keeps the "\n" and needs a byte for the terminating NUL.
            if len(line) + 1 > MAX_LINE_BYTES - 1:
                print(f"{lang}:{number}: line is {len(line)} bytes, "
                      f"the loader reads at most {MAX_LINE_BYTES - 2}")
                problems += 1
        if not open(path, "rb").read().endswith(b"\n"):
            # parse_line ignores a line without "\n", so the last entry is lost.
            print(f"{lang}: the file does not end with a newline")
            problems += 1
        entries = read_messages(path)
        if len(entries) != len(names):
            print(f"{lang}: {len(entries)} entries, message.h has {len(names)}")
            problems += 1
        for i, ((label, _), name) in enumerate(zip(entries, names)):
            if label != name:
                print(f"{lang}: entry {i} is labelled {label}, expected {name}")
                problems += 1
        texts = {name: text for name, (_, text) in zip(names, entries)}
        for name, accepted in sorted(expect.items()):
            if name not in texts:
                continue
            got = printf_codes(texts[name])
            # Fewer codes than arguments is safe; the types must line up.
            if not all(got == want[:len(got)] for want in accepted):
                print(f"{lang}: {name} has printf codes {got}, "
                      f"code passes {sorted(accepted)}")
                problems += 1
    print(f"checked {len(files)} message files against {len(names)} messages:"
          f" {problems} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
