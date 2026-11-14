#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2026 Hachi
#
# Rewrite the XMB title of an EBOOT.PBP, keeping everything else.
# Used by "make testpkg" so test builds with a custom TEST_NAME show that
# name in the XMB, not only in the folder name.
#
# Usage: tools/retitle_eboot.sh EBOOT.PBP "Title" DISC_VERSION
set -e
eboot=$(realpath "$1")
title=$2
disc_version=$3
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work"
unpack-pbp "$eboot" > /dev/null
# Keep MEMSIZE (the PSP-2000/3000's extra 32 MB): without it the game
# buffer drops to what a PSP-1000 has.
memsize=$(python3 -I -c '
import struct, sys
d = open(sys.argv[1], "rb").read()
keys, data, n = struct.unpack("<III", d[8:20])
for i in range(n):
    kofs, fmt, ln, mx, dofs = struct.unpack("<HHIII", d[20 + 16 * i:36 + 16 * i])
    key = d[keys + kofs:d.index(b"\0", keys + kofs)]
    if key == b"MEMSIZE" and fmt == 0x404:
        print(struct.unpack("<I", d[data + dofs:data + dofs + 4])[0])
' PARAM.SFO)
mksfoex ${memsize:+-d MEMSIZE="$memsize"} -s DISC_VERSION="$disc_version" "$title" PARAM.SFO
part() { if [ -f "$1" ]; then echo "$1"; else echo NULL; fi; }
pack-pbp "$eboot" PARAM.SFO "$(part ICON0.PNG)" "$(part ICON1.PMF)" \
  "$(part PIC0.PNG)" "$(part PIC1.PNG)" "$(part SND0.AT3)" DATA.PSP \
  "$(part DATA.PSAR)" > /dev/null
