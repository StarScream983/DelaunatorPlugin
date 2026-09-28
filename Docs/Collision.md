# Collision

Source: `UGeoDelaunatorComponent`

- Header: `Source/DelaunatorPlugin/Public/GeoDelaunatorComponent.h`
- Cpp: `Source/DelaunatorPlugin/Private/GeoDelaunatorComponent.cpp`

## Dependencies

### Header — `GeoDelaunatorComponent.h`

```cpp
#include "Components/PrimitiveComponent.h"              // UPrimitiveComponent, GetBodySetup, RecreatePhysicsState, BodyInstance
#include "Interfaces/Interface_CollisionDataProvider.h" // IInterface_CollisionDataProvider, FTriMeshCollisionData
#include "PhysicsEngine/BodySetup.h"                    // UBodySetup
```

Class must inherit the interface:

```cpp
class UYourClass : public UPrimitiveComponent, public IInterface_CollisionDataProvider, ...
```

### Cpp — `GeoDelaunatorComponent.cpp`

```cpp
#include "Engine/CollisionProfile.h"     // UCollisionProfile::BlockAll_ProfileName (constructor + UpdateBodySetup)
```

### Module — `DelaunatorPlugin.Build.cs`

Collision types live in **Engine**. Already listed under `PrivateDependencyModuleNames`:

```csharp
"Engine",
```

`PhysicsCore` / `Chaos` come in through Engine; no extra module for this path.

## Header

```cpp
	/*****************************************************************************
	*                                                                           *
	*                     COLLISION DATA PROVIDER INTERFACE                      *
	*                                                                           *
	*  BodySetup creation and triangle-mesh collision data used by Chaos        *
	*  for complex collision on the procedural spherical mesh.                  *
	*                                                                           *
	*  These overrides make the component act like a procedural collision       *
	*  source: the physics system asks for triangle data, and we provide        *
	*  it from the generated spherical Delaunay mesh.                           *
	*                                                                           *
	*****************************************************************************/
public:
	/** Returns the BodySetup used by Chaos for collision creation/caching. */
	virtual UBodySetup* GetBodySetup() override;

	/** Exports the generated spherical mesh as triangle collision data. */
	virtual bool GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;

	/** Returns whether this component currently has valid triangle collision data. */
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;

	/** We do not need mirrored negative-X collision data for this component. */
	virtual bool WantsNegXTriMesh() override { return false; }

	/** Creates/configures the BodySetup used for procedural triangle collision. */
	void UpdateBodySetup();

	/** Rebuilds collision after the procedural spherical mesh changes. */
	void UpdateCollision();

protected:
	/** Runtime BodySetup that stores cooked collision data for the generated mesh. */
	UPROPERTY(Transient)
	TObjectPtr<UBodySetup> MeshBodySetup = nullptr;
	/*****************************************************************************
	*                 END COLLISION DATA PROVIDER INTERFACE                      *
	*****************************************************************************/
```

## Cpp

Constructor (collision bits):

```cpp
	// Coarse Chaos collision (Delaunay shell) so pawns can stand on the planet.
	SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetCanEverAffectNavigation(false);
	BodyInstance.bSimulatePhysics = false;
	CanCharacterStepUpOn = ECB_Yes;
```

```cpp
/*****************************************************************************
*                                                                           *
*                     COLLISION DATA PROVIDER INTERFACE                      *
*                                                                           *
*  BodySetup creation and triangle-mesh collision data used by Chaos        *
*  for complex collision on the procedural spherical mesh.                  *
*                                                                           *
*  The generated Delaunay sphere already exists as CPU-side vertex/index     *
*  arrays (`FibonacciPoints` + `SphericalTriangles`). These functions are    *
*  the bridge between that procedural geometry and Unreal's collision        *
*  system.                                                                  *
*                                                                           *
*****************************************************************************/

UBodySetup* UGeoDelaunatorComponent::GetBodySetup()
{
	if (MeshBodySetup == nullptr)
	{
		UpdateBodySetup();
	}
	return MeshBodySetup;
}

bool UGeoDelaunatorComponent::GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
	if (!CollisionData || !ContainsPhysicsTriMeshData(InUseAllTriData))
	{
		return false;
	}

	// Same radial scale as GeoVoronoiIndirectInstancingVertexFactory.ush
	constexpr float CollisionElevationScale = 0.03f;
	const float BaseRadius = static_cast<float>(PlanetRadius);
	const int32 NumVerts = FibonacciPoints.Num();

	CollisionData->Vertices.Reset(NumVerts);
	for (int32 i = 0; i < NumVerts; ++i)
	{
		const float Elev = ElevationPerSite.IsValidIndex(i) ? ElevationPerSite[i] : 0.0f;
		const float Radius = BaseRadius * (1.0f + Elev * CollisionElevationScale);
		CollisionData->Vertices.Add(FVector3f(FibonacciPoints[i] * Radius));
	}

	CollisionData->Indices.Reset(SphericalTriangles.Num());
	for (const FIntVector& Tri : SphericalTriangles)
	{
		if (!CollisionData->Vertices.IsValidIndex(Tri.X)
			|| !CollisionData->Vertices.IsValidIndex(Tri.Y)
			|| !CollisionData->Vertices.IsValidIndex(Tri.Z))
		{
			continue;
		}

		FTriIndices OutTri;
		OutTri.v0 = Tri.X;
		OutTri.v1 = Tri.Y;
		OutTri.v2 = Tri.Z;

		// Outward winding: Chaos needs faces that block from outside the planet.
		const FVector3f A = CollisionData->Vertices[OutTri.v0];
		const FVector3f B = CollisionData->Vertices[OutTri.v1];
		const FVector3f C = CollisionData->Vertices[OutTri.v2];
		const FVector3f FaceN = FVector3f::CrossProduct(B - A, C - A);
		const FVector3f FaceCenter = (A + B + C) * (1.0f / 3.0f);
		if (FVector3f::DotProduct(FaceN, FaceCenter) < 0.0f)
		{
			Swap(OutTri.v1, OutTri.v2);
		}

		CollisionData->Indices.Add(OutTri);
	}

	CollisionData->bFlipNormals = false;
	CollisionData->bDeformableMesh = true;
	CollisionData->bFastCook = true;
	return CollisionData->Indices.Num() > 0;
}

bool UGeoDelaunatorComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	return FibonacciPoints.Num() >= 3 && SphericalTriangles.Num() > 0;
}

void UGeoDelaunatorComponent::UpdateBodySetup()
{
	if (MeshBodySetup == nullptr)
	{
		MeshBodySetup = NewObject<UBodySetup>(this, NAME_None, RF_Transient | RF_DuplicateTransient);
		MeshBodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
		MeshBodySetup->bMeshCollideAll = true;
		MeshBodySetup->bDoubleSidedGeometry = false;
	}

	MeshBodySetup->DefaultInstance.SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
}

void UGeoDelaunatorComponent::UpdateCollision()
{
	if (!ContainsPhysicsTriMeshData(true))
	{
		return;
	}

	const double T0 = FPlatformTime::Seconds();

	UpdateBodySetup();
	MeshBodySetup->InvalidatePhysicsData();
	MeshBodySetup->CreatePhysicsMeshes();

	if (IsRegistered())
	{
		RecreatePhysicsState();
	}

	UE_LOG(LogTemp, Warning, TEXT("GeoDelaunator collision cooked: %d verts, %d tris, %.2f ms"),
		FibonacciPoints.Num(),
		SphericalTriangles.Num(),
		(FPlatformTime::Seconds() - T0) * 1000.0);
}

/*****************************************************************************
*                 END COLLISION DATA PROVIDER INTERFACE                      *
*****************************************************************************/
```

---

## Explanation

Collision is the `IInterface_CollisionDataProvider` path: Chaos asks for a triangle mesh, we fill it from the Delaunay sphere, cook it, and attach it to the component. Call order after planet gen: `GeoDelaunayFrom()` → `UpdateCollision()`.

---

### Constructor (`UGeoDelaunatorComponent`)

Not a collision function, but it turns the primitive into a blocker:

- `BlockAll` — hits pawn, camera, world traces
- `QueryAndPhysics` — both line traces and physics
- `bSimulatePhysics = false` — static world, not a rigid body
- `CanCharacterStepUpOn = Yes` — characters can stand on it

Without this, cooking a mesh still would not block the explorer.

---

### `GetBodySetup()`

Engine entry: “what collision asset does this component use?”

Creates `MeshBodySetup` via `UpdateBodySetup()` if missing, then returns it. `UPrimitiveComponent` uses this to create the physics state. Returning `nullptr` (the old stub) meant **no collision**.

---

### `ContainsPhysicsTriMeshData(bool)`

Yes/no: do we have a mesh to cook?

True if there are at least 3 Fibonacci verts and at least one Delaunay triangle. `InUseAllTriData` is unused (ProceduralMesh leftover).

`CreatePhysicsMeshes()` calls this first; if false, nothing is cooked.

---

### `WantsNegXTriMesh()` (header)

Always `false`. Old PhysX mirrored some meshes in −X. A sphere does not need that.

---

### `GetPhysicsTriMeshData(FTriMeshCollisionData*)`

This is the mesh export. Chaos calls it while cooking.

1. **Vertices** — each `FibonacciPoints[i]` is a unit-sphere direction. Same formula as the shader:

   `Radius = PlanetRadius * (1 + ElevationPerSite[i] * 0.03)`
   `Vertex = direction * Radius`

2. **Indices** — copy `SphericalTriangles`. Skip bad indices.

3. **Winding** — if face normal points inward (`dot(N, faceCenter) < 0`), swap v1/v2 so Chaos blocks from **outside**.

4. **Flags**
   - `bFlipNormals = false` — winding already outward
   - `bDeformableMesh = true` — runtime cook, not a static `.uasset`
   - `bFastCook = true` — faster, slightly looser cook

Returns true if at least one triangle was added.

This is **Delaunay**, not the Voronoi render mesh. Close enough to walk on; not pixel-identical to the terraces.

---

### `UpdateBodySetup()`

Creates/configures the `UBodySetup` object (the cook container), not the triangles.

- `NewObject` owned by the component, transient
- `CTF_UseComplexAsSimple` — no capsule/sphere simple shape; the triangle mesh is used for **all** queries (pawn included)
- `bMeshCollideAll` — collide with everything that queries it
- `bDoubleSidedGeometry = false` — one-sided shell (outside only)
- `DefaultInstance` = `BlockAll`

---

### `UpdateCollision()`

Rebuild after the planet mesh changes. Called at the end of `GeoDelaunayFrom()`.

1. Bail if no tri mesh
2. Ensure body setup exists
3. `InvalidatePhysicsData()` — drop the previous cook
4. `CreatePhysicsMeshes()` — Chaos calls `ContainsPhysicsTriMeshData` + `GetPhysicsTriMeshData`, cooks acceleration structures
5. `RecreatePhysicsState()` if already registered — attach the new cook to the running component (BeginPlay already registered, then `GeoDelauny()` runs)
6. Log vert/tri counts and cook time

---

### How they connect

```text
BeginPlay
  GeoDelauny() → GeoDelaunayFrom()
    AssignElevations / mesh arrays filled
    UpdateCollision()
      UpdateBodySetup()
      CreatePhysicsMeshes()
        ContainsPhysicsTriMeshData?  → GetPhysicsTriMeshData()
        Chaos cooks MeshBodySetup
      RecreatePhysicsState()
        GetBodySetup() → MeshBodySetup
        component BlockAll + QueryAndPhysics
```

After that, the explorer capsule hits the elevated Delaunay shell. The GPU Voronoi mesh is **not** in Chaos; only this CPU triangle mesh is.

