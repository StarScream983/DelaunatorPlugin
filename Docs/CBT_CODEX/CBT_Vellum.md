# CBT_Vellum is the step by step Implementation of CBT

# CBT CPU update (paper §3.3)

`PawnTick` through the neighbor spheres picks the closest site and `SubdivDepth`. The split under that is one paper update per generation:

1. `ResetBisectorUpdateFields` — resets `BisectorCommand = 0`, `Child0`–`Child3` for next split.

2. `GenerateSplitCommands` — BisectorSplitDepth reads one bisector and returns how many times it has already been split. GenerateSplitCommands uses that count: below SubdivDepth gets a split mark, and anything already there stays keep. RootHalfEdgeFromBisectorID reads the same bisector and returns the original half-edge it came from. GenerateSplitCommands uses that edge to decide whether the bisector belongs to the closest site. The debug draw uses it to place the triangle’s corners.

3. `ReserveSplitBlocks` — the parent's `Child0` gets the current `AllocationCounter`. That number is the odd child's pool slot. The parent slot becomes the even child later, in `FillSplitBlocks`, and `Child0` is cleared. `OccupyCbtPoolSlot` sets that slot's CBT leaf to 1 (occupied), then updates each parent sum up to `CBT[1]`. `Child1`–`Child3` stay empty. then `AllocationCounter` adds one (for next child)

4. `FillSplitBlocks` — this site's ring only, in pool order. Algorithm 3: split with the twin only when it points back at the same depth. A live twin is not a border. Table 1: even stays in the parent slot (`j` becomes `2j`), odd is the reserved slot (`2j+1`). Then command and `Child0`–`Child3` are cleared.

Root index is `j = NumLeaves + h`, and `NumLeaves` is `1 << D` from the existing H calculation. Algorithm 2: split depth is `floor(log2 j) - D`, root half-edge is `(j >> depth) - NumLeaves`. `Child0`–`Child3` are not child pointers and not the root half-edge.

Not yet: merge, Algorithm 8 free-slot reuse, pointer buffer, the repo's multi-edge compatibility chain in one update.

if i do CBT buffers size at D = Ceillog2(H)+d, then 2^D and 2^(D+1) to increase size and make room for more subdiv depth, if we use same CBT scheme will it work

---

## Neighbor twin half-edges from two sites (minimal perfect hash)

Goal: given two neighboring Voronoi sites `s1` and `s2`, get `{s1, he1, s2, he2}` with no ring scan.
`TArray<TwinPairs>'

`TwinPairs` is a `TArray` of those structs. Its length is `HalfEdge_Buffer.Num() / 2` (same as `3 * SphericalTriangles.Num() / 2` on a closed sphere). Each slot is one twin pair. The ID is assigned when the twin is recorded: if this half-edge index is the lower one, take the next integer `0, 1, 2, …` and write the struct at `TwinPairs[ID]`.

A plain mix of the two site IDs plus `% TwinPairs.Num()` does **not** give each neighbor pair a unique ID. Two different pairs can land on the same number. The numbers of neighbor pairs change with the planet, so the table that turns two sites into that ID must be built with the planet.

### `h0`, `h1`, `h2`

These are not in the mesh. They are three functions you write. Each takes the same two sites and returns a different integer (an index into `g`).

Pack the sites so the key is unique regardless of order:

```cpp
const uint32 A = FMath::Min(s1, s2);
const uint32 B = FMath::Max(s1, s2);
const uint64 Key = ((uint64)A << 32) | (uint64)B;
```

Any three mixes are fine if they disagree. One common set (`M` = `g.Num()`):

```cpp
int32 h0(uint64 Key, int32 M) { return (int32)((Key * 0x9E3779B97F4A7C15ull) % (uint64)M); }
int32 h1(uint64 Key, int32 M) { return (int32)((Key * 0xBF58476D1CE4E5B9ull) % (uint64)M); }
int32 h2(uint64 Key, int32 M) { return (int32)((Key * 0x94D049BB133111EBull) % (uint64)M); }
```

### `g`

`g` is a second `TArray<int32>`, built once when the planet’s twin pairs are known. It is **not** `TwinPairs`. Its length is about `1.23 * TwinPairs.Num()`.

Why ~1.23×: each neighbor pair names three slots in `g` through `h0`, `h1`, and `h2`. That is a 3-edge on a graph whose vertices are the slots of `g`. The build can assign the numbers in `g` only when that graph has no cycle. With three hashes, that works with high probability at about `1.23 ×` the pair count (BDZ / Botelho–Pagh–Ziviani). Smaller fails more often. Larger works too. `g` does **not** line up one-to-one with `TwinPairs` indices.

At first `g` is empty. The build walks every neighbor pair and writes three numbers into the three slots those mixes named, so this holds for the ID already assigned at the twin match:

```cpp
ID == (g[h0(Key)] + g[h1(Key)] + g[h2(Key)]) % TwinPairs.Num()
```

If the build gets stuck, repeat with new mixes until every neighbor pair solves.

### Lookup once built

`g` is only used to compute the ID. The three values read from `g` are added; the remainder by the pair count lands in `0 .. TwinPairs.Num() - 1`:

```cpp
const uint32 A = FMath::Min(s1, s2);
const uint32 B = FMath::Max(s1, s2);
const uint64 Key = ((uint64)A << 32) | (uint64)B;

const int32 M = g.Num();
const int32 ID =
	(g[h0(Key, M)] + g[h1(Key, M)] + g[h2(Key, M)]) % TwinPairs.Num();

const FTwinPair& Pair = TwinPairs[ID]; // {s1, he1, s2, he2}
```

After `g` is solved, each neighbor pair used to build it gets a different ID. Non-neighbor sites are not in the table; this path is for sites that already share an edge.

we can link neighboring sites with their corresponding spherical triangle half-edges and voronoi half-edges.