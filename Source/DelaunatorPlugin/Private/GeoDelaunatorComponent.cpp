// Fill out your copyright notice in the Description page of Project Settings.


#include "GeoDelaunatorComponent.h"
#include <string>
#include <iostream>

// Sets default values for this component's properties
UGeoDelaunatorComponent::UGeoDelaunatorComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}


// Called when the game starts
void UGeoDelaunatorComponent::BeginPlay()
{
	Super::BeginPlay();

	RngStream.Initialize(RandomSeed);
	GeoDelauny();
}

void UGeoDelaunatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	GetWorld()->GetTimerManager().ClearTimer(THandle_Interpolate);
}


// Called every frame
void UGeoDelaunatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// ...
	FVector Location = GetOwner()->GetActorLocation();
	
	for(int32 i = 0; i < N; i++)
	{
		//UE_LOG(LogTemp, Warning, TEXT("Point: %s"), *point.ToString());
		DrawDebugPoint(GetWorld(), Location + FibonacciPoints[i] * PlanetRadius, DebugPointScale, FColor::Red);
		//DrawDebugPoint(GetWorld(), Location + PivotedPoints[i] * PlanetRadius, DebugPointScale, FColor::Blue);
		//if (i < 999) DrawDebugPoint(GetWorld(), Location + FVector(Projected2D[i].X, Projected2D[i].Y, Location.Z)*PlanetRadius, DebugPointScale, FColor::Magenta);
	}

	for (const FIntVector& tri : SphericalTriangles)
	{
		const FVector A = FibonacciPoints[tri.X] * PlanetRadius + Location;
		const FVector B = FibonacciPoints[tri.Y] * PlanetRadius + Location;
		const FVector C = FibonacciPoints[tri.Z] * PlanetRadius + Location;

		DrawDebugLine(GetWorld(), A, B, FColor::Yellow, false, 0.f, 0, DebugLineThickness);
		DrawDebugLine(GetWorld(), B, C, FColor::Yellow, false, 0.f, 0, DebugLineThickness);
		DrawDebugLine(GetWorld(), C, A, FColor::Yellow, false, 0.f, 0, DebugLineThickness);
	}
}

void UGeoDelaunatorComponent::Timer_FibonacciInterpolation()
{
	UE_LOG(LogTemp, Warning, TEXT("InterpolationT = %d"), InterpolationT);
	FVector Location = GetOwner()->GetActorLocation();

	float t = FMath::Clamp(InterpolationT, 0.f, 1.f);

	// Slerp between identity and pivot-rotation
	FQuat Rot = FQuat::Slerp(FQuat::Identity, PivotToSouthQuat, t);

	for(int32 i = 0; i < N; ++i)
	{
		const FVector P = FibonacciPoints[i];
		const FVector Rotated = Rot.RotateVector(P);
		DrawDebugPoint(GetWorld(), Location + Rotated * PlanetRadius, 4.f, FColor::Green);
	}
	InterpolationT += 0.01f;
}

void UGeoDelaunatorComponent::GenerateFibonacciSphere1()
{
	FibonacciPoints.Empty();
	FibonacciPoints.Reserve(N);
	LonLat.Empty();
	LonLat.Reserve(N);

	// First algorithm from RedBlob / CGA FAQ
	const double s = 3.6 / Sleef_sqrt_u05((double)N);
	const double dz = 2.0 / (double)N;

	double z = 1.0 - dz * 0.5;
	double lon = 0.0;

	// Deterministic jitter
	FRandomStream Rng(123456);

	for (int32 k = 0; k < N; ++k, z -= dz)
	{
		// Base spherical band
		double r = Sleef_sqrt_u05(FMath::Max(0.0, 1.0 - z * z));
		double lat = Sleef_asin_u10(z);   // radians
		double lng = lon;                 // radians

		if (Jitter > 0.0)
		{
			const double randLat = (double)Rng.GetFraction() - (double)Rng.GetFraction();
			const double randLon = (double)Rng.GetFraction() - (double)Rng.GetFraction();

			// Lat jitter (matches JS exactly)
			const double z2 = FMath::Max(-1.0, z - dz * 2.0 * _PI * r / s);
			const double latMin = Sleef_asin_u10(z2);

			lat += Jitter * randLat * (lat - latMin);

			// Lon jitter
			const double safeR = (r > 1e-12) ? r : 1e-12;
			lng += Jitter * randLon * (s / safeR);
		}

		// Convert to degrees (repo structure: LonLat stores degrees)
		const double latDeg = lat * DEGREES;
		double lonDeg = lng * DEGREES;

		lonDeg = FMath::Fmod(lonDeg, 360.0);
		if (lonDeg < 0.0) lonDeg += 360.0;

		LonLat.Add(FVector2D(lonDeg, latDeg));

		// Convert to Cartesian (unit sphere), consistent with repo patterns
		const double cosLat = Sleef_cos_u10(lat);
		const double x = cosLat * Sleef_cos_u10(lng);
		const double y = Sleef_sin_u10(lat);
		const double zCart = cosLat * Sleef_sin_u10(lng);

		FibonacciPoints.Add(FVector(x, y, zCart));

		// RedBlob increment
		const double safeR2 = (r > 1e-12) ? r : 1e-12;
		lon += s / safeR2;
	}
}

void UGeoDelaunatorComponent::GenerateFibonacciSphere2()
{
	if (N <= 99) N = 100; // force minimum points 100

	FibonacciPoints.Empty();
	FibonacciPoints.Reserve(N);
	LonLat.Empty();
	LonLat.Reserve(N); // pre-allocates memory for N elements up front.
	const double dlong = _PI * (3.0 - Sleef_sqrt_u05(5.0)); // golden angle
	const double dz = 2.0 / N;
	double z = 1.0 - dz * 0.5;
	double lon = 0.0;

	for (int32 k = 0; k < N; ++k)
	{
		const double r = Sleef_sqrt_u05(1.0 - z * z);
		const double lat = Sleef_asin_u10(z);

		// Convert to degrees
		const double lonDeg = lon * (DEGREES);
		const double latDeg = lat * (DEGREES);

		LonLat.Add(FVector2D(lonDeg, latDeg));
		FibonacciPoints.Add(FVector(r * Sleef_cos_u10(lon), r * Sleef_sin_u10(lon), z));

		lon += dlong;
		z -= dz;
	}
}

void UGeoDelaunatorComponent::GeoRotation(int32 PivotIndex)
{
	PivotedPoints.Empty();
	PivotedPoints.Reserve(N);

	FVector From = FibonacciPoints[PivotIndex].GetSafeNormal(); // unit vector
	FVector To = FVector(0.0, 0.0, -1.0);                     // south pole

	// This gives you the shortest-arc rotation from From → To
	PivotToSouthQuat = FQuat::FindBetweenNormals(From, To);

	for (int32 i = 0; i < N; ++i)
	{
		FVector Rotated{ 0. };
		if (i == PivotIndex) Rotated = FVector(0.0, 0.0, -1.0);
		else Rotated = PivotToSouthQuat.RotateVector(FibonacciPoints[i]);
		PivotedPoints.Add(Rotated);
	}
}

void UGeoDelaunatorComponent::StereographicProjection(TArray<FVector>& Points)
{
	Projected2D.Empty();
	IndexMap.Empty();

	coords.clear();
	coords.reserve(N * 2); // Points.Num() in Fil's code is N

	if (Points.Num() == 0) return;

	Projected2D.Reserve(N); // Points.Num() in Fil's code is N
	IndexMap.Reserve(N); // Points.Num() in Fil's code is N

	const double eps = 1e-15;

	for (int32 i = 0; i < N; ++i) // Points.Num() in Fil's code is N
	{
		const FVector P = Points[i] ; // scale from unit sphere to world radius "* PlanetRadius"
		const double denom = 1 + P.Z;          // 1→Radius to preserve projection scale "PlanetRadius + P.Z"
		FVector2D uv{ 0., 0. };
		if (denom <= eps) 
		{
			uv.X = uv.Y = NAN;// Mark pivot as infinite, “at infinity” (south pole or behind horizon)
		}
		else {
			uv.X = (P.X) / denom; // "PlanetRadius * P.X"
			uv.Y = (P.Y) / denom; // "PlanetRadius * P.Y"
		}

		coords.push_back(uv.X);
		coords.push_back(uv.Y);
		Projected2D.Add(FVector2D(uv.X, uv.Y));
		IndexMap.Add(i);
	}
}

void UGeoDelaunatorComponent::CheckUnusedVertices()
{
	const int32 F = FibonacciPoints.Num();

	TArray<int32> UsageCount;
	UsageCount.Init(0, F);

	for (const FIntVector& tri : SphericalTriangles)
	{
		UsageCount[tri.X]++;
		UsageCount[tri.Y]++;
		UsageCount[tri.Z]++;
	}

	UE_LOG(LogTemp, Warning, TEXT("Vertex %d is UNUSED (no triangles)"), UsageCount.Num());

	for (int32 i = 0; i < F; ++i)
	{
		if (UsageCount[i] == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Vertex %d is UNUSED (no triangles)"), i);
		}
	}
}

void UGeoDelaunatorComponent::GeoDelauny()
{
	GeoDelaunayFrom();
}

void UGeoDelaunatorComponent::GeoDelaunayFrom()
{
	GenerateFibonacciSphere2();

	// find a valid point to send to infinity
	int32 PivotIndex = 0;
	while (PivotIndex < FibonacciPoints.Num() &&
		std::isnan(FibonacciPoints[PivotIndex][0] + FibonacciPoints[PivotIndex][1]))
	{
		++PivotIndex;
	}

	GeoRotation(PivotIndex);

	//GetWorld()->GetTimerManager().SetTimer(THandle_Interpolate, this, &UGeoDelaunatorComponent::Timer_FibonacciInterpolation, 0.033333f, true, 3.f);
	StereographicProjection(PivotedPoints);
	
	// DELAUNAY
	TArray<int32> Zeros;
	Zeros.Reserve(4);

	double max2 = 1.0;

	for (int32 i = 0; i < Projected2D.Num(); ++i)
	{
		const double m = Projected2D[i].X * Projected2D[i].X + Projected2D[i].Y * Projected2D[i].Y;
		if (!FMath::IsFinite(m) || m > 1e32)
		{
			Zeros.Add(i);          // like zeros.push(i)
		}
		else if (m > max2)
		{
			max2 = m;
		}
	}

	// --- 2) FAR and "infinite" points (Fil's zeros + 3 horizon points) ---

	// Remember how many *real* sphere points we had:
	const int32 OriginalCount = Projected2D.Num(); // = N

	const double FAR = 1e6 * Sleef_sqrt_u05(max2);

	for (int32 idx : Zeros)
	{
		Projected2D[idx] = FVector2D(FAR, 0.0); coords[2 * idx] = FAR; coords[2 * idx + 1] = 0.; // points[i] = [FAR, 0] in Projected2D and coords (for Delaunator)
	}

	// Add the 3 "infinite horizon" vertices
	Projected2D.Add(FVector2D(0.0, FAR)); coords.push_back(0.0); coords.push_back(FAR);
	Projected2D.Add(FVector2D(-FAR, 0.0)); coords.push_back(-FAR); coords.push_back(0.0);
	Projected2D.Add(FVector2D(0.0, -FAR)); coords.push_back(0.0); coords.push_back(-FAR);

	const int32 TotalCount = Projected2D.Num(); // = N + 3

	// --- 3) Delaunator ---
	Delaunator = NewObject<UDelaunator>(this);
	Delaunator->InitDelaunator(coords);

	ensure(Delaunator != nullptr);
	// --- 4) "Clean up the triangulation" – Fil's stitching loop ---

	// In C++ delaunator, INVALID is size_t(-1)
	const std::size_t INVALID = Delaunator->INVALID_INDEX;

	for (std::size_t i = 0, l = Delaunator->halfedges.size(); i < l; ++i)
	{
		if (Delaunator->halfedges[i] == INVALID) // hull edge
		{
			const std::size_t j = (i % 3 == 2) ? i - 2 : i + 1;
			const std::size_t k = (i % 3 == 0) ? i + 2 : i - 1;

			const std::size_t a = Delaunator->halfedges[j];
			const std::size_t b = Delaunator->halfedges[k];

			// Rewire neighbors across removed hull triangle
			if (a != INVALID && b != INVALID)
			{
				Delaunator->halfedges[a] = b;
				Delaunator->halfedges[b] = a;
			}

			Delaunator->halfedges[j] = INVALID;
			Delaunator->halfedges[k] = INVALID;

			// Collapse this triangle to the pivot index (in *3D index space*)
			Delaunator->triangles[i] = Delaunator->triangles[j] = Delaunator->triangles[k] = static_cast<std::size_t>(PivotIndex);

			// Fil also updates inedges[a], inedges[b] here; we skip because
			// delaunator.h doesn't expose inedges, and we don't need it for triangles.

			// Skip to end of this triangle
			i += 2 - (i % 3);
		}
		else if (Delaunator->triangles[i] >= static_cast<std::size_t>(OriginalCount))
		{
			// Any reference to the 3 synthetic FAR vertices (N, N+1, N+2)
			// is replaced with the pivot.
			Delaunator->triangles[i] = static_cast<std::size_t>(PivotIndex);
		}

		// --- 5) Export final triangles in original 3D index space ---

		SphericalTriangles.Empty();
		SphericalTriangles.Reserve(static_cast<int32>(Delaunator->triangles.size() / 3));

		for (std::size_t t = 0; t < Delaunator->triangles.size(); t += 3)
		{
			const int32 a = static_cast<int32>(Delaunator->triangles[t + 0]);
			const int32 b = static_cast<int32>(Delaunator->triangles[t + 1]);
			const int32 c = static_cast<int32>(Delaunator->triangles[t + 2]);

			// Skip degenerate triangles (any repeated vertex)
			if (a == b || b == c || c == a)
				continue;

			SphericalTriangles.Add(FIntVector(a, b, c));
		}
	}

	// CheckUnusedVertices();

	//*******************************************************************
	//TEST for lambda function capture of inner parameters with [=, this]
	/*MyStruct TestStruct;
	TestStruct.a = 2;
	TestStruct.b = 5.6;
	auto func = TestStruct.getFunction();
	func();*/
	//*******************************************************************
}