// Fill out your copyright notice in the Description page of Project Settings.


#include "GeoDelaunatorComponent.h"
#include "CBTResource_Interface.h"
#include "IndirectInstancingSceneProxy.h"
#include <string>
#include <iostream>
#include "Interfaces/IPluginManager.h"
#ifdef IMGUI_API
#include <imgui.h>
#endif


/*****************************************************************************
*                                                                           *
*              INDIRECT INSTANCING PRIMITIVE COMPONENT                       *
*                                                                           *
*  Constructor, Material, SceneProxy, Bounds, and UPrimitiveComponent       *
*  overrides needed for indirect instanced rendering.                       *
*                                                                           *
*****************************************************************************/

UGeoDelaunatorComponent::UGeoDelaunatorComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	CastShadow = true;
	bCastContactShadow = false;
	bUseAsOccluder = true;
	bAffectDynamicIndirectLighting = false;
	bAffectDistanceFieldLighting = false;
	bNeverDistanceCull = true;
#if WITH_EDITORONLY_DATA
	bEnableAutoLODGeneration = false;
#endif
	Mobility = EComponentMobility::Static;
}

void UGeoDelaunatorComponent::OnRegister()
{
	Super::OnRegister();
	UE_LOG(LogTemp, Warning, TEXT("GeoDelaunatorComponent::OnRegister"));
}

void UGeoDelaunatorComponent::OnUnregister()
{
	Super::OnUnregister();
}

void UGeoDelaunatorComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);
	MarkRenderStateDirty();
}

bool UGeoDelaunatorComponent::IsVisible() const
{
	return Super::IsVisible();
}

FBoxSphereBounds UGeoDelaunatorComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	//return FBoxSphereBounds(FBox(FVector(0.f, 0.f, 0.f), FVector(PlanetRadius * 2.0))).TransformBy(LocalToWorld);

	const FVector Extent(2.0*PlanetRadius);
	return FBoxSphereBounds(FBox(-Extent, Extent)).TransformBy(LocalToWorld);
}

FPrimitiveSceneProxy* UGeoDelaunatorComponent::CreateSceneProxy()
{
	UE_LOG(LogTemp, Warning, TEXT("GeoDelaunatorComponent::CreateSceneProxy"));
	return new FGeoVoronoiIndirectInstancingSceneProxy(this);
}

void UGeoDelaunatorComponent::SetMaterial(int32 InElementIndex, UMaterialInterface* InMaterial)
{
	if (InElementIndex == 0 && Material != InMaterial)
	{
		Material = InMaterial;
		MarkRenderStateDirty();
	}
}

void UGeoDelaunatorComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (Material != nullptr)
	{
		OutMaterials.Add(Material);
	}
}

/*****************************************************************************
*          END INDIRECT INSTANCING PRIMITIVE COMPONENT                       *
*****************************************************************************/

// Called when the game starts
void UGeoDelaunatorComponent::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("GeoDelaunatorComponent::BeginPlay"));

	RngStream.Initialize(RandomSeed);
	GeoDelauny();

	UE_LOG(LogTemp, Warning, TEXT("GeoDelaunatorComponent::BeginPlay after GeoDelauny, CBTResources valid=%d"),
		CBTResources.IsValid() ? 1 : 0);
}

void UGeoDelaunatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UGeoDelaunatorComponent::BeginDestroy()
{
	if (CBTResources.IsValid())
	{
		if (CBTResources->IsInitialized())
		{
			BeginReleaseResource(CBTResources.Get());
			FlushRenderingCommands();
		}

		CBTResources.Reset();
	}

	Super::BeginDestroy();
}


// Called every frame
void UGeoDelaunatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
	FVector Location = GetOwner()->GetActorLocation();
	UWorld* WorldActual = GetWorld();
	if (!WorldActual) return;

	static bool bShowVoronoiVerts = false;
	static bool bShowVoronoiEdges = false;
	static bool bShowDelaunaySites = false;
	static bool bShowDelaunayEdges = false;
#ifdef IMGUI_API

#pragma region PLANET_GENERAL_DATA_WINDOW
	if (ImGui::Begin("Delaunay General Data Debug")) {
		ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
		ImGui::Text("Number of Sites: %d", N);
		ImGui::Text("Half_Edge Buffer: %d", HalfEdge_Buffer.Num());
		ImGui::Text("Root Bisectors Buffer: %d", HalfEdge_Buffer.Num());
		ImGui::Text("CBT Buffer: %d", CBT_Buffer.Num());
		ImGui::Text("Voronoi Sites: %d", VoronoiGeoCenters.Num());
		ImGui::Text("Triangles: %d", SphericalTriangles.Num());
	}
	ImGui::End();
#pragma endregion PLANET_GENERAL_DATA_WINDOW

#pragma region DRAW_VORONOI_DEBUG_WINDOW
	static bool bDebugVHEs = false;
	static int32 SiteID{ 0 };
	static int32 PolyID{ 0 };
	if (ImGui::Begin("VHE Debug")) {
		ImGui::Checkbox("DEBUG VHEs", &bDebugVHEs);
		if (ImGui::Button("PREV SITE"))
		{
			SiteID = FMath::Clamp(SiteID - 1, 0, N - 1);
			PolyID = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button("NEXT SITE"))
		{
			SiteID = FMath::Clamp(SiteID + 1, 0, N - 1);
			PolyID = 0;
		}
		if (bDebugVHEs)
		{
			ImGui::Text("CURRENT SITE ID: %d", SiteID);
			ImGui::Text("VHEs in Site: %d", VoronoiHalfEdges_Map[SiteID].Num());
			int32 NumPolys = VoronoiHalfEdges_Map[SiteID].Num();
			if (ImGui::Button("PREV VHE"))
			{
				PolyID = (PolyID - 1 + NumPolys) % NumPolys;
			}
			ImGui::SameLine();
			if (ImGui::Button("NEXT VHE"))
			{
				PolyID = (PolyID + 1) % NumPolys;
			}
			ImGui::Text("CURRENT POLY ID: %d", PolyID);
			if (VoronoiHalfEdges_Map[SiteID].IsValidIndex(PolyID))
			{
				const FVoronoiHalfEdge& VHE = VoronoiHalfEdges_Map[SiteID][PolyID];
				ImGui::Text("VHE Start: %d", VHE.VHE_Start);
				ImGui::Text("VHE End: %d", VHE.VHE_End);
				ImGui::Text("VHE Start Face: %d (Current Site)", VHE.Start_Face);
				ImGui::Text("VHE End Face: %d", VHE.End_Face);
				// DEBUG DRAW CURRENT VHE
				DrawDebugSphere(GetWorld(), Location + VoronoiGeoCenters[VHE.VHE_Start] * PlanetRadius, 20, 20, FColor::Green, false, DeltaTime);
				DrawDebugString(GetWorld(), Location + VoronoiGeoCenters[VHE.VHE_Start] * PlanetRadius + FVector(0, 0, 30), FString::Printf(TEXT("VHE START: %d"), VHE.VHE_Start), nullptr, FColor::White, DeltaTime);
				DrawDebugSphere(GetWorld(), Location + VoronoiGeoCenters[VHE.VHE_End] * PlanetRadius, 20, 20, FColor::Blue, false, DeltaTime);
				DrawDebugString(GetWorld(), Location + VoronoiGeoCenters[VHE.VHE_End] * PlanetRadius + FVector(0, 0, 30), FString::Printf(TEXT("VHE END: %d"), VHE.VHE_End), nullptr, FColor::White, DeltaTime);
				DrawDebugLine(GetWorld(),
					Location + VoronoiGeoCenters[VHE.VHE_Start] * PlanetRadius,
					Location + VoronoiGeoCenters[VHE.VHE_End] * PlanetRadius,
					FColor::Red, false, 1.f / 20.f, 0, DebugLineThickness+3);
				DrawDebugSphere(GetWorld(), Location + FibonacciPoints[VHE.Start_Face] * PlanetRadius, 20, 20, FColor::Red, false, DeltaTime);
				DrawDebugString(GetWorld(), Location + FibonacciPoints[VHE.Start_Face] * PlanetRadius + FVector(0, 0, 30), FString::Printf(TEXT("VHE START FACE: %d"), VHE.Start_Face), nullptr, FColor::White, DeltaTime);
				DrawDebugSphere(GetWorld(), Location + FibonacciPoints[VHE.End_Face] * PlanetRadius, 20, 20, FColor::Red, false, DeltaTime);
				DrawDebugString(GetWorld(), Location + FibonacciPoints[VHE.End_Face] * PlanetRadius + FVector(0, 0, 30), FString::Printf(TEXT("VHE END: %d"), VHE.VHE_End), nullptr, FColor::White, DeltaTime);
				DrawDebugLine(GetWorld(),
					Location + FibonacciPoints[VHE.Start_Face] * PlanetRadius,
					Location + FibonacciPoints[VHE.End_Face] * PlanetRadius,
					FColor::Orange, false, 1.f / 20.f, 0, DebugLineThickness+3);
			}
		}
	}
	ImGui::End();
#pragma endregion DRAW_VORONOI_DEBUG_WINDOW

#pragma region CBT_DEBUG_WINDOW
	static int32 CurrentHalfEdge = 0;
	// CURRENT HALF EDGE NAVIGATION BOOLEAN DEBUGs
	static bool bHE_Vert = false;
	static bool bHE_Face = false;
	static bool bHE_Next = false;
	static bool bHE_Prev = false;
	static bool bHE_Twin = false;
	static bool bHE_Edge = false;
	// TWIN HALF EDGE NAVIGATION BOOLEAN DEBUGs
	static bool bTWIN_Vert = false;
	static bool bTWIN_Face = false;
	static bool bTWIN_Next = false;
	static bool bTWIN_Prev = false;
	static bool bTWIN_Twin = false;
	static bool bTWIN_Edge = false;
	if (ImGui::Begin("CBT Debug")) {
		ImGui::Checkbox("Show Voronoi Verts", &bShowVoronoiVerts);
		ImGui::SameLine();
		ImGui::Checkbox("Show Voronoi Edges", &bShowVoronoiEdges);
		ImGui::Checkbox("Show Delaunay Sites", &bShowDelaunaySites);
		ImGui::SameLine();
		ImGui::Checkbox("Show Delaunay Edges", &bShowDelaunayEdges);

		ImGui::Dummy(ImVec2(0.0f, 7.0f));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 7.0f));

		if (ImGui::CollapsingHeader("My Collapsible Area"))
		{
			// --- Start two columns ---
			ImGui::Columns(2, "DEBUG TOGGLEs", true); // false = no border
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
			}

			ImGui::SameLine();

			if (ImGui::Button("NEXT"))
			{
				CurrentHalfEdge = FMath::Clamp(CurrentHalfEdge + 1, 0, HalfEdge_Buffer.Num() - 1);
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
	}
	ImGui::End();
#pragma endregion CBT_DEBUG_WINDOW

#endif

	// SPHERE DELAUNAY DEBUG DRAW
	const FTransform& ComponentTransform = GetComponentTransform();

	if (bShowDelaunaySites)
	{
		for (int32 i = 0; i < N; i++)
		{
			DrawDebugPoint(
				GetWorld(),
				ComponentTransform.TransformPosition(FibonacciPoints[i] * PlanetRadius),
				DebugPointScale,
				FColor::Red);
		}
	}

	if (bShowDelaunayEdges)
	{
		for (const FIntVector& tri : SphericalTriangles)
		{
			const FVector A = ComponentTransform.TransformPosition(FibonacciPoints[tri.X] * PlanetRadius);
			const FVector B = ComponentTransform.TransformPosition(FibonacciPoints[tri.Y] * PlanetRadius);
			const FVector C = ComponentTransform.TransformPosition(FibonacciPoints[tri.Z] * PlanetRadius);

			DrawDebugLine(GetWorld(), A, B, FColor::Black, false, 0.0f, 0, DebugLineThickness);
			DrawDebugLine(GetWorld(), B, C, FColor::Black, false, 0.0f, 0, DebugLineThickness);
			DrawDebugLine(GetWorld(), C, A, FColor::Black, false, 0.0f, 0, DebugLineThickness);
		}
	}

#pragma region VORONOI_DEBUG_DRAW_2D
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
#pragma endregion

	// SPHERE VORONOI DEBUG DRAW
	if (bShowVoronoiVerts || bShowVoronoiEdges)
	{
		//const FTransform& ComponentTransform = GetComponentTransform();

		if (bShowVoronoiVerts || bShowVoronoiEdges)
		{
			for (int32 Site = 0; Site < VoronoiGeoMesh.Num(); ++Site)
			{
				const TArray<int32>& TriIndices = VoronoiGeoMesh[Site];
				if (TriIndices.Num() == 0) continue;

				for (int32 i = 0; i < TriIndices.Num(); ++i)
				{
					const int32 i0 = TriIndices[i];
					const int32 i1 = TriIndices[(i + 1) % TriIndices.Num()];

					if (!VoronoiGeoCenters.IsValidIndex(i0) || !VoronoiGeoCenters.IsValidIndex(i1)) continue;

					const FVector A = ComponentTransform.TransformPosition(VoronoiGeoCenters[i0] * PlanetRadius);
					const FVector B = ComponentTransform.TransformPosition(VoronoiGeoCenters[i1] * PlanetRadius);

					if (bShowVoronoiVerts) DrawDebugPoint(GetWorld(), A, DebugPointScale, FColor::Blue);
					if (bShowVoronoiEdges) DrawDebugLine(GetWorld(), A, B, FColor::White, false, 0.0f, 0, DebugLineThickness);
				}
			}
		}
	}
}

void UGeoDelaunatorComponent::GenerateFibonacciSphere1()
{
	//if (N <= 99) N = 100; // force minimum points 100

	FibonacciPoints.Empty();
	FibonacciPoints.Reserve(N);
	FibonacciPoints_HL.Empty();
	FibonacciPoints_HL.Reserve(N);
	LonLat.Empty();
	LonLat.Reserve(N);

	// init colors
	VoronoiCellColors.Empty();
	VoronoiCellColors.Reserve(N);

	// First algorithm from RedBlob / CGA FAQ
	const double s = 3.6 / Sleef_sqrt_u05((double)N);
	const double dz = 2.0 / (double)N;

	double z = 1.0 - dz * 0.5;
	double lon = 0.0;

	// Deterministic jitter
	FRandomStream Rng(RandomSeed);

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

		const FVector FibPoint(x, y, zCart);
		FibonacciPoints.Add(FibPoint);
		FibonacciPoints_HL.Add(FVector3_HighLow(FibPoint));

		// RedBlob increment
		const double safeR2 = (r > 1e-12) ? r : 1e-12;
		lon += s / safeR2;

		/****************************************************
		*Voronoi cell colors (randomized but deterministic)*/
		const uint32 R = RngStream.RandRange(0, 255);
		const uint32 G = RngStream.RandRange(0, 255);
		const uint32 B = RngStream.RandRange(0, 255);
		const uint32 A = 255;

		const uint32 PackedColor = R | (G << 8) | (B << 16) | (A << 24);
		VoronoiCellColors.Add(PackedColor);
	}
}

void UGeoDelaunatorComponent::GenerateFibonacciSphere2()
{
	// Second algorithm from CGA FAQ / RedBlob
	if (N <= 99) N = 100; // force minimum points 100

	FibonacciPoints.Empty();
	FibonacciPoints.Reserve(N);
	FibonacciPoints_HL.Empty();
	FibonacciPoints_HL.Reserve(N);
	LonLat.Empty();
	LonLat.Reserve(N); // pre-allocates memory for N elements up front.
	const double s = 3.6 / Sleef_sqrt_u05((double)N);
	const double dlong = _PI * (3.0 - Sleef_sqrt_u05(5.0)); // golden angle
	const double dz = 2.0 / N;
	double z = 1.0 - dz * 0.5;
	double lon = 0.0;

	FRandomStream Rng(RandomSeed); // deterministic jitter

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

		const FVector FibPoint(x, y, zCart);
		FibonacciPoints.Add(FibPoint);
		FibonacciPoints_HL.Add(FVector3_HighLow(FibPoint));

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
	ReverseEdgesHash.Init(TArray<FReverseHE>(), N);

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

		if (a != b && b != c && a != c) // i don't know if this is necessary && a != c, the triangles linking to center of planet was fixed in Geo_Circumcenters
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
	TArray<FVector3_HighLow> Circumcenters_HL; // transiant to build VoronoiGeoCenters_HL
	Circumcenters_HL.Empty();
	Circumcenters_HL.Reserve(SphericalTriangles.Num());
	Geo_Circumcenters(Circumcenters, Circumcenters_HL);

	FGeoPolygonResult tempResult = Geo_Polygons(Circumcenters, Circumcenters_HL);
	VoronoiGeoMesh = tempResult.Polygons;
	VoronoiGeoCenters = tempResult.Centers;
	VoronoiGeoCenters_HL = tempResult.Centers_HL;
	VoronoiGeoMesh_Ranges = tempResult.VoronoiGeoMesh_Ranges;
	VoronoiGeoMesh_Flat = tempResult.VoronoiGeoMesh_Flat;

	// --- BUILD HALF-EDGE BUFFER FOR CBT ---
	HalfEdge_Buffer.Empty();
	RootBisectors_Buffer.Empty();
	for(int32 s=0; s < N; ++s)
	{
		const TArray<FVoronoiHalfEdge> PolyRing = VoronoiHalfEdges_Map[s];
		const int32 RingSize = PolyRing.Num();
		for (int32 v = 0; v < RingSize; ++v)
		{
			// create the CBT half-edge structure
			FHalfEdge_CBT CBT_HE;

			// Wrap Next and Prev with modulo
			int32 LocalNext = (v + 1) % RingSize;
			int32 LocalPrev = (v - 1 + RingSize) % RingSize;

			CBT_HE.Next = SitePrefixSums[s] + LocalNext;
			CBT_HE.Prev = SitePrefixSums[s] + LocalPrev;
			CBT_HE.Vert = PolyRing[v].VHE_Start;
			CBT_HE.Face = s;
			CBT_HE.Edge = SitePrefixSums[s] + v;

			// get end_face
			const TArray<FVoronoiHalfEdge> Twin_Poly = VoronoiHalfEdges_Map[PolyRing[v].End_Face];
			// loop through VoronoiHalfEdges_Map[end_face] to find VHE where its end_face == s
			for (int32 t_v = 0; t_v < Twin_Poly.Num(); ++t_v)
			{
				// get SitePrefixSums[end_face] and add VHE_v to calcualte twin global index in CBT HE array
				if(Twin_Poly[t_v].End_Face == s)
				{
					// found the twin half-edge
					CBT_HE.Twin = SitePrefixSums[PolyRing[v].End_Face] + t_v;
					break;
				}
			}

			// add the CBT half-edges to the array
			HalfEdge_Buffer.Add(CBT_HE);

			// Build Root Bisectors Buffer
			RootBisectors_Buffer.Add(FRootBisector_CBT(CBT_HE.Edge, CBT_HE.Twin, CBT_HE.Next, CBT_HE.Prev));
		}
	}

	// CALCULATING DEPTH AND BUILDING THE CBT BUFFER
	const int32 NumRootBisectors = HalfEdge_Buffer.Num();
	// ----------------------------------------------------
	// 1) Choose a safe Depth based on number of bisectors
	// ----------------------------------------------------
	// We want 2^Depth >= NumRootBisectors, with some margin.
	float Log2N = FMath::Log2((float)NumRootBisectors);
	int32 Depth = FMath::CeilToInt(Log2N);

	// Add 1 level of safety (you can add 2 if you want more headroom)
	D = Depth;

	// Optional: clamp to something sane (avoid 1<<31 overflow)
	D = FMath::Clamp(Depth, 1, 24);   // 2^(24+1) = 33 million leaves

	// CBT bitfield layout (0-based):
	// internal nodes: 0 .. NumLeaves-1
	// leaves        : NumLeaves .. 2*NumLeaves-1
	const int32 NumLeaves = 1 << D;             // 2^D

	CBT_Buffer.Empty();
	CBT_Buffer.SetNumZeroed(2 * NumLeaves);           // all bits = 0

	// Set the first H leaves (root bisectors) to 1, and calculating the sum reduction tree
	for (int32 h = 0; h < NumLeaves; ++h)
	{
		if (CBT_Buffer.IsValidIndex(NumLeaves + h)) 
		{
			if (h < NumRootBisectors)CBT_Buffer[NumLeaves + h] = 1;
		}
		const int32 Reverse_h = NumLeaves - 1 - h;
		const int32 Reverse_Double_h = 2 * Reverse_h; // 2k node index => child of k node index
		const int32 Node_2k = (Reverse_Double_h >= NumLeaves) ? ((Reverse_Double_h >= NumLeaves + NumRootBisectors) ? 1 : 0) : CBT_Buffer[Reverse_Double_h]; // 2k child node value
		const int32 Node_2kplus1 = (Reverse_Double_h + 1 >= NumLeaves) ? ((Reverse_Double_h+1 >= NumLeaves + NumRootBisectors) ? 1 : 0) : CBT_Buffer[Reverse_Double_h + 1]; // 2k+1 child node value
		CBT_Buffer[Reverse_h] =  Node_2k + Node_2kplus1;

		//if (Node_2k + Node_2kplus1>0) UE_LOG(LogTemp, Warning, TEXT("SUM ID: %d || SUM: %d"), Reverse_h, Node_2k + Node_2kplus1); // LOG INDICES WHICH SUM IS HIGHER THAN 0
	}


	if (CBTResources.IsValid())
	{
		if (CBTResources->IsInitialized())
		{
			BeginReleaseResource(CBTResources.Get());
			FlushRenderingCommands();
		}

		CBTResources.Reset();
	}

	CBTResources = MakeShared<FCBTResource_Interface>();
	CBTResources->PrimeVoronoiBuffers(VoronoiGeoCenters_HL, VoronoiGeoMesh_Ranges, VoronoiGeoMesh_Flat, VoronoiCellColors);
	CBTResources->PrimeTrianglesBuffers(FibonacciPoints_HL, SphericalTrisFlat, SphericalHalfEdges);
	CBTResources->InitFromCPU(D, HalfEdge_Buffer, VoronoiGeoCenters_HL, RootBisectors_Buffer, CBT_Buffer);

	// CBTResources is now valid — recreate the scene proxy so it captures the new pointer.
	// InitRHI runs asynchronously on the render thread; the proxy's GetViewRelevance
	// gates on IsGPUReady() so it will suppress drawing until upload completes.
	//MarkRenderStateDirty();

	//*******************************************************************
	//TEST for lambda function capture of inner parameters with [=, this]
	/*MyStruct TestStruct;
	TestStruct.a = 2;
	TestStruct.b = 5.6;
	auto func = TestStruct.getFunction();
	func();*/
	//*******************************************************************
}

void UGeoDelaunatorComponent::Geo_Circumcenters(TArray<FVector>& Circumcenters, TArray<FVector3_HighLow>& Circumcenters_HL)
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

		/** THIS FIXES THE CASE OF DEGENERATE TRIANGLES LINKING TO THE CENTER OF THE PLANET(0, 0, 0) 
		* WHICH CAUSES NAN CIRCUMCENTERS AND BREAKS EVERYTHING IN THE VORONOI BUILDING
		* VERY IMPORTANT FIX, DON'T SKIP OR TRY TO "CLEAN" THESE TRIANGLES, JUST FIX THE CIRCUMCENTER CALCULATION TO AVOID NANs
		*/
		FVector Normalized = V.GetSafeNormal(); // This is the circumcenter on the unit sphere
		if (Normalized.IsNearlyZero())
		{
			Normalized = (A + B + C).GetSafeNormal();
		}
		/****************************************************************************
		* IMPORTANT FIX
		*****************************************************************************/

		Circumcenters.Add(Normalized);
		Circumcenters_HL.Add(FVector3_HighLow(Normalized));
	}
}

void UGeoDelaunatorComponent::Geo_Centroids(TArray<FVector>& Circumcenters, TArray<FVector3_HighLow>& Circumcenters_HL)
{
	for (const FIntVector& Tri : SphericalTriangles)
	{
		const FVector3d& A = FibonacciPoints[Tri.X];
		const FVector3d& B = FibonacciPoints[Tri.Y];
		const FVector3d& C = FibonacciPoints[Tri.Z];

		const FVector3d Centroid = (A + B + C) / 3.0;
		Circumcenters.Add(Centroid);
		Circumcenters_HL.Add(FVector3_HighLow(Centroid));
	}
}

FGeoPolygonResult UGeoDelaunatorComponent::Geo_Polygons(TArray<FVector>& Circumcenters, TArray<FVector3_HighLow>& Circumcenters_HL)
{
	FGeoPolygonResult Result;

	const int32 NumSites = FibonacciPoints.Num();
	const int32 NumTris = SphericalTriangles.Num();

	// Copy circumcenters into output
	Result.Centers = Circumcenters;
	Result.Centers_HL = Circumcenters_HL;
	Result.Polygons.SetNum(NumSites);

	// init voronoi geo mesh ranges and flat -GPU friendly- arrays
	Result.VoronoiGeoMesh_Ranges.SetNumZeroed(NumSites);
	Result.VoronoiGeoMesh_Flat.Reserve(NumTris * 3); // rough upper bound / over-reserve is fine

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

			// SPHERICAL VORONOI HALF-EDGE 2ND PASS -- BUILDS THE ACTUAL HALF-EDGE STRUCTURE -- NOT FIL's CODE
			int32 ReverseEdgeStart = A;		// START OF REVERSE HALF-EDGE
			int32 ReverseEdgeEnd = B;		// END OF REVERSE HALF-EDGE
			int32 HE_Index = t * 3 + j;		// INDEX OF REVERSE HALF-EDGE

			// Look for reverse match: B → A
			for (int i = 0; i < ReverseEdgesHash[A].Num(); ++i)
			{
				const FReverseHE& entry = ReverseEdgesHash[ReverseEdgeStart][i];
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

	VoronoiHalfEdges_Map.Empty();
	VoronoiHalfEdges_Map.SetNum(NumSites);
	SitePrefixSums.Empty();
	SitePrefixSums.SetNum(NumSites);
	SitePrefixSums[0] = 0;

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

		int32 _VHE_Start = Poly[0].Get<2>();     // Triangle index = circumcenter index
		const int32 _Start_Face = s;             // The site this polygon belongs to

		for (int32 i = 1; i < Poly.Num(); ++i)
		{
			bool Found = false;
			for (const auto& Entry : Poly)
			{
				if (Entry.Get<0>() == k)
				{
					// VORONOI HALF-EDGE STORAGE -- NOT FIL's CODE
					const int32 _VHE_End = Entry.Get<2>(); // Triangle index = circumcenter index
					const int32 _End_Face = k;             // The site the twin half-edge belongs to
					VoronoiHalfEdges_Map[s].Add(FVoronoiHalfEdge(_VHE_Start, _VHE_End, _Start_Face, _End_Face));
					_VHE_Start = _VHE_End;

					// VORONOI HALF-EDGE BUILDING -- FIL's CODE
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

		// CLOSE THE HALF-EDGE RING FOR THE CURRENT SITE --- NOT FIL's CODE
		if (OrderedTris.Num() >= 2)
		{
			int32 Last = _VHE_Start;            // Last triangle index added
			int32 First = Poly[0].Get<2>();     // First triangle index
			int32 EndFace = Poly[0].Get<0>();   // Twin face of first triangle

			VoronoiHalfEdges_Map[s].Add(FVoronoiHalfEdge(Last, First, _Start_Face, EndFace));
		}
		// BUILD PREFIX SUMS FOR VORONOI HALF-EDGE ACCESS ---
		if (s > 0) SitePrefixSums[s] = SitePrefixSums[s - 1] + VoronoiHalfEdges_Map[s].Num();


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

			Result.Centers_HL.Add(FVector3_HighLow(R0));
			Result.Centers_HL.Add(FVector3_HighLow(R1));

			// Final polygon is 4-point pseudo-loop: [C0, R1, C1, R0]
			TArray<int32> FakePoly = { OrderedTris[0], i1, OrderedTris[1], i0 };
			Result.Polygons[s] = FakePoly;

			// populate voronoi geo mesh flat
			const uint32 Start = static_cast<uint32>(Result.VoronoiGeoMesh_Flat.Num());
			Result.VoronoiGeoMesh_Flat.Append(FakePoly);
			Result.VoronoiGeoMesh_Ranges[s] = FUintVector2(Start, static_cast<uint32>(FakePoly.Num()));
		}
		else
		{
			Result.Polygons[s] = OrderedTris;
			const uint32 Start = static_cast<uint32>(Result.VoronoiGeoMesh_Flat.Num());
			Result.VoronoiGeoMesh_Flat.Append(OrderedTris);
			Result.VoronoiGeoMesh_Ranges[s] = FUintVector2(Start, static_cast<uint32>(OrderedTris.Num()));
		}
	}

	return Result;
}
