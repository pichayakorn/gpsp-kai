#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2026 Hachi
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version. See COPYING.
"""Translation files of the 4.0 interface (data/lang).

  po.py extract  write data/lang/kai.pot from the T("...") and TN("...")
                 texts in src/ui/*.c
  po.py check    the template is up to date, no two texts share a lookup
                 hash, and every data/lang/<code>.po keeps the printf
                 conversions of its English texts (kai ignores one that
                 doesn't, but it is a translator's mistake to fix)
  po.py pseudo CODE DIR
                 write DIR/CODE.po with every text marked [like this], to
                 see on a PSP which texts are not translatable yet (use a
                 test folder, e.g. testdata/lang, not data/lang)

Exit status 1 on problems.
"""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
POT = os.path.join(ROOT, 'data', 'lang', 'kai.pot')
SOURCES = sorted(glob.glob(os.path.join(ROOT, 'src', 'ui', '*.c'))) + [os.path.join(ROOT, 'src', 'cheat_engine.c')]
SKIP = {'ui_bench.c', 'ui_components.c'}  # developer screens, English only

STRING = r'"(?:[^"\\\n]|\\.)*"'
CALL = re.compile(r'\b(?:T|TN)\(\s*((?:' + STRING + r'\s*)+)\)')
ESCAPES = {'n': '\n', 't': '\t', 'r': '\r', '"': '"', '\\': '\\', "'": "'", '0': '\0'}


def c_string(literals):
    """The bytes of adjacent C string literals, as text."""
    out = bytearray()
    for lit in re.findall(STRING, literals):
        s, i = lit[1:-1], 0
        while i < len(s):
            c = s[i]
            if c != '\\':
                out += c.encode('utf-8')
                i += 1
                continue
            e = s[i + 1]
            if e == 'x':
                m = re.match(r'[0-9A-Fa-f]+', s[i + 2:])
                out.append(int(m.group(0), 16) & 0xFF)
                i += 2 + len(m.group(0))
            elif e in '01234567':
                m = re.match(r'[0-7]{1,3}', s[i + 1:])
                out.append(int(m.group(0), 8))
                i += 1 + len(m.group(0))
            else:
                out += ESCAPES[e].encode('utf-8')
                i += 2
    return out.decode('utf-8')


def collect():
    """[(text, [file:line, ...])] in order of first use."""
    found = {}
    for path in SOURCES:
        if os.path.basename(path) in SKIP:
            continue
        src = open(path, encoding='utf-8').read()
        code = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), src, flags=re.S)
        for m in CALL.finditer(code):
            text = c_string(m.group(1))
            line = code.count('\n', 0, m.start()) + 1
            found.setdefault(text, []).append('%s:%d' % (os.path.relpath(path, ROOT), line))
    return list(found.items())


def po_quote(s):
    s = s.replace('\\', '\\\\').replace('"', '\\"').replace('\t', '\\t')
    parts = s.split('\n')
    if len(parts) == 1:
        return '"%s"' % s
    lines = [p + '\\n' for p in parts[:-1]] + ([parts[-1]] if parts[-1] else [])
    return '""\n' + '\n'.join('"%s"' % l for l in lines)


HEADER = ('msgid ""\nmsgstr ""\n'
          '"Project-Id-Version: gpSP kai\\n"\n'
          '"MIME-Version: 1.0\\n"\n'
          '"Content-Type: text/plain; charset=UTF-8\\n"\n'
          '"Content-Transfer-Encoding: 8bit\\n"\n')


def pot_text(entries, translate=None, lang=None):
    out = ['# Texts of the gpSP kai 4.0 interface. Made by tools/po.py extract;',
           '# do not edit: translations go in <code>.po beside it.', '']
    head = HEADER
    if lang:
        head = head.replace('"MIME', '"Language: %s\\n"\n"MIME' % lang)
    out.append(head)
    for text, refs in entries:
        out.append('#: ' + ' '.join(refs))
        if formats(text):
            out.append('#, c-format')
        out.append('msgid ' + po_quote(text))
        out.append('msgstr ' + po_quote(translate(text) if translate else ''))
        out.append('')
    return '\n'.join(out)


def fnv1a(s):
    h = 2166136261
    for b in s.encode('utf-8'):
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


CONVERSION = re.compile(r'[-+ #0]*[0-9]*(?:\.[0-9]*)?[hlz]*[diouxXcsfeEgGp]')


def formats(s):
    """The printf conversions of s, as kai's ui_tr compares them: a % that
    isn't followed by a whole conversion ("100 % of") is text."""
    out, i = [], 0
    while i < len(s):
        if s[i] != '%':
            i += 1
            continue
        i += 1
        if i < len(s) and s[i] == '%':
            i += 1
            continue
        m = CONVERSION.match(s, i)
        if m:
            out.append(m.group(0))
            i = m.end()
    return out


def read_po(path):
    """{msgid: msgstr} of the entries kai would use (not fuzzy, no context)."""
    entries, cur, field, fuzzy = {}, None, None, False
    def unq(s):
        return re.sub(r'\\(.)', lambda m: {'n': '\n', 't': '\t'}.get(m.group(1), m.group(1)), s)
    def flush():
        if cur and not cur.get('skip') and cur.get('id') and cur.get('str'):
            entries[cur['id']] = cur['str']
    for line in open(path, encoding='utf-8').read().split('\n'):
        m = re.match(r'(msgctxt|msgid|msgstr) "(.*)"$', line)
        if line.startswith('#,') and 'fuzzy' in line:
            fuzzy = True
        elif m and m.group(1) in ('msgctxt', 'msgid') and not (cur and cur.get('ctxt') and m.group(1) == 'msgid'):
            if m.group(1) == 'msgid':
                flush()
                cur = {'id': unq(m.group(2)), 'skip': fuzzy}
                fuzzy, field = False, 'id'
            else:
                flush()
                cur, field = {'ctxt': True, 'skip': True}, None
        elif m and m.group(1) == 'msgid':
            cur['id'], field = unq(m.group(2)), 'id'
        elif m and m.group(1) == 'msgstr' and cur is not None:
            cur['str'], field = unq(m.group(2)), 'str'
        elif line.startswith('"') and cur is not None and field:
            cur[field] = cur.get(field, '') + unq(line[1:-1])
        elif not line.strip():
            field = None
    flush()
    return entries


def check():
    problems = []
    entries = collect()
    want = pot_text(entries)
    if not os.path.exists(POT) or open(POT, encoding='utf-8').read() != want:
        problems.append('data/lang/kai.pot is out of date: run tools/po.py extract')
    seen = {}
    for text, refs in entries:
        h = fnv1a(text)
        if h in seen:
            problems.append('two texts share a lookup hash: %r and %r' % (seen[h], text))
        seen[h] = text
    known = {t for t, _ in entries}
    for path in sorted(glob.glob(os.path.join(ROOT, 'data', 'lang', '*.po'))):
        name = os.path.relpath(path, ROOT)
        for en, tr in read_po(path).items():
            if en not in known:
                continue  # an old text: kai never asks for it
            if formats(en) != formats(tr):
                problems.append('%s: %r keeps %s, not %s' % (name, tr, formats(tr), formats(en)))
    for p in problems:
        print(p)
    if not problems:
        print('translations: %d texts, %d language file(s) ok' %
              (len(entries), len(glob.glob(os.path.join(ROOT, 'data', 'lang', '*.po')))))
    return 1 if problems else 0


def pseudo(text):
    """[text], keeping printf conversions and line breaks."""
    return '\n'.join('[' + part + ']' if part else part for part in text.split('\n'))


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'check'
    if cmd == 'extract':
        os.makedirs(os.path.dirname(POT), exist_ok=True)
        open(POT, 'w', encoding='utf-8', newline='\n').write(pot_text(collect()))
        return 0
    if cmd == 'check':
        return check()
    if cmd == 'pseudo' and len(sys.argv) == 4:
        os.makedirs(sys.argv[3], exist_ok=True)
        path = os.path.join(sys.argv[3], sys.argv[2] + '.po')
        open(path, 'w', encoding='utf-8', newline='\n').write(pot_text(collect(), pseudo, sys.argv[2]))
        return 0
    print(__doc__)
    return 1


if __name__ == '__main__':
    sys.exit(main())
