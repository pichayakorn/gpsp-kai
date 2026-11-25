# Contributing to gpSP kai

Thanks for helping. Bug reports from PSP models and firmware other than a
PSP-3000 on ARK-5 are especially welcome.

## Reporting a bug

Open an issue with the **Bug report** form. Say which PSP model, custom
firmware, kai version and game, and the steps that show the problem.

Please don't attach or link ROMs, BIOS dumps or game box art: issues that do
are edited or closed.

Savestates (`.svs`) only load in the version that made them. That is by
design, not a bug. Game saves (`.sav`) keep working across versions.

## Building

See [Building](README.md#building) in the README. `make check` runs the same
checks as the Build workflow.

## Making a change

- Branches, commit messages and releases follow [RELEASING.md](RELEASING.md).
  Commits use the conventional `type(scope): summary` format, because the
  version number is made from them.
- Test the change on a real PSP and say in the pull request which model and
  firmware you used. A change that only builds is not ready to merge.
- Keep each pull request to one change. Pull requests are squash-merged.
- Match the style of the code around your change. Write comments in English,
  and only for what the code can't say itself.
- Contributions are licensed under the GNU General Public License, version 2
  or later (see [COPYING](COPYING)).

## Interface text and translations

- Interface text is written in English. The old menu's messages live in
  `data/cfg/english.msg`; the other `.msg` files are no longer updated.
- In the 4.0 interface, every text a player sees goes through `T("...")`, or
  `TN("...")` in a table. After adding or changing one, run
  `python3 tools/po.py extract` and commit `data/lang/kai.pot`; `make check`
  fails while the template is out of date.
- Translations will be made on Weblate. Until it opens, please don't send
  `.po` files as pull requests.
