#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2026 Hachi
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version. See COPYING.
"""Convert a BDF bitmap font to gpSP kai's FBM format.

FBM layout (see src/fbm_print.c), all integers little-endian:
  header  u16 fontcnt, u16 mapcnt, u16 defaultchar, u8 width, u8 height,
          u8 byteperchar
  maps    mapcnt x (u16 start, u16 end, u16 distance); a code point c in
          [start, end] is glyph index c - distance
  glyphs  fontcnt x (u8 advance width, byteperchar bytes of bitmap); each
          bitmap row is ceil(width / 8) bytes, most significant bit = left

Glyphs are placed in a width x height cell whose baseline is FONT_ASCENT
pixels below the top. Code points become consecutive runs (maps) in
ascending order.

Characters FBM cannot hold are left out and counted on stderr: code points
above U+FFFF (FBM stores 16-bit code points) and glyphs with pixels outside
the cell (combining marks, extra-wide symbols).

--subset keeps only some characters. It takes a set name (latin, jis0208,
gb2312, big5-1, ksx1001) or a UTF-8 text file whose characters are kept, and
can be given more than once (the union is kept). ASCII and the default
character are always kept.

Usage: bdf2fbm.py input.bdf output.fbm [--cell WxH] [--default CODEPOINT]
                  [--subset NAME|FILE ...]
"""
import argparse
import os
import re
import struct
import sys


def parse_bdf(path):
    text = open(path, encoding="latin-1").read()
    ascent = int(re.search(r"^FONT_ASCENT (\d+)", text, re.M).group(1))
    bb = re.search(r"^FONTBOUNDINGBOX (\d+) (\d+)", text, re.M)
    glyphs = {}
    for m in re.finditer(r"^STARTCHAR.*?^ENDCHAR", text, re.M | re.S):
        block = m.group(0)
        enc = int(re.search(r"^ENCODING (-?\d+)", block, re.M).group(1))
        if enc < 0:
            continue
        dwidth = int(re.search(r"^DWIDTH (-?\d+)", block, re.M).group(1))
        w, h, xoff, yoff = map(int, re.search(
            r"^BBX (-?\d+) (-?\d+) (-?\d+) (-?\d+)", block, re.M).groups())
        rows = block.split("BITMAP", 1)[1].split()[:-1]  # drop ENDCHAR
        glyphs[enc] = (dwidth, w, h, xoff, yoff, [int(r, 16) for r in rows],
                       [len(r) * 4 for r in rows])
    return ascent, (int(bb.group(1)), int(bb.group(2))), glyphs


class OutsideCell(Exception):
    pass


def double_byte_set(encoding, lead, trail):
    """Characters of a two-byte character set, decoded byte pair by pair."""
    chars = set()
    for hi in range(lead[0], lead[1] + 1):
        for lo in range(trail[0], trail[1] + 1):
            try:
                chars.add(ord(bytes((hi, lo)).decode(encoding)))
            except UnicodeDecodeError:
                pass
    return chars


def big5_level1_set():
    """Big5 symbols and level 1 (frequently used) characters: A140-C67E."""
    chars = set()
    for code in range(0xA140, 0xC67F):
        hi, lo = code >> 8, code & 0xFF
        if not (0x40 <= lo <= 0x7E or 0xA1 <= lo <= 0xFE):
            continue
        try:
            chars.add(ord(bytes((hi, lo)).decode("big5")))
        except UnicodeDecodeError:
            pass
    return chars


def latin_set():
    """Latin, Greek, Cyrillic, punctuation, arrows and the shapes kai's
    menus use (button glyphs, boxes)."""
    ranges = [(0x20, 0x17F), (0x370, 0x3FF), (0x400, 0x4FF), (0x2000, 0x206F),
              (0x20A0, 0x20CF), (0x2100, 0x214F), (0x2190, 0x21FF),
              (0x25A0, 0x25FF)]
    extra = [0x2605, 0x2606, 0x2661, 0x2665, 0x266A, 0x266B]  # stars, hearts, notes
    return {c for lo, hi in ranges for c in range(lo, hi + 1)} | set(extra)


SUBSETS = {
    "latin": latin_set,
    "jis0208": lambda: double_byte_set("euc_jp", (0xA1, 0xFE), (0xA1, 0xFE)),
    "gb2312": lambda: double_byte_set("gb2312", (0xA1, 0xF7), (0xA1, 0xFE)),
    "big5-1": lambda: big5_level1_set(),
    "ksx1001": lambda: double_byte_set("euc_kr", (0xA1, 0xFE), (0xA1, 0xFE)),
}


def load_subset(names):
    chars = set(range(0x20, 0x7F))
    for name in names:
        if name in SUBSETS:
            chars |= SUBSETS[name]()
        elif os.path.isfile(name):
            chars |= {ord(c) for c in open(name, encoding="utf-8").read()
                      if c not in "\r\n\t"}
        else:
            raise SystemExit(f"unknown subset: {name} (not a set name or a file)")
    return chars


def render(glyph, cell_w, cell_h, ascent):
    """Return cell_h row integers (bit cell_w-1 = leftmost pixel)."""
    dwidth, w, h, xoff, yoff, rows, bits = glyph
    cell = [0] * cell_h
    top = ascent - (yoff + h)  # cell row of the glyph's first bitmap row
    for i, (row, nbits) in enumerate(zip(rows, bits)):
        y = top + i
        for x in range(w):
            if row >> (nbits - 1 - x) & 1:
                cx = xoff + x
                if not (0 <= y < cell_h and 0 <= cx < cell_w):
                    raise OutsideCell()
                cell[y] |= 1 << (cell_w - 1 - cx)
    return cell


def fit_glyphs(glyphs, cell_w, cell_h, ascent):
    """Render every glyph; drop those FBM cannot hold. Returns
    ({code: rows}, dropped_wide_codepoints, dropped_outside_cell)."""
    cells, above_bmp, outside = {}, [], []
    for c, g in glyphs.items():
        if c > 0xFFFF:
            above_bmp.append(c)
            continue
        try:
            cells[c] = (g[0], render(g, cell_w, cell_h, ascent))
        except OutsideCell:
            outside.append(c)
    return cells, above_bmp, outside


def build_fbm(cells, cell_w, cell_h, default_cp):
    codes = sorted(cells)
    runs = []
    for c in codes:
        if runs and c == runs[-1][1] + 1:
            runs[-1][1] = c
        else:
            runs.append([c, c])
    bytes_per_row = (cell_w + 7) // 8
    bpc = bytes_per_row * cell_h
    out = bytearray()
    index, maps = 0, []
    for start, end in runs:
        maps.append((start, end, start - index))
        index += end - start + 1
    default_index = codes.index(default_cp) if default_cp in cells else 0
    out += struct.pack("<HHHBBB", len(codes), len(maps), default_index,
                       cell_w, cell_h, bpc)
    for m in maps:
        out += struct.pack("<HHH", *m)
    pad = bytes_per_row * 8 - cell_w
    for c in codes:
        advance, rows = cells[c]
        out.append(min(advance, 255))
        for row in rows:
            out += (row << pad).to_bytes(bytes_per_row, "big")
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("bdf")
    ap.add_argument("fbm")
    ap.add_argument("--cell", help="cell size WxH (default: FONTBOUNDINGBOX)")
    ap.add_argument("--default", type=lambda s: int(s, 0), default=0x20,
                    help="code point shown for missing characters")
    ap.add_argument("--subset", action="append", default=[],
                    help="keep only this set (" + ", ".join(SUBSETS) +
                    ") or the characters of this UTF-8 text file; repeatable")
    a = ap.parse_args()
    ascent, (bw, bh), glyphs = parse_bdf(a.bdf)
    if a.subset:
        keep = load_subset(a.subset) | {a.default}
        glyphs = {c: g for c, g in glyphs.items() if c in keep}
    cw, ch = map(int, a.cell.lower().split("x")) if a.cell else (bw, bh)
    cells, above_bmp, outside = fit_glyphs(glyphs, cw, ch, ascent)
    if len(cells) > 0xFFFF:
        raise SystemExit(f"too many glyphs for FBM: {len(cells)}")
    data = build_fbm(cells, cw, ch, a.default)
    open(a.fbm, "wb").write(data)
    print(f"{a.fbm}: {len(cells)} glyphs, {cw}x{ch} cell, {len(data)} bytes",
          file=sys.stderr)
    if above_bmp:
        print(f"  left out {len(above_bmp)} above U+FFFF", file=sys.stderr)
    if outside:
        print(f"  left out {len(outside)} outside the cell: " +
              " ".join(f"U+{c:04X}" for c in sorted(outside)[:12]) +
              (" ..." if len(outside) > 12 else ""), file=sys.stderr)


if __name__ == "__main__":
    main()
