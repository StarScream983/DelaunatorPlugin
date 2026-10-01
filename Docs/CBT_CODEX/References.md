# REFERENCES
https://github.com/AnisB/large_cbt/tree/main
https://advances.realtimerendering.com/s2024/content/Intel/large_scale_cbt_slides_siggraph_advances_2024.pdf
https://www.youtube.com/watch?v=-Ce-XC4MtWk&list=PLD5wBo3-8n8LKGnobk4WDxYOF8aZve7MT&index=2

## CBT / CBT-Large (tessellation)

## 2024 — Concurrent Binary Trees for Large-Scale Game Components (primary)

Benyoub + Dupuy, HPG 2024. CBT as a **GPU memory pool**; one **bisector per input half-edge**; split/merge on arbitrary meshes (planetary scale).

- Paper (arXiv HTML): https://arxiv.org/html/2407.02215
- Paper (arXiv PDF): https://arxiv.org/pdf/2407.02215
- Intel write-up: https://www.intel.com/content/www/us/en/developer/articles/technical/cbt-for-large-scale-game-components.html
- Demo / source (DX12, SM 6.6, `outer_space`): https://github.com/AnisB/large_cbt

## 2020 — Concurrent Binary Trees (the bit tree)

Dupuy. Sum-reduction heap used as the **pool** inside Large (`CBT_Buffer`). Implicit triangulation over a square in the original paper — we do **not** use that as the planet mesh.

- Paper page: https://onrendering.com/
- `libcbt` (C + GLSL): https://github.com/jdupuy/libcbt
- LEB 2D demo: https://github.com/jdupuy/LongestEdgeBisection2D

## Half-edge refinement (same lineage)

Dupuy / Vanhoey — *A Halfedge Refinement Rule for Parallel Adaptive Mesh Refinement*. Bisector ↔ half-edge operators. Search the PDF; add a pinned URL under extra if you have one.

---

## Two papers — which one we are

| Paper | What it is | Role here |
|---|---|---|
| Dupuy 2020 — *Concurrent Binary Trees (with application to Longest Edge Bisection)* | Binary heap / sum-reduction tree. Implicit triangulation over a **square**. | The **pool** (`CBT_Buffer`). `libcbt`. |
| Benyoub + Dupuy 2024 — *Concurrent Binary Trees for Large-Scale Game Components* (HPG) | Same CBT used as a **memory pool**. One **bisector per input half-edge**. Split/merge via half-edge ops. Arbitrary meshes, planetary scale. | **This.** `AnisB/large_cbt`. `FRootBisector_CBT`, `FHalfEdge_CBT`, `FPointer_CBT`. |

2020 CBT *is* the triangulation. 2024 CBT *allocates* bisectors; the mesh lives in the bisector / half-edge arrays.

---

## Already in this repo

Built in `GeoDelaunatorComponent.cpp` (Root Bisectors + `CBT_Buffer`), uploaded in `CBTResource_Interface::InitFromCPU`:

- `HalfEdge_Buffer` — `FHalfEdge_CBT` from Voronoi polygon rings (Edge / Twin / Next / Prev).
- `RootBisectors_Buffer` — one `FRootBisector_CBT` per half-edge. Constructor sets `BisectorID, Twin, Next, Prev` from the HE; `Child0..3` start as `INVALID_POINTER`; `BisectorCommand = UNCHANGED_ELEMENT`.
- `CBT_Buffer` — sum-reduction tree, size `2 * 2^D`. First `H` leaves = 1 (one live root bisector each). Internal nodes = child sums.
- `FPointer_CBT` — paper Algorithms 7 / 8 (decode heap index → neighbor pointers). CPU/GPU pointer buffers exist; cache-pointers CS is a stub.
- Elevation SRV already primed before RHI init.
- GPU upload of the same arrays in `FCBTResource_Interface`. Shaders: `CBT_Common.ust`, `CBT_ResetCounter.usf`, `CBT_CachePointers.usf`.
**Not implemented yet:** split, merge, extra bisector allocation, neighbor-bit updates, collision from leaves.

---