---
layout: page
title: Calculations
permalink: /guide/calculations/
---

The **Analysis** stage shows the numerical story of your model. Results use the **limit-matrix settings** stored on the current network (Structure → Inspector).

You can also press **F5** or use **Compute → Show Analysis**.

## Synthesis (main report)

Left nav under **Synthesis** scrolls the HTML report:

1. **Unscaled Supermatrix** — local priorities by column  
2. **Cluster matrix** — cluster-to-cluster weights  
3. **Scaled Supermatrix** — weighted, column-stochastic matrix  
4. **Limit Matrix** — long-run matrix, with a summary of the method and options used  
5. **Global priorities** — node priorities from the limit matrix  
6. **Subnetwork synthesis results** — when nested models exist  
7. **Alternative Scores** — priorities for alternatives (after synthesis if needed)

Open a sample (e.g. `02_ahp_best_car.json`), enter Analysis, and walk that list top to bottom.

## Limit matrix settings

On **Structure**, select the network and set **Limit matrix** in the Inspector:

- **Calculus** — default SuperDecisions / pyanp-style calculus limit  
- **New Hierarchy** — hierarchy/network decomposition; optional **With limit**  
- **Sinks** — sinks decomposition; **Straight normalizer** on or off  

Analysis, sensitivity, and influence use these network defaults. The Researcher stage can still override method on a single command if you need an experiment.

## Sensitivity

Under **Sensitivity**:

- **Interactive** — pick a wrt node and slide parameter *p*; see alternative bars update (row sensitivity).  
- **Global** — sweep *p* and plot each alternative’s score as a line.

## Influence

Under **Influence analysis**, tables summarize how adjusting rows moves alternative scores (raw, rank, marginal, total). Use these after the base synthesis looks sensible.

## Researcher (optional)

**Compute → Show Researcher** opens a notebook-style command panel for inspection (`limit`, `globals`, matrices, loads of other models, and more). Type `help` there for commands. Prefer Analysis for everyday reading of results.

Next: [Glossary]({{ '/guide/glossary/' | relative_url }}) · [User guide home]({{ '/guide/' | relative_url }})
