---
layout: page
title: Pairwise comparing
permalink: /guide/pairwise/
---

**Pairwise comparison** asks: for a given parent (“with respect to” this node), how much more important or preferred is A than B? Studio stores those ratios and derives local priorities for that column of the unscaled supermatrix.

## When to use pairwise

Use pairwise when you have a manageable set of alternatives or criteria and can compare them two at a time. For many items or categorical intensity, [ratings]({{ '/guide/ratings/' | relative_url }}) are often easier.

## Entering judgments

1. Open the **Judgments** stage.  
2. In the left navigation, select a **node** judgment (parent and destination cluster).  
3. Fill the comparison grid: each entry is a ratio; the reciprocal fills in automatically.  
4. Review the derived priorities / inconsistency indicators the panel shows (treat high inconsistency as a signal to revisit extreme judgments).

Try **`02_ahp_best_car.anpstudio`**: open Judgments, pick a criterion, and inspect the pairwise table for the alternatives.

## Tips

- Compare all required pairs for that context; incomplete sets weaken the column.  
- Prefer consistent intensity language (e.g. Saaty’s 1–9 verbal scale) across the model.  
- Switch a link to ratings from the judgment UI when that fits better—see [Ratings]({{ '/guide/ratings/' | relative_url }}).

Next: [Ratings]({{ '/guide/ratings/' | relative_url }}) · [Calculations]({{ '/guide/calculations/' | relative_url }})
