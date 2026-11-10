# Changelog

## 3.5.0 (2026-11-10)

First release of this continuation. It brings takka's gpSP kai source
(3.4 test 3 build 152) up to the features of the community releases
3.4 test 4 build 225 (ErikPshat) and build 230 (Oldvic), and builds it with
the current pspdev toolchain. Tested on a PSP-3000 with ARK-5.

### Features

* Menus and translations of build 225/230: 237 messages in 12 languages,
  the "Language and other options" menu, and the b230 menu layout.
* New public-domain UI font gpsp10 (Latin-1, Greek, Cyrillic), replacing
  b225's eureka font, with classic 5x7 digits and square dots.
* "Update backup" defaults to Automatic, as in build 230.
* Defaults for new configs: analog input off (as in 225/230) and scaled 3:2,
  the GBA's own aspect ratio.
* Versioned builds: the XMB title shows the version (development builds add
  the commit), and `make package` / `make release` build the PSP folder and
  zip.

### Bug Fixes

* The PSP no longer powers off when the menu is opened during a game: the
  dynarec now keeps the stack 8-byte aligned when calling C.
* No crash with the MM/DD/YYYY and DD/MM/YYYY date settings (menu date and
  screenshot names).
* The pad config menu lists Cross before Circle, matching the buttons it
  configures; the default mapping stays Circle = A, Cross = B.
* Builds with GCC 15 and the current pspsdk.
