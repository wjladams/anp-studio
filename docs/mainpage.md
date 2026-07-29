# ANP Studio GUI Reference {#mainpage}

[Project home](https://bamath.org/anp-studio/)

Desktop application for modeling, editing, and calculating **Analytic Network
Process (ANP)** networks. Built with **Qt 6 Widgets**.

## Architecture

```text
MainWindow (stages: Structure | Judgments | Synthesis)
  ├── Document          (model, undo, selection, results freshness)
  ├── Structure stage
  │     NetworkCanvas · InspectorPanel (+ clickable breadcrumb)
  ├── Judgments stage
  │     JudgmentNavPanel (top selector) · PairwisePanel | RatingsPanel · JudgmentPrioritiesPanel
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
| @ref NetworkCanvas | Interactive cluster/node diagram (double-click opens subnet) |
| @ref InspectorPanel | Selected node/cluster properties |
| @ref JudgmentNavPanel | Top selector: Node/Cluster, Wrt, Other Cluster, Pairwise/Ratings |
| @ref PairwisePanel | Pairwise matrix / questionnaire editor |
| @ref RatingsPanel | Ratings scale definition and votes |
| @ref JudgmentPrioritiesPanel | Horizontal bar chart of local judgment priorities |
| @ref ResultsPanel | Supermatrices, limit matrix, and alternative scores |
| @ref SynthesisSummaryPanel | Ranked alternatives and stale badge |
