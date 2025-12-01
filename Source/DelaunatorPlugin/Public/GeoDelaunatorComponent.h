// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Delaunator.h"
#include <sleef.h>
#include <functional>
#include <array>
#include "CBTStructs.h"
#include "GeoDelaunatorComponent.generated.h"


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


// Edge Key for Half-Edge Mesh building
struct FEdgeKey
{
	int32 A;
	int32 B;

	FEdgeKey() : A(0), B(0) {}
	FEdgeKey(int32 InA, int32 InB)
	{
		// store as sorted (min,max) so (a,b) and (b,a) map to same key
		if (InA < InB) { A = InA; B = InB; }
		else { A = InB; B = InA; }
	}

	bool operator==(const FEdgeKey& Other) const
	{
		return A == Other.A && B == Other.B;
	}
};

FORCEINLINE uint32 GetTypeHash(const FEdgeKey& Key)
{
	// simple hash: combine the two ints
	return HashCombine(::GetTypeHash(Key.A), ::GetTypeHash(Key.B));
}

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DELAUNATORPLUGIN_API UGeoDelaunatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UGeoDelaunatorComponent();

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	// PSEUDO-RNG
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	int64 RandomSeed{ 2236 };
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Terrain")
	FRandomStream RngStream;

	// FIBONACCI SPHERE
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "100", UIMin = "100"), Category = "GeoDelaunator")
	int32 N = 100; // number of Fibonacci points

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0", UIMin = "1.0"), Category = "GeoDelaunator")
	double PlanetRadius = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"), Category = "GeoDelaunator")
	double Jitter = 1.0;

	// DEBUG DRAW POINTS AND TRIANGLE LINES
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "4.0", UIMin = "4.0", ClampMax = "12.0", UIMax="12.0"), Category = "GeoDelaunator")
	float DebugPointScale = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0", UIMin = "1.0", ClampMax = "12.0", UIMax = "12.0"), Category = "GeoDelaunator")
	float DebugLineThickness = 1.f;

	UPROPERTY()
	UDelaunator* Delaunator = nullptr;

	TArray<FVector2D> LonLat;
	TArray<FVector> FibonacciPoints;
	std::vector<double> coords; // FOR DELAUNAYTOR
	TArray<FIntVector> SphericalTriangles;
	TArray<int32> SphericalTrisFlat;

	//STICHING
	TArray<FVector> PivotedPoints;

	TArray<FVector2D> Projected2D; // FibonacciPoints after Stereographic Projection
	TArray<int32> IndexMap; // map from ProjectedPoints to LonLat, dunno if needed

	// INTERPOLATION DRAW DEBUG
	float InterpolationT = 0.f;
	FQuat PivotToSouthQuat = FQuat::Identity;
	FTimerHandle THandle_Interpolate;
	void Timer_FibonacciInterpolation();

	// HALF EDGE DEBUG
	int32 TriToDraw{ 0 };
	int32 VertToDraw{ 0 };
	FTimerHandle THandle_HalfEdgeDebug;
	FORCEINLINE int32 IncrementVertID()
	{
		if (VertToDraw >= 0 && VertToDraw <= 2) VertToDraw++;
		if (VertToDraw > 2) VertToDraw = 0;
		return VertToDraw;
	};

	FORCEINLINE void Timer_HalfEdgeDebug()
	{
		int32 VertID = 0;
		int32 EndID = 0;
		if (VertToDraw == 0) { VertID = SphericalTriangles[TriToDraw].X; EndID = SphericalTriangles[TriToDraw].Y; }
		if (VertToDraw == 1) { VertID = SphericalTriangles[TriToDraw].Y; EndID = SphericalTriangles[TriToDraw].Z; }
		if (VertToDraw == 2) { VertID = SphericalTriangles[TriToDraw].Z; EndID = SphericalTriangles[TriToDraw].X; }
		FVector Vert = FibonacciPoints[VertID];
		FVector End = FibonacciPoints[EndID];
		DrawDebugPoint(GetWorld(), GetOwner()->GetActorLocation() + Vert*PlanetRadius, 12.f, FColor::Green);
		DrawDebugLine(GetWorld(), GetOwner()->GetActorLocation() + Vert * PlanetRadius, GetOwner()->GetActorLocation() + End * PlanetRadius, FColor::Blue, false, 1.f/2.f, 0, 12.f);
		IncrementVertID();
		if (VertToDraw == 2) TriToDraw++;
		if (TriToDraw >= SphericalTriangles.Num()) TriToDraw = 0;
	};

	// VORONOI
	TArray<FVector> VorVert3D;
	TArray<bool> TriValid;
	TArray<FVector2d> VorVert2D; // circumcenters indexed by tri
	int32 Pivot{ -1 };

	// CBT STRUCTURE
	uint32 D{ 16 }; // CBT Depth
	TArray<int32> HalfEdge_Mesh;
	TArray<FHalfEdge_CBT> HalfEdge_Buffer;

public:
	
	void GenerateFibonacciSphere1();
	void GenerateFibonacciSphere2();
	void GeoRotation(int32 PivotIndex);
	void StereographicProjection(TArray<FVector>& Points);
	FVector UnprojectVoronoiVertexToSphereAndInvertRotation(const FVector2d& V2D);

	// FOR DEBUGGING UNUSED VERTICES
	void CheckUnusedVertices();

	 //GEO-DELAUNAY
	void GeoDelauny();
	void GeoDelaunayFrom();

	// VORONOI
	// points are in your projection plane (same coords given to Delaunator)
	static bool ComputeCircumcenter2D(
		const FVector2d& A,
		const FVector2d& B,
		const FVector2d& C,
		FVector2d& OutCenter);


	// CBT STRUCTURE
	void BuildHalfedgeMesh();
	void BuildCBT();
};