# Changelog

## [3.6.0](https://github.com/pichayakorn/gpsp-kai/compare/v3.5.1...v3.6.0) (2026-11-11)


### Features

* **ui:** delete a savestate, and don't ask to load an empty slot ([#50](https://github.com/pichayakorn/gpsp-kai/issues/50)) ([5774020](https://github.com/pichayakorn/gpsp-kai/commit/5774020aac3577339ded3ed9112ecc838512bc52))
* **ui:** fixed-width menu text, aligned values and a tidier header ([#35](https://github.com/pichayakorn/gpsp-kai/issues/35)) ([243aef3](https://github.com/pichayakorn/gpsp-kai/commit/243aef363d21d5804532565342bce6019c857cc4))
* **ui:** show slot thumbnails in the main menu and mark the memory slot ([#53](https://github.com/pichayakorn/gpsp-kai/issues/53)) ([c0e2fe3](https://github.com/pichayakorn/gpsp-kai/commit/c0e2fe3954ab1916920d4b755cb0f24a649f45d4))


### Bug Fixes

* **build:** recompile objects when any header they include changes ([#37](https://github.com/pichayakorn/gpsp-kai/issues/37)) ([6bbd6b7](https://github.com/pichayakorn/gpsp-kai/commit/6bbd6b726a19b38fe6771c868b1bc3dd3d0f885f))
* **recompiler:** stop fill_tag_dual overwriting the next Thumb tag ([#33](https://github.com/pichayakorn/gpsp-kai/issues/33)) ([7761589](https://github.com/pichayakorn/gpsp-kai/commit/7761589e05278e33b50708cb4a81a0b23b44c025))
* refuse a savestate made with another game instead of freezing ([#40](https://github.com/pichayakorn/gpsp-kai/issues/40)) ([2b9fd52](https://github.com/pichayakorn/gpsp-kai/commit/2b9fd529585cfa3337f641d24919beef16d696b2))
* show an error when a ROM or ZIP fails to load ([#29](https://github.com/pichayakorn/gpsp-kai/issues/29)) ([f92c20f](https://github.com/pichayakorn/gpsp-kai/commit/f92c20f502437d5b1588d9e3af63e3c9ea67ee73)), closes [#6](https://github.com/pichayakorn/gpsp-kai/issues/6)
* **sound:** wrap the sound ring buffer at the same size on both sides ([#52](https://github.com/pichayakorn/gpsp-kai/issues/52)) ([4092e93](https://github.com/pichayakorn/gpsp-kai/commit/4092e93874df236c16fb1a50ef66a1b7b0199635))
* **ui:** keep the savestate preview in step with the selected slot ([#42](https://github.com/pichayakorn/gpsp-kai/issues/42)) ([8c9dec5](https://github.com/pichayakorn/gpsp-kai/commit/8c9dec5cab058af545262465824eaf29fee75ecf))

## [3.5.1](https://github.com/pichayakorn/gpsp-kai/compare/v3.5.0...v3.5.1) (2026-11-10)


### Bug Fixes

* build file paths with a bounded helper instead of sprintf ([#21](https://github.com/pichayakorn/gpsp-kai/issues/21)) ([f41373c](https://github.com/pichayakorn/gpsp-kai/commit/f41373cc97428a2b65746894b2d965be1a9524f7)), closes [#3](https://github.com/pichayakorn/gpsp-kai/issues/3)
* **build:** rebuild all objects when version.txt or Makefile.set change ([9e79cde](https://github.com/pichayakorn/gpsp-kai/commit/9e79cde074d4e66befafbefeed7b41e652c72776)), closes [#1](https://github.com/pichayakorn/gpsp-kai/issues/1)
* **ui:** show the audio buffer as sound delay ([#24](https://github.com/pichayakorn/gpsp-kai/issues/24)) ([75d08b6](https://github.com/pichayakorn/gpsp-kai/commit/75d08b6bfbc49fc49ee071c2a065ccab48718dba)), closes [#4](https://github.com/pichayakorn/gpsp-kai/issues/4)

### Notes

* Savestates are tied to the build that saved them. States from older builds (including b230) may load with wrong sound; when updating, make an in-game save and create new states with this version.

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
