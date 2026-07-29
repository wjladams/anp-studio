# ANP Studio GUI Reference {#mainpage}

[Project home](https://bamath.org/anp-studio/)

Desktop application for modeling, editing, and calculating **Analytic Network
Process (ANP)** networks. Built with **Qt 6 Widgets**.

## Architecture

```text
MainWindow (stages: Structure | Judgments | Synthesis)
  ├── Document          (model, undo, selection, results freshness)
  ├── Structure stage
  │     StructurePanel · NetworkCanvas · InspectorPanel
  ├── Judgments stage
  │     JudgmentNavPanel · PairwisePanel | RatingsPanel · SessionStubPanel
  └── Synthesis stage
        ResultsPanel · SynthesisSummaryPanel
```

Computational types (`anpcpp::AnpNetwork`, `RatingsPrioritizer`, etc.) come from
**libanpcpp**. See the libanpcpp API documentation for the numerical library.

## Key classes

| Class | Role |
|-------|------|
| @ref Document | Owns the root network, undo stack, selection, and calc freshness |
| @ref MainWindow | Stage strip shell and shared menus |
| @ref NetworkCanvas | Interactive cluster/node diagram |
| @ref StructurePanel | Hierarchy browser with CRUD |
| @ref InspectorPanel | Selected node/cluster properties |
| @ref JudgmentNavPanel | Judgment parents, coverage, Pairwise/Ratings switch |
| @ref PairwisePanel | Node and cluster pairwise comparison tables |
| @ref RatingsPanel | Ratings scale definition and votes |
| @ref ResultsPanel | Supermatrices, limit matrix, and alternative scores |
| @ref SynthesisSummaryPanel | Ranked alternatives and stale badge |
