# CBT_Vellum is the step by step Implementation of CBT

# CBT CPU update (paper §3.3)

`PawnTick` through the neighbor spheres picks the closest site and `SubdivDepth`. The split under that is one paper update per generation:

1. `ResetBisectorUpdateFields` — resets `BisectorCommand = 0`, `Child0`–`Child3` for next split.

2. `GenerateSplitCommands` — BisectorSplitDepth reads one bisector and returns how many times it has already been split. GenerateSplitCommands uses that count: below SubdivDepth gets a split mark, and anything already there stays keep. RootHalfEdgeFromBisectorID reads the same bisector and returns the original half-edge it came from. GenerateSplitCommands uses that edge to decide whether the bisector belongs to the closest site. The debug draw uses it to place the triangle’s corners.

3. `ReserveSplitBlocks` — the parent's `Child0` gets the current `AllocationCounter`. That number is the odd child's pool slot. The parent slot becomes the even child later, in `FillSplitBlocks`, and `Child0` is cleared. `OccupyCbtPoolSlot` sets that slot's CBT leaf to 1 (occupied), then updates each parent sum up to `CBT[1]`. `Child1`–`Child3` stay empty. then `AllocationCounter` adds one (for next child)

4. `FillSplitBlocks` — this site's ring only, in pool order. Algorithm 3: split with the twin only when it points back at the same depth. A live twin is not a border. Table 1: even stays in the parent slot (`j` becomes `2j`), odd is the reserved slot (`2j+1`). Then command and `Child0`–`Child3` are cleared.

Root index is `j = NumLeaves + h`, and `NumLeaves` is `1 << D` from the existing H calculation. Algorithm 2: split depth is `floor(log2 j) - D`, root half-edge is `(j >> depth) - NumLeaves`. `Child0`–`Child3` are not child pointers and not the root half-edge.

Not yet: merge, Algorithm 8 free-slot reuse, pointer buffer, the repo's multi-edge compatibility chain in one update.
