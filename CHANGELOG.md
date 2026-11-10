# Changelog

## [3.5.1](https://github.com/pichayakorn/gpsp-kai/compare/v3.5.0...v3.5.1) (2026-11-10)


### Bug Fixes

* build file paths with a bounded helper instead of sprintf ([#21](https://github.com/pichayakorn/gpsp-kai/issues/21)) ([f41373c](https://github.com/pichayakorn/gpsp-kai/commit/f41373cc97428a2b65746894b2d965be1a9524f7)), closes [#3](https://github.com/pichayakorn/gpsp-kai/issues/3)
* **build:** rebuild all objects when version.txt or Makefile.set change ([a0f4a87](https://github.com/pichayakorn/gpsp-kai/commit/a0f4a87f152cef6c364568d01d71295d63dabf34))
* **build:** rebuild all objects when version.txt or Makefile.set change ([9e79cde](https://github.com/pichayakorn/gpsp-kai/commit/9e79cde074d4e66befafbefeed7b41e652c72776)), closes [#1](https://github.com/pichayakorn/gpsp-kai/issues/1)
* **ui:** show the audio buffer as sound delay ([#24](https://github.com/pichayakorn/gpsp-kai/issues/24)) ([75d08b6](https://github.com/pichayakorn/gpsp-kai/commit/75d08b6bfbc49fc49ee071c2a065ccab48718dba)), closes [#4](https://github.com/pichayakorn/gpsp-kai/issues/4)

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
