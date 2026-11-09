#!/usr/bin/env python3
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

Usage: bdf2fbm.py input.bdf output.fbm [--cell WxH] [--default CODEPOINT]
"""
import argparse
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
                    raise ValueError("pixel outside the cell")
                cell[y] |= 1 << (cell_w - 1 - cx)
    return cell


def build_fbm(glyphs, cell_w, cell_h, ascent, default_cp):
    codes = sorted(glyphs)
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
    default_index = codes.index(default_cp) if default_cp in glyphs else 0
    out += struct.pack("<HHHBBB", len(codes), len(maps), default_index,
                       cell_w, cell_h, bpc)
    for m in maps:
        out += struct.pack("<HHH", *m)
    pad = bytes_per_row * 8 - cell_w
    for c in codes:
        out.append(glyphs[c][0])
        for row in render(glyphs[c], cell_w, cell_h, ascent):
            out += (row << pad).to_bytes(bytes_per_row, "big")
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("bdf")
    ap.add_argument("fbm")
    ap.add_argument("--cell", help="cell size WxH (default: FONTBOUNDINGBOX)")
    ap.add_argument("--default", type=lambda s: int(s, 0), default=0x20,
                    help="code point shown for missing characters")
    a = ap.parse_args()
    ascent, (bw, bh), glyphs = parse_bdf(a.bdf)
    cw, ch = map(int, a.cell.lower().split("x")) if a.cell else (bw, bh)
    data = build_fbm(glyphs, cw, ch, ascent, a.default)
    open(a.fbm, "wb").write(data)
    print(f"{a.fbm}: {len(glyphs)} glyphs, {cw}x{ch} cell, {len(data)} bytes",
          file=sys.stderr)


if __name__ == "__main__":
    main()
