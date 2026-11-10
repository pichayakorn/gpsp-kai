# Changes from takka's source

This is what the first release of this continuation, **3.5.0**, changed in
takka's UO gpSP kai source (3.4 test 3 build 152, svn r433). Later changes
are listed per release in [CHANGELOG.md](../CHANGELOG.md).

**Matching build 225 and build 230**
- Version 3.4 test 4 build 230, built like the release: `USE_DEBUG` off (this
  hides the Debug mode option) and `PSP_LARGE_MEMORY` on (`MEMSIZE=1`).
- **Update backup** now defaults to **Automatic**; it is still set per game.
- New configs start with **analog input off**, as in b225 and b230. Turn
  analog on in Configure analog.
- New configs start in **scaled 3:2**, the GBA's own aspect ratio, as in
  takka's source. b225 and b230 default to fullscreen 16:9, which stretches
  the picture; it is still available in Graphics and Sound options.
- The pad config menu lists Cross before Circle, as in b225, with the button
  tables swapped to match. The default mapping is unchanged: Circle = GBA A,
  Cross = GBA B, and the analog nub mapped separately from the D-pad, as in
  takka's source.
- The message table now has 237 entries and uses the b225 message files for
  all 12 languages (UTF-8). As in b225, the Dutch, Chinese and Korean files are
  still in English.
- Menu texts that were hard-coded now come from the message files: the misc
  menu (now "Language and other options"), display scale, tilt sensor, the
  yes/no prompt and the ROM/state progress texts.
- The menus use the b230 order and layout, including the b225 header layout
  (date, battery, version and ROM info lines) and colors.
- UI font `gpsp10.fbm`: proportional, covering ASCII, the Latin-1 accented
  letters, Greek and Russian Cyrillic. It replaces b225's `eureka.fbm`, whose
  rights are unclear. Its glyphs come from the public-domain X11 misc-fixed
  6x10 font, except `. , : ;` (misc-fixed 6x9, for square 2x2 dots) and the
  digits `0 1 6 9`, drawn for this project in the classic 5x7 dot-matrix style.
  In the message files, `$` is a 1-pixel space used to line up
  columns. The padding was recomputed for gpsp10, so every column stays where
  it was with eureka. The source is `tools/fonts/bdf/gpsp10.bdf`; rebuild it
  with `tools/fonts/bdf2fbm.py`.
- `knj10.fbm` is the version shipped in the b230 release, which has small glyph
  fixes over takka's.
- The b225 menu and savestate date formats put the weekday after the date, and
  the code now passes its arguments in that order. Without this fix, the menu
  crashed when the PSP was set to MM/DD/YYYY or DD/MM/YYYY.

**Port to the current toolchain**
- Builds with GCC 15 and the current pspsdk. Changes:
  - removed compiler flags;
  - renamed RTC types;
  - `PSP_HEAP_SIZE_KB(-1)` in place of the removed `PSP_HEAP_SIZE_MAX()`;
  - a full prototype for C23;
  - no `swab()`;
  - header globals defined once (`-fno-common`);
  - an explicit dynarec branch target.
- The dynarec now keeps the stack 8-byte aligned when it calls C. Without this,
  a GCC 15 build turned the PSP off when the in-game menu opened.
- `EBOOT.PBP` and the PRX modules now go to `out/` instead of overwriting
  committed binaries. The PRX builds still regenerate the `src/homehook.S` and
  `src/SystemButtons.*` stubs, normally with the same content.
- The version comes from `version.txt` (semantic versioning, starting at
  3.5.0) and replaces takka's build counter.
