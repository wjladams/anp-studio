# Agent notes — ANP Studio

## Build directory

Use a single out-of-source CMake tree named **`build/`** at the repo root:

```bash
cmake -S . -B build -DANPSTUDIO_BUILD_GUI=ON
cmake --build build --target anpstudio
./build/gui/anpstudio
```

- Do **not** create or reuse alternate trees such as `build-anpstudio`, `build-anpstudio2`, `build-asan`, or `build-docs` unless the user explicitly asks for a separate configuration (e.g. ASan).
- Prefer rebuilding in `build/` over configuring a second binary directory.
- Binary path for smoke runs: `build/gui/anpstudio`.
- Local sample copy after build: `build/gui/samples/`.

Special-purpose trees (ASan, docs-only) are fine when requested; name them clearly (`build-asan`, etc.) and do not treat them as the default.
