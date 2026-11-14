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
mksfoex -s DISC_VERSION="$disc_version" "$title" PARAM.SFO
part() { if [ -f "$1" ]; then echo "$1"; else echo NULL; fi; }
pack-pbp "$eboot" PARAM.SFO "$(part ICON0.PNG)" "$(part ICON1.PMF)" \
  "$(part PIC0.PNG)" "$(part PIC1.PNG)" "$(part SND0.AT3)" DATA.PSP \
  "$(part DATA.PSAR)" > /dev/null
