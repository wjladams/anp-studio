# ANP Studio — Stage IA & One-Page Wireframe

Target information architecture for ANP Studio. Consistency with the current
dock layout is not required.

**Status (cut line 1):** Structure → Judgments → Synthesis stage shell is the
implementation target. Ratings under Judgments (scale + votes via libanpcpp
`RatingsPrioritizer`) is MVP, not deferred. Insight / Research remain later.

**Stages:** Structure → Judgments → Synthesis → Insight → Research

**Shell (all stages):** stage strip · File / Edit / Undo · document title / dirty ·
Calculate (F5) · subnet breadcrumb

**Legend:** ● MVP · ○ Later

---

## Narrative

| Stage | Job |
|-------|-----|
| **Structure** | Design clusters, nodes, subnetworks, connections |
| **Judgments** | Pairwise (primary) and Ratings (secondary); multi-user; bounce comparison UIs |
| **Synthesis** | Calculate; unscaled / scaled / limit / globals; synthesis method |
| **Insight** | Sensitivity, graphs, interpretation |
| **Research** | Python on the open model; experiment; share with researchers |

Ratings live under **Judgments**, not as a top-level stage. Research is
post-pipeline extensibility (live binding + share package), not a judgment method.

Stages are freely clickable. Insight / Research soft-gate before first calculate
(empty CTA, still reachable).

---

## Shared shell

```text
┌─ Structure  Judgments  Synthesis  Insight  Research ──────┐
├────────────┬───────────────────────────────┬──────────────┤
│ Left       │ Center                        │ Right        │
│ (stage)    │ (stage)                       │ (stage)      │
├────────────┴───────────────────────────────┴──────────────┤
│ Bottom (optional; stage-dependent)                        │
└───────────────────────────────────────────────────────────┘
```

One open document; switching stage remaps chrome only. Subnet stack / breadcrumb
survives stage switches.

---

## 1. Structure

```text
┌ Structure │ Judgments │ Synthesis │ Insight │ Research ┐
├──────────┬────────────────────────────┬────────────────┤
│ TREE     │         CANVAS             │ INSPECTOR      │
│ clusters │  clusters/nodes/links      │ name           │
│ nodes    │  connect mode              │ invert         │
│ alts     │  enter subnet (dbl-click)  │ alternatives ● │
│ ↑ root   │                            │ open subnet    │
└──────────┴────────────────────────────┴────────────────┘
```

| Panel | Controls |
|-------|----------|
| Tree | ● add/rename/delete · ● reorder · ● select syncs canvas · ● up/root |
| Canvas | ● place/move · ● connect mode · ● context menu · ○ auto-layout |
| Inspector | ● name · ● invert · ● alternatives · ● open subnet |
| Bottom | ○ validation strip |

**Empty state:** “Add a cluster to start.”

---

## 2. Judgments

```text
├──────────┬────────────────────────────┬────────────────┤
│ NAV      │  [Matrix│Quest.|Graph] ●   │ SESSION        │
│ parents  │                            │ participants ○ │
│ cover %  │     comparison surface     │ whose data ○   │
│ CR ⚠     │                            │ aggregate ○    │
│ Pair│Rate│     (Ratings sub-mode ●)   │                │
└──────────┴────────────────────────────┴────────────────┘
```

| Panel | Controls |
|-------|----------|
| Nav | ● parent list · ● node/cluster filter · ● coverage · ● CR badge · ● Pairwise \| Ratings |
| Center | ● matrix edit · ○ questionnaire · ○ graphical · ● priorities + CR readout |
| Ratings | ● alt×node scores · ● scale editor (categorical / numeric interpreters) · ● weighted L1 column via RatingsPrioritizer |
| Session | ○ users · ○ switch judge · ○ aggregate rule |
| Bottom | ○ read-only mini-canvas for parent context |

**Empty state:** “Connect nodes in Structure first” if no comparison parents.

---

## 3. Synthesis

```text
├──────────┬────────────────────────────┬────────────────┤
│ CALC     │ Unscaled│Cl.W│Scaled│Limit │ SUMMARY        │
│ Calculate│ Global│Alternatives        │ alt priorities │
│ method ● │                            │ warnings       │
│ stale ⚠  │      matrix / vector       │                │
└──────────┴────────────────────────────┴────────────────┘
```

| Panel | Controls |
|-------|----------|
| Calc | ● Calculate · ● Additive / Multiplicative / Custom · ● last-run · ● stale badge |
| Center | ● matrix tabs · ○ copy/export table |
| Summary | ● ranked alternatives · ● top CR / issues |

**Empty state:** “Calculate to populate matrices.”

---

## 4. Insight

```text
├──────────┬────────────────────────────┬────────────────┤
│ CATALOG  │      chart / analysis      │ PARAMS         │
│ sens. ○  │                            │ node to vary ○ │
│ charts ● │                            │ range/step ○   │
│ rank ○   │   [↗ open in Judgments]    │                │
└──────────┴────────────────────────────┴────────────────┘
```

| Panel | Controls |
|-------|----------|
| Catalog | ● priority bar/pie · ○ sensitivity · ○ rank stability · ○ scenarios |
| Center | ● chart · ● drill → Judgments · ○ export image |
| Params | ○ vary-node · ○ range · ○ display options |
| Bottom | ○ linked data table |

**Empty state:** “Run Synthesis first” (or one-click Calculate).

---

## 5. Research

```text
├──────────┬────────────────────────────┬────────────────┤
│ SCRIPTS  │  console / notebook ●      │ BINDINGS       │
│ starters │                            │ model handle   │
│ project  │  output / plots            │ results handle │
│ API ●    │                            │ Share pkg ○    │
└──────────┴────────────────────────────┴────────────────┘
```

| Panel | Controls |
|-------|----------|
| Scripts | ● starter snippets · ○ project file list · ● API outline |
| Center | ● REPL bound to open document · ○ notebook cells · ● stdout / errors |
| Bindings | ● `model` / `calculate` / `results` · ○ Share (model + script + freeze) |
| Mutations | ● mark dirty · ● stale Synthesis / Insight · ○ undo bridge |

**Empty state:** Starter notebook (list clusters, print globals, one sensitivity loop).

**Share package:** model file + scripts/notebook + optional frozen results +
Studio / lib version note.

---

## Cross-stage rules

| Rule | MVP |
|------|-----|
| Free stage jump | ● |
| Selection soft-persists (each stage maps it) | ● |
| Dirty → stale results badge on Synthesis / Insight | ● |
| Subnet breadcrumb global | ● |
| Ratings under Judgments only | ● |
| Research last; live binding to open document | ● |
| Soft-gate Insight / Research pre-calc | ● |

### Transition defaults

| From → To | Center focuses on |
|-----------|-------------------|
| Structure → Judgments | First incomplete parent, or selection’s parent |
| Judgments → Synthesis | Alternatives / globals after calc (or prompt Calculate) |
| Synthesis → Insight | Priority chart (or sensitivity on top driver) |
| Insight → Research | Starter cell with `results` bound |
| Any → Structure | Canvas with last selection |

---

## Ship order

1. **Structure + Judgments (matrix + Ratings scale/votes) + Synthesis** — full loop (cut line 1)
2. **Insight (priority charts)**
3. **Judgments multi-view bounce + multi-user**
4. **Insight sensitivity**
5. **Research REPL + share package**
