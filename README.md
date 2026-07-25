# cppanp

`cppanp` is a planned cross-platform C++ toolkit for modeling, calculating,
editing, and studying Analytic Network Process (ANP) decision models.

The project will contain two layers:

1. A reusable C++ library that owns ANP models, persistence, validation, and
   numerical calculations.
2. A desktop application for Windows, macOS, and Linux that uses the library
   to create, edit, calculate, visualize, and research ANP models.

The application is inspired by the workflow of
[SuperDecisions](https://www.superdecisions.com/). Matrix algorithms and
expected numerical behavior will be compared with the open-source
[pyanp](https://pypi.org/project/pyanp/) library and its
[documentation](https://pyanp.org/), especially its limit-matrix calculations.
These projects are references, not dependencies, and file-format compatibility
is not yet promised.

## Project goals

### C++ ANP library

The library should provide:

- Models for networks, clusters, nodes, alternatives, connections, and optional
  subnetworks.
- Pairwise-comparison judgments, rating models, direct data, and documented
  judgment scales.
- Local priorities, inconsistency measures, and incomplete-comparison
  validation.
- Unweighted, cluster-weighted, and normalized supermatrices.
- Limit-matrix calculations for hierarchical and general network models,
  including cyclic or periodic cases.
- Final alternative priorities and sensitivity-analysis inputs.
- Stable identifiers and metadata for notes, citations, authorship, provenance,
  and reproducible research.
- Versioned serialization with clear validation and migration errors.
- A public API that can be used without linking the GUI.

Numerical behavior should be covered by small analytic examples and by golden
tests generated independently or cross-checked against `pyanp`.

### Desktop application

The GUI should support:

- Creating and editing networks visually.
- Managing clusters, nodes, alternatives, links, and subnetworks.
- Entering pairwise comparisons and showing inconsistency feedback.
- Inspecting intermediate priority vectors and supermatrices.
- Calculating and comparing final results.
- Visualizing priorities and sensitivity analyses.
- Attaching notes, sources, assumptions, and other research metadata.
- Importing, exporting, autosaving, undo/redo, and recovering projects.
- Producing useful reports without making the model depend on GUI classes.

## Proposed architecture

The numerical and model layers should remain independent of the desktop
framework:

```text
cppanp_core       Matrix/vector, eigen, consistency, pairwise, ANP network, JSON I/O
cppanp_gui        Qt 6 Widgets desktop application
tests             GoogleTest unit tests via CTest
examples          Small documented ANP models
```

### Build tooling

| Piece | Choice |
|---|---|
| Build | CMake 3.20+ with CTest |
| Language | C++20 |
| Core library | `cppanp_core` (static) |
| GUI | `cppanp_gui` (Qt 6 Widgets, optional) |
| Unit tests | GoogleTest (fetched with CMake `FetchContent`) |
| JSON | nlohmann/json (fetched with CMake `FetchContent`) |
| Math deps | None (custom dense matrix; no Eigen/BLAS) |

### Build and test

Requires a C++20 compiler (GCC, Clang, or MSVC) and CMake. Network access is
needed on the first configure so CMake can download GoogleTest.

```bash
cmake -S . -B build -DCPPANP_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Disable tests with `-DCPPANP_BUILD_TESTS=OFF`. The same commands work on
Linux, macOS, and Windows.

## GUI toolkit options

The primary selection criterion is a large body of open-source C++ examples
that an LLM can reuse reliably. The toolkit must also support Windows, macOS,
and Linux.

### Qt 6 Widgets — recommended

[Qt](https://www.qt.io/) has the largest and most varied body of C++ desktop
examples among the candidates. Qt Widgets is mature, well documented, and
suited to a document-oriented editor. Its model/view classes, undo framework,
graphics scene/view framework, docking, printing, accessibility, testing, and
internationalization cover most of this application's needs.

`QGraphicsScene` and `QGraphicsView` are particularly useful for an editable
ANP network diagram. Qt Quick/QML remains available for selected modern or
animated views, but starting with Qt Widgets keeps most application code in
C++ and gives code-generation tools more established examples to follow.

Qt is offered under commercial and open-source licenses. Many core modules are
available under LGPLv3, while some modules have different terms. The exact
modules and distribution method must be reviewed before fixing the project's
license.

### wxWidgets — strongest permissive/native alternative

[wxWidgets](https://www.wxwidgets.org/) is a mature C++ toolkit that uses native
controls and ships with more than one hundred examples. Its license explicitly
supports open-source and commercial applications, including static linking
under its stated exception.

It is a good choice when native appearance and uncomplicated application
licensing matter more than an integrated graphics framework. It has fewer
ready-made examples for sophisticated node-graph editors, data visualization,
and model/view workflows than Qt, so more custom GUI infrastructure would
likely be required.

### Dear ImGui — best for a rapid technical prototype

[Dear ImGui](https://github.com/ocornut/imgui) is a popular, MIT-licensed,
immediate-mode C++ GUI library with many examples and extensions. It is
excellent for quickly building matrix inspectors, calculation tools, and
experimental graph editors.

It does not provide a complete desktop application framework or native widgets;
windowing and rendering require a backend such as SDL or GLFW plus OpenGL,
Metal, Direct3D, or Vulkan. A polished document editor would need additional
work for accessibility, printing, text input, platform integration, and native
look and feel. It is better suited to a prototype or developer tool than the
first choice for the final application.

### Slint — modern declarative option

[Slint](https://slint.dev/) provides a modern declarative UI language with C++
integration and supports the target desktop platforms. It offers a clean
separation between presentation and C++ logic.

Its C++ ecosystem and collection of mature desktop-editor examples are much
smaller than Qt's or wxWidgets'. It also introduces a separate UI language and
is implemented partly in Rust. Slint is worth reconsidering if a custom modern
appearance becomes more important than maximizing reusable C++ examples.

## Initial recommendation

Start with **Qt 6 Widgets** for the desktop application and keep all ANP model
and calculation code in a toolkit-independent library. This combination gives
the project the best access to examples while supplying mature components for
network editing, matrices, undo/redo, docking, reports, and cross-platform
packaging.

Before committing to Qt, build a small spike that:

1. Displays and edits a cluster/node network.
2. Presents a pairwise-comparison matrix.
3. Supports undo/redo and project serialization.
4. Builds and packages on all three desktop platforms.
5. Confirms that the chosen Qt modules and deployment method satisfy the
   intended project license.

## Status

The core library provides:

- Dense `Matrix` / `Vector`
- Principal eigen (`pri_eigen`-compatible power iteration) and Saaty CI/CR
- `PairwiseJudgments` for named comparison tables
- `AnpNetwork` / `AnpCluster` / `AnpNode` with pairwise wiring
- Unscaled / cluster-weighted / scaled supermatrices
- Limit matrix via pyanp's calculus method (with hierarchy short-circuit)
- Recursive subnetworks with selectable synthesis (additive / multiplicative /
  custom expression), including invert
- Versioned JSON save/load (`cppanp` format v1) with layout hints
- Mutators for disconnect / remove node / remove cluster

`cppanp::principal_eigen` follows pyanp's `pri_eigen`: power iteration from the
all-ones vector, sum-normalizing each iterate, stopping when the max-norm
change between successive iterates falls to the error threshold (default
`1e-10`). The eigenvalue is then `sum(A * v)`. Unlike pyanp, the iteration has
a `max_iterations` guard.

Consistency uses the official Saaty formulas `CI = (λ_max - n) / (n - 1)` and
`CR = CI / RI`, with Saaty's RI table for `n ≤ 15` and Alonso–Lamata for larger
`n` (fixing pyanp's missing `(n-1)` factor for `n > 15`).

### Examples

The `examples/` directory has runnable programs that build a model, then print
the pairwise inputs, the intermediate matrices (unscaled/cluster/scaled/limit),
and the final priorities. They are built by default (toggle with
`-DCPPANP_BUILD_EXAMPLES=OFF`) and end up in `build/examples/`:

- `tree134` – goal / 3 criteria / 3 alternatives AHP hierarchy
- `network23` – fully connected 2-cluster ANP network with feedback
- `hamburger_std` – SuperDecisions hamburger market-share ANP network
  (reconstructed from the published unscaled/cluster matrices; targets
  McDonald's 0.5549 / Burger King 0.2801 / Wendy's 0.1650)
- `benefits_costs_subnet` – a Benefits/Costs control network whose control nodes
  own subnetworks that are synthesized upward (with Costs inverted)

```sh
cmake -S . -B build
cmake --build build
./build/examples/tree134
```

### GUI (`cppanp_gui`)

Qt 6 Widgets application (SuperDecisions-style canvas + docks) for creating
networks/subnetworks, editing connections and pairwise judgments, calculating
matrices/priorities, choosing synthesis formulas, and saving/loading JSON —
with full undo/redo via `QUndoStack`.

Requires Qt 6 Widgets (e.g. `qt6-base-dev` and `libgl-dev` on Ubuntu/Pop!_OS).
Built by default when Qt6 is found (`-DCPPANP_BUILD_GUI=OFF` to skip):

```sh
cmake -S . -B build -DCPPANP_BUILD_GUI=ON
cmake --build build --target cppanp_gui
./build/gui/cppanp_gui
```

Remaining work: multi-user judgments, Rating/Direct prioritizers, sensitivity
analysis, SuperDecisions `.sdmod` import, contribution guidelines, and
licensing.

