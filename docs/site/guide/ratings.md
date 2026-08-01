---
layout: page
title: Ratings
permalink: /guide/ratings/
---

**Ratings** assign each alternative (or rated item) a level on a **scale** instead of comparing every pair. Studio turns those votes into local priorities for that judgment column.

## When to use ratings

- Many alternatives  
- Natural categories (High / Medium / Low, star ratings, numeric bands)  
- Faster data entry than a full pairwise matrix  

Open **`05_ratings_price_quality.json`** or **`16_ahp_smartphone_ratings.json`** from **File → Open Sample…**.

## Scales and votes

On the Judgments stage, open a ratings-type node judgment:

1. Choose a **scale preset** (built-in or **My scales**).  
2. Cast votes with the label dropdowns for each item.  
3. Use **Advanced** to customize the scale or interpreter; **changes apply when Advanced closes**.

Presets help keep ratings comparable across columns when that is intentional.

## Pairwise vs ratings on a link

A node→destination-cluster judgment is either pairwise or ratings. Pick the mode that matches how experts can answer. You can rethink structure if a column is awkward in one mode.

## Tips

- Document what each scale category means for collaborators.  
- After ratings look right, open [Calculations]({{ '/guide/calculations/' | relative_url }}) to see their effect in the matrices and scores.

Next: [Calculations]({{ '/guide/calculations/' | relative_url }}) · [Glossary: ratings]({{ '/guide/glossary/' | relative_url }}#ratings)
