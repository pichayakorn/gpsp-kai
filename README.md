# gpSP kai (unofficial continuation)

gpSP kai is a Game Boy Advance emulator for the PSP. This repository continues
takka's **UO ("UnOfficial") gpSP kai** source, which stopped at 3.4 test 3
build 152 (svn r433, 2009). It brings that source up to the features of the
community releases **3.4 test 4 build 225** (ErikPshat, 2016) and **build 230**
(Oldvic, 2017), and builds it with the current
[pspdev](https://github.com/pspdev/pspdev) toolchain.

The PSP game list shows it as **UO gpSP kai 3.4 test 4 Build 230 (src)**. The
`(src)` suffix tells it apart from the official binary release; see
[Building](#building).

## Status

Tested on a PSP-3000 running ARK-5 custom firmware, with the English UI:
game loading (`.gba` and `.zip`), savestates, sleep/resume, screenshots, the
in-game menu and performance. Other models and other languages have not been
tested on hardware yet.

Custom firmware is required; the EBOOT is not signed for official firmware.

## What changed from takka's source

**Matching build 225 and build 230**
- Version 3.4 test 4 build 230, built like the release: `USE_DEBUG` off (this
  hides the Debug mode option) and `PSP_LARGE_MEMORY` on (`MEMSIZE=1`).
- **Update backup** now defaults to **Automatic**; it is still set per game.
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
- The menus use the b230 order and layout.
- New UI font `eureka.fbm` (from b225): proportional, covering ASCII, the
  Latin-1 accented letters, Greek and Russian Cyrillic. In the message files,
  `$` is a 1-pixel space used to line up columns. `knj10.fbm` is updated to the
  b225 version.
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
  `src/SystemButtons.*` stubs, normally with the same content. The perl build
  counter is replaced by `make bump_build`, which you run on purpose.
- The game list title can carry a suffix (`TITLE_SUFFIX`, default `(src)`).

## Building

You need:
- the [pspdev toolchain](https://github.com/pspdev/pspdev/releases);
- [psp-cfw-sdk](https://github.com/pspdev/psp-cfw-sdk/releases), for
  `kubridge.h`/`systemctrl_se.h` and the `libpspkubridge` and
  `libpspsystemctrl_user` libraries.

Unzip psp-cfw-sdk over the toolchain; its paths start at `usr/local/pspdev/`.

```sh
export PSPDEV=/path/to/pspdev
export PATH="$PSPDEV/bin:$PATH"
make            # builds the three PRX modules and the EBOOT into out/
```

Instead of a local toolchain, the `pspdev/pspdev` Docker image works too, with
psp-cfw-sdk unzipped into the container.

Options are in `Makefile.set`:
- `VERSION_*`, `TEST_MODE`, `USE_DEBUG`, `USE_ADHOC`, `USE_C_CORE`
- `PSP_LARGE_MEMORY`:
  - `1` (default) makes pspsdk set `MEMSIZE=1` in PARAM.SFO, so the extra
    memory of the PSP-2000 and later can be used;
  - `0` instead builds takka's old `USE_EXT_MEM` code, which reaches the extra
    memory directly.
- `TITLE_SUFFIX`: text appended to the game list title; leave it empty for the
  plain release title.

`make bump_build` increases `BUILD_COUNT` in `build_count.inc`.

## Installing

Copy these into a folder under `PSP/GAME/` on the Memory Stick:

```
EBOOT.PBP
homehook.prx  dvemgr.prx  SystemButtons.prx
cfg/          (data/cfg)
font/         (data/font) eureka.fbm, knj10.fbm, batang10.fbm (the fonts cfg/*.fnt uses)
game_config.txt (data/game_config.txt)
gba_bios.bin  (your own dump of the GBA BIOS; not included)
GBA/ROM GBA/SAV GBA/CFG GBA/SS GBA/CHT   (empty folders)
```

About the PRX modules:
- `homehook.prx` is always loaded; `dvemgr.prx` (TV output) is loaded on
  PSP-2000 and later, unless the Fake Fat option is on.
- `SystemButtons.prx` is shipped as in b230 but not loaded by the current code.
- Prebuilt PRX modules from takka's tree are in `prx/prebuilt/`; they are the ones used
  in the tested package. `make` also rebuilds them into `out/`.

When upgrading from another build, delete the old `gpsp.cfg` first.

## Credits

- **Exophase**: gpSP (gameplaySP), the original emulator
- **takka**: gpSP kai
- **NJ**: homehook.prx and UI/drawing code (from his CPS2 emulator)
- **SiberianSTAR**: ZIP support
- **psp298**: analog nub code
- **Dark_Alex**: dvemgr.prx (TV output)
- **mok**: the FBM font printer (`fbm_print.c`)
- **pochi**, **frmb0001**: icons and images
- **ErikPshat**: build 225, with its translations and the `eureka.fbm` font
- **Oldvic**: build 230
- Fonts:
  - **NAGAO Sadakazu**: Naga10 (`knj10`, `knj10B`, `maru10`, `min10`, `5x10*`)
  - **Kim Jeong-Hwan**: Baekmuk; the 10px bitmap `batang10.fbm` was made by Won-kyu Park
- **ITOU Hiroki**: bdf2bmp/bmp2bdf font tools (`tools/fonts/bdf_tool`)

## License

Source files that carry a GPL header are licensed under the **GNU General
Public License, version 2 or (at your option) any later version**; see
[COPYING](COPYING). The other parts have their
own terms or none stated:

| Part | Terms |
|---|---|
| `src/fbm_print.c`, `src/fbm_print.h` | Based on mok's FBM print library (modified for gpSP); free to use in PSP freeware (`docs/licenses/fbm_print/fbm_print.txt`) |
| `prx/homehookprx`, `prx/systembuttons_prx`, `src/homehook.*`, `src/SystemButtons.*` | Based on NJ's homehook.prx; no license stated |
| `prx/dvemgr`, `src/pspDveManager.S`, `src/dvemgr.h` | Based on Dark_Alex's dvemgr.prx; no license stated |
| `prx/dvemgr/lib/*.a` | Import-stub libraries for the sceDve, sceHprm and sceImpose kernel drivers; no license stated |
| `data/font/knj10.fbm`, `knj10B.fbm`, `maru10.fbm`, `min10.fbm`, `5x10*.fbm` (Naga10) | Free for non-commercial use; for commercial use, consult the author. Converted copies must ship with the original README (`docs/licenses/knj10/README`) and keep its terms |
| `data/font/batang10.fbm` | Baekmuk license; keep `docs/licenses/batang/COPYRIGHT` and `docs/licenses/batang/COPYRIGHT.ks` with it. Baekmuk Batang, Baekmuk Dotum, Baekmuk Gulim, and Baekmuk Headline are registered trademarks owned by Kim Jeong-Hwan |
| `data/font/eureka.fbm` | From the b225 release; no license stated |
| `tools/fonts/bdf_tool/*.zip` | bdf2bmp/bmp2bdf by ITOU Hiroki, BSD-style license (inside the archives) |

Do not add the GBA BIOS or game ROMs to this repository.
