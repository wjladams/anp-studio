---
layout: page
title: Glossary
permalink: /guide/glossary/
---

Short definitions used in ANP Studio. Each entry points to a guide page when useful.

### Alternative {#alternative}

An option you are ranking or choosing among; often collected in an alternatives cluster. See [Concepts]({{ '/guide/concepts/' | relative_url }}) and [Structure]({{ '/guide/structure/' | relative_url }}).

### BOCR / BCR {#bocr}

**Benefits / Opportunities / Costs / Risks** (or Benefits / Costs / Risks) control structures whose subnetwork scores are combined by a synthesis formula. See [Structure]({{ '/guide/structure/' | relative_url }}) (Formula).

### Cluster {#cluster}

A group of related [nodes](#node) on the Structure canvas. See [Structure]({{ '/guide/structure/' | relative_url }}).

### Cluster matrix {#cluster-matrix}

Matrix of weights showing how strongly clusters influence each other; used to scale the unscaled supermatrix. See [Calculations]({{ '/guide/calculations/' | relative_url }}).

### Connection {#connection}

A directed link meaning a node influences a destination cluster. See [Connections]({{ '/guide/connections/' | relative_url }}).

### Global priorities {#global-priorities}

Priorities of all nodes derived from the [limit matrix](#limit-matrix). See [Calculations]({{ '/guide/calculations/' | relative_url }}).

### Inconsistency {#inconsistency}

A measure of how much pairwise judgments disagree with themselves; high values suggest revisiting comparisons. See [Pairwise comparing]({{ '/guide/pairwise/' | relative_url }}).

### Influence {#influence}

Analysis of how changing a row’s priorities moves alternative scores. See [Calculations]({{ '/guide/calculations/' | relative_url }}).

### Invert {#invert}

Treating a priority or score so that higher raw values mean lower preference (common for costs). Used in synthesis contexts; see network Formula and subnetwork settings in [Structure]({{ '/guide/structure/' | relative_url }}).

### Limit matrix {#limit-matrix}

Long-run influence matrix from powers (or decomposition) of the scaled supermatrix. Methods: **Calculus**, **New Hierarchy**, **Sinks**—set in the Structure Inspector. See [Calculations]({{ '/guide/calculations/' | relative_url }}).

### Node {#node}

One element inside a [cluster](#cluster) (criterion, alternative, stakeholder, …). See [Structure]({{ '/guide/structure/' | relative_url }}).

### Pairwise {#pairwise}

Judgment mode that compares items two at a time under a parent ([wrt](#wrt)). See [Pairwise comparing]({{ '/guide/pairwise/' | relative_url }}).

### Ratings {#ratings}

Judgment mode that assigns each item a level on a [scale](#scale). See [Ratings]({{ '/guide/ratings/' | relative_url }}).

### Scale {#scale}

Ordered set of rating categories with ideal or priority values used to turn votes into local priorities. See [Ratings]({{ '/guide/ratings/' | relative_url }}).

### Scaled supermatrix {#scaled-supermatrix}

Weighted, column-stochastic form of the unscaled supermatrix after applying the [cluster matrix](#cluster-matrix). See [Calculations]({{ '/guide/calculations/' | relative_url }}).

### Sensitivity {#sensitivity}

How alternative scores change as a parameter *p* varies for a chosen row or globally. See [Calculations]({{ '/guide/calculations/' | relative_url }}).

### Subnetwork {#subnetwork}

A nested network owned by a node (common under BOCR/BCR control nodes). See [Structure]({{ '/guide/structure/' | relative_url }}).

### Synthesis {#synthesis}

Combining subnetwork (or control) results into overall alternative scores—**additive**, **multiplicative**, or a **custom** expression. See [Structure]({{ '/guide/structure/' | relative_url }}) and [Calculations]({{ '/guide/calculations/' | relative_url }}).

### Unscaled supermatrix {#unscaled-supermatrix}

Block matrix of local priorities from pairwise/ratings, before cluster weighting. See [Calculations]({{ '/guide/calculations/' | relative_url }}).

### Wrt {#wrt}

“With respect to”—the parent node that defines the judgment context (a column in the unscaled supermatrix). See [Connections]({{ '/guide/connections/' | relative_url }}) and [Pairwise comparing]({{ '/guide/pairwise/' | relative_url }}).

---

[User guide home]({{ '/guide/' | relative_url }}) · [Concepts]({{ '/guide/concepts/' | relative_url }})
