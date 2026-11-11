# Development and releases

## Branches

`main` is always stable: only changes that were tested on a PSP are merged
into it. Every change is made on its own short-lived branch:

| Branch | For |
|---|---|
| `fix/<name>` | a bug fix |
| `feat/<name>` | a new feature |
| `chore/<name>`, `docs/<name>` | tooling, cleanup, documentation |

```sh
git switch -c fix/audio-buffer main   # start
# ... commit ...
make check package                    # test build in out/gpSP-kai/
# copy out/gpSP-kai/ to the PSP and test, then merge:
git switch main && git merge --no-ff fix/audio-buffer
git branch -d fix/audio-buffer
```

On GitHub, push the branch and open a pull request instead of merging
locally: the Build workflow builds it, checks the message files and offers
the PSP folder as a download (Actions, the run, Artifacts).

## Commit messages decide the version

Commits use the conventional format `type(scope): summary`:

| Commit type | Next version | Use for |
|---|---|---|
| `fix:` | 3.5.0 to 3.5.1 | a bug fix users notice |
| `feat:` | 3.5.1 to 3.6.0 | a new feature |
| `feat!:` or a `BREAKING CHANGE:` footer | 3.6.0 to 4.0.0 | old savestates, configs or setups stop working |
| `chore:`, `docs:`, `refactor:` | no release | everything else |

## Releasing

Releases are automatic once the repository is on GitHub:

1. Every push to `main` updates an open **release PR** that sets the next
   version in `version.txt` and adds the CHANGELOG entry, both generated from
   the commits since the last release. The release PR has no Build check:
   it only changes those files, and the same code is already built on main.
   To try it on a PSP first, check out its branch and run `make testpkg`.
2. To release, merge that PR. The Release workflow tags the version (for
   example `v3.5.1`), creates the GitHub release with the changelog, and
   attaches `gpSP-kai-<version>.zip`.

**When to release:** whenever `main` has at least one user-visible `fix` or
`feat` that was tested on a PSP. Until then, leave the release PR open; it
keeps updating itself.

The version comes only from `version.txt`. A build of the commit tagged
`v<version>` shows `gpSP kai <version>` on the XMB; any other build shows
`<version>-dev+<commit>`.

A dev build also looks different on the XMB: it gets the gray icon with the
amber DEV ribbon (`image/ICON0_dev.png`) and no background. On the PSP the XMB
hides a game's title whenever it has a background, so only a dev build shows
its full version there; a release shows the background, which carries the
name, and its version is in gpSP's own menu.

## Local release build

```sh
make release   # checks messages, builds, writes out/gpSP-kai-<version>.zip
```

## First release (3.5.0)

The first release was tagged by hand. GitHub only runs workflows once the
repository has a default branch, so `main` is pushed before the tag:

```sh
git push -u origin main    # 1. creates the default branch and the workflows
git push origin v3.5.0     # 2. the tag: the Release workflow builds 3.5.0
```

`release-please-config.json` sets `bootstrap-sha` to the v3.5.0 commit, so
the release bot only looks at commits after 3.5.0 and never treats the older
history as unreleased. Finally, edit the 3.5.0 release on GitHub and paste
the 3.5.0 section of CHANGELOG.md as its description.
