# CBT implementation notes

## Sequential CPU split (2026-10-01)

- **Where:** `UGeoDelaunatorComponent::PawnTick` after `ClosestSite` is set.
- **Which slots:** live pool entries (`0 .. AllocationCounter`) whose `Child1` (input half-edge) is in the closest site's prefix range.
- **What:** paper §2 `Refine` (Alg. 3) → paired/boundary `Split` (Table 1) + neighbor rewrite (Alg. 5). Even child stays in the parent pool slot; odd child is `AllocationCounter_Buffer++`. New CBT leaf set to 1, heap reduced to root.
- **Depth:** `UnitVoronoiSubdivRadius = VoronoiSubdivRadius × VoronoiSubdivRadiusMultiplier` (default 1.2). Yellow = 1× unit, orange = 5× unit. Five 1× bands: enter 5× → depth 1, inside yellow → depth 5. One `PawnTick` runs generations until the closest site’s leaves hit that cap (or the pool is full).
- **Debug:** yellow = 1×, orange = 5×. Magenta while inside orange.
- **Not yet:** merge (children stay when the pawn leaves), GPU commands, collision recook from leaves.

## Pawn → split (function map)

Gravity overlap → `RegisterPawn` → `PawnTick` every 0.1s.

Yellow `UnitVoronoiSubdivRadius` = 1× band. Orange = 5× trigger. Refine + magenta while `PawnToSite <= 5 × UnitVoronoiSubdivRadius`. Target depth = how many 1× bands the pawn has crossed (1 at orange, 5 inside yellow).

```
    PawnTick
        │  closest site = max Dot(pawnDir, FibonacciPoints)
        │  yellow = 1× SubdivRadius, orange = 5×
        │  Refine if Dist <= 5×; target depth from 1× bands
        │  loop generations this tick until site leaves reach that depth
        ▼
    RefineBisector          Alg. 3 — if Twin needs it, Refine twin first, then Split
        │
        ├─ SplitBisector    Table 1 — even stays in slot, odd = new slot, j → 2j / 2j+1
        │      │
        │      ├─ AllocateBisectorSlot   pool index = AllocationCounter++
        │      ├─ OccupyCbtPoolSlot      leaf 1, reduce path to CBT[1]
        │      └─ RefineBisectorPointers Alg. 5 — rewrite Next/Prev/Twin of neighbors
        │
        ├─ GetBisectorVertices   Alg. 1 + 2 — decode triangle verts from j + root HE
        └─ DrawBisectorDebug     magenta edges of that triangle
```

| Function | Role |
|---|---|
| `IsLiveBisector` | slot valid and `BisectorID != 0` |
| `RefineBisector` | conforming split; skip if `BisectorCommand != KEEP` this tick |
| `SplitBisector` | write children + topology |
| `RefineBisectorPointers` | neighbors point at the new children |
| `AllocateBisectorSlot` | next free pool index |
| `OccupyCbtPoolSlot` | CBT bitfield + path reduce |
| `GetBisectorVertices` | `v0,v1` from HE, `v2` face average, then `j` bits |
| `DrawBisectorDebug` | draw that triangle at ground radius |