// Fill out your copyright notice in the Description page of Project Settings.


#include "GeoDelaunatorComponent.h"
#include <string>
#include <iostream>
#include "Interfaces/IPluginManager.h"
#ifdef IMGUI_API
#include <imgui.h>
#endif

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
}


// Called every frame
void UGeoDelaunatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	#ifdef IMGUI_API
	static bool bShowVoronoi = true;
	static bool bShowDelaunay = true;
	static int32 CurrentHalfEdge = 0;
	// CURRENT HALF EDGE NAVIGATION BOOLEAN DEBUGs
	static bool bHE_Vert = true;
	static bool bHE_Face = true;
	static bool bHE_Next = true;
	static bool bHE_Prev = true;
	static bool bHE_Twin = true;
	static bool bHE_Edge = true;
	// TWIN HALF EDGE NAVIGATION BOOLEAN DEBUGs
	static bool bTWIN_Vert = false;
	static bool bTWIN_Face = false;
	static bool bTWIN_Next = false;
	static bool bTWIN_Prev = false;
	static bool bTWIN_Twin = false;
	static bool bTWIN_Edge = false;
	if (ImGui::Begin("CBT Debug")) {
		ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
		ImGui::Text("Halfedges: %d", HalfEdge_Buffer.Num());
		ImGui::Text("Voronoi Sites: %d", VoronoiGeoCenters.Num());
		ImGui::Text("Triangles: %d", SphericalTriangles.Num());
		ImGui::Checkbox("Show Voronoi Graph", &bShowVoronoi);
		ImGui::Checkbox("Show Delaunay Triangles", &bShowDelaunay);

		ImGui::Dummy(ImVec2(0.0f, 7.0f));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 7.0f));

		// --- Start two columns ---
		ImGui::Columns(2, "DEBUG TOGGLEs", false); // false = no border
		// HALF EDGE DEBUG DRAW TOGGLES
		ImGui::Checkbox("HE Vert", &bHE_Vert);
		ImGui::Checkbox("HE Face", &bHE_Face);
		ImGui::Checkbox("HE Next", &bHE_Next);
		ImGui::Checkbox("HE Prev", &bHE_Prev);
		ImGui::Checkbox("HE Twin", &bHE_Twin);
		ImGui::Checkbox("HE Edge", &bHE_Edge);

		// ===== RIGHT COLUMN =====
		ImGui::NextColumn();
		// HALF EDGE TWIN DEBUG DRAW TOGGLES
		ImGui::Checkbox("TWIN Vert", &bTWIN_Vert);
		ImGui::Checkbox("TWIN Face", &bTWIN_Face);
		ImGui::Checkbox("TWIN Next", &bTWIN_Next);
		ImGui::Checkbox("TWIN Prev", &bTWIN_Prev);
		ImGui::Checkbox("TWIN Twin", &bTWIN_Twin);
		ImGui::Checkbox("TWIN Edge", &bTWIN_Edge);

		// Done with columns
		ImGui::Columns(1);

		ImGui::Dummy(ImVec2(0.0f, 7.0f));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 7.0f));

		if (ImGui::Button("PREV"))
		{
			CurrentHalfEdge = FMath::Clamp(CurrentHalfEdge - 1, 0, HalfEdge_Buffer.Num() - 1);
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan, TEXT("BACK"));
		}

		ImGui::SameLine();

		if (ImGui::Button("NEXT"))
		{
			CurrentHalfEdge = FMath::Clamp(CurrentHalfEdge + 1, 0, HalfEdge_Buffer.Num() - 1);
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("NEXT"));
		}

		// --- Start two columns ---
		ImGui::Columns(2, "HalfEdgeColumns", false); // false = no border

		ImGui::Text("CURRENT HALF EDGE NODE");
		if (HalfEdge_Buffer.IsValidIndex(CurrentHalfEdge))
		{
			const FHalfEdge_CBT& HE = HalfEdge_Buffer[CurrentHalfEdge];
			ImGui::Text("HalfEdge ID: %d", CurrentHalfEdge);
			ImGui::Text("Vert: %d", HE.Vert); // debug draw sphere
			if (bHE_Vert)
			{
				DrawDebugSphere(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[HE.Vert] * PlanetRadius, 20, 20, FColor::Green, false, 1.f / 20.f);
				DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[HE.Vert] * PlanetRadius + FVector(0, 0, 30), FString::Printf(TEXT("VERT: %d"), HE.Vert), nullptr, FColor::White, 1.f);
			}
			ImGui::Text("Face: %d", HE.Face); // debug draw Fibonacci Point
			if (bHE_Face)
			{
				DrawDebugSphere(GetWorld(), GetOwner()->GetActorLocation() + FibonacciPoints[HE.Face] * PlanetRadius, 20, 20, FColor::Green, false, 1.f / 20.f);
				DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + FibonacciPoints[HE.Face] * PlanetRadius + FVector(0, 0, 30), FString::Printf(TEXT("Face: %d"), HE.Face), nullptr, FColor::White, 1.f);
			}
			ImGui::Text("Next: %d", HE.Next); // debug draw sphere
			if (bHE_Next)
			{
				DrawDebugSphere(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[HE.Next] * PlanetRadius, 20, 20, FColor::Green, false, 1.f / 20.f);
				DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[HE.Next] * PlanetRadius + FVector(0, 0, 30), FString::Printf(TEXT("Next: %d"), HE.Next), nullptr, FColor::White, 1.f);
			}
			ImGui::Text("Prev: %d", HE.Prev); // debug draw sphere
			if (bHE_Prev)
			{
				DrawDebugSphere(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[HE.Prev] * PlanetRadius, 20, 20, FColor::Green, false, 1.f / 20.f);
				DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[HE.Prev] * PlanetRadius + FVector(0, 0, 30), FString::Printf(TEXT("Prev: %d"), HE.Prev), nullptr, FColor::White, 1.f);
			}
			ImGui::Text("Twin: %d", HE.Twin); // debug draw vertex Blue and/or arrow
			if (bHE_Twin)
			{
				DrawDebugSphere(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[HE.Twin] * PlanetRadius, 25, 12, FColor::Blue, false, 1.f / 20.f);
				DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[HE.Twin] * PlanetRadius + FVector(0, 0, 30), FString::Printf(TEXT("Twin: %d"), HE.Twin), nullptr, FColor::White, 1.f);
			}
			ImGui::Text("Edge: %d", HE.Edge); // debug draw arrow
			if (bHE_Edge)
			{
				FVector Start = GetOwner()->GetActorLocation() + VoronoiGeoCenters[HE.Vert] * PlanetRadius;
				FVector End = GetOwner()->GetActorLocation() + VoronoiGeoCenters[HE.Next] * PlanetRadius;
				DrawDebugDirectionalArrow(GetWorld(), Start, End, 10.f, FColor::Green, false, 1.f / 20.f, 0, DebugLineThickness + 3);
			}
		}
		else
		{
			ImGui::Text("Invalid HalfEdge ID");
		}

		// ===== RIGHT COLUMN =====
		ImGui::NextColumn();

		ImGui::Text("TWIN HALF EDGE NODE");
		if (HalfEdge_Buffer.IsValidIndex(HalfEdge_Buffer[CurrentHalfEdge].Twin))
		{
			const FHalfEdge_CBT& HE = HalfEdge_Buffer[CurrentHalfEdge];
			const FHalfEdge_CBT& TWIN = HalfEdge_Buffer[HalfEdge_Buffer[CurrentHalfEdge].Twin];
			ImGui::Text("Twin ID: IGNORE");
			ImGui::Text("Twin.Vert: %d", TWIN.Vert);
			if (bTWIN_Vert)
			{
				DrawDebugSphere(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[TWIN.Vert] * PlanetRadius, 20, 12, FColor::Blue, false, 1.f / 10.f);
				DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[TWIN.Vert] * PlanetRadius + FVector(0, 0, 30), FString::Printf(TEXT("TWIN VERT: %d"), TWIN.Vert), nullptr, FColor::Orange, 1.f);
			}
			ImGui::Text("Twin.Face: %d", TWIN.Face);
			if (bTWIN_Face)
			{
				DrawDebugSphere(GetWorld(), GetOwner()->GetActorLocation() + FibonacciPoints[TWIN.Face] * PlanetRadius, 20, 12, FColor::Blue, false, 1.f / 10.f);
				DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + FibonacciPoints[TWIN.Face] * PlanetRadius + FVector(0, 0, 30), FString::Printf(TEXT("TWIN FACE: %d"), TWIN.Face), nullptr, FColor::Orange, 1.f);
			}
			ImGui::Text("Twin.Next: %d", TWIN.Next);
			if (bTWIN_Next)
			{
				DrawDebugSphere(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[TWIN.Next] * PlanetRadius, 20, 12, FColor::Blue, false, 1.f / 10.f);
				DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[TWIN.Next] * PlanetRadius + FVector(0, 0, 30), FString::Printf(TEXT("TWIN NEXT: %d"), TWIN.Next), nullptr, FColor::Orange, 1.f);
			}
			ImGui::Text("Twin.Prev: %d", TWIN.Prev);
			if (bTWIN_Prev)
			{
				DrawDebugSphere(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[TWIN.Prev] * PlanetRadius, 20, 12, FColor::Blue, false, 1.f / 10.f);
				DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[TWIN.Prev] * PlanetRadius + FVector(0, 0, 30), FString::Printf(TEXT("TWIN PREV: %d"), TWIN.Prev), nullptr, FColor::Orange, 1.f);
			}
			ImGui::Text("Twin.Twin: %d", TWIN.Twin);
			if (bTWIN_Twin)
			{
				/*DrawDebugSphere(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[TWIN.Twin] * PlanetRadius, 25, 12, FColor::Green, false, 1.f / 10.f);
				DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + VoronoiGeoCenters[TWIN.Twin] * PlanetRadius + FVector(0, 0, 30), FString::Printf(TEXT("TWIN TWIN: %d"), TWIN.Twin), nullptr, FColor::Orange, 1.f);*/
				if (HalfEdge_Buffer.IsValidIndex(TWIN.Twin))
				{
					const FHalfEdge_CBT& TwinTwin = HalfEdge_Buffer[TWIN.Twin];
					FVector Pos = GetOwner()->GetActorLocation() + VoronoiGeoCenters[TwinTwin.Vert] * PlanetRadius;
					DrawDebugSphere(GetWorld(), Pos, 25, 12, FColor::Green, false, 1.f / 10.f);
					DrawDebugString(GetWorld(), Pos + FVector(0, 0, 30), FString::Printf(TEXT("TWIN TWIN: %d"), TWIN.Twin), nullptr, FColor::Orange, 1.f);
				}
				else
				{
					DrawDebugString(GetWorld(), GetOwner()->GetActorLocation(), TEXT("TWIN TWIN INVALID"), nullptr, FColor::Red, 1.f);
				}
			}
			ImGui::Text("Edge.Edge: %d", TWIN.Edge);
			if (bTWIN_Edge)
			{
				FVector Start = GetOwner()->GetActorLocation() + VoronoiGeoCenters[TWIN.Vert] * PlanetRadius;
				FVector End = GetOwner()->GetActorLocation() + VoronoiGeoCenters[TWIN.Next] * PlanetRadius;
				DrawDebugDirectionalArrow(GetWorld(), Start, End, 10.f, FColor::Blue, false, 1.f / 10.f, 0, DebugLineThickness + 3);
			}
		}
		else
		{
			ImGui::Text("No valid twin");
		}

		// Done with columns
		ImGui::Columns(1);
	}
	ImGui::End();
	#endif

	// ...
	FVector Location = GetOwner()->GetActorLocation();
	UWorld* WorldActual = GetWorld();
	if (!WorldActual) return;
	
	if (bShowDelaunay)
	{
		for (int32 i = 0; i < N; i++)
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
	if (bShowVoronoi)
	{
		for (int32 Site = 0; Site < VoronoiGeoMesh.Num(); ++Site)
		{
			const TArray<int32>& TriIndices = VoronoiGeoMesh[Site];
			if (TriIndices.Num() == 0) continue;

			for (int32 i = 0; i < TriIndices.Num(); ++i)
			{
				int32 i0 = TriIndices[i];
				int32 i1 = TriIndices[(i + 1) % TriIndices.Num()];

				if (!VoronoiGeoCenters.IsValidIndex(i0) || !VoronoiGeoCenters.IsValidIndex(i1)) continue;

				const FVector A = VoronoiGeoCenters[i0] * PlanetRadius + Location;
				const FVector B = VoronoiGeoCenters[i1] * PlanetRadius + Location;

				DrawDebugLine(GetWorld(), A, B, FColor::White, false, 0, 0, DebugLineThickness);
				DrawDebugPoint(GetWorld(), A, DebugPointScale, FColor::Blue);
			}
		}
	}
}

void UGeoDelaunatorComponent::GenerateFibonacciSphere1()
{
	//if (N <= 99) N = 100; // force minimum points 100

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

			Delaunator->halfedges[j] = Delaunator->halfedges[k] = INVALID;

			// Collapse this triangle to the pivot index (in *3D index space*)
			Delaunator->triangles[i] = Delaunator->triangles[j] = Delaunator->triangles[k] = PivotIndex;

			// Fil also updates inedges[a], inedges[b] here; we skip because
			// delaunator.h doesn't expose inedges, and we don't need it for triangles.

			// Skip to end of this triangle
			i += 2 - (i % 3);
		}
		else if (Delaunator->triangles[i] >= OriginalCount)
		{
			// Any reference to the 3 synthetic FAR vertices (N, N+1, N+2)
			// is replaced with the pivot.
			Delaunator->triangles[i] = PivotIndex;
		}
	}

	// --- 5) Export final triangles in original 3D index space ---

	SphericalTriangles.Empty();
	SphericalTriangles.Reserve(static_cast<int32>(Delaunator->triangles.size() / 3));
	SphericalTrisFlat.Empty();
	SphericalTrisFlat.SetNum(SphericalTriangles.Num() * 3);

	// Prepare half-edge structures
	SphericalHalfEdges.Empty();
	ReverseEdgesHash.Init(TArray<ReverseHE>(), N);

	// TO DETERMINE THE VALID TRIANGLE HALF-EDGE INDEX, SKIP DEGENERATE TRIANGLES AND TAKE THE VALID HE INDICES
	int32 ValidTriangleIndex = 0;

	// FIL's GEO_TRIANGLES FUNCTION
	for (std::size_t t = 0; t < Delaunator->triangles.size(); t += 3)
	{
		const int32 a = static_cast<int32>(Delaunator->triangles[t + 0]);
		const int32 b = static_cast<int32>(Delaunator->triangles[t + 1]);
		const int32 c = static_cast<int32>(Delaunator->triangles[t + 2]);

		// Skip degenerate triangles (any repeated vertex)
		// if (a == b || b == c || c == a)	continue;

		if (a != b && b != c)
		{
			const FIntVector& tri = FIntVector(a, b, c);
			SphericalTriangles.Add(tri); // SphericalTriangles.Add(FIntVector(a, b, c));

			SphericalTrisFlat.Add(a);
			SphericalTrisFlat.Add(b);
			SphericalTrisFlat.Add(c);

			// HALF-EDGES BUILDING SETUP --- FIRST PASS ---
			for (int32 j = 0; j < 3; ++j)
			{
				int32 A = tri[j];
				int32 B = tri[(j + 1) % 3];
				int32 HE_Index = ValidTriangleIndex * 3 + j; // Half-edge index for valid triangles, to avoid out of bounds indexes

				// Initialize all HEs with -1, we Init here to match the same size as triangles and avoid out of bound indexes.
				SphericalHalfEdges.Add(-1);
				// Add reverse mapping (B → A)
				ReverseEdgesHash[B].Add({ A, HE_Index });
			}

			// Increment only when a valid triangle
			ValidTriangleIndex++;
		}
	}

	// --- VORONOI on SPHERE from CIRCUMCENTERS of SPHERICAL TRIANGLES ---
	TArray<FVector3d> Circumcenters; // transiant to build VoronoiGeoCenters
	Circumcenters.Empty();
	Circumcenters.Reserve(SphericalTriangles.Num());
	Geo_Circumcenters(Circumcenters);

	FGeoPolygonResult tempResult = Geo_Polygons(Circumcenters);
	VoronoiGeoMesh = tempResult.Polygons;
	VoronoiGeoCenters = tempResult.Centers;


	// --- BUILD HALF-EDGE BUFFER FOR CBT ---
	//TMap<TPair<int32, int32>, int32> TwinMap; // maps directed edge (from, to) → halfedge index
	//TwinMap.Empty();

	//for (int32 Site = 0; Site < VoronoiGeoMesh.Num(); ++Site)
	//{
	//	const TArray<int32>& Ring = VoronoiGeoMesh[Site];
	//	int32 NumVerts = Ring.Num();
	//	if (NumVerts < 3) continue;

	//	int32 FirstH = HalfEdge_Buffer.Num(); // index of first halfedge for this ring

	//	for (int32 k = 0; k < NumVerts; ++k)
	//	{
	//		int32 V0 = Ring[k];
	//		int32 V1 = Ring[(k + 1) % NumVerts];
	//		int32 VPrev = Ring[(k - 1 + NumVerts) % NumVerts];

	//		int32 h = HalfEdge_Buffer.Emplace(); // reserve new entry
	//		FHalfEdge_CBT& HE = HalfEdge_Buffer[h];

	//		HE.Vert = V0;    // start vertex of the halfedge
	//		HE.Face = Site;  // owning Voronoi site
	//		HE.Next = Ring[(k + 1) % NumVerts];
	//		HE.Prev = VPrev;
	//		HE.Twin = V1;

	//		// Set undirected edge key (for optional bisector or hashing later)
	//		TPair<int32, int32> Edge;
	//		HE.Edge = V0;

	//		// Handle twins
	//		TPair<int32, int32> ForwardEdge(V0, V1);
	//		TPair<int32, int32> ReverseEdge(V1, V0);

	//		if (int32* TwinH = TwinMap.Find(ReverseEdge))
	//		{
	//			HE.Twin = *TwinH;
	//			HalfEdge_Buffer[*TwinH].Twin = h;
	//		}
	//		else
	//		{
	//			TwinMap.Emplace(ForwardEdge, h);
	//		}
	//	}
	//}

	TMap<TPair<int32, int32>, int32> EdgeToHalfEdge;

	for (int32 Site = 0; Site < VoronoiGeoMesh.Num(); ++Site)
	{
		const TArray<int32>& Ring = VoronoiGeoMesh[Site];
		int32 NumVerts = Ring.Num();

		for (int32 k = 0; k < NumVerts; ++k)
		{
			int32 V0 = Ring[k];
			int32 V1 = Ring[(k + 1) % NumVerts];

			// Create a new half-edge
			int32 h = HalfEdge_Buffer.Emplace();
			FHalfEdge_CBT& HE = HalfEdge_Buffer[h];

			HE.Vert = V0;                      // Start vertex (Voronoi center index)
			HE.Next = V1;                      // Next Voronoi center in ring
			HE.Prev = Ring[(k - 1 + NumVerts) % NumVerts];
			HE.Face = Site;
			HE.Twin = V1;
			HE.Edge = V0;

			// Twin assignment
			TPair<int32, int32> EdgeKey(V0, V1);
			TPair<int32, int32> ReverseKey(V1, V0);

			if (int32* TwinIndex = EdgeToHalfEdge.Find(ReverseKey))
			{
				int32 TwinH = *TwinIndex;

				//HE.Twin = TwinH;                   // ✅ This is correct!
				HalfEdge_Buffer[TwinH].Twin = V0;   // ✅ Also set reverse
			}
			else
			{
				EdgeToHalfEdge.Add(EdgeKey, h);    // Map edge to THIS half-edge index
			}
		}
	}


	//UE_LOG(LogTemp, Warning, TEXT("HE: %d"), HalfEdge_Mesh.Num());

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

void UGeoDelaunatorComponent::Geo_Circumcenters(TArray<FVector>& Circumcenters)
{
	// Assumes: SphericalTriangles is filled with FIntVector(a, b, c) from Delaunator
	//          and you have FibonacciPoints[N] as FVector on unit sphere

	for (const FIntVector& Tri : SphericalTriangles)
	{
		const FVector& A = FibonacciPoints[Tri.X];
		const FVector& B = FibonacciPoints[Tri.Y];
		const FVector& C = FibonacciPoints[Tri.Z];

		// Fil's vector sum of cross products
		const FVector V = FVector::CrossProduct(B, A)
			+ FVector::CrossProduct(C, B)
			+ FVector::CrossProduct(A, C);

		FVector Normalized = V.GetSafeNormal(); // This is the circumcenter on the unit sphere

		Circumcenters.Add(Normalized);
	}
}

void UGeoDelaunatorComponent::Geo_Centroids(TArray<FVector>& Circumcenters)
{
}

FGeoPolygonResult UGeoDelaunatorComponent::Geo_Polygons(TArray<FVector>& Circumcenters)
{
	FGeoPolygonResult Result;

	const int32 NumSites = FibonacciPoints.Num();
	const int32 NumTris = SphericalTriangles.Num();

	// Copy circumcenters into output
	Result.Centers = Circumcenters;
	Result.Polygons.SetNum(NumSites);

	if (NumTris == 0)
	{
		if (NumSites < 2)
			return Result;
		if (NumSites == 2)
		{
			// Edge case: 2 sites only — hemisphere split (not needed for CBT)
			return Result;
		}
	}

	// --- STEP 1: Collect per-site triangle triples (B, C, TriangleIndex) ---
	TArray<TArray<TTuple<int32, int32, int32>>> RawPolys;
	RawPolys.SetNum(NumSites);

	for (int32 t = 0; t < NumTris; ++t)
	{
		const FIntVector& Tri = SphericalTriangles[t];
		for (int j = 0; j < 3; ++j)
		{
			int32 A = Tri[j];
			int32 B = Tri[(j + 1) % 3];
			int32 C = Tri[(j + 2) % 3];
			RawPolys[A].Add(MakeTuple(B, C, t));

			// SPHERICAL VORONOI HALF-EDGE 2ND PASS -- BUILDS THE ACTUAL HALF-EDGE STRUCTURE
			int32 ReverseEdgeStart = A;		// START OF REVERSE HALF-EDGE
			int32 ReverseEdgeEnd = B;		// END OF REVERSE HALF-EDGE
			int32 HE_Index = t * 3 + j;		// INDEX OF REVERSE HALF-EDGE

			// Look for reverse match: B → A
			for (int i = 0; i < ReverseEdgesHash[A].Num(); ++i)
			{
				const ReverseHE& entry = ReverseEdgesHash[ReverseEdgeStart][i];
				if (entry.From == ReverseEdgeEnd)
				{
					SphericalHalfEdges[HE_Index] = entry.HalfEdgeIndex;
					SphericalHalfEdges[entry.HalfEdgeIndex] = HE_Index;
					ReverseEdgesHash[A].RemoveAtSwap(i); // Remove matched reverse HE to reduce future searches
					break;
				}
			}
		}
	}

	// --- STEP 2: Reorder each polygon CCW using neighbors ---
	for (int32 s = 0; s < NumSites; ++s)
	{
		const TArray<TTuple<int32, int32, int32>>& Poly = RawPolys[s];
		if (Poly.Num() == 0) continue;

		TArray<int32> OrderedTris;
		OrderedTris.Reserve(Poly.Num());

		// Start with the first triple
		OrderedTris.Add(Poly[0].Get<2>()); // triangle index
		int32 k = Poly[0].Get<1>();        // next B = C of first triple

		for (int32 i = 1; i < Poly.Num(); ++i)
		{
			bool Found = false;
			for (const auto& Entry : Poly)
			{
				if (Entry.Get<0>() == k)
				{
					k = Entry.Get<1>();
					OrderedTris.Add(Entry.Get<2>());
					Found = true;
					break;
				}
			}
			if (!Found)
			{
				break; // Incomplete loop — probably open polygon
			}
		}

		// --- STEP 3: Check if only two triangles (degenerate case) ---
		if (OrderedTris.Num() == 2)
		{
			const FIntVector& tri = SphericalTriangles[OrderedTris[0]];
			const FVector3d& P0 = FibonacciPoints[tri[0]];
			const FVector3d& P1 = FibonacciPoints[tri[1]];
			const FVector3d& P2 = FibonacciPoints[tri[2]];

			FVector3d R0 = SphericalMidpoint(P0, P1, Circumcenters[OrderedTris[0]]);
			FVector3d R1 = SphericalMidpoint(P2, P0, Circumcenters[OrderedTris[0]]);

			int32 i0 = Result.Centers.Add(R0);
			int32 i1 = Result.Centers.Add(R1);

			// Final polygon is 4-point pseudo-loop: [C0, R1, C1, R0]
			TArray<int32> FakePoly = { OrderedTris[0], i1, OrderedTris[1], i0 };
			Result.Polygons[s] = FakePoly;
		}
		else
		{
			Result.Polygons[s] = OrderedTris;
		}
	}

	return Result;
}
