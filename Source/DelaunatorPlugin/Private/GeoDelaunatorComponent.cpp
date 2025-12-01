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
	UWorld* WorldActual = GetWorld();
	if (!WorldActual) return;
	
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

		DrawDebugLine(GetWorld(), A, B, FColor::Black, false, 0.f, 0, DebugLineThickness);
		DrawDebugLine(GetWorld(), B, C, FColor::Black, false, 0.f, 0, DebugLineThickness);
		DrawDebugLine(GetWorld(), C, A, FColor::Black, false, 0.f, 0, DebugLineThickness);
	}

	// 2D VORONOI
	//auto To3D = [](const FVector2d& P2D)
	//	{
	//		// Draw in XY plane at Z = 0
	//		return FVector((float)P2D.X, (float)P2D.Y, 0.0f);
	//	};

	//const int32 NumH = HalfEdge_Buffer.Num();

	//if(NumH>0)
	//{
	//	for (int32 h = 0; h < HalfEdge_Buffer.Num(); ++h)
	//	{
	//		int32 Twin = HalfEdge_Buffer[h].Twin;
	//		if (Twin < 0 || h > Twin) continue;

	//		int32 siteL = HalfEdge_Buffer[h].Face;
	//		int32 siteR = HalfEdge_Buffer[Twin].Face;

	//		FColor Color = FColor::White;
	//		if (siteL == Pivot || siteR == Pivot)
	//		{
	//			Color = FColor::Red; // rays touching pivot's cell
	//		}

	//		const int32 v0 = HalfEdge_Buffer[h].Vert;
	//		const int32 v1 = HalfEdge_Buffer[Twin].Vert;

	//		FVector2d P0_2D = VorVert2D[v0];
	//		FVector2d P1_2D = VorVert2D[v1];

	//		double MaxR2 = 10000.0; // adjust

	//		if (P0_2D.SquaredLength() > MaxR2 ||
	//			P1_2D.SquaredLength() > MaxR2)
	//		{
	//			continue;
	//		}

	//		DrawDebugLine(WorldActual,
	//			FVector(P0_2D.X, P0_2D.Y, 0.0)*100. + Location,
	//			FVector(P1_2D.X, P1_2D.Y, 0.0)*100. + Location,
	//			Color, false, 0.f, 0, DebugLineThickness);
	//	}
	//}

	// SPHERE VORONOI
	const int32 NumH = HalfEdge_Buffer.Num();

	for (int32 h = 0; h < NumH; ++h)
	{
		int32 Twin = HalfEdge_Buffer[h].Twin;
		if (Twin < 0 || h > Twin) continue; // draw each undirected edge once

		int32 v0 = HalfEdge_Buffer[h].Vert;
		int32 v1 = HalfEdge_Buffer[Twin].Vert;

		const FVector& P0 = VorVert3D[v0];
		const FVector& P1 = VorVert3D[v1];

		DrawDebugPoint(GetWorld(), P0 * PlanetRadius + Location, DebugPointScale, FColor::Blue);
		DrawDebugLine(WorldActual, P0 * PlanetRadius + Location, P1 * PlanetRadius + Location, FColor::White,
			false, 0.0f, 0, DebugLineThickness);
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
	if (N <= 99) N = 100; // force minimum points 100

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
		double latDeg = lat * DEGREES;
		double lonDeg = lng * DEGREES;

		lonDeg = FMath::Fmod(lonDeg, 360.0);
		if (lonDeg < 0.0) lonDeg += 360.0;

		LonLat.Add(FVector2D(lonDeg, latDeg));

		// Convert to Cartesian (unit sphere) with Z as "up" (north pole at +Z)
		const double sinLat = Sleef_sin_u10(lat);
		const double cosLat = Sleef_cos_u10(lat);

		const double x = cosLat * Sleef_cos_u10(lng);
		const double y = cosLat * Sleef_sin_u10(lng);
		const double zCart = sinLat;                 // Z is latitude

		FibonacciPoints.Add(FVector(x, y, zCart));

		// RedBlob increment
		const double safeR2 = (r > 1e-12) ? r : 1e-12;
		lon += s / safeR2;
	}
}

void UGeoDelaunatorComponent::GenerateFibonacciSphere2()
{
	// Second algorithm from CGA FAQ / RedBlob
	if (N <= 99) N = 100; // force minimum points 100

	FibonacciPoints.Empty();
	FibonacciPoints.Reserve(N);
	LonLat.Empty();
	LonLat.Reserve(N); // pre-allocates memory for N elements up front.
	const double s = 3.6 / Sleef_sqrt_u05((double)N);
	const double dlong = _PI * (3.0 - Sleef_sqrt_u05(5.0)); // golden angle
	const double dz = 2.0 / N;
	double z = 1.0 - dz * 0.5;
	double lon = 0.0;

	FRandomStream Rng(123456); // deterministic jitter

	for (int32 k = 0; k < N; ++k, z -= dz)
	{
		const double r = Sleef_sqrt_u05(1.0 - z * z);
		const double lat = Sleef_asin_u10(z);

		// Convert to degrees
		double lonDeg = lon * (DEGREES);
		double latDeg = lat * (DEGREES);

		if (Jitter > 0.0)
		{
			const double randLat = (double)Rng.GetFraction() - (double)Rng.GetFraction(); // [-1,1]
			const double randLon = (double)Rng.GetFraction() - (double)Rng.GetFraction(); // [-1,1]

			// latDeg += jitter * randLat * (latDeg - asin(max(-1, z - dz*2π*r/s)) * 180/π);
			const double z2 = FMath::Max(-1.0, z - dz * 2.0 * _PI * r / s);
			const double latMin = Sleef_asin_u10(z2) * DEGREES;

			latDeg += Jitter * randLat * (latDeg - latMin);

			// lonDeg += jitter * randLon * (s/r * 180/π);
			const double safeR = (r > 1e-12) ? r : 1e-12;
			lonDeg += Jitter * randLon * (s / safeR * DEGREES);
		}

		// wrap longitude to [0,360) like lonDeg % 360.0
		lonDeg = FMath::Fmod(lonDeg, 360.0);
		if (lonDeg < 0.0) lonDeg += 360.0;

		LonLat.Add(FVector2D(lonDeg, latDeg));

		// Cartesian on unit sphere, Z-up (Unreal)
		const double latRad = latDeg / 180.0 * _PI;
		const double lonRad = lonDeg / 180.0 * _PI;

		const double cosLat = Sleef_cos_u10(latRad);
		const double sinLat = Sleef_sin_u10(latRad);

		const double x = cosLat * Sleef_cos_u10(lonRad);
		const double y = cosLat * Sleef_sin_u10(lonRad);
		const double zCart = sinLat;

		FibonacciPoints.Add(FVector(x, y, zCart));

		lon += dlong;
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

FVector UGeoDelaunatorComponent::UnprojectVoronoiVertexToSphereAndInvertRotation(const FVector2d& V2D)
{
		const double u = V2D.X;
		const double v = V2D.Y;

		const double r2 = u * u + v * v;
		const double den = 1.0 + r2;

		const double Xr = 2.0 * u / den;
		const double Yr = 2.0 * v / den;
		const double Zr = (1.0 - r2) / den;

		FVector Rotated((float)Xr, (float)Yr, (float)Zr);

		const FQuat InvQuat = PivotToSouthQuat.Inverse();
		FVector Original = InvQuat.RotateVector(Rotated);
		Original.Normalize();

		return Original;
		//return FVector(Xr, Yr, Zr); // return unrotated points
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
	GenerateFibonacciSphere1();

	// find a valid point to send to infinity
	int32 PivotIndex = 0;
	while (PivotIndex < FibonacciPoints.Num() &&
		std::isnan(FibonacciPoints[PivotIndex][0] + FibonacciPoints[PivotIndex][1]))
	{
		++PivotIndex;
	}
	Pivot = PivotIndex;

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
		SphericalTrisFlat.Empty();
		SphericalTrisFlat.SetNum(SphericalTriangles.Num() * 3);

		for (std::size_t t = 0; t < Delaunator->triangles.size(); t += 3)
		{
			const int32 a = static_cast<int32>(Delaunator->triangles[t + 0]);
			const int32 b = static_cast<int32>(Delaunator->triangles[t + 1]);
			const int32 c = static_cast<int32>(Delaunator->triangles[t + 2]);

			// Skip degenerate triangles (any repeated vertex)
			if (a == b || b == c || c == a)
				continue;

			SphericalTriangles.Add(FIntVector(a, b, c));

			SphericalTrisFlat.Add(a);
			SphericalTrisFlat.Add(b);
			SphericalTrisFlat.Add(c);
		}
	}

	// VORONOI
	int32 NumTris = (int32)Delaunator->triangles.size() / 3;
	TriValid.Empty();
	TriValid.SetNum(NumTris);
	VorVert2D.Empty();
	VorVert2D.SetNum(NumTris);

	for (int32 tri = 0; tri < NumTris; ++tri)
	{
		int32 ia = (int32)Delaunator->triangles[3 * tri + 0];
		int32 ib = (int32)Delaunator->triangles[3 * tri + 1];
		int32 ic = (int32)Delaunator->triangles[3 * tri + 2];

		// skip collapsed hull tris (all pivot, or any duplicate)
		if (ia == ib || ib == ic || ic == ia)
		{
			TriValid[tri] = false;
			continue;
		}

		TriValid[tri] = true;

		FVector2d A = Projected2D[ia];
		FVector2d B = Projected2D[ib];
		FVector2d C = Projected2D[ic];

		FVector2d CC;
		ComputeCircumcenter2D(A, B, C, CC);
		VorVert2D[tri] = CC;
	}

	HalfEdge_Buffer.Empty();

	auto NextCorner = [](int32 h)
		{
			return (h % 3 == 2) ? h - 2 : h + 1;
		};

	int32 NumCorners = (int32)Delaunator->halfedges.size(); 
	
	for (int32 h = 0; h < NumCorners; ++h)
	{
		int32 hTwin = (int32)Delaunator->halfedges[h];
		if (hTwin == (int32)Delaunator->INVALID_INDEX)
			continue;                       // dead hull edge, ignore

		if (h > hTwin)
			continue;                       // process each undirected edge once

		int32 triL = h / 3;
		int32 triR = hTwin / 3;
		if (!TriValid[triL] || !TriValid[triR])
			continue;

		// Delaunay edge between sites i0 and i1
		int32 i0 = (int32)Delaunator->triangles[h];
		int32 i1 = (int32)Delaunator->triangles[NextCorner(h)];

		// create two Voronoi halfedges for this dual edge
		int32 hv0 = HalfEdge_Buffer.AddDefaulted();
		int32 hv1 = HalfEdge_Buffer.AddDefaulted();

		// halfedge belonging to cell of site i0
		HalfEdge_Buffer[hv0].Vert = triR;   // ends at circumcenter of opposite triangle
		HalfEdge_Buffer[hv0].Face = i0;

		// halfedge belonging to cell of site i1
		HalfEdge_Buffer[hv1].Vert = triL;
		HalfEdge_Buffer[hv1].Face = i1;

		// twins
		HalfEdge_Buffer[hv0].Twin = hv1;
		HalfEdge_Buffer[hv1].Twin = hv0;
	}

	int32 NumSites = OriginalCount; // number of geo points
	TArray<TArray<int32>> FaceHalfedges;
	FaceHalfedges.SetNum(NumSites);

	for (int32 h = 0; h < HalfEdge_Buffer.Num(); ++h)
	{
		int32 f = HalfEdge_Buffer[h].Face;
		FaceHalfedges[f].Add(h);
	}
	
	for (int32 f = 0; f < NumSites; ++f)
	{
		auto& List = FaceHalfedges[f];
		if (List.Num() < 2) continue;

		const FVector2d Pi = Projected2D[f];

		struct FAngleH { double Angle; int32 H; };
		TArray<FAngleH> Sorted;
		Sorted.Reserve(List.Num());

		for (int32 h : List)
		{
			int32 vIdx = HalfEdge_Buffer[h].Vert;          // triangle index
			const FVector2d C = VorVert2D[vIdx]; // circumcenter
			FVector2d V = C - Pi;
			double ang = Sleef_atan2_u10(V.Y, V.X);
			Sorted.Add({ ang, h });
		}

		Sorted.Sort([](const FAngleH& A, const FAngleH& B)
			{
				return A.Angle < B.Angle;
			});

		int32 m = Sorted.Num();
		for (int32 k = 0; k < m; ++k)
		{
			int32 hCur = Sorted[k].H;
			int32 hNext = Sorted[(k + 1) % m].H;
			int32 hPrev = Sorted[(k - 1 + m) % m].H;

			HalfEdge_Buffer[hCur].Next = hNext;
			HalfEdge_Buffer[hCur].Prev = hPrev;
		}
	}

	VorVert3D.SetNum(VorVert2D.Num());
	for (int32 tri = 0; tri < VorVert2D.Num(); ++tri)
	{
		FVector VorVert = UnprojectVoronoiVertexToSphereAndInvertRotation(VorVert2D[tri]); // or _Plus
		VorVert3D[tri] = VorVert;
	}

	//BuildHalfedgeMesh();

	UE_LOG(LogTemp, Warning, TEXT("HE: %d"), HalfEdge_Mesh.Num());

	// DEBUG
	// GetWorld()->GetTimerManager().SetTimer(THandle_HalfEdgeDebug, this, &UGeoDelaunatorComponent::Timer_HalfEdgeDebug, 1.f/2.f, true, 3.f);
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

bool UGeoDelaunatorComponent::ComputeCircumcenter2D(const FVector2d& A, const FVector2d& B, const FVector2d& C, FVector2d& OutCenter)
{
	const double ax = A.X, ay = A.Y;
	const double bx = B.X, by = B.Y;
	const double cx = C.X, cy = C.Y;

	const double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
	if (FMath::IsNearlyZero(d))
	{
		// Degenerate / collinear; fallback: use centroid
		OutCenter = FVector2d(
			(ax + bx + cx) / 3.0,
			(ay + by + cy) / 3.0);
		return false;
	}

	const double a2 = ax * ax + ay * ay;
	const double b2 = bx * bx + by * by;
	const double c2 = cx * cx + cy * cy;

	const double ux = (a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / d;
	const double uy = (a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / d;

	OutCenter = FVector2d(ux, uy);
	return true;
}

// DEPRECATED
void UGeoDelaunatorComponent::BuildHalfedgeMesh()
{
	const int32 NumFaces = SphericalTriangles.Num();
	const int32 NumEdges = NumFaces * 3;

	//HalfEdge_Mesh.Empty();
	HalfEdge_Mesh.Init(-1, NumEdges); // each triangle has 3 half-edges, each half-edge has a twin

	//for (int32 i = 0; i < SphericalTriangles.Num(); ++i)
	//{
	//	const FIntVector& tri = SphericalTriangles[i];
	//	// Half-edges of triangle i
	//	const int32 he0 = i * 3 + 0; // from tri.X to tri.Y
	//	const int32 he1 = i * 3 + 1; // from tri.Y to tri.Z
	//	const int32 he2 = i * 3 + 2; // from tri.Z to tri.X
	//	// Search for twin half-edges in other triangles
	//	for (int32 j = 0; j < SphericalTriangles.Num(); ++j)
	//	{
	//		if (i == j) continue; // skip same triangle
	//		const FIntVector& otherTri = SphericalTriangles[j];
	//		// Check each edge of other triangle for twin
	//		if (tri.X == otherTri.Y && tri.Y == otherTri.X)
	//		{
	//			const int32 otherHe = j * 3 + 1; // other triangle's edge from Y to X
	//			HalfEdge_Mesh[he0] = otherHe;
	//			HalfEdge_Mesh[otherHe] = he0;
	//		}
	//		else if (tri.Y == otherTri.Z && tri.Z == otherTri.Y)
	//		{
	//			const int32 otherHe = j * 3 + 2; // other triangle's edge from Z to Y
	//			HalfEdge_Mesh[he1] = otherHe;
	//			HalfEdge_Mesh[otherHe] = he1;
	//		}
	//		else if (tri.Z == otherTri.X && tri.X == otherTri.Z)
	//		{
	//			const int32 otherHe = j * 3 + 0; // other triangle's edge from X to Z
	//			HalfEdge_Mesh[he2] = otherHe;
	//			HalfEdge_Mesh[otherHe] = he2;
	//		}
	//	}
	//}

	// --- Map from undirected edge -> one oriented edge index ---
	//TMap<FEdgeKey, int32> EdgeMap;
	//EdgeMap.Reserve(NumEdges);

	//for (int32 t = 0; t < NumFaces; ++t)
	//{
	//	const int32 Base = 3 * t;

	//	// local edges: (0->1), (1->2), (2->0)
	//	for (int32 k = 0; k < 3; ++k)
	//	{
	//		const int32 e = Base + k;
	//		const int32 v0 = SphericalTrisFlat[e];
	//		const int32 v1 = SphericalTrisFlat[Base + (k + 1) % 3];

	//		const FEdgeKey Key(v0, v1);

	//		if (int32* ExistingEdge = EdgeMap.Find(Key))
	//		{
	//			// found twin
	//			const int32 e2 = *ExistingEdge;
	//			HalfEdge_Mesh[e] = e2;
	//			HalfEdge_Mesh[e2] = e;
	//		}
	//		else
	//		{
	//			EdgeMap.Add(Key, e);
	//		}
	//	}
	//}

	for (int32 i = 0; i < SphericalTrisFlat.Num(); i++)
	{

	}
}

void UGeoDelaunatorComponent::BuildCBT()
{
}
