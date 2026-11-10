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
make check package                    # test build in out/gpSP/
# copy out/gpSP/ to the PSP and test, then merge:
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
   the commits since the last release.
2. To release, merge that PR. The Release workflow tags the version (for
   example `v3.5.1`), creates the GitHub release with the changelog, and
   attaches `gpSP-kai-<version>.zip`.

**When to release:** whenever `main` has at least one user-visible `fix` or
`feat` that was tested on a PSP. Until then, leave the release PR open; it
keeps updating itself.

The version comes only from `version.txt`. A build of the commit tagged
`v<version>` shows `UO gpSP kai <version>` on the XMB; any other build shows
`<version>-dev+<commit>`.

## Local release build

```sh
make release   # checks messages, builds, writes out/gpSP-kai-<version>.zip
```

## First release (3.5.0)

The first release is tagged by hand when the repository is first pushed:

```sh
git tag -a v3.5.0 -m "gpSP kai 3.5.0"
git push origin v3.5.0     # 1. the tag first
# 2. wait until the Release workflow has finished (Actions tab)
git push -u origin main    # 3. then main
```

Pushing the tag runs the Release workflow, which creates the 3.5.0 release
and attaches the zip. Push `main` only after that: the release bot then knows
3.5.0 is the last release and starts its changelog from there (pushed the
other way round, it would treat the whole history as unreleased). Finally,
edit the 3.5.0 release on GitHub and paste the 3.5.0 section of CHANGELOG.md
as its description.
