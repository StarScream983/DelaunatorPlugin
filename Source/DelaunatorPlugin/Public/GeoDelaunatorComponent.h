// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
//COLLISION
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "PhysicsEngine/BodySetup.h"

#include "Delaunator.h"
#include <sleef.h>
#include <functional>
#include <array>
#include "CBTStructs.h"
#include "GeoDelaunatorComponent_Interface.h"
#include "GeoDelaunatorComponent.generated.h"

class FCBTResource_Interface;
class UMaterialInterface;

#define _PI UE_DOUBLE_PI
#define _TAU UE_DOUBLE_TWO_PI
#define DEGREES (180. / UE_DOUBLE_PI)
#define RADIANS (UE_DOUBLE_PI / 180.)


struct FRotation
{
	std::function<FVector2D(FVector2D)> forward; // like rotate(?, ?)
	std::function<FVector2D(FVector2D)> invert;  // like rotate.invert(?, ?)
};

// Wrap lambda to [-PI, PI]
static inline double WrapPi(double lambda)
{
	if (FMath::Abs(lambda) > _PI)
		lambda -= FMath::RoundToDouble(lambda / _TAU) * _TAU;
	return lambda;
}

static inline FRotation Compose(const FRotation& a, const FRotation& b)
{
	FRotation c;

	// forward: a ? b
	c.forward = [a, b](FVector2D p) {
		return b.forward(a.forward(p));
		};

	// invert: b^{-1} ? a^{-1}   (only if both inverses exist)
	if (a.invert && b.invert)
	{
		c.invert = [a, b](FVector2D p) {
			/*first undo 'a', then undo 'b'’s input mapping order from JS:
			x = b.invert(x,y); x && a.invert(x[0], x[1])
			x && ... is a short - circuit check :
			if x is truthy(an array like[?, ?]), call a.invert(x[0], x[1]);
			else skip it and return x(likely null).*/
			FVector2D q = b.invert(p);
			return a.invert ? a.invert(q) : q; // safety; mirrors JS guard
			};
	}
	else
	{
		// leave c.invert empty if either a/b has no invert
		c.invert = {};
	}

	return c;
}

struct FGeoRotation
{

private:
	FRotation RotateR; // radians-domain rotation (forward + invert)

public:
	FGeoRotation() = default;

	// pivot in DEGREES, just like d3.geoRotation(pivot)
	explicit FGeoRotation(FVector2D pivotDeg)
	{
		RotateR = RotateRadians(pivotDeg.X * RADIANS, pivotDeg.Y * RADIANS, 0.0);
	}

	// forward: degrees in → degrees out
	FVector2D Forward(FVector2D coordinatesDeg) const
	{
		FVector2D rad = coordinatesDeg * RADIANS;
		FVector2D out = RotateR.forward ? RotateR.forward(rad) : rad;
		return out * DEGREES;
	}

	// invert: degrees in → degrees out (this is what we'll actually use)
	FVector2D Invert(FVector2D coordinatesDeg) const
	{
		FVector2D rad = coordinatesDeg * RADIANS;
		FVector2D out = RotateR.invert ? RotateR.invert(rad) : rad;
		return out * DEGREES;
	}

	static FRotation RotationIdentity() {
		FRotation r;
		r.forward = [](FVector2D p) { return FVector2D(WrapPi(p.X), p.Y); };
		r.invert = r.forward;
		return r;
	}

	static FRotation RotateRadians(double deltaLambda, double deltaPhi, double deltaGamma = 0.0)
	{
		deltaLambda = FMath::Fmod(deltaLambda, _TAU);

		if (deltaLambda != 0.0)
		{
			if (deltaPhi != 0.0 || deltaGamma != 0.0)
				return Compose(RotationLambda(-deltaLambda), RotationPhiGamma(deltaPhi, deltaGamma));
			else
				return RotationLambda(-deltaLambda);
		}
		else
		{
			if (deltaPhi != 0.0 || deltaGamma != 0.0)
				return RotationPhiGamma(deltaPhi, deltaGamma);
			else
				return RotationIdentity();
		}
	}

	static FRotation RotationLambda(double deltaLambda) {
		FRotation r;
		// lambda = p.X, phi = p.Y
		r.forward = [=](FVector2D p) { return FVector2D(WrapPi(p.X + deltaLambda), p.Y); };
		r.invert = [=](FVector2D p) { return FVector2D(WrapPi(p.X - deltaLambda), p.Y); };
		return r;
	}

	static FRotation RotationPhiGamma(double deltaPhi, double deltaGamma = 0.0)
	{
		// Precompute once (radians)
		const double cosDeltaPhi = Sleef_cos_u10(deltaPhi);
		const double sinDeltaPhi = Sleef_sin_u10(deltaPhi);
		const double cosDeltaGamma = Sleef_cos_u10(deltaGamma); // replace by 1.0
		const double sinDeltaGamma = Sleef_sin_u10(deltaGamma); // replace by 0.0

		FRotation rPhiGamma;

		// forward
		rPhiGamma.forward = [=](FVector2D coordinates)
			{
				const double lambda = coordinates.X;
				const double phi = coordinates.Y;

				const double cosPhi = Sleef_cos_u10(phi);
				const double x = Sleef_cos_u10(lambda) * cosPhi;
				const double y = Sleef_sin_u10(lambda) * cosPhi;
				const double z = Sleef_sin_u10(phi);

				const double k = z * cosDeltaPhi + x * sinDeltaPhi;

				const double lambdaOut = Sleef_atan2_u10(
					y * cosDeltaGamma - k * sinDeltaGamma,
					x * cosDeltaPhi - z * sinDeltaPhi
				);
				const double phiOut = Sleef_asin_u10(k * cosDeltaGamma + y * sinDeltaGamma); // SClampUnit -1.0, 1.0

				return FVector2D(lambdaOut, phiOut);
			};

		// invert
		rPhiGamma.invert = [=](FVector2D coordinates)
			{
				const double lambda = coordinates.X;
				const double phi = coordinates.Y;

				const double cosPhi = Sleef_cos_u10(phi);
				const double x = Sleef_cos_u10(lambda) * cosPhi;
				const double y = Sleef_sin_u10(lambda) * cosPhi;
				const double z = Sleef_sin_u10(phi);

				const double k = z * cosDeltaGamma - y * sinDeltaGamma;

				const double lambdaOut = Sleef_atan2_u10(
					y * cosDeltaGamma + z * sinDeltaGamma,
					x * cosDeltaPhi + k * sinDeltaPhi
				);
				const double phiOut = Sleef_asin_u10(k * cosDeltaPhi - x * sinDeltaPhi); // SClampUnit -1.0, 1.0

				return FVector2D(lambdaOut, phiOut);
			};

		return rPhiGamma;
	}
};

struct MyStruct {
public:
	int a;
	double b;

	// Define a member function that returns a std::function
	std::function<void()> getFunction() {
		int da = 2;
		double db = 2.1;
		// [=, this] Captures member variables 'a' and 'b' of the struct and da, db in getFunction by value in the lambda
		return [=, this]() {
			// Inside the lambda, you can access 'a' and 'b' directly
			//UE_LOG(LogTemp, Warning, TEXT("a: %d, b: %f"), a+da, b+db);
			};
	}
};

// STRUCT TO HOLD VORONOI POLYGONS AND CIRCUMCENTERs - USED IN GEO_POLYGONS
struct FGeoPolygonResult {
	TArray<TArray<int32>> Polygons; // site index -> list of CCW triangle indices (into Circumcenters)
	TArray<FVector3d> Centers; // final augmented circumcenters
	TArray<FVector3_HighLow> Centers_HL; // final augmented circumcenters_HL for GPU

	// GPU FRIENDLY GEO MESH
	TArray<FUintVector2> VoronoiGeoMesh_Ranges;
	TArray<int32> VoronoiGeoMesh_Flat;
};

// STRUCT TO HOLD REVERSE HALF-EDGE MAPPING
struct FReverseHE {
	int32 From; // index of starting vertex in forward half-edge
	int32 HalfEdgeIndex; // index into HalfEdges array
};

struct FVoronoiHalfEdge {
	int32 VHE_Start; // CC and SphericalTriangle
	int32 VHE_End;	 // CC and SphericalTriangle
	int32 Start_Face; // face index for HE_Start
	int32 End_Face;  // face index for HE_End
	// maybe add IDs of start and end into spherical triangles

	FVoronoiHalfEdge() : VHE_Start(-1), VHE_End(-1), Start_Face(-1), End_Face(-1) {}
	FVoronoiHalfEdge(int32 InStart, int32 InEnd, int32 InStartFace, int32 InEndFace)
		: VHE_Start(InStart), VHE_End(InEnd), Start_Face(InStartFace), End_Face(InEndFace) {
	}
};

// STRUCT TO HOLD PLATE DATA, PURE CPU FOR NOW, MAYBE SPLIT INTO CPU/GPU VERSIONS LATER
USTRUCT()
struct FPlateData
{
	GENERATED_BODY()

	FPlateData() = default;
	FPlateData(int32 InSeedSite)
		: SeedSite(InSeedSite), bIsOceanic(false), DesiredElevation(0.0), DriftDirection(FVector::ZeroVector), DriftSpeed(0.0) {}
	FPlateData(int32 InSeedSite, bool InIsOceanic, double InDesiredElevation, FVector InDriftDirection, double InDriftSpeed)
		: SeedSite(InSeedSite), bIsOceanic(InIsOceanic), DesiredElevation(InDesiredElevation), DriftDirection(InDriftDirection), DriftSpeed(InDriftSpeed) {}

	int32   SeedSite;
	bool    bIsOceanic;

	// Gainey-style: base resting elevation for this plate
	// Oceanic:     [-0.8, -0.3]  (deep ocean to shallow sea)
	// Continental: [ 0.1,  0.6]  (lowlands to high plateau)
	double  DesiredElevation;

	// Step 2 (next session)
	FVector DriftDirection;
	double  DriftSpeed;

	uint32 PackedColor;
};
USTRUCT()
struct FPlateBoundary
{
	GENERATED_BODY()

	int32  SiteA = -1;   // site on plate A
	int32  SiteB = -1;   // site on plate B (different plate)
	int32  PlateA = -1;   // seed site id of plate A
	int32  PlateB = -1;   // seed site id of plate B

    // Global flat indices into FHalfEdge_CBT array
	int32  HalfEdgeAB = -1;   // SitePrefixSums[SiteA] + local ring idx → SiteB
	int32  HalfEdgeBA = -1;   // SitePrefixSums[SiteB] + local ring idx → SiteA 

	double Pressure  = 0.0;  // dot(RelativeMotion, BoundaryNormal),  >0 = converge, <0 = diverge
    double Shear     = 0.0;  // |dot(RelativeMotion, BoundaryTangent)|, >=0
	double Elevation = 0.0;  // computed boundary elevation
};

/** VertexColor / debug visualization for the Voronoi planet vertex factory (`ColorDebugMode` in HLSL). */
UENUM(BlueprintType)
enum class EGeoVoronoiPlanetColorDebug : uint8
{
	/** `VoronoiCellColors` (plate-tinted cells). */
	VoronoiPlateColors = 0,
	/** Pseudocolor from `ElevationPerSite`. */
	ElevationHeatmap = 1,
	/** Deterministic RGB from site index. */
	SiteIdHash = 2,
	OceanLandMask = 3,       // Red Blob 1843 `colormap.js` elevation colors (ocean depth + land→white peaks)
	DistanceToBoundary = 4, // heatmap of BFS distance — confirms mountain shapes
};

UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, Object, Physics, VirtualTexture))
class DELAUNATORPLUGIN_API UGeoDelaunatorComponent : public UPrimitiveComponent, public IInterface_CollisionDataProvider, public IGeoDelaunatorComponent_Interface
{
	GENERATED_UCLASS_BODY()

public:	
	// Sets default values for this component's properties
	//UGeoDelaunatorComponent();

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void BeginDestroy() override; // better to release the CBTResources

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


	/*****************************************************************************
	*                                                                           *
	*              INDIRECT INSTANCING PRIMITIVE COMPONENT                       *
	*                                                                           *
	*  Material, SceneProxy, Bounds, and UPrimitiveComponent overrides needed    *
	*  for indirect instanced rendering via FDelaunatorIndirectInstancingSceneProxy. *
	*                                                                           *
	*****************************************************************************/
protected:
	/** Material applied to each instance. */
	UPROPERTY(EditAnywhere, Category = Rendering)
	UMaterialInterface* Material = nullptr;

public:
	UMaterialInterface* GetMaterial() const { return Material; }
protected:
	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	//~ End UActorComponent Interface

	//~ Begin USceneComponent Interface
	virtual bool IsVisible() const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface

	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual bool SupportsStaticLighting() const override { return true; }
	virtual void SetMaterial(int32 ElementIndex, class UMaterialInterface* InMaterial) override;
	virtual UMaterialInterface* GetMaterial(int32 Index) const override { return Material; }
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	//~ End UPrimitiveComponent Interface
	/*****************************************************************************
	*          END INDIRECT INSTANCING PRIMITIVE COMPONENT                       *
	*****************************************************************************/


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

protected:

	// CUBEMAP SPATIAL BINNING

	// PSEUDO-RNG
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	int64 RandomSeed{ 2236 };
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Terrain")
	FRandomStream RngStream;

	// FIBONACCI SPHERE
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "100", UIMin = "100"), Category = "GeoDelaunator") 
	int32 N = 100; // number of Fibonacci points

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "100.0", UIMin = "100.0"), Category = "GeoDelaunator")
	double PlanetRadius = 3000.0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0", UIMin = "1.0"), Category = "GeoDelaunator")
	double MaxElevation = 10.0; // Everest ~ 8.8km, Mariana ~ -11km

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"), Category = "GeoDelaunator")
	double Jitter = 1.0;

	// DEBUG DRAW POINTS AND TRIANGLE LINES
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "4.0", UIMin = "4.0", ClampMax = "12.0", UIMax="12.0"), Category = "GeoDelaunator")
	float DebugPointScale = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0", UIMin = "1.0", ClampMax = "12.0", UIMax = "12.0"), Category = "GeoDelaunator")
	float DebugLineThickness = 1.f;

	/** Per-cell color mode for indirect planet rendering (mirrored each tick into `PlanetColorDebugShaderValue` for the render thread). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GeoDelaunator|Debug")
	EGeoVoronoiPlanetColorDebug PlanetColorDebug = EGeoVoronoiPlanetColorDebug::VoronoiPlateColors;

	UPROPERTY()
	UDelaunator* Delaunator = nullptr;

	TArray<FVector2D> LonLat;
	TArray<FVector> FibonacciPoints; // BUFFER FOR TRIANGLES VERTICES
	TArray<uint32> VoronoiCellColors; // random colors for Voronoi cells, generated on CPU and sent to GPU for rendering
	TArray<FVector3_HighLow> FibonacciPoints_HL; // HIGH-LOW BUFFER FOR GPU TRIANGLE VERTICES
	std::vector<double> coords; // FOR DELAUNAYTOR
	TArray<FIntVector> SphericalTriangles; // TRANSIENT - USED IN ORIGINAL CODE
	TArray<int32> SphericalTrisFlat; // TRIANGLES BUFFER
	TArray<TArray<FReverseHE>> ReverseEdgesHash; // TRANSIENT - NOT TO BE SAVED - USED IN GEO_POLYGONS
	TArray<int32> SphericalHalfEdges; // BUFFER FOR FINDING TRIANGLES WITHOUT LOOPING


	TArray<FVector2D> Projected2D; // FibonacciPoints after Stereographic Projection
	TArray<int32> IndexMap; // map from ProjectedPoints to LonLat, dunno if needed

	//STICHING
	TArray<FVector> PivotedPoints;

	// TEMPORARY ROTATION QUAT - PREVIOUSLY STORED FOR DEBUGGING - NEEDS TO BE REMOVED LATER 
	FQuat PivotToSouthQuat = FQuat::Identity;

	// VORONOI
	TArray<int32> SitePrefixSums; // prefix sum (stack) of previous Voronoi polygon sizes
	TArray<TArray<FVoronoiHalfEdge>> VoronoiHalfEdges_Map; // LookUp table for CBT half-edge buffer
	TArray<TArray<int32>> VoronoiGeoMesh; // site index -> list of CCW triangle indices into Voronoi Sites (AKA VoronoiGeoCenters)
	TArray<FVector> VoronoiGeoCenters; // CBT VERTEX BUFFER --- a copy of circumcenters, possibly with extra points appended — they are the same base data.But centers can grow later
	TArray<FVector3_HighLow> VoronoiGeoCenters_HL; // HIGH-LOW BUFFER FOR GPU VORONOI GEO CENTERS

	// VORONOI GEO MESH FLAT - GPU FRIENDLY
	TArray<FUintVector2> VoronoiGeoMesh_Ranges;
	TArray<int32> VoronoiGeoMesh_Flat;


	// CBT STRUCTURE
	uint32 D{ 16 }; // CBT Depth
	TArray<FHalfEdge_CBT> HalfEdge_Buffer;
	TArray<FRootBisector_CBT> RootBisectors_Buffer;
	TArray<int32> CBT_Buffer;
	/*int32 AllocationCounter_Buffer = 0;
	TArray<FPointer_CBT> Pointer_Buffer;*/

	TSharedPtr<FCBTResource_Interface> CBTResources;

	/** Game thread: `SyncPlanetColorDebugToRenderThread`; render thread: `GetPlanetColorDebugShaderValue_RenderThread`. */
	TAtomic<uint32> PlanetColorDebugShaderValue;

public:
	FORCEINLINE float GetPlanetRadius() const { return (float)PlanetRadius; }
	FORCEINLINE TSharedPtr<FCBTResource_Interface> GetCBTResources() const { return CBTResources; }

	//~ Begin IGeoDelaunatorComponent_Interface
	virtual uint32 GetPlanetColorDebugShaderValue_RenderThread() const override;
	//~ End IGeoDelaunatorComponent_Interface

	/** Call from game thread (Tick / property changes) so the scene proxy can read a stable value on the render thread. */
	FORCEINLINE void SyncPlanetColorDebugToRenderThread()
	{
		PlanetColorDebugShaderValue = static_cast<uint32>(PlanetColorDebug);
	}

public:
	
	void GenerateFibonacciSphere1();
	void GenerateFibonacciSphere2();
	void GeoRotation(int32 PivotIndex);						// NEEDS TO BE MERGED WITH STEREOGRAPHIC PROJECTION
	void StereographicProjection(TArray<FVector>& Points);	// NEEDS TO MERGE GEOROTATION
	// DEPRECATED --- TO BE REMOVED LATER
	FVector UnprojectVoronoiVertexToSphereAndInvertRotation(const FVector2d& V2D);

	 //GEO-DELAUNAY
	void GeoDelauny();
	void GeoDelaunayFrom();

	// VORONOI
	void Geo_Circumcenters(TArray<FVector>& Circumcenters, TArray<FVector3_HighLow>& Circumcenters_HL);
	void Geo_Centroids(TArray<FVector>& Circumcenters, TArray<FVector3_HighLow>& Circumcenters_HL);
	FGeoPolygonResult Geo_Polygons(TArray<FVector>& Circumcenters, TArray<FVector3_HighLow>& Circumcenters_HL);
	// Optional midpoint helper
	FORCEINLINE FVector3d SphericalMidpoint(const FVector3d& A, const FVector3d& B, const FVector3d& RefCenter)
	{
		FVector3d Mid = (A + B).GetSafeNormal();
		if (Mid.Dot(RefCenter) < 0.0) Mid *= -1.0; // ensure same hemisphere
		return Mid;
	}

	// CBT STRUCTURE
	// 100000 sites => 599988 half-edges

protected:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.2", UIMin = "0.2", ClampMax = "0.9", UIMax="0.9"), Category = "GeoDelaunator")
	double OceanicRatio = 0.47;

	// TECTONIC PLATES
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GeoDelaunator")
	int32 NumPlates = 24;

	TArray<int32> PlateIdPerSite;     // size = FibonacciPoints.Num(), value = plate index into `Plates`
	TArray<FPlateData> Plates;  // chosen root sites, replaces PlateSeeds
	TArray<FPlateBoundary> PlateBoundaries;
	TArray<int32> SiteParent;  // -1 = seed (root), else = the site that propagated into this one
	TArray<float> ElevationPerSite;        // [-1 = deep ocean, +1 = high mountain]
	// How many hops each site is from its nearest boundary, INT32_MAX = not yet visited (used as the "unvisited" sentinel)
	TArray<int32> DistanceToBoundary;      // useful later for moisture + rivers
	TArray<uint32> PlateDebugColors;  // optional packed color per site

	void GeneratePlates_RedBlobRandomFill();
	void GetVoronoiNeighbors(int32 SiteIndex, TArray<int32>& OutNeighbors, TArray<int32>& OutHalfEdgeIndices) const;
	// add fisher-yates shuffle to randomize the order of neighbors and avoid similar plate IDs
	TArray<int32> PickRandomPlateSeeds(int32 Count, TArray<FPlateData>& OutSeeds);
	TArray<int32> GetAncestorChain(int32 StartSite) const;
	double  ComputeBoundaryElevation(const FPlateBoundary& Boundary, const FPlateData& PlateA, const FPlateData& PlateB);
	double  ComputeBoundaryElevation_Gainey(const FPlateBoundary& Boundary, const FPlateData& PlateA, const FPlateData& PlateB);
	double  ComputeBoundaryElevation_Hybrid(const FPlateBoundary& Boundary, const FPlateData& PlateA, const FPlateData& PlateB);
	double  ComputeBoundaryElevation_Hybrid2(const FPlateBoundary& Boundary, const FPlateData& PlateA, const FPlateData& PlateB);

	// Mirror of Gainey's blurPlateBoundaryStress(): smooth Pressure/Shear across
	// adjacent PlateBoundaries before they get classified into elevation regimes.
	// Removes high-frequency speckle that otherwise creates single-cell spikes when
	// neighbouring boundary edges fall on opposite sides of the |x| > 0.3 threshold.
	void BlurBoundaryStress(int32 Iterations, double CenterWeight);

	/**
	 * Red Blob Games 1843 `assignRegionElevation` (planet-generation.js). Writes into `OutElevation`
	 * (pass `ElevationPerSite` from `AssignElevations_RedBlob1843` if you want the main height buffer).
	 */
	void AssignElevationFromRedBlob1843(TArray<float>& OutElevation);
	void RedBlobAssignDistanceField(const TSet<int32>& Seeds, const TSet<int32>& StopBlocks, TArray<float>& OutDist);

	/** BFS hops from `PlateBoundaries` → `DistanceToBoundary` (for Red Blob path; Gainey fills this inside its own BFS). */
	void FillDistanceToBoundaryBFS();

	/** Alternative pipeline: blur stress → Red Blob 1843 heightfield → boundary-distance BFS. Same `ElevationPerSite` + `DistanceToBoundary` as Gainey. */
	void AssignElevations_RedBlob1843();

	void AssignElevations();

	uint32 BuildPackedColor(const int32 PlateIndex) const;
	void BuildPlateDebugColors();

protected:
	// TECTONIC PLATES WITH WARP: PlateScore = BaseGrowth + Warp1 * OwnershipWarpStrength + RandomBias


// New warp controls
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GeoDelaunator|Plates")
	float PlateWarpStrength = 0.25f;   // how far positions are displaced

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GeoDelaunator|Plates")
	float PlateWarpFrequency = 0.0002f; // scale relative to world units (planet radius ~500–6000)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GeoDelaunator|Plates")
	int32 PlateWarpOctaves = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GeoDelaunator|Plates")
	float PlateWarpLacunarity = 2.0f;  // frequency multiplier per octave

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GeoDelaunator|Plates")
	float PlateWarpGain = 0.5f;        // amplitude multiplier per octave

	/*void GeneratePlates_NearestNeighbor();
	void GeneratePlates_NearestNeighbor_DomainWarped();*/
};