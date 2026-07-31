# ANP Studio

**ANP Studio** is a cross-platform **desktop application** for modeling, editing,
calculating, and studying Analytic Network Process (ANP) decision models.

<p align="center">
  <img src="gui/resources/anpstudio-256.png" alt="ANP Studio icon" width="128" height="128">
</p>

The computational library (matrices, networks, pairwise judgments, limit
matrix, synthesis, JSON I/O) lives in a separate repository:
**[libanpcpp](https://github.com/wjadams/libanpcpp)** (CMake target
`anpcpp::anpcpp`, C++ namespace `anpcpp`). Model files use JSON format
`anpcpp` (v1).

This application is inspired by the workflow of
[SuperDecisions](https://www.superdecisions.com/). Numerical behavior is
cross-checked against concepts from [pyanp](https://pyanp.org/) (reference
only).

## Sample models

Ready-to-open JSON models live in [`samples/`](samples/). Use **File → Open** in
ANP Studio. See [`samples/README.md`](samples/README.md) for the catalog
(Hamburger market share, classic AHP hierarchies, BCR/BOCR subnetworks, ratings
examples, and more). Regenerate with libanpcpp’s `export_sample_models` example.

## Architecture

```text
libanpcpp (anpcpp::anpcpp)   Toolkit-independent ANP library
anpstudio                    Qt 6 Widgets desktop application (this repo)
```

## Dependencies

| Piece | Choice |
|---|---|
| Build | CMake 3.20+ |
| Language | C++20 |
| ANP library | [libanpcpp](https://github.com/wjadams/libanpcpp) via FetchContent (or sibling checkout) |
| GUI | Qt 6 Widgets |

## Build the GUI

Requires a C++20 compiler, CMake, and Qt 6 Widgets (e.g. `qt6-base-dev` and
`libgl-dev` on Ubuntu/Pop!_OS).

### Sibling checkout (local development)

Place `libanpcpp` next to this repo:

```text
Documents/github/libanpcpp/
Documents/github/anp-studio/
```

Then:

```bash
cmake -S . -B build -DANPSTUDIO_BUILD_GUI=ON
cmake --build build --target anpstudio
./build/gui/anpstudio
```

CMake automatically uses `../libanpcpp` when that tree exists. Override with
`-DANPCPP_SOURCE_DIR=/path/to/libanpcpp` if needed.

### FetchContent from GitHub

If there is no sibling checkout, CMake fetches `libanpcpp` from GitHub
(`GIT_TAG v0.1.0`).

## GUI features

Qt 6 Widgets application (SuperDecisions-style canvas + docks) for creating
networks/subnetworks, editing connections and pairwise judgments, calculating
matrices/priorities, choosing synthesis formulas, and saving/loading JSON —
with full undo/redo via `QUndoStack`.

## Library development

Build, test, and examples for the ANP library belong in **libanpcpp**:

```bash
cd ../libanpcpp
cmake -S . -B build -DANPCPP_BUILD_TESTS=ON -DANPCPP_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

See that repository’s README for FetchContent / `find_package` consumer docs.

## Documentation

### Library API (libanpcpp)

**Site:** [https://bamath.org/libanpcpp/](https://bamath.org/libanpcpp/)

**API reference:** [https://bamath.org/libanpcpp/api/](https://bamath.org/libanpcpp/api/)

For a local sibling checkout:

```bash
cd ../libanpcpp
cmake -S . -B build -DANPCPP_BUILD_DOCS=ON -DANPCPP_BUILD_TESTS=OFF -DANPCPP_BUILD_EXAMPLES=OFF
cmake --build build --target anpcpp_docs
```

Open **[../libanpcpp/build/docs/html/index.html](../libanpcpp/build/docs/html/index.html)**.

### GUI reference (this repo)

**Site:** [https://bamath.org/anp-studio/](https://bamath.org/anp-studio/) (README landing page, built from `main` via GitHub Actions)

**API reference:** [https://bamath.org/anp-studio/api/](https://bamath.org/anp-studio/api/)

**Local Doxygen** requires Doxygen (`sudo apt install doxygen` on Ubuntu).
With the default `ANPSTUDIO_BUILD_DOCS=ON`, `cmake --build build` runs the
same Doxygen pass as CI (`WARN_AS_ERROR`). Docs-only:

```bash
cmake -S . -B build -DANPSTUDIO_BUILD_DOCS=ON -DANPSTUDIO_BUILD_GUI=OFF
cmake --build build --target anpstudio_docs
```

Open **[build/docs/html/index.html](build/docs/html/index.html)**.

**Local full site preview** (README landing + API under `/api/`):

```bash
cmake -S . -B build -DANPSTUDIO_BUILD_DOCS=ON -DANPSTUDIO_BUILD_GUI=OFF
cmake --build build --target anpstudio_docs
cp -a build/docs/html docs/site/api
python3 - <<'PY'
from pathlib import Path
import re
src = Path("README.md").read_text(encoding="utf-8")
src = re.sub(r"^# ANP Studio\n+", "", src, count=1)
src = re.sub(
    r"<p align=\"center\">\s*<img[^>]*>\s*</p>\n*",
    "",
    src,
    count=1,
    flags=re.IGNORECASE,
)
Path("docs/site/README.content.md").write_text(src.lstrip(), encoding="utf-8")
PY
cd docs/site && bundle install && bundle exec jekyll serve --baseurl /anp-studio
```

Enable Pages with **Settings → Pages → Source: GitHub Actions**; see
`.github/workflows/docs.yml`.

## Releases (prebuilt binaries)

GitHub Actions builds portable binaries for each published release:

| Platform | Artifact |
|---|---|
| Windows x64 | `anpstudio-<tag>-windows-x64.zip` (unzip and run `anpstudio.exe`) |
| macOS Apple Silicon | `anpstudio-<tag>-macos-arm64.zip` (unsigned `.app`) |
| Linux x86_64 | `anpstudio-<tag>-linux-x86_64.AppImage` |

macOS builds are **not code-signed**. Windows builds are **not** a Setup installer (zip only). Intel Mac builds are not produced yet.

### How to publish a release

1. Merge the changes you want on `main`.
2. On GitHub open the repo → **Releases** → **Draft a new release**.
3. Under **Choose a tag**, create a new tag such as `v0.1.0` (target: `main`).
4. Set the release title (e.g. `ANP Studio 0.1.0`) and optional notes.
5. Click **Publish release** (not “Save draft”—drafts do not start the build).
6. Open the **Actions** tab and watch **Release builds** (Windows, macOS, Linux).
7. When the jobs finish, refresh the Release page; the three assets appear under the release.

### Running the downloads

- **Windows:** unzip the zip, run `anpstudio.exe`. SmartScreen may warn because the build is unsigned.
- **macOS:** unzip, then **right-click** the app → **Open** the first time (Gatekeeper; unsigned).
- **Linux:** `chmod +x anpstudio-*-linux-x86_64.AppImage` and run it.

### Test a build without publishing

**Actions** → **Release builds** → **Run workflow**. Download artifacts from the workflow run page; nothing is attached to a Release.

## Status / remaining work

- Multi-user judgments, Rating/Direct prioritizers, sensitivity analysis
- SuperDecisions `.sdmod` import
- Contribution guidelines

## License

MIT License. See [LICENSE](LICENSE).
