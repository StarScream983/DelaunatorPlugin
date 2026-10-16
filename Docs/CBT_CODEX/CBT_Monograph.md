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


| Algorithm | What it does                                    | In the tick                                                              |
| --------- | ----------------------------------------------- | ------------------------------------------------------------------------ |
| 1         | Root triangle corners from the half-edge        | Yes, inside the debug draw                                               |
| 2         | Child corners from the low bits of `BisectorID` | Yes, same draw                                                           |
| 3         | Refine the twin first, then split the pair      | Written, not called. The tick splits this cell and the current twin only |
| 4         | Merge a same-depth diamond or a border pair     | Yes, as merge-by-heap-id. It frees the odd slot                          |
| 5         | Point neighbors at the new children             | Yes                                                                      |
| 6         | Point neighbors back at the parent              | Yes, inside the merge                                                    |
| 7         | i-th occupied slot                              | Written, never called                                                    |
| 8         | i-th free slot                                  | Yes, that is the allocator                                               |


Also missing for a GPU sync: the pointer buffer is allocated and filled with empty entries. Nothing writes Algorithm 7/8 results into it, and the repo's one-update double and triple splits are not in this path.

---

A root triangle at depth d is 2^d live triangles. A cell with n roots is n \times 2^d.


| Depth | 1 triangle | 5          | 6           | 7           | 8           | 9           |
| ----- | ---------- | ---------- | ----------- | ----------- | ----------- | ----------- |
| 8     | 256        | 1,280      | 1,536       | 1,792       | 2,048       | 2,304       |
| 9     | 512        | 2,560      | 3,072       | 3,584       | 4,096       | 4,608       |
| 10    | 1,024      | 5,120      | 6,144       | 7,168       | 8,192       | 9,216       |
| 11    | 2,048      | 10,240     | 12,288      | 14,336      | 16,384      | 18,432      |
| 12    | 4,096      | 20,480     | 24,576      | 28,672      | 32,768      | 36,864      |
| 13    | 8,192      | 40,960     | 49,152      | 57,344      | 65,536      | 73,728      |
| 14    | 16,384     | 81,920     | 98,304      | 114,688     | 131,072     | 147,456     |
| 15    | 32,768     | 163,840    | 196,608     | 229,376     | 262,144     | 294,912     |
| 16    | 65,536     | 327,680    | 393,216     | 458,752     | 524,288     | 589,824     |
| 17    | 131,072    | 655,360    | 786,432     | 917,504     | 1,048,576   | 1,179,648   |
| 18    | 262,144    | 1,310,720  | 1,572,864   | 1,835,008   | 2,097,152   | 2,359,296   |
| 19    | 524,288    | 2,621,440  | 3,145,728   | 3,670,016   | 4,194,304   | 4,718,592   |
| 20    | 1,048,576  | 5,242,880  | 6,291,456   | 7,340,032   | 8,388,608   | 9,437,184   |
| 21    | 2,097,152  | 10,485,760 | 12,582,912  | 14,680,064  | 16,777,216  | 18,874,368  |
| 22    | 4,194,304  | 20,971,520 | 25,165,824  | 29,360,128  | 33,554,432  | 37,748,736  |
| 23    | 8,388,608  | 41,943,040 | 50,331,648  | 58,720,256  | 67,108,864  | 75,497,472  |
| 24    | 16,777,216 | 83,886,080 | 100,663,296 | 117,440,512 | 134,217,728 | 150,994,944 |


A 6-root cell at depth 16 is about 393,000 triangles. At depth 24 it is about 101 million. Foliage is on top of that, one set of points per triangle you keep.

Depth 12. That was for 100,000 sites at about 1/6 Earth radius, which is the same scale as your 1000 km planet. A triangle at that depth is about 115 m across.

Each depth cuts the edge in half. On your 1000 km planet with 100,000 sites, one Voronoi edge is about 7.0 km, so that is the depth 0 size.

---



## `1000KM` PLANET, 1/6 TH SCALE


| Depth | 1 triangle | Edge    |
| ----- | ---------- | ------- |
| 8     | 256        | 27.2 m  |
| 9     | 512        | 13.6 m  |
| 10    | 1,024      | 6.79 m  |
| 11    | 2,048      | 3.40 m  |
| 12    | 4,096      | 1.70 m  |
| 13    | 8,192      | 84.8 cm |
| 14    | 16,384     | 42.4 cm |
| 15    | 32,768     | 21.2 cm |
| 16    | 65,536     | 10.6 cm |
| 17    | 131,072    | 5.30 cm |
| 18    | 262,144    | 2.65 cm |
| 19    | 524,288    | 1.33 cm |
| 20    | 1,048,576  | 6.63 mm |
| 21    | 2,097,152  | 3.31 mm |
| 22    | 4,194,304  | 1.66 mm |
| 23    | 8,388,608  | 0.83 mm |
| 24    | 16,777,216 | 0.41 mm |


Depth 12 is 1.7 m on this planet. The earlier 115 m figure does not match this edge.

---



## 6400KM PLANET

Same 100,000 sites. The edge scales with the radius, so a Voronoi edge here is about 44.5 km. The triangle counts do not change.


| Depth | 1 triangle | Edge    |
| ----- | ---------- | ------- |
| 8     | 256        | 174 m   |
| 9     | 512        | 86.9 m  |
| 10    | 1,024      | 43.5 m  |
| 11    | 2,048      | 21.7 m  |
| 12    | 4,096      | 10.9 m  |
| 13    | 8,192      | 5.43 m  |
| 14    | 16,384     | 2.72 m  |
| 15    | 32,768     | 1.36 m  |
| 16    | 65,536     | 67.9 cm |
| 17    | 131,072    | 34.0 cm |
| 18    | 262,144    | 17.0 cm |
| 19    | 524,288    | 8.49 cm |
| 20    | 1,048,576  | 4.24 cm |
| 21    | 2,097,152  | 2.12 cm |
| 22    | 4,194,304  | 1.06 cm |
| 23    | 8,388,608  | 5.31 mm |
| 24    | 16,777,216 | 2.65 mm |

---

## 1400KM PLANET, 6400 x 1/4 scale

Radius is 1600 km. Same 100,000 sites, so the triangle counts stay the same. A Voronoi edge is about 11.1 km.

| Depth | 1 triangle | Edge |
|---:|---:|---:|
| 8 | 256 | 43.5 m |
| 9 | 512 | 21.7 m |
| 10 | 1,024 | 10.9 m |
| 11 | 2,048 | 5.43 m |
| 12 | 4,096 | 2.72 m |
| 13 | 8,192 | 1.36 m |
| 14 | 16,384 | 67.9 cm |
| 15 | 32,768 | 34.0 cm |
| 16 | 65,536 | 17.0 cm |
| 17 | 131,072 | 8.49 cm |
| 18 | 262,144 | 4.24 cm |
| 19 | 524,288 | 2.12 cm |
| 20 | 1,048,576 | 1.06 cm |
| 21 | 2,097,152 | 5.31 mm |
| 22 | 4,194,304 | 2.65 mm |
| 23 | 8,388,608 | 1.33 mm |
| 24 | 16,777,216 | 0.66 mm |

---

## 9600KM PLANET, Earth × 1.5 (max habitable)

Same 100,000 sites as the monograph tables. Edge scales with radius from the 1000 km baseline. A Voronoi edge here is about 67.2 km. Triangle counts do not change.

| Depth | 1 triangle | Edge    |
| ----- | ---------- | ------- |
| 8     | 256        | 261 m   |
| 9     | 512        | 131 m   |
| 10    | 1,024      | 65.2 m  |
| 11    | 2,048      | 32.6 m  |
| 12    | 4,096      | 16.3 m  |
| 13    | 8,192      | 8.14 m  |
| 14    | 16,384     | 4.07 m  |
| 15    | 32,768     | 2.04 m  |
| 16    | 65,536     | 1.02 m  |
| 17    | 131,072    | 50.9 cm |
| 18    | 262,144    | 25.4 cm |
| 19    | 524,288    | 12.8 cm |
| 20    | 1,048,576  | 6.36 cm |
| 21    | 2,097,152  | 3.18 cm |
| 22    | 4,194,304  | 1.59 cm |
| 23    | 8,388,608  | 7.97 mm |
| 24    | 16,777,216 | 3.94 mm |

---

## 10000KM PLANET

Same 100,000 sites. A Voronoi edge here is about 70.0 km. Triangle counts do not change.

| Depth | 1 triangle | Edge    |
| ----- | ---------- | ------- |
| 8     | 256        | 272 m   |
| 9     | 512        | 136 m   |
| 10    | 1,024      | 67.9 m  |
| 11    | 2,048      | 34.0 m  |
| 12    | 4,096      | 17.0 m  |
| 13    | 8,192      | 8.48 m  |
| 14    | 16,384     | 4.24 m  |
| 15    | 32,768     | 2.12 m  |
| 16    | 65,536     | 1.06 m  |
| 17    | 131,072    | 53.0 cm |
| 18    | 262,144    | 26.5 cm |
| 19    | 524,288    | 13.3 cm |
| 20    | 1,048,576  | 6.63 cm |
| 21    | 2,097,152  | 3.31 cm |
| 22    | 4,194,304  | 1.66 cm |
| 23    | 8,388,608  | 8.30 mm |
| 24    | 16,777,216 | 4.10 mm |