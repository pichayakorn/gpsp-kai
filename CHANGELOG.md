# Changelog

## [3.7.1](https://github.com/pichayakorn/gpsp-kai/compare/v3.7.0...v3.7.1) (2026-11-13)


### Bug Fixes

* **cpu:** a store with writeback to its own base stores the old value ([#91](https://github.com/pichayakorn/gpsp-kai/issues/91)) ([8b195a5](https://github.com/pichayakorn/gpsp-kai/commit/8b195a53074791db99380f3940b77e6104e0298e))
* **cpu:** a Thumb BL split across two blocks jumps to the right place ([#92](https://github.com/pichayakorn/gpsp-kai/issues/92)) ([4336448](https://github.com/pichayakorn/gpsp-kai/commit/43364487cbd4a396f93ab9999af937eedb39e2be))
* **cpu:** instructions with Rd = PC that don't jump no longer crash ([#89](https://github.com/pichayakorn/gpsp-kai/issues/89)) ([23d56a4](https://github.com/pichayakorn/gpsp-kai/commit/23d56a49129bd3aa7b43337eab4868a1eff2e3cf))
* **cpu:** LDM with the PC and the S bit restores the CPSR ([#78](https://github.com/pichayakorn/gpsp-kai/issues/78)) ([ec3ac88](https://github.com/pichayakorn/gpsp-kai/commit/ec3ac8827e48151c64f40c4313216ea8f14d76ad))
* **cpu:** LDRSH from an odd address loads the byte at that address ([#76](https://github.com/pichayakorn/gpsp-kai/issues/76)) ([e552ec5](https://github.com/pichayakorn/gpsp-kai/commit/e552ec5bc62f9d2c93c996d72bddedf09a9aeeaa))
* **cpu:** read the PC as address + 12 with a shift by register ([#85](https://github.com/pichayakorn/gpsp-kai/issues/85)) ([d442f34](https://github.com/pichayakorn/gpsp-kai/commit/d442f34cb6cae8a2887c1155c7d3cfe804cf624a))
* **cpu:** set the carry flag for a rotated immediate ([#82](https://github.com/pichayakorn/gpsp-kai/issues/82)) ([95f086b](https://github.com/pichayakorn/gpsp-kai/commit/95f086b4a57d802569c61a5ff6f515de45e853c6))
* **cpu:** STM stores the PC as the instruction's address + 12 ([#90](https://github.com/pichayakorn/gpsp-kai/issues/90)) ([4d30707](https://github.com/pichayakorn/gpsp-kai/commit/4d3070763834c9bae8ec627269d27b0dfa481af7))
* **cpu:** Thumb LDMIA/STMIA with an empty register list ([#83](https://github.com/pichayakorn/gpsp-kai/issues/83)) ([04befc2](https://github.com/pichayakorn/gpsp-kai/commit/04befc27cffb83d971f5e398dc5f1dc5565e8341))
* **cpu:** use only the lowest byte of Rs for shifts by register ([#75](https://github.com/pichayakorn/gpsp-kai/issues/75)) ([e338ce8](https://github.com/pichayakorn/gpsp-kai/commit/e338ce8dea93a1eb413f3fd39aea5910cde7562e))
* **video:** raise the HBlank IRQ on every line, also during V-blank ([#93](https://github.com/pichayakorn/gpsp-kai/issues/93)) ([7a7847c](https://github.com/pichayakorn/gpsp-kai/commit/7a7847c4071eb4056a5de7a876da53fe35fb3e0b))

## [3.7.0](https://github.com/pichayakorn/gpsp-kai/compare/v3.6.0...v3.7.0) (2026-11-12)


### Features

* new XMB art with an animated icon; dev builds get a gray icon and no background ([#57](https://github.com/pichayakorn/gpsp-kai/issues/57)) ([47ae8aa](https://github.com/pichayakorn/gpsp-kai/commit/47ae8aa3450d2e4d3c455f584bde9142a78f7761))
* **ui:** put Return to game first in the main menu ([#60](https://github.com/pichayakorn/gpsp-kai/issues/60)) ([74a217a](https://github.com/pichayakorn/gpsp-kai/commit/74a217acf97a3ef7411f04728e35bd2e1da57391)), closes [#44](https://github.com/pichayakorn/gpsp-kai/issues/44)


### Bug Fixes

* **cpu:** store the right base value for STM with writeback ([#63](https://github.com/pichayakorn/gpsp-kai/issues/63)) ([b4ba1d8](https://github.com/pichayakorn/gpsp-kai/commit/b4ba1d83189d8c4a32892117e5f46a3c2523afe8))
* **zip:** load ZIPs that store sizes after the data ([#61](https://github.com/pichayakorn/gpsp-kai/issues/61)) ([e301c50](https://github.com/pichayakorn/gpsp-kai/commit/e301c50c778178d2debcf0a27c89b92f2317eb4a)), closes [#32](https://github.com/pichayakorn/gpsp-kai/issues/32)

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
