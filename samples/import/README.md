# Import fixtures for `18_multiuser_pairwise_ahp.json`

Test data for judgment import against sample **18**.

In the app, generate blank respondent workbooks with
**Participants → Export Excel templates…** (one `ANP_judgments_<Name>.xlsx`
per person). Import with **Participants → Import judgment templates…**.
Identity always comes from the hidden `_meta` sheet (or legacy
`participant_*` CSV cells) — never the filename.

Open `../18_multiuser_pairwise_ahp.json` first. Structure is a single pairwise
slot: **Goal → Alternatives** with alternatives **A, B, C**.

| Pair | Alice | Bob | Carol | Diego (new) |
|------|------:|----:|------:|------------:|
| A vs B | 2 | 8 | 0.5 | 4 |
| A vs C | 3 | 3 | 3 | 2 |
| B vs C | 1 | 1 | 1 | 1 |

Alice / Bob / Carol match the sample roster (`alice@example.com`, …).
**Diego Soto** is not in the model — use him (or Eve in `single_user/`) to
test auto-create-on-import.

Golden check (Alice+Bob+Carol only): geometric mean of A vs B is
\((2 \cdot 8 \cdot 0.5)^{1/3} = 2\).

## Files

| File | Shape | Use for |
|------|--------|---------|
| `single_user/ANP_judgments_*.xlsx` | **App Excel template** (Your judgments + hidden `_meta`; new votes + Eve) | Multi-select import smoke test |
| `18_forms_responses.csv` | Wide; plain-English Forms column titles (live import uses questionId→tag map) | Reference for Forms-style answers |
| `18_votes_long.csv` | Tidy; one row per judgment | Long-format reference |
| `18_votes.xlsx` | Multi-sheet workbook (legacy flat sheets) | Legacy Excel import path |
| `18_carol_only.xlsx` | Single participant (Carol), legacy sheet | Legacy Excel import path |

See [`single_user/README.md`](single_user/README.md) for the recommended
import test set (Alice/Bob/Carol overwritten + Eve auto-created).

## App Excel template shape

- Sheet **Your judgments** (visible): title, short instructions, Saaty legend,
  then columns `#` / `Comparison` / `Your rating` (yellow) / `Notes`
- Sheet **_meta** (hidden): `participant_id` / `participant_name` /
  `participant_email` / `template_version=1`, then `row` / `anp_tag` / `kind`
  joining rating cells without parsing English question text

Legacy CSV templates (`participant_*` + `kind` table) still import if you have
older exports.

## `[anp:]` tags used

```
[anp:pw|Goal|Alternatives|A|B]
[anp:pw|Goal|Alternatives|A|C]
[anp:pw|Goal|Alternatives|B|C]
```
