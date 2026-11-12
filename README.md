# gpSP kai (unofficial continuation)

gpSP kai is a Game Boy Advance emulator for the PSP. This repository continues
takka's **UO ("UnOfficial") gpSP kai** source, which stopped at 3.4 test 3
build 152 (svn r433, 2009). It brings that source up to the features of the
community releases **3.4 test 4 build 225** (ErikPshat, 2016) and **build 230**
(Oldvic, 2017), and builds it with the current
[pspdev](https://github.com/pspdev/pspdev) toolchain.

The b225 changes follow ErikPshat's published source, archived in
[PSP-Archive/unofficial-gpSP-kai](https://github.com/PSP-Archive/unofficial-gpSP-kai)
(branch `erikpshat-mod`). No b230 source is known, so the b230 changes were
reconstructed from the release binary.

This continuation has its own version numbers, starting at **3.5.0**; the
PSP game list shows **UO gpSP kai** followed by the version. See [CHANGELOG.md](CHANGELOG.md)
for the changes in each release and [RELEASING.md](RELEASING.md) for how
development and releases work.

## Status

Tested on a PSP-3000 running ARK-5 custom firmware, with the English UI:
game loading (`.gba` and `.zip`), savestates, sleep/resume, screenshots, the
in-game menu and performance. Other models and other languages have not been
tested on hardware yet.

Custom firmware is required; the EBOOT is not signed for official firmware.

## What changed from takka's source

In short:
- the menus, messages (12 languages) and defaults of build 225 and build 230;
- a new public-domain UI font, gpsp10;
- fixes for the PSP turning off when the in-game menu opens, and for a menu
  crash with some date settings;
- builds with the current pspdev toolchain, with versioned builds and
  packaging.

Full details are in [docs/changes-from-takka.md](docs/changes-from-takka.md);
later changes are in [CHANGELOG.md](CHANGELOG.md).

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
make check      # checks the message files in data/cfg against the code
make release    # check + package + out/gpSP-kai-<version>.zip (dev builds: <version>-dev+<commit>)
make testpkg    # check + package + out/test/gpSP-kai-<version>.zip for testing on a PSP
```

`make testpkg` makes a zip that unzips to a folder named after the build,
e.g. `gpSP-kai-3.5.1-dev+5b7190b/`, to paste into `PSP/GAME/` next to other
builds. It adds your own test files from `testdata/` (git-ignored, never
committed), which has the same layout as the folder on the PSP:
`testdata/gba_bios.bin`, `testdata/GBA/ROM/`, `testdata/GBA/SAV/` and so on.
Each test build starts from the same clean saves.

Add `-j4` (or your number of CPU cores) to build in parallel, e.g.
`make -j4 release`.

On GitHub, every push is built by the Build workflow, which also offers the
PSP folder as a download; see [RELEASING.md](RELEASING.md).

Instead of a local toolchain, the `pspdev/pspdev` Docker image works too, with
psp-cfw-sdk unzipped into the container.

Options are in `Makefile.set`:
- `USE_DEBUG`, `USE_ADHOC`, `USE_C_CORE`
- `PSP_LARGE_MEMORY`:
  - `1` (default) makes pspsdk set `MEMSIZE=1` in PARAM.SFO, so the extra
    memory of the PSP-2000 and later can be used;
  - `0` instead builds takka's old `USE_EXT_MEM` code, which reaches the extra
    memory directly.

The version number is in `version.txt`. A build of the commit tagged
`v<version>` shows that version on the XMB; any other build adds
`-dev+<commit>`, so test builds are easy to tell apart.

## Installing

```sh
make package    # builds, then assembles the Memory Stick folder in out/gpSP-kai/
```

Copy `out/gpSP-kai/` to `PSP/GAME/` on the Memory Stick, then add your own
dump of the GBA BIOS as `gba_bios.bin` next to `EBOOT.PBP` (it is not
included). Put games in `GBA/ROM/`. To update, copy the new folder over the
old one; saves, states and configs in `GBA/` are kept. The folder contains:

```
EBOOT.PBP
homehook.prx  dvemgr.prx  SystemButtons.prx
cfg/          message, font and directory settings (from data/cfg)
font/         gpsp10.fbm, knj10.fbm, batang10.fbm (the fonts cfg/*.fnt uses)
game_config.txt
licenses/     COPYING and the font/library license notices
GBA/ROM GBA/SAV GBA/CFG GBA/SS GBA/CHT   (empty)
```

About the PRX modules:
- `homehook.prx` is always loaded. `dvemgr.prx` (TV output) is loaded on
  PSP-2000 and later, unless the Fake Fat option is on.
- `SystemButtons.prx` is shipped as in b230 but not loaded by the current code.
- By default the package ships the hardware-tested modules from
  `prx/prebuilt/` (takka's builds). `make package PACKAGE_PRX=built` ships the
  ones just built into `out/` instead.

When upgrading from another build, delete the old `gpsp.cfg` first.

### Extra memory (PSP-2000 and later)

gpSP loads the whole ROM into a 32 MB buffer when the PSP gives it the extra
memory of the PSP-2000 and later; otherwise the buffer is only 16 MB and
larger ROMs are read from the Memory Stick while playing, which can stutter.

On ARK-5, set **Use extra memory** to **Default** in ARK's settings: with
**Off**, ARK-5 does not give the extra memory, even though gpSP's EBOOT asks
for it (`MEMSIZE=1`). Tested on a PSP-3000 with ARK-5.2.0: Off gives a 16 MB
buffer, Default gives 32 MB. A PSP-1000 has no extra memory and always uses
16 MB.

## Repository layout

```
src/            emulator source
prx/            kernel PRX module sources; prx/prebuilt/ holds tested binaries
eboot/          EBOOT Makefile
data/           files for the Memory Stick: cfg/, font/, game_config.txt
image/          XMB icon and background
docs/           changes from takka's source, original and Japanese docs, licenses/
tools/fonts/    font tools and the BDF sources of gpsp10 and batang10
```

## Credits

- **Exophase**: gpSP (gameplaySP), the original emulator
- **takka**: gpSP kai
- **NJ**: homehook.prx and UI/drawing code (from his CPS2 emulator)
- **SiberianSTAR**: ZIP support
- **psp298**: analog nub code
- **Dark_Alex**: dvemgr.prx (TV output)
- **mok**: the FBM font printer (`fbm_print.c`)
- **pochi**, **frmb0001**: icons and images
- **ErikPshat**: build 225, with its translations
- **Oldvic**: build 230
- Fonts:
  - **NAGAO Sadakazu**: Naga10 (`knj10`, `knj10B`, `maru10`, `min10`, `5x10*`)
  - **Kim Jeong-Hwan**: Baekmuk; the 10px bitmap `batang10.fbm` was made by Won-kyu Park
  - **Markus Kuhn** and the X11 misc-fixed contributors: most glyphs of `gpsp10.fbm`
- **ITOU Hiroki**: bdf2bmp/bmp2bdf font tools (`tools/fonts/bdf_tool`)

## License

Source files that carry a GPL header are licensed under the **GNU General
Public License, version 2 or (at your option) any later version**; see
[COPYING](COPYING). The other parts have their
own terms or none stated:

| Part | Terms |
|---|---|
| `src/fbm_print.c`, `src/fbm_print.h` | Based on mok's FBM print library (modified for gpSP); free to use in PSP freeware (`docs/licenses/fbm_print/fbm_print.txt`) |
| `prx/homehookprx`, `src/homehook.*` | Based on NJ's homehook.prx (per takka's changelog); no license stated |
| `prx/systembuttons_prx`, `src/SystemButtons.*` | Origin not documented; no license stated |
| `prx/dvemgr`, `src/pspDveManager.S`, `src/dvemgr.h` | Based on Dark_Alex's dvemgr.prx; no license stated |
| `prx/dvemgr/lib/*.a` | Import-stub libraries for the sceDve, sceHprm and sceImpose kernel drivers; no license stated |
| `data/font/knj10.fbm`, `knj10B.fbm`, `maru10.fbm`, `min10.fbm`, `5x10*.fbm` (Naga10) | Free for non-commercial use; for commercial use, consult the author. Converted copies must ship with the original README (`docs/licenses/knj10/README`) and keep its terms |
| `data/font/batang10.fbm`, `tools/fonts/bdf/batang10.bdf` | Baekmuk license; keep `docs/licenses/batang/COPYRIGHT` and `docs/licenses/batang/COPYRIGHT.ks` with it. Baekmuk Batang, Baekmuk Dotum, Baekmuk Gulim, and Baekmuk Headline are registered trademarks owned by Kim Jeong-Hwan |
| `data/font/gpsp10.fbm`, `tools/fonts/bdf/gpsp10.bdf` | Public domain: glyphs from the X11 misc-fixed 6x10 and 6x9 fonts ("Public domain font. Share and enjoy."), plus four digits drawn for this project and released into the public domain |
| `tools/fonts/bdf_tool/*.zip` | bdf2bmp/bmp2bdf by ITOU Hiroki, BSD-style license (inside the archives) |
| `tools/fonts/ucs_tool/any2ucs.pl` | Based on Markus Kuhn's ucs2any.pl (per its header); no license stated |
| `tools/fonts/ucs_tool/sjis.map` | Shift_JIS to Unicode mapping table; no source or license stated |
| `tools/fonts/bdf2fbm.py` | GPL-2.0-or-later (see its header) |

The Naga10 BDF sources are not included: Naga10's terms allow the original
files to be distributed only as the unchanged original archive. To rebuild the
Naga10 FBM fonts, get the official Naga10 archive.

Do not add the GBA BIOS or game ROMs to this repository.
