# Voronoi Planet — Implementation Status

Living document. Update whenever a subsystem changes. Each subsystem entry
captures: *purpose · key files · algorithm + reference · current status ·
known limitations · next steps*.

> Reference priority: Gainey → squeakyspacebar → Red Blob.
> See `References.md` for URLs and the per‑step table.

---

## Conventions for entries

- Date entries `YYYY‑MM‑DD` when adding/updating significant sections.
- Status tags: `not started` · `in progress` · `working (basic)` ·
  `working (full)` · `needs rework`.
- When a step deviates from Gainey, **state which source it follows and why**.
- File paths should be relative to the repo root.

---

## Change log

(Append dated bullet points as substantive changes happen. Keep them short —
the per‑subsystem sections above are the source of truth.)

- _YYYY‑MM‑DD — initial skeleton._
- **2026‑05‑04** — **§6 Elevation:** full pipeline (boundary stress, `Hybrid2`, `BlurBoundaryStress`, `AssignElevations`, `GeoDelaunayFrom` order), GPU prime/InitRHI/upload/release, **§6.8 binding table** with file paths + line numbers; **§9** status note for `ElevationPerSite` draw path.
- **2026‑05‑04** — **§12 DF64:** planned GPU/LWC double-float position path; **§12.1–12.5** now embed full HLSL + CPU split pseudocode (transcript retains extra caveats).

---

## 6. Elevation field

- **Purpose:** one scalar elevation per Voronoi/Delaunay site (`ElevationPerSite`), used for terrain logic and GPU debug displacement (radial scale in VS).
- **Reference:** Gainey (boundary stress, `blurPlateBoundaryStress`); active path uses **`ComputeBoundaryElevation_Hybrid2`** (continuous `SmoothStep` blend, no `if (P > 0.3)` ladder) and **`MinElev + Pressure * 0.25`** for divergence (deviation from Gainey’s original divergent boundary).
- **Key files:**
  - `Plugins/DelaunatorPlugin/Source/DelaunatorPlugin/Private/GeoDelaunatorComponent.cpp` — boundaries, `BlurBoundaryStress`, `AssignElevations`, `ComputeBoundaryElevation_*`
  - `Plugins/DelaunatorPlugin/Source/Large_CBT/Private/CBTResource_Interface.{h,cpp}` — `CPU_ElevationPerSite_Buffer`, RHI buffer, SRV, upload
  - `Plugins/DelaunatorPlugin/Source/IndirectInstancingCore/Private/IndirectInstancingSceneProxy.cpp` — per-frame `UserData->ElevationPerSiteSRV`
  - `Plugins/DelaunatorPlugin/Source/IndirectInstancingCore/Private/IndirectInstancingVertexFactory.{h,cpp}` — `ElevationPerSiteParameter` bind + layout
  - `Plugins/DelaunatorPlugin/Shaders/Private/GeoVoronoiIndirectInstancing.ush` — `Buffer<float> ElevationPerSite`
  - `Plugins/DelaunatorPlugin/Shaders/Private/GeoVoronoiIndirectInstancingVertexFactory.ush` — sample by `Instance.SiteId`
- **Status:** working (basic).
- **Critical order:** `PrimeElevationPerSiteBuffer(ElevationPerSite)` must run **before** `InitFromCPU(...)` so `InitRHI` sees non-empty `CPU_ElevationPerSite_Buffer` (otherwise no GPU buffer is created).
- **Also in source (not used by `AssignElevations`):** `ComputeBoundaryElevation`, `ComputeBoundaryElevation_Gainey`, `ComputeBoundaryElevation_Hybrid` — kept for comparison.
- **Limitations:** per-cell constant height + extrusion ⇒ visible wedges between neighbours with large Δelev; inland decay is a single exponential BFS, not Gainey’s per-regime decay curves.

### 6.1 Source-of-truth line map (`GeoDelaunatorComponent.cpp`)

| Piece | Approx. lines |
|-------|----------------|
| Plate boundary detection + `Pressure` / `Shear` | 1390–1425 |
| `ComputeBoundaryElevation` | 1519–1558 |
| `ComputeBoundaryElevation_Gainey` | 1565–1614 |
| `ComputeBoundaryElevation_Hybrid` | 1623–1661 |
| **`ComputeBoundaryElevation_Hybrid2`** (**used**) | 1664–1707 |
| **`BlurBoundaryStress`** | 1729–1795 |
| **`AssignElevations`** | 1797–1909 |
| **`GeoDelaunayFrom`** — prime elevation before `InitFromCPU` | 1125–1132 |

### 6.2 Boundary record — raw stress (`GeneratePlates_RedBlobRandomFill`)

When a primal Delaunay edge connects two different plates (`CurrentSite < NeighborSite` dedupes undirected edges):

```cpp
const FVector RelativeMotion =
	(PA.DriftDirection * (float)PA.DriftSpeed) -
	(PB.DriftDirection * (float)PB.DriftSpeed);

const FVector BoundaryNormal =
	(FibonacciPoints[NeighborSite] - FibonacciPoints[CurrentSite]).GetSafeNormal();

const FVector BoundaryTangent =
	FVector::CrossProduct(FibonacciPoints[CurrentSite], BoundaryNormal).GetSafeNormal();

FPlateBoundary Boundary;
Boundary.SiteA = CurrentSite;
Boundary.SiteB = NeighborSite;
Boundary.PlateA = PlateIdPerSite[CurrentSite];
Boundary.PlateB = PlateIdPerSite[NeighborSite];
Boundary.HalfEdgeAB = GlobalAB;
Boundary.HalfEdgeBA = GlobalBA;
Boundary.Pressure = (double)FVector::DotProduct(RelativeMotion, BoundaryNormal);
Boundary.Shear    = FMath::Abs((double)FVector::DotProduct(RelativeMotion, BoundaryTangent));
PlateBoundaries.Add(Boundary);
```

### 6.3 `ComputeBoundaryElevation_Hybrid2` — **active** boundary elevation (continuous blend)

```cpp
double UGeoDelaunatorComponent::ComputeBoundaryElevation_Hybrid2(
	const FPlateBoundary& Boundary,
	const FPlateData& PlateA,
	const FPlateData& PlateB)
{
	constexpr double StressSaturation = 1.0;
	auto Sigmoid = [](double Raw) -> double
	{
		return 2.0 / (1.0 + FMath::Exp(-Raw / StressSaturation)) - 1.0;
	};
	const double Pressure = Sigmoid(Boundary.Pressure);   // [-1, +1] signed
	const double Shear    = Sigmoid(Boundary.Shear);      // [0, +1] style (raw shear >= 0)
	const double ElevA = PlateA.DesiredElevation;
	const double ElevB = PlateB.DesiredElevation;
	const double MaxElev = FMath::Max(ElevA, ElevB);
	const double MinElev = FMath::Min(ElevA, ElevB);
	const double MeanElev = (ElevA + ElevB) * 0.5;
	const double EConv  = MaxElev + Pressure;
	const double EDiv   = MinElev + Pressure * 0.25;
	const double EShear = MaxElev + Shear * 0.125;
	const double EDorm  = MeanElev;
	const double WConvRaw  = FMath::SmoothStep(0.10, 0.40, Pressure);
	const double WDivRaw   = FMath::SmoothStep(0.10, 0.40, -Pressure);
	const double WShearRaw = FMath::SmoothStep(0.12, 0.42, Shear)
		* (1.0 - FMath::Max(WConvRaw, WDivRaw));
	double Wc = WConvRaw;
	double Wd = WDivRaw;
	double Ws = WShearRaw;
	double Wsum = Wc + Wd + Ws;
	if (Wsum > 1.0 - KINDA_SMALL_NUMBER)
	{
		const double Inv = 1.0 / Wsum;
		Wc *= Inv;
		Wd *= Inv;
		Ws *= Inv;
	}
	const double W0 = 1.0 - Wc - Wd - Ws;
	return W0 * EDorm + Wc * EConv + Wd * EDiv + Ws * EShear;
}
```

### 6.4 `BlurBoundaryStress` — Gainey-style smoothing along the boundary graph

`AssignElevations` calls `BlurBoundaryStress(3, 0.4)`. Neighbour boundaries = share `SiteA` or `SiteB`. Each iteration:

`new = own * CenterWeight + mean(neighbours) * (1 - CenterWeight)`.

```cpp
void UGeoDelaunatorComponent::BlurBoundaryStress(int32 Iterations, double CenterWeight)
{
	const int32 NumB = PlateBoundaries.Num();
	if (NumB == 0 || Iterations <= 0) return;

	const int32  NumSites       = PlateIdPerSite.Num();
	const double NeighborWeight = 1.0 - CenterWeight;

	TArray<double> SitePressureSum; SitePressureSum.SetNumZeroed(NumSites);
	TArray<double> SiteShearSum;    SiteShearSum   .SetNumZeroed(NumSites);
	TArray<int32>  SiteCount;       SiteCount      .SetNumZeroed(NumSites);

	TArray<double> NewPressure; NewPressure.SetNumUninitialized(NumB);
	TArray<double> NewShear;    NewShear   .SetNumUninitialized(NumB);

	for (int32 Iter = 0; Iter < Iterations; ++Iter)
	{
		FMemory::Memzero(SitePressureSum.GetData(), sizeof(double) * NumSites);
		FMemory::Memzero(SiteShearSum   .GetData(), sizeof(double) * NumSites);
		FMemory::Memzero(SiteCount      .GetData(), sizeof(int32)  * NumSites);

		for (int32 i = 0; i < NumB; ++i)
		{
			const FPlateBoundary& B = PlateBoundaries[i];
			SitePressureSum[B.SiteA] += B.Pressure;
			SitePressureSum[B.SiteB] += B.Pressure;
			SiteShearSum   [B.SiteA] += B.Shear;
			SiteShearSum   [B.SiteB] += B.Shear;
			SiteCount      [B.SiteA] += 1;
			SiteCount      [B.SiteB] += 1;
		}

		for (int32 i = 0; i < NumB; ++i)
		{
			const FPlateBoundary& B = PlateBoundaries[i];
			const double SumP  = SitePressureSum[B.SiteA] + SitePressureSum[B.SiteB] - 2.0 * B.Pressure;
			const double SumS  = SiteShearSum   [B.SiteA] + SiteShearSum   [B.SiteB] - 2.0 * B.Shear;
			const int32  Count = SiteCount[B.SiteA] + SiteCount[B.SiteB] - 2;

			if (Count > 0)
			{
				const double Inv = 1.0 / Count;
				NewPressure[i] = B.Pressure * CenterWeight + SumP * Inv * NeighborWeight;
				NewShear   [i] = B.Shear    * CenterWeight + SumS * Inv * NeighborWeight;
			}
			else
			{
				NewPressure[i] = B.Pressure;
				NewShear   [i] = B.Shear;
			}
		}

		for (int32 i = 0; i < NumB; ++i)
		{
			PlateBoundaries[i].Pressure = NewPressure[i];
			PlateBoundaries[i].Shear    = NewShear[i];
		}
	}
}
```

### 6.5 `AssignElevations` — average seed + BFS inland lerp

```cpp
void UGeoDelaunatorComponent::AssignElevations()
{
	// One elevation slot per Voronoi cell — same count as Fibonacci points
	const int32 NumSites = PlateIdPerSite.Num();
	ElevationPerSite.Init(0.0, NumSites);

	// ── Stress smoothing pass (Gainey: blurPlateBoundaryStress, 3 iters @ 0.4) ──
	// Has to run BEFORE ComputeBoundaryElevation_Hybrid is consumed below, otherwise
	// adjacent boundaries with near-threshold pressures keep producing single-cell spikes.
	BlurBoundaryStress(/*Iterations=*/3, /*CenterWeight=*/0.4);

	// The elevation of the boundary this site was first reached from
	// Inherited parent-to-child through the BFS — every site in a chain
	// traces back to the same boundary spike it originated from
	TArray<double> NearestBoundaryElevation;
	DistanceToBoundary.Init(INT32_MAX, NumSites);
	NearestBoundaryElevation.Init(0.0, NumSites);

	// Sites enter when first discovered, processed in order of discovery
	// = order of distance from boundaries (BFS property)
	TQueue<int32> Queue;

	// ── Seeding Phase ────────────────────────────────────────────────────────
	// Seed from all boundary sites
    // Set distance 0 on every boundary site and push them into the queue.
    // They are the source — elevation propagates inward from here.
	// FIX (replaces old "first-touched-wins"): a boundary site usually has 2–3
	// cross-plate edges. The previous loop captured only the first edge that
	// reached it during iteration; every other edge's contribution was dropped,
	// which produced iteration-order-dependent single-cell elevation spikes.
	//
	// Now: accumulate every boundary's elevation into the sites it touches,
	// then assign each site the average. Smooth, deterministic, no spikes.
	TArray<double> BoundarySum;   BoundarySum.Init(0.0, NumSites);
	TArray<int32>  BoundaryCount; BoundaryCount.Init(0,  NumSites);

	for (const FPlateBoundary& Boundary : PlateBoundaries)
	{
		const FPlateData& PlateA = Plates[Boundary.PlateA];
		const FPlateData& PlateB = Plates[Boundary.PlateB];

		// Peak or trough for this boundary — the value the BFS will decay from
		const double BoundaryElev = ComputeBoundaryElevation_Hybrid2(Boundary, PlateA, PlateB);

		// Each boundary edge contributes to both of its endpoint sites
		BoundarySum  [Boundary.SiteA] += BoundaryElev;
		BoundarySum  [Boundary.SiteB] += BoundaryElev;
		BoundaryCount[Boundary.SiteA] += 1;
		BoundaryCount[Boundary.SiteB] += 1;
	}

	// Materialise per-site averages and seed the BFS queue
	for (int32 Site = 0; Site < NumSites; ++Site)
	{
		if (BoundaryCount[Site] > 0)
		{
			const double Avg = BoundarySum[Site] / BoundaryCount[Site];
			DistanceToBoundary[Site]       = 0;     // distance zero — it is the boundary
			NearestBoundaryElevation[Site] = Avg;   // inland decay anchor
			ElevationPerSite[Site]         = Avg;   // boundary cells take their average outright
			Queue.Enqueue(Site);
		}
	}

	/** ── Propagation Phase ────────────────────────────────────────────────────
	* Controls how fast boundary elevation fades toward the plate resting floor
	* Higher = narrower mountains, steeper falloff
	* Lower  = wider ranges, elevation lingers further inland
	* At 0.15: after ~7 hops the site is ~35% of the way toward the plate floor
	* BFS outward — uniform hop cost → BFS ≡ Dijkstra */
	const double DecayRate = 0.30;

	TArray<int32> Neighbors;
	TArray<int32> HalfEdgeIndices;  // required by GetVoronoiNeighbors signature, unused here

	// Visits every site exactly once — O(N) total, same cost as plate fill BFS
	int32 CurrentSite;
	while (Queue.Dequeue(CurrentSite))
	{
		int32  CurrentDist = DistanceToBoundary[CurrentSite];
		// Look up this site's plate to get its resting elevation (the interior anchor)
		int32  PlateIdx = PlateIdPerSite[CurrentSite];
		double DesiredElev = Plates[PlateIdx].DesiredElevation;
		GetVoronoiNeighbors(CurrentSite, Neighbors, HalfEdgeIndices);
		for (int32 NeighborSite : Neighbors)
		{
			// INT32_MAX = not yet reached → this is its shortest path from a boundary
			if (DistanceToBoundary[NeighborSite] == INT32_MAX)
			{
				int32  NewDist = CurrentDist + 1;
				// Carry the boundary elevation forward — every site in this chain
				// traces back to the same original boundary spike
				double NearestElev = NearestBoundaryElevation[CurrentSite];
				// Exponential decay: 1.0 at the boundary → approaching 0.0 deep inland
				// Steep near the boundary, flattening out quickly — natural bell shape — natural mountain profile
				double DistanceFactor = Sleef_exp_u10(-NewDist * DecayRate);

				DistanceToBoundary[NeighborSite] = NewDist;
				NearestBoundaryElevation[NeighborSite] = NearestElev;

				// Lerp: DistanceFactor = 1.0 (at boundary)  → pure NearestElev (spike or trench)
				// DistanceFactor = 0.0 (deep interior) → pure DesiredElev (plate resting floor)
				ElevationPerSite[NeighborSite] = FMath::Lerp(
					DesiredElev,   // plate resting elevation (anchor)
					NearestElev,   // boundary spike or trench
					DistanceFactor
				);

				Queue.Enqueue(NeighborSite);
			}
		}
	}
}
```

*Mirror of `GeoDelaunatorComponent.cpp` lines 1797–1909.*

### 6.6 CPU → GPU: `GeoDelaunayFrom` scheduling (`GeoDelaunatorComponent.cpp` ~1125–1132)

```cpp
GeneratePlates_RedBlobRandomFill();
AssignElevations();

CBTResources = MakeShared<FCBTResource_Interface>();
CBTResources->PrimeVoronoiBuffers(VoronoiGeoCenters_HL, VoronoiGeoMesh_Ranges, VoronoiGeoMesh_Flat, VoronoiCellColors);
CBTResources->PrimeTrianglesBuffers(FibonacciPoints_HL, SphericalTrisFlat, SphericalHalfEdges);
CBTResources->PrimeElevationPerSiteBuffer(ElevationPerSite);  // MUST precede InitFromCPU
CBTResources->InitFromCPU(D, HalfEdge_Buffer, VoronoiGeoCenters_HL, RootBisectors_Buffer, CBT_Buffer);
```

### 6.7 GPU buffer: `CBTResource_Interface.cpp` / `.h`

**Prime (lines 38–42):**

```cpp
void FCBTResource_Interface::PrimeElevationPerSiteBuffer(const TArray<float>& InElevationPerSite)
{
    CPU_ElevationPerSite_Buffer.Empty();
    CPU_ElevationPerSite_Buffer = InElevationPerSite;
}
```

**`InitRHI` create + SRV + upload (lines 181–198):**

```cpp
if (CPU_ElevationPerSite_Buffer.Num() > 0)
{
    const uint32 NumBytes = CPU_ElevationPerSite_Buffer.Num() * sizeof(float);
    FRHIResourceCreateInfo CreateInfo(TEXT("LargeCBT_ElevationPerSiteBuffer"));

    ElevationPerSiteBuffer_RHI = RHICmdList.CreateVertexBuffer(
        NumBytes,
        BUF_ShaderResource | BUF_Static,
        ERHIAccess::SRVMask,
        CreateInfo);

    ElevationPerSiteBuffer_SRV = RHICmdList.CreateShaderResourceView(
        ElevationPerSiteBuffer_RHI,
        sizeof(float),
        PF_R32_FLOAT);

    UploadElevationPerSiteBufferToGPU();
}
```

**Upload (lines 632–648):** lock `ElevationPerSiteBuffer_RHI`, `Memcpy` from `CPU_ElevationPerSite_Buffer`, unlock.

**Release (lines 349–350):** `ElevationPerSiteBuffer_SRV.SafeRelease();` `ElevationPerSiteBuffer_RHI.SafeRelease();`

**Accessor:** `GetElevationPerSiteSRV()` in `.h` line ~81.

### 6.8 Binding chain — where each line lives

`FMeshBatchElement::UserData` points at **`FGeoVoronoiIndirectInstancingUserData`** (per draw, per frame). Elevation SRV is one field on that struct.

| Step | File | Lines | What |
|------|------|-------|------|
| Struct field | `IndirectInstancingCore/Private/IndirectInstancingVertexFactory.h` | 41 | `FRHIShaderResourceView* ElevationPerSiteSRV` |
| Fill from CBT | `IndirectInstancingCore/Private/IndirectInstancingSceneProxy.cpp` | 332+ | `GetDynamicMeshElements` |
| Assign SRV | same | **396, 406** | init nullptr; then `CBTResources->GetElevationPerSiteSRV()` |
| Bind HLSL name | `IndirectInstancingCore/Private/IndirectInstancingVertexFactory.cpp` | **66** | `ElevationPerSiteParameter.Bind(..., TEXT("ElevationPerSite"))` |
| Per-draw bind | same | **111–113** | `ShaderBindings.Add(ElevationPerSiteParameter, UserData->ElevationPerSiteSRV)` |
| Layout | same | **126** | `LAYOUT_FIELD(FShaderResourceParameter, ElevationPerSiteParameter)` |
| Declares buffer | `Shaders/Private/GeoVoronoiIndirectInstancing.ush` | **152** | `Buffer<float> ElevationPerSite` |
| Use in VS | `Shaders/Private/GeoVoronoiIndirectInstancingVertexFactory.ush` | **53–76** | `Elev = ElevationPerSite[Instance.SiteId]`; `Radius = PlanetRadius * (1 + Elev * ElevationScale)` |

**How to find it yourself:** search the plugin for `ElevationPerSite` (one grep shows every hit).

---
---

## 12. DF64 (double-float) — GPU position path (planned)

- **Purpose:** Keep **~48-bit effective precision** for planet-surface positions when combining **unit directions**, **planet radius** (large cm magnitude), and **huge actor / world translations**. Today `ReconstructPosition()` does `High + Low` into a single `float3`; that rounding drops the low word, so you only get ~FP32 on the reconstructed value — enough for many scenes, but not true df64 through **radius scale × LWC**.
- **Status:** **not implemented** in the repo shaders; the blocks below are the **intended** drop-in design (also archived with discussion in `Plugins/DelaunatorPlugin/Docs/Transcripts/voronoi planet followup.md`).
- **Key files:** `GeoVoronoiIndirectInstancing.ush` (helpers + `ReconstructPositionDF64`), `GeoVoronoiIndirectInstancingVertexFactory.ush` (intermediates, `GetVertexFactoryIntermediates`, `VertexFactoryGetWorldPosition`). Keep existing **`ReconstructPosition`** for normals / helpers that do not need df64.

### 12.1 Problem — current early collapse (`GeoVoronoiIndirectInstancing.ush`)

```hlsl
/** Reconstruct double-precision position from HighLow pair (as float math on GPU). */
float3 ReconstructPosition(FVector3_HighLow v)
{
    return float3(
        v.X.High + v.X.Low,
        v.Y.High + v.Y.Low,
        v.Z.High + v.Z.Low
    );
}
```

### 12.2 Add to `GeoVoronoiIndirectInstancing.ush` (above `ReconstructPosition`)

```hlsl
// =============================================================================
// df64 (double-float) helpers — keep ~48-bit precision through GPU math.
// =============================================================================

/** A df64 vector: full value per component = High + Low, with |Low| <= ulp(High)/2. */
struct FVector3_DF64
{
    float3 High;
    float3 Low;
};

/** TwoProduct: split a*b exactly into (high, low) so high+low == a*b, no rounding error.
 *  Requires IEEE FMA. SM6 has fma(); on SM5 mad() is *usually* fused but not guaranteed —
 *  if you need to be defensive, replace with a Dekker/Veltkamp split. */
void TwoProduct(float a, float b, out float hi, out float lo)
{
    hi = a * b;
    lo = mad(a, b, -hi);   // fma(a, b, -hi) on SM6
}

/** Renormalize a (hi, lo) pair so |lo| <= ulp(hi)/2.  Requires |a| >= |b| (use TwoSum if not). */
void FastTwoSum(float a, float b, out float hi, out float lo)
{
    hi = a + b;
    lo = b - (hi - a);
}

/** df64 * f32  ->  df64. Full ~48-bit precision in the result. */
FVector3_DF64 ScaleDF64(FVector3_DF64 v, float s)
{
    FVector3_DF64 r;
    UNROLL
    for (int i = 0; i < 3; ++i)
    {
        float ph, pl;
        TwoProduct(v.High[i], s, ph, pl);
        pl += v.Low[i] * s;            // residual is already small enough that f32 mul is fine
        FastTwoSum(ph, pl, r.High[i], r.Low[i]);
    }
    return r;
}

/** Reconstruct df64 position WITHOUT collapsing to f32. */
FVector3_DF64 ReconstructPositionDF64(FVector3_HighLow v)
{
    FVector3_DF64 r;
    r.High = float3(v.X.High, v.Y.High, v.Z.High);
    r.Low  = float3(v.X.Low,  v.Y.Low,  v.Z.Low);
    return r;
}
```

### 12.3 `GeoVoronoiIndirectInstancingVertexFactory.ush` — intermediates + `GetVertexFactoryIntermediates`

**Struct** — extend with df64 local position (planet-local, cm); keep `LocalPos` as f32 fallback for motion / non-critical paths:

```hlsl
struct FVertexFactoryIntermediates
{
    float2 LocalUV;
    float3 LocalPosHigh;   // df64 hi part (planet-local, in cm)
    float3 LocalPosLow;    // df64 lo part
    float3 LocalPos;       // f32 collapse; prev-frame / etc.
    float3 WorldNormal;
    float4 VertexColor;
    FSceneDataIntermediates SceneData;
};
```

**`GetVertexFactoryIntermediates`** — use df64 through radius scale; f32 only for `normalize(cross(...))`:

```hlsl
FVertexFactoryIntermediates GetVertexFactoryIntermediates(FVertexFactoryInput Input)
{
    FVertexFactoryIntermediates Intermediates;
    Intermediates.SceneData = VF_GPUSCENE_GET_INTERMEDIATES(Input);

    const QuadRenderInstance Instance = InstanceBuffer[Input.InstanceId];
    Intermediates.VertexColor = UnpackColorRGBA8(VoronoiCellColors[Instance.SiteId]);

    // df64 unit-sphere directions (range ~[-1,1], ~48-bit precision).
    const FVector3_DF64 V0_df = ReconstructPositionDF64(VoronoiGeoCenters[Instance.GeoCenterA]);
    const FVector3_DF64 V1_df = ReconstructPositionDF64(VoronoiGeoCenters[Instance.GeoCenterB]);
    const FVector3_DF64 V2_df = ReconstructPositionDF64(CBT_FibonacciPoints[Instance.SiteId]);

    // f32 versions — only used for the world normal, where ~24-bit precision is fine.
    const float3 V0 = V0_df.High + V0_df.Low;
    const float3 V1 = V1_df.High + V1_df.Low;
    const float3 V2 = V2_df.High + V2_df.Low;

    FVector3_DF64 VertexPos_df;
    if (Input.VertexId == 0)      VertexPos_df = V0_df;
    else if (Input.VertexId == 1) VertexPos_df = V1_df;
    else                          VertexPos_df = V2_df;

    // df64 scale by planet radius — preserves ~48-bit precision at planet-radius magnitude.
    const FVector3_DF64 LocalPos_df = ScaleDF64(VertexPos_df, GeoVoronoiIndirectInstancingParams.PlanetRadius);

    Intermediates.LocalPosHigh = LocalPos_df.High;
    Intermediates.LocalPosLow  = LocalPos_df.Low;
    Intermediates.LocalPos     = LocalPos_df.High + LocalPos_df.Low; // f32 fallback for non-precision-critical paths

    Intermediates.WorldNormal  = normalize(cross(V1 - V0, V2 - V0));

    const float2 BarycentricUVs[3] =
    {
        float2(0.0f, 0.0f),
        float2(1.0f, 0.0f),
        float2(0.0f, 1.0f)
    };
    Intermediates.LocalUV = BarycentricUVs[min(Input.VertexId, 2u)];

    return Intermediates;
}
```

### 12.4 `VertexFactoryGetWorldPosition` — LWC from df64 pair

`LWCPromote` from a single float loses when `|offset|` exceeds the LWC tile size (~21 km in cm). Split **High** into tile + remainder, then add **Low** into offset:

```hlsl
/** Build an FLWCVector3 from a df64 pair (both components in cm).
 *  Splits the High part into integer tiles + remainder so Offset stays small,
 *  then folds the Low part into Offset — full ~48-bit precision survives. */
FLWCVector3 MakeLWCVectorFromDF64(float3 High, float3 Low)
{
    const float TileSize = UE_LWC_RENDER_TILE_SIZE;     // 2^21 cm in UE5
    const float InvTileSize = 1.0 / TileSize;

    float3 Tile   = floor(High * InvTileSize);
    float3 Offset = (High - Tile * TileSize) + Low;     // remainder + lo; |Offset| ~< TileSize
    return MakeLWCVector3(Tile, Offset);
}

/** Computes the world space position of this vertex. */
float4 VertexFactoryGetWorldPosition(FVertexFactoryInput Input, FVertexFactoryIntermediates Intermediates)
{
    FPrimitiveSceneData PrimitiveData = GetPrimitiveDataFromUniformBuffer();
    FLWCMatrix    LocalToWorld   = PrimitiveData.LocalToWorld;

    // df64 -> LWC, then LWC * LWCMatrix (planet-actor offset goes here, fully precise).
    FLWCVector3   LocalPos_LWC   = MakeLWCVectorFromDF64(Intermediates.LocalPosHigh, Intermediates.LocalPosLow);
    FLWCVector3   WorldPosition  = LWCMultiply(LocalPos_LWC, LocalToWorld);

    // PreViewTranslation re-centers near the camera; the final f32 collapse is camera-relative.
    float3 TranslatedWorldPosition = LWCToFloat(LWCAdd(WorldPosition, ResolvedView.PreViewTranslation));
    return float4(TranslatedWorldPosition, 1.0f);
}
```

`VertexFactoryGetPreviousWorldPosition` can keep using `Intermediates.LocalPos` (f32) if motion vectors need no sub-mm precision.

**Caveats:** on SM5, `mad` may not be fused; prefer `fma` on SM6+ or a Dekker `TwoProduct` if you see regressions. Confirm `UE_LWC_RENDER_TILE_SIZE`, `MakeLWCVector3` against `Engine/Shaders/Private/LargeWorldCoordinates.ush` for your engine version.

### 12.5 CPU producer — must split doubles or df64 is pointless

```cpp
// Pseudocode: D is the authoritative double (or extended scalar).
float H = static_cast<float>(D);
float L = static_cast<float>(D - static_cast<double>(H));
// Store { H, L } per component into FVector3_HighLow for the GPU buffers.
```

If `Low` is always `0`, the shader df64 path does nothing useful — audit `VoronoiGeoCenters` / `CBT_FibonacciPoints` upload sites (`DelaunatorPlugin`, `Large_CBT`, etc.).

---
