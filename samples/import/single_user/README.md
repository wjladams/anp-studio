# Single-user import templates (sample 18)

Use with `../../18_multiuser_pairwise_ahp.json`.

**Participants → Import judgment templates…** and multi-select these Excel
workbooks (or import one at a time). Identity comes from the hidden `_meta`
sheet (`participant_id` / `participant_name` / `participant_email`), not the
filename.

Each file has a respondent-facing **Your judgments** sheet (fill the yellow
**Your rating** column) and a hidden **_meta** sheet used on import.

## Files

| File | Matches roster? | A vs B | A vs C | B vs C |
|------|-----------------|-------:|-------:|-------:|
| `ANP_judgments_Alice_Chen.xlsx` | Yes (`alice`) | 5 | 2 | 3 |
| `ANP_judgments_Bob_Rivera.xlsx` | Yes (`bob`) | 1/3 | 5 | 2 |
| `ANP_judgments_Carol_Ng.xlsx` | Yes (`carol`) | 7 | 1/5 | 4 |
| `ANP_judgments_Eve_Morales.xlsx` | **No — new user** | 4 | 1/2 | 3 |

Sample 18 originally had Alice 2/3/1, Bob 8/3/1, Carol 0.5/3/1. These files
use **different** values so you can confirm overwrite on import.

After importing Eve, the roster should gain **Eve Morales**
(`eve` / `eve@example.com`) with the votes above.

Regenerate with the real exporter:

```bash
cmake --build build --target gen_sample18_xlsx_fixtures
./build/gui/gen_sample18_xlsx_fixtures samples/18_multiuser_pairwise_ahp.json \
  samples/import/single_user
```

## Quick test

1. File → Open Sample… → `18_multiuser_pairwise_ahp.json`
2. Participants → Import judgment templates…
3. Select all four `.xlsx` files in this folder
4. Confirm Alice/Bob/Carol judgments changed; Eve was created
