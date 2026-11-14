# The built-in BIOS, used when gba_bios.bin is missing or not valid (#73).
# An open-source replacement for the GBA BIOS by Normmatt and the VBA/VBA-M
# team (GPL-2.0-or-later), as built into libretro gpSP (commit f6f3a91).
# Source: bios/source, see bios/README.md.

.global open_gba_bios_rom

.data
.balign 4
open_gba_bios_rom:
  .incbin "../bios/open_gba_bios.bin"
