# ANP Studio sample models

Open any of these JSON files via **File → Open Sample…** (preferred) or
**File → Open** in ANP Studio to explore a ready-made AHP/ANP model.

Opening a sample does not bind Save to the installed copy — use **Save As** to
keep your own file.

## Where samples live

| Context | Location |
|---------|----------|
| Source / git checkout | `samples/` at the repo root |
| Local build | `samples/` next to the `anpstudio` binary (CMake copies on build) |
| Windows zip | `samples/` next to `anpstudio.exe` |
| macOS app | `anpstudio.app/Contents/Resources/samples/` |
| Linux AppImage | `usr/share/anpstudio/samples/` inside the AppImage |

Regenerate from libanpcpp:

```bash
cmake -S ../libanpcpp -B ../libanpcpp/build -DANPCPP_BUILD_EXAMPLES=ON
cmake --build ../libanpcpp/build --target export_sample_models
../libanpcpp/build/examples/export_sample_models ./samples
```

## Catalog

| File | Type | Inspired by |
|------|------|-------------|
| `01_hamburger_marketshare.json` | ANP feedback network | SuperDecisions “Hamburger” market-share demo (Creative Decisions Foundation tutorial tables) |
| `02_ahp_best_car.json` | AHP hierarchy | Classic car/hierarchy teaching example (Cost / Quality / Style) |
| `03_anp_network23_feedback.json` | Small ANP with feedback | Compact 2-cluster feedback network (Price / Quality ↔ alternatives) |
| `04_bcr_benefits_costs_plans.json` | Control + subnetworks | Benefits/Costs control pattern (BOCR-family, inverted Costs) |
| `05_ratings_price_quality.json` | Hierarchy + ratings | Categorical + numeric `RatingsPrioritizer` columns |
| `06_ahp_best_house.json` | AHP hierarchy | Saaty “buy a house” textbook / Interfaces-style criteria set |
| `07_ahp_school_choice.json` | AHP hierarchy | Saaty school-choice style criteria (learning, friends, athletics, …) |
| `08_ahp_vacation.json` | AHP hierarchy | Vacation destination selection (common AHP teaching scenario) |
| `09_ahp_job_offer.json` | AHP hierarchy | Job-offer comparison (salary, growth, location, …) |
| `10_bcr_car_purchase.json` | BCR subnetworks | SuperDecisions-style Car Purchase Benefits/Costs/Risks (American / European / Japanese) |
| `11_bocr_product_launch.json` | Full BOCR | Product-launch decision with Benefits × Opportunities / (Costs × Risks) synthesis |
| `12_anp_bridge_feedback.json` | ANP feedback | Small infrastructure “bridge” policy network with criterion feedback |
| `13_ahp_laptop.json` | AHP hierarchy | Modern laptop purchase |
| `14_ahp_supplier_selection.json` | AHP hierarchy | Supplier selection (quality, cost, delivery, …) |
| `15_ahp_project_portfolio.json` | AHP hierarchy | Project portfolio prioritization |
| `16_ahp_smartphone_ratings.json` | Mixed pairwise + ratings | Smartphone choice with ratings for battery / price / storage |
| `17_anp_water_reservoir.json` | ANP policy | Dam water-level style criteria (flood, supply, environment, recreation) |
| `18_multiuser_pairwise_ahp.json` | Multi-user AHP | 3 judges; pairwise geometric mean (A vs B → 2) |
| `19_multiuser_ratings.json` | Multi-user ratings | Numeric ratings; arithmetic mean of scores |
| `20_multiuser_mixed.json` | Multi-user mixed | Pairwise + ratings; Executives group |
| `21_multiuser_partial.json` | Multi-user sparse | Missing judgments skipped in aggregation |

### Import fixtures (`import/`)

CSV / Excel files for testing multi-user vote import against sample 18. See
[`import/README.md`](import/README.md).

| File | Notes |
|------|--------|
| `import/18_forms_responses.csv` | Google Forms–style wide export (`[anp:]` column titles) |
| `import/18_votes_long.csv` | One row per pairwise judgment |
| `import/18_votes.xlsx` | All participants + per-person sheets + blank template (legacy) |
| `import/18_carol_only.xlsx` | Single-participant Excel (Carol), legacy sheet |
| `import/single_user/ANP_judgments_*.xlsx` | Respondent Excel templates (Your judgments + `_meta`; new votes + Eve) |

## Notes

- Format: `anpcpp` JSON v2 (v1 still loads). Multi-user samples include `participants`, `groups`, `judgment_session`, and per-user `users` under prioritizers.
- Pairwise group aggregation: **geometric mean** of ratios. Ratings: **arithmetic mean** of scores.
- Except where noted (Hamburger), judgment values are **illustrative** reconstructions of well-known *structures*, not verbatim proprietary SuperDecisions `.sdmod` dumps.
- Hamburger local priorities follow the published SuperDecisions tutorial unweighted-supermatrix / cluster-weight tables reconstituted as pairwise ratios.
- Suggested first opens: `02_ahp_best_car.json` (simple hierarchy), then `01_hamburger_marketshare.json` (full ANP), then `18_multiuser_pairwise_ahp.json` (multi-user).
