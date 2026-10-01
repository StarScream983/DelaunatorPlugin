# CBT implementation notes

## Sequential CPU split (2026-10-01)

- **Where:** `UGeoDelaunatorComponent::PawnTick` after `ClosestSite` is set.
- **Which slots:** `SitePrefixSums[ClosestSite]` .. `+ VoronoiHalfEdges_Map[ClosestSite].Num()`. Roots have `BisectorID == 1`.
- **What:** paper §2 `Refine` (Alg. 3) → paired/boundary `Split` (Table 1) + neighbor rewrite (Alg. 5). Even child stays in the parent pool slot; odd child is `AllocationCounter_Buffer++`. New CBT leaf set to 1, heap reduced to root.
- **Debug:** magenta triangles from Alg. 1 + 2 (`GetBisectorVertices`). One level only (`j==1`); deeper splits not yet.
- **Not yet:** merge (children stay when the pawn leaves), GPU commands, collision recook from leaves.
