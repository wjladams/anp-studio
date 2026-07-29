# cppanp

`cppanp` is a cross-platform **desktop application** for modeling, editing,
calculating, and studying Analytic Network Process (ANP) decision models.

The computational library (matrices, networks, pairwise judgments, limit
matrix, synthesis, JSON I/O) lives in a separate repository:
**[libanpcpp](https://github.com/wjadams/libanpcpp)** (CMake target
`anpcpp::anpcpp`, C++ namespace `cppanp`).

This application is inspired by the workflow of
[SuperDecisions](https://www.superdecisions.com/). Numerical behavior is
cross-checked against concepts from [pyanp](https://pyanp.org/) (reference
only).

## Architecture

```text
libanpcpp (anpcpp::anpcpp)   Toolkit-independent ANP library
cppanp_gui                   Qt 6 Widgets desktop application (this repo)
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
Documents/github/cppanp/
```

Then:

```bash
cmake -S . -B build -DCPPANP_BUILD_GUI=ON
cmake --build build --target cppanp_gui
./build/gui/cppanp_gui
```

CMake automatically uses `../libanpcpp` when that tree exists. Override with
`-DANPCPP_SOURCE_DIR=/path/to/libanpcpp` if needed.

### FetchContent from GitHub

If there is no sibling checkout, CMake fetches `libanpcpp` from GitHub
(`GIT_TAG v0.1.0`). Publish and tag that repo before relying on this path.

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

**Online:** [https://bamath.org/libanpcpp/](https://bamath.org/libanpcpp/)

For a local sibling checkout:

```bash
cd ../libanpcpp
cmake -S . -B build -DANPCPP_BUILD_DOCS=ON -DANPCPP_BUILD_TESTS=OFF -DANPCPP_BUILD_EXAMPLES=OFF
cmake --build build --target anpcpp_docs
```

Open **[../libanpcpp/build/docs/html/index.html](../libanpcpp/build/docs/html/index.html)**.

### GUI reference (this repo)

**Online:** [https://bamath.org/cppanp/](https://bamath.org/cppanp/)

**Local build** requires Doxygen (`sudo apt install doxygen` on Ubuntu):

```bash
cmake -S . -B build -DCPPANP_BUILD_DOCS=ON
cmake --build build --target cppanp_docs
```

Open **[build/docs/html/index.html](build/docs/html/index.html)**.

Enable Pages with **Settings → Pages → Source: GitHub Actions**; see
`.github/workflows/docs.yml`.

## Status / remaining work

- Multi-user judgments, Rating/Direct prioritizers, sensitivity analysis
- SuperDecisions `.sdmod` import
- Contribution guidelines

## License

MIT License. See [LICENSE](LICENSE).
