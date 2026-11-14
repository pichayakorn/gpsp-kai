# 4.0 UI fonts

[Fusion Pixel](https://github.com/TakWolf/fusion-pixel-font) 2026.11.10 at
10 and 12 px, by TakWolf, under the SIL Open Font License 1.1
(`licenses/fusion-pixel-OFL.txt`). Fusion Pixel includes glyphs from Ark
Pixel, Cubic 11 and Galmuri; their licences are in `licenses/`.

Converted to kai's `.fbm` format with `tools/fonts/make_ui_fonts.sh` (the
BDF release, subset per language). Only used by `UI4=1` builds for now.

| File | Characters |
|---|---|
| `fp10-latin`, `fp12-latin` | Latin, Greek, Cyrillic, punctuation, arrows, shapes |
| `fp10-ja`, `fp12-ja` | JIS X 0208 |
| `fp10-zh_hans`, `fp12-zh_hans` | GB 2312 |
| `fp10-zh_hant`, `fp12-zh_hant` | Big5 level 1 |
| `fp10-ko`, `fp12-ko` | KS X 1001 |
