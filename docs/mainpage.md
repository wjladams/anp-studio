# ANP Studio GUI Reference {#mainpage}

[Project home](https://bamath.org/anp-studio/)

Desktop application for modeling, editing, and calculating **Analytic Network
Process (ANP)** networks. Built with **Qt 6 Widgets**.

## Architecture

```text
MainWindow
  ├── Document          (model, undo, file I/O, subnet navigation)
  ├── NetworkCanvas     (visual network editor)
  ├── StructurePanel    (tree of clusters/nodes)
  ├── PairwisePanel     (judgment entry)
  └── ResultsPanel      (matrices and priorities)
```

Computational types (`anpcpp::AnpNetwork`, `Matrix`, etc.) come from
**libanpcpp**. See the libanpcpp API documentation for the numerical library.

## Key classes

| Class | Role |
|-------|------|
| @ref Document | Owns the root network, undo stack, and current subnet view |
| @ref MainWindow | Application shell, menus, and dock layout |
| @ref NetworkCanvas | Interactive cluster/node diagram |
| @ref StructurePanel | Hierarchy browser and subnet breadcrumbs |
| @ref PairwisePanel | Node and cluster pairwise comparison tables |
| @ref ResultsPanel | Supermatrices, limit matrix, and alternative scores |
