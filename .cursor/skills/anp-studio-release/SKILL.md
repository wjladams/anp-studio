---
name: anp-studio-release
description: >-
  Cut coordinated libanpcpp + anp-studio GitHub releases: prompt for version,
  bump pins, tag both repos as vX.Y.Z, publish releases in order, and verify
  Release builds attached Windows/macOS/Linux artifacts. Use when the user
  asks to release anp-studio, ship a new version, tag a release, or bump
  libanpcpp for studio CI.
disable-model-invocation: true
---

# anp-studio-release

Coordinated release of **libanpcpp** then **anp-studio**. Assumes sibling checkouts:

```text
…/libanpcpp/
…/anp-studio/
```

Do not force-push tags. Do not publish drafts (studio CI only runs on **published** releases). Do not commit `.env` / secrets.

## Versioning

**Both repos use the same tag form: `vX.Y.Z`.**

| Repo | CMake / app `VERSION` | Git tag | Release title |
|------|----------------------|---------|---------------|
| libanpcpp | `X.Y.Z` | **`vX.Y.Z`** | `libanpcpp X.Y.Z` |
| anp-studio | `X.Y.Z` | **`vX.Y.Z`** | `ANP Studio X.Y.Z` |

CMake `VERSION` and `applicationVersion` stay bare `X.Y.Z` (no `v`). Only Git tags / FetchContent `GIT_TAG` / release tag names use the `v` prefix.

Default next version: same **major**, **minor + 1**, **patch 0** (e.g. `0.3.0` → `0.4.0`). Use the same `X.Y.Z` for both repos unless the user overrides.

**Prompt the user** with the default before changing files. Example:

> Proposed next version: **0.4.0** (from 0.3.0).  
> Tags (both repos): `v0.4.0`. Proceed / override?

**History note:** older anp-studio releases used bare tags (`0.1.0` … `0.3.0`). Do not recreate those. All **new** studio tags must be `v…` like libanpcpp.

When reading “current” version from `gh release view`, strip a leading `v` if present before computing the next minor bump.

## Progress checklist

Copy into the chat and tick as you go:

```
Release progress:
- [ ] Preflight (clean trees, auth)
- [ ] Version confirmed with user
- [ ] libanpcpp: bump + commit + push main
- [ ] libanpcpp: tag vX.Y.Z + push + gh release
- [ ] anp-studio: bump VERSION, app version, GIT_TAG + commit + push main
- [ ] anp-studio: publish release tag vX.Y.Z
- [ ] Verify three assets on the release page
```

## 1. Preflight

In **both** repos:

```bash
git status -sb
git fetch origin
git checkout main
git pull --ff-only origin main
```

- Working trees clean (or only the version bumps you are about to make).
- Commit any feature work the user wants **in** this release before bumping.
- `gh auth status` succeeds for `wjladams/libanpcpp` and `wjladams/anp-studio`.

Discover current studio version (prefer latest release tag; strip leading `v`):

```bash
gh release view -R wjladams/anp-studio --json tagName -q .tagName
# fallback: grep VERSION in anp-studio/CMakeLists.txt
```

## 2. libanpcpp first

CI for anp-studio FetchContent uses `GIT_TAG`; the lib tag must exist on GitHub before studio release builds configure.

1. Edit `CMakeLists.txt` `project(anpcpp VERSION X.Y.Z …)`.
2. Commit and push:

```bash
git add CMakeLists.txt
git commit -m "Bump version to X.Y.Z"
git push origin main
```

3. Tag and publish:

```bash
git tag vX.Y.Z
git push origin vX.Y.Z
gh release create vX.Y.Z --title "libanpcpp X.Y.Z" --generate-notes --repo wjladams/libanpcpp
```

## 3. anp-studio second

Bump **all** of:

| File | Change |
|------|--------|
| `CMakeLists.txt` | `project(… VERSION X.Y.Z)` |
| `CMakeLists.txt` | FetchContent `GIT_TAG vX.Y.Z` |
| `gui/main.cpp` | `setApplicationVersion("X.Y.Z")` |
| `README.md` (and `docs/site/README.content.md` if it mirrors) | any documented `GIT_TAG v…` pin |

Commit and push:

```bash
git add CMakeLists.txt gui/main.cpp README.md docs/site/README.content.md  # if changed
git commit -m "Bump version to X.Y.Z and pin libanpcpp vX.Y.Z"
git push origin main
```

Publish release (**not** draft):

```bash
gh release create vX.Y.Z --title "ANP Studio X.Y.Z" --generate-notes --target main --repo wjladams/anp-studio
```

That triggers [`.github/workflows/release.yml`](.github/workflows/release.yml) (`on: release: types: [published]`). Asset names use the full tag string (including `v`).

## 4. Verify Release builds artifacts

Watch the workflow:

```bash
gh run list -R wjladams/anp-studio --workflow "Release builds" --limit 3
gh run watch -R wjladams/anp-studio <RUN_ID>
```

Expected assets on the release (label = tag name):

- `anpstudio-vX.Y.Z-windows-x64.zip`
- `anpstudio-vX.Y.Z-macos-arm64.zip`
- `anpstudio-vX.Y.Z-linux-x86_64.AppImage`

Confirm:

```bash
gh release view vX.Y.Z -R wjladams/anp-studio --json assets --jq '.assets[].name'
```

**Success criteria:** all three names present. If a matrix leg failed, report which asset is missing; the publish job may attach partial results—do not claim a full release until the user accepts gaps.

Optional dry-run (no release page assets): `gh workflow run "Release builds" -R wjladams/anp-studio`.

## 5. Callouts

- **Order:** lib tag → studio pin → studio release. Never reverse.
- **One tag convention:** always `vX.Y.Z` for both repos going forward.
- Local sibling `../libanpcpp` is for day-to-day builds; release CI ignores it and clones `GIT_TAG`.
- OAuth / `.env` are unrelated to binary packaging; do not include secrets in release notes.
- After assets land, optionally suggest the user smoke-test one downloaded binary.

## Reference

Maintainer notes also live in anp-studio `README.md` → **Publishing releases**. Prefer this skill’s checklist over reinventing steps from memory.
