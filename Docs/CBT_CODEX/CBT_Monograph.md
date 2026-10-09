this file contains definitions of the scheme.

# DEFINITIONS

starts at page 8

## Table 2 — Memory requirements (paper p.10)


| attribute name          | type          | element size |
| ----------------------- | ------------- | ------------ |
| (input) halfedge buffer | array         | ×6 integer   |
| (input) vertex buffer   | array         | ×3 floats    |
| bisector pool           | 2^{D} array   | ×9 integers  |
| CBT                     | 2^{D+1} array | ×1 integer   |
| allocation counter      | scalar        | ×1 integer   |
| pointer buffer          | 2^{D} array   | ×2 integers  |


`D` must satisfy D \geq \lceil \log_2(H) \rceil, `H` = number of input half-edges. Paper typically uses `D ∈ [16, 19]`.

Here: halfedge → `FHalfEdge_CBT` / `HalfEdge_Buffer`. Vertex → `FVector3_HighLow` pool. Bisector pool → `FRootBisector_CBT` (`BisectorID` is `uint64` `j`; other 8 fields `int32`). CBT → `CBT_Buffer`. Pointer buffer → `FPointer_CBT` (Alg. 7 + 8).

We store the following data for each bisector within the memory pool:

- `(×3)` integers that act as pointers within the memory pool for the neighbors `Next`, `Prev`, and
`Twin` of each bisector,
- `(×1)` integer for the bisector index, i.e., the value `𝑗` that is required to decode the vertices of
the bisector as shown in `Algorithm 2`.

Note that the width of these integer is important: the neighbor pointers must be sufficiently large
to address the entire pool, while the bisector index enforces a maximum subdivision depth. In
our implementation, we use `32-bit` integers for the neighbor pointers and a `64-bit` integer for the
bisector index. In addition to these two attributes, we also require data to concurrently split and/or
merge each bisector. We store the following additional data for each bisector:

- `(×1)` integer that encodes a split/merge command as a bit code, which we modify concurrently
via atomic bitwise-ORs,
- `(×4)` integers that store unused memory pool locations where the bisector can write to.

The former integer allows us to make sure splits and/or merges are only evaluated once per bisector. The latter ones are sized based on the fact that we only allow one refinement and/or decimation level per bisector during each incremental update. This means that in the worst case, a bisector will split into four new ones (hence four integers). Such a case happens whenever the bisector belongs to the compatibility chain of two foreign bisectors under refinement. In Figure 4 for instance, splitting bisector  b_{14}^{1}  results in splitting  b_{11}^{0}  into three new ones due to its location in  b_{14}^{1} 's compatibility chain. Finally, we further allocate memory for the following:

- an atomic counter, i.e., an integer, that we increment for each memory allocation,
- a  2^{D}  buffer of  (\x 2)  integers per element, which caches the results of Algorithms 7 and 8.

We further detail the purpose of each of these attributes in the following paragraphs.

## CBT_Buffer (size 2^{D+1})

- `[0 … 2^D − 1]` — sum-reduction tree. `CBT[0]` unused. `CBT[k] = CBT[2k] + CBT[2k+1]`. `CBT[1]` = number of live bisectors. Used to find the k-th occupied slot (Alg. 7) or a free slot (Alg. 8) in O(D).
- `[2^D … 2^{D+1} − 1]` — bitfield, one bit per pool slot. `1` = occupied, `0` = free. Not split/merge (that is `BisectorCommand` / `j`).

On a bit flip: set the leaf, then only the path to `CBT[1]`. Full reduce is init only.

---

No. The eight paper algorithms are only partly in the tick path.

| Algorithm | What it does | In the tick |
|---|---|---|
| 1 | Root triangle corners from the half-edge | Yes, inside the debug draw |
| 2 | Child corners from the low bits of `BisectorID` | Yes, same draw |
| 3 | Refine the twin first, then split the pair | Written, not called. The tick splits this cell and the current twin only |
| 4 | Merge a same-depth diamond or a border pair | Yes, as merge-by-heap-id. It frees the odd slot |
| 5 | Point neighbors at the new children | Yes |
| 6 | Point neighbors back at the parent | Yes, inside the merge |
| 7 | i-th occupied slot | Written, never called |
| 8 | i-th free slot | Yes, that is the allocator |

Also missing for a GPU sync: the pointer buffer is allocated and filled with empty entries. Nothing writes Algorithm 7/8 results into it, and the repo's one-update double and triple splits are not in this path.