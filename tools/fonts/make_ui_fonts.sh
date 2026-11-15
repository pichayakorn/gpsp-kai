#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2026 Hachi
#
# Build the 4.0 UI fonts (data/font/ui/*.fbm) from Fusion Pixel's BDF
# release (OFL, https://github.com/TakWolf/fusion-pixel-font), version
# 2026.11.10: fusion-pixel-font-10px-proportional-bdf and
# fusion-pixel-font-12px-proportional-bdf, unzipped into DIR/10 and DIR/12.
#
# Each size gets a Latin set (Latin, Greek, Cyrillic, symbols) and one
# common set per CJK language, each from that language's own BDF (the
# shapes differ between Japanese and Chinese).
#
# Usage: tools/fonts/make_ui_fonts.sh DIR
set -e
src=$1
out=data/font/ui
conv="python3 -I tools/fonts/bdf2fbm.py"
mkdir -p "$out"
for px in 10 12; do
  if [ "$px" = 10 ]; then cell=10x14; else cell=12x16; fi
  bdf="$src/$px/fusion-pixel-${px}px-proportional"
  $conv "$bdf-latin.bdf"   "$out/fp$px-latin.fbm"   --cell $cell --subset latin
  $conv "$bdf-ja.bdf"      "$out/fp$px-ja.fbm"      --cell $cell --subset jis0208
  $conv "$bdf-zh_hans.bdf" "$out/fp$px-zh_hans.fbm" --cell $cell --subset gb2312
  $conv "$bdf-zh_hant.bdf" "$out/fp$px-zh_hant.fbm" --cell $cell --subset big5-1
  $conv "$bdf-ko.bdf"      "$out/fp$px-ko.fbm"      --cell $cell --subset ksx1001
done
