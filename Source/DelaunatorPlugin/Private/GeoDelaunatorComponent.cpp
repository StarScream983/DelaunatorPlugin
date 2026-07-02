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

	SyncPlanetColorDebugToRenderThread();
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

uint32 UGeoDelaunatorComponent::GetPlanetColorDebugShaderValue_RenderThread() const
{
	return PlanetColorDebugShaderValue.Load();
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
	/** Return the runtime BodySetup used by Chaos.
	*  If collision has not been initialized yet, the final implementation
	*  should lazily create/configure it here via `UpdateBodySetup()`.
	*/
	return nullptr;
}

bool UGeoDelaunatorComponent::GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
	/** Export the generated spherical mesh as raw triangle collision data.
	*
	*  Final implementation should:
	*  1) Push all `FibonacciPoints` into `CollisionData->Vertices`
	*  2) Push all `SphericalTriangles` into `CollisionData->Indices`
	*  3) Set collision flags such as:
	*     - `bFlipNormals`
	*     - `bDeformableMesh`
	*     - `bFastCook`
	*
	*  This is the equivalent of what `UProceduralMeshComponent` does when
	*  providing complex collision from procedural mesh sections.
	*/
	/*if (!MeshBodySetup)
	{
		UpdateBodySetup();
	}

	return MeshBodySetup;*/
	return false;
}

bool UGeoDelaunatorComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	/** Report whether valid procedural triangle collision currently exists.
	*
	*  Final implementation should typically return true when:
	*  - there are generated sphere vertices in `FibonacciPoints`
	*  - there are generated indices in `SphericalTriangles`
	*/
	return false;
}

void UGeoDelaunatorComponent::UpdateBodySetup()
{
	/** Create/configure the runtime BodySetup used for procedural collision.
	*
	*  Final implementation should:
	*  - allocate `MeshBodySetup` if needed
	*  - set `CollisionTraceFlag` (likely `CTF_UseComplexAsSimple`)
	*  - disable mirrored collision if not needed
	*  - enable double-sided geometry if appropriate for the sphere shell
	*/
}

void UGeoDelaunatorComponent::UpdateCollision()
{
	/** Rebuild collision after the procedural spherical mesh changes.
	*
	*  Final implementation should:
	*  - ensure BodySetup exists/configured
	*  - invalidate old physics data
	*  - recreate physics meshes
	*  - recreate the component's physics state if already registered
	*
	*  This should be called after `GeoDelaunayFrom()` updates the CPU mesh.
	*/
	if (!MeshBodySetup)
	{
		MeshBodySetup = NewObject<UBodySetup>(this, UBodySetup::StaticClass());
		MeshBodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
		MeshBodySetup->bMeshCollideAll = true;
	}
}

/*****************************************************************************
*                 END COLLISION DATA PROVIDER INTERFACE                      *
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

	SyncPlanetColorDebugToRenderThread();
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
		// ImGui::Text("Voronoi GeoCenters: %d", VoronoiGeoCenters.Num()); // same as Delaunay Triangles
		ImGui::Text("Delaunay Triangles: %d", SphericalTriangles.Num());

		static const char* PlanetColorDebugLabels[] =
		{
			"Voronoi / plate colors",
			"Elevation heatmap",
			"SiteId hash",
			"1843 colormap (Red Blob)",
			"Distance to boundary (BFS)",
			"Erosion control (Minecraft)",
		};
		int32 ColorDbgIdx = static_cast<int32>(PlanetColorDebug);
		if (ImGui::Combo("Planet color debug", &ColorDbgIdx, PlanetColorDebugLabels, UE_ARRAY_COUNT(PlanetColorDebugLabels)))
		{
			PlanetColorDebug = static_cast<EGeoVoronoiPlanetColorDebug>(FMath::Clamp(ColorDbgIdx, 0, 5));
		}
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

	SyncPlanetColorDebugToRenderThread();

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
		/*const uint32 R = RngStream.RandRange(0, 255);
		const uint32 G = RngStream.RandRange(0, 255);
		const uint32 B = RngStream.RandRange(0, 255);
		const uint32 A = 255;

		const uint32 PackedColor = R | (G << 8) | (B << 16) | (A << 24);
		VoronoiCellColors.Add(PackedColor);*/
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

	GeneratePlates_RedBlobRandomFill();
	//AssignElevations_RedBlob1843();
	AssignElevations();
	BuildErosionControlPerSite();

	/**IT CAN BE ADDED TO THE SECOND PASS OF THE BFS, FROM BOUDARY TO PLATE CENTER,
	*  WHERE THE DISTANCE TO BOUNDARY IS CALCULATED SIMULTANEOUSLY AS THE ELEVATION ASSIGNMENT, 
	*  THIS WAY WE AVOID AN EXTRA PASS JUST FOR THE DISTANCE CALCULATION */
	const int32 NumSitesForDebug = PlateIdPerSite.Num();
	TArray<float> DistanceToBoundaryNorm;
	DistanceToBoundaryNorm.SetNumUninitialized(NumSitesForDebug);
	int32 MaxBoundaryDist = 1;
	for (int32 Si = 0; Si < NumSitesForDebug; ++Si)
	{
		const int32 d = DistanceToBoundary[Si];
		if (d != INT32_MAX && d > MaxBoundaryDist)
		{
			MaxBoundaryDist = d;
		}
	}
	const float InvMaxBoundaryDist = 1.0f / static_cast<float>(MaxBoundaryDist);
	for (int32 Si = 0; Si < NumSitesForDebug; ++Si)
	{
		const int32 d = DistanceToBoundary[Si];
		DistanceToBoundaryNorm[Si] = (d == INT32_MAX) ? 0.0f : static_cast<float>(d) * InvMaxBoundaryDist;
	}
	//*******************************************************************

	CBTResources = MakeShared<FCBTResource_Interface>();
	CBTResources->PrimeVoronoiBuffers(VoronoiGeoCenters_HL, VoronoiGeoMesh_Ranges, VoronoiGeoMesh_Flat, VoronoiCellColors);
	CBTResources->PrimeTrianglesBuffers(FibonacciPoints_HL, SphericalTrisFlat, SphericalHalfEdges);
	CBTResources->PrimeElevationPerSiteBuffer(ElevationPerSite);
	CBTResources->PrimeDistanceToBoundaryNormPerSiteBuffer(DistanceToBoundaryNorm);
	CBTResources->PrimeErosionControlPerSiteBuffer(ErosionControlPerSite);
	CBTResources->InitFromCPU(D, HalfEdge_Buffer, VoronoiGeoCenters_HL, RootBisectors_Buffer, CBT_Buffer);

	// CBTResources is now valid — recreate the scene proxy so it captures the new pointer.
	// InitRHI runs asynchronously on the render thread; the proxy's GetViewRelevance
	// gates on IsGPUReady() so it will suppress drawing until upload completes.
	MarkRenderStateDirty();

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

		/****************************************************************************
		 * IMPORTANT FIX FOR NAN CIRCUMCENTERS
		 * In the case of degenerate triangles linking to the center of the planet (0,0,0), the cross product sum can be zero, leading to NaN circumcenters after normalization. 
		 * This breaks everything in the Voronoi building since circumcenters are used as Voronoi vertices. 
		 * The fix is to check if the cross product sum is nearly zero, and if so, use the normalized average of the triangle vertices as a fallback circumcenter. 
		 * This ensures we get a valid point on the sphere even for degenerate triangles, and avoids NaNs that would break the Voronoi construction.
		 * VERY IMPORTANT FIX, DON'T SKIP OR TRY TO "CLEAN" THESE TRIANGLES, JUST FIX THE CIRCUMCENTER CALCULATION TO AVOID NANs
		 ****************************************************************************/
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

void UGeoDelaunatorComponent::GeneratePlates_RedBlobRandomFill()
{
	if (N <= 100) return;

	PlateIdPerSite.Init(-1, N);
	SiteParent.Init(-1, N);
	PlateBoundaries.Reset();
	VoronoiCellColors.Init(0, N);  // initialize all black

	TArray<int32> Queue = PickRandomPlateSeeds(NumPlates, Plates);
	TArray<int32> Neighbors;
	TArray<int32> HalfEdges;

	for (int32 QueueOut = 0; QueueOut < Queue.Num(); ++QueueOut)
	{
		const int32 Remaining = Queue.Num() - QueueOut;
		const int32 Pos = QueueOut + RngStream.RandRange(0, Remaining - 1);
		const int32 CurrentSite = Queue[Pos];
		Queue[Pos] = Queue[QueueOut];

		GetVoronoiNeighbors(CurrentSite, Neighbors, HalfEdges);

		for (int32 j = 0; j < Neighbors.Num(); ++j)
		{
			const int32 NeighborSite = Neighbors[j];

			if (PlateIdPerSite[NeighborSite] == -1)
			{
				PlateIdPerSite[NeighborSite] = PlateIdPerSite[CurrentSite];
				VoronoiCellColors[NeighborSite] = Plates[PlateIdPerSite[CurrentSite]].PackedColor;
				SiteParent[NeighborSite] = CurrentSite;  // ← spanning tree
				Queue.Add(NeighborSite);
			}
			else if (PlateIdPerSite[NeighborSite] != PlateIdPerSite[CurrentSite]
				&& CurrentSite < NeighborSite)  // process each pair once
			{
				// Border detected → paint both sites white
				VoronoiCellColors[CurrentSite] = 0xFFFFFFFF;
				VoronoiCellColors[NeighborSite] = 0xFFFFFFFF;

				const int32 GlobalAB = HalfEdges[j];
				const int32 GlobalBA = HalfEdge_Buffer[GlobalAB].Twin;

				const FPlateData& PA = Plates[PlateIdPerSite[CurrentSite]];
				const FPlateData& PB = Plates[PlateIdPerSite[NeighborSite]];

				const FVector RelativeMotion =
					(PA.DriftDirection * (float)PA.DriftSpeed) -
					(PB.DriftDirection * (float)PB.DriftSpeed);

				const FVector BoundaryNormal =
					(FibonacciPoints[NeighborSite] - FibonacciPoints[CurrentSite]).GetSafeNormal();

				// Tangent along the boundary on the sphere surface.
				// Cross of the local radial (Fibonacci point ≈ unit normal at the surface) with
				// the boundary normal gives the direction perpendicular to both → along the border.
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
			}
		}
	}

	//BuildPlateDebugColors();
}

void UGeoDelaunatorComponent::GetVoronoiNeighbors(
	int32               SiteIndex,
	TArray<int32>& OutNeighbors,
	TArray<int32>& OutHalfEdgeIndices)  // ← index into VoronoiHalfEdges_Map[SiteIndex] 
	const
{
	OutNeighbors.Reset();
	OutHalfEdgeIndices.Reset();

	if (!VoronoiHalfEdges_Map.IsValidIndex(SiteIndex)) return;

	const TArray<FVoronoiHalfEdge>& Ring = VoronoiHalfEdges_Map[SiteIndex];
	const int32 Base = SitePrefixSums[SiteIndex];

	OutNeighbors.Reserve(Ring.Num());
	OutHalfEdgeIndices.Reserve(Ring.Num());

	for (int32 i = 0; i < Ring.Num(); ++i)
	{
		if (Ring[i].End_Face >= 0 && Ring[i].End_Face != SiteIndex)
		{
			OutNeighbors.Add(Ring[i].End_Face);
			OutHalfEdgeIndices.Add(Base + i);
		}
	}
}

TArray<int32> UGeoDelaunatorComponent::PickRandomPlateSeeds(int32 Count, TArray<FPlateData>& OutSeeds)
{
	OutSeeds.Reset();

	const int32 Target = FMath::Min(Count, N);

	TSet<int32> Chosen;
	OutSeeds.Reserve(Target);

	while (Chosen.Num() < Target)
	{
		const int32 SeedSite = RngStream.RandRange(0, N - 1);
		if (Chosen.Contains(SeedSite)) continue;

		Chosen.Add(SeedSite);
		FPlateData newPlate(SeedSite);
		newPlate.bIsOceanic = (RngStream.FRand() < OceanicRatio);
		newPlate.DesiredElevation = newPlate.bIsOceanic
			? -0.8 + RngStream.FRand() * 0.4   // [-0.8, -0.3] // originally -0.8 + RngStream.FRand() * 0.5
			: 0.1 + RngStream.FRand() * 0.2;  // [ 0.1,  0.6] // originally 0.1 + RngStream.FRand() * 0.5

		/** Compute a random tangent vector at this seed's surface point,
		* Gram-Schmidt projection. DotProduct(RandomVec, SeedNormal) measures how much of RandomVec 
		* points in the normal direction. Multiplying by SeedNormal gives that component as a vector. 
		* Subtracting it removes it — what remains lies entirely in the tangent plane.
		*/
		const FVector SeedNormal = FibonacciPoints[SeedSite];  // already unit
		// Pick a random vector, project out the normal component → tangent
		FVector RandomVec, Tangent;
		do {
			RandomVec = FMath::VRand();
			RandomVec -= SeedNormal * FVector::DotProduct(RandomVec, SeedNormal);
			Tangent = RandomVec.GetSafeNormal();
		} while (Tangent.IsNearlyZero());  // retry if degenerate — happens < 0.001% of the time
		newPlate.DriftDirection = Tangent;
		newPlate.DriftSpeed = 0.5 + RngStream.FRand();  // [0.5, 1.5]

		newPlate.PackedColor = BuildPackedColor(OutSeeds.Num());
		PlateIdPerSite[SeedSite] = OutSeeds.Num();
		OutSeeds.Add(newPlate);
		VoronoiCellColors[SeedSite] = newPlate.PackedColor; // paint immediately
	}
	return Chosen.Array();
}

TArray<int32> UGeoDelaunatorComponent::GetAncestorChain(int32 StartSite) const
{
	TArray<int32> Chain;
	int32 Current = StartSite;
	while (Current != INDEX_NONE)
	{
		Chain.Add(Current);
		Current = SiteParent[Current];
	}
	return Chain;  // [StartSite → ... → PlateSeed]
}

#pragma region Plate boundary elevation - ComputeBoundaryElevation (legacy step)
// Given one boundary edge and the two plates on either side,
// returns the elevation spike or trough at that boundary in [-1.0, +1.0].
double  UGeoDelaunatorComponent::ComputeBoundaryElevation(const FPlateBoundary& Boundary, const FPlateData& PlateA, const FPlateData& PlateB)
{
	// Positive = converging (colliding), negative = diverging (spreading apart)
	double Pressure = Boundary.Pressure;
	// Higher of the two resting elevations — crust piles toward the higher plate on collision
	double MaxDesired = FMath::Max(PlateA.DesiredElevation, PlateB.DesiredElevation);
	// Lower of the two — crust thins toward the weaker plate on divergence
	double MinDesired = FMath::Min(PlateA.DesiredElevation, PlateB.DesiredElevation);

	// Exactly one of these three will be true — determines the geological event
	bool bBothOceanic = PlateA.bIsOceanic && PlateB.bIsOceanic;
	bool bBothContinental = !PlateA.bIsOceanic && !PlateB.bIsOceanic;
	bool bSubduction = PlateA.bIsOceanic != PlateB.bIsOceanic;

	if (Pressure > 0.0) // converging
	{
		if (bBothContinental)
			// Head-on collision, neither subducts — crust crumples upward (Himalayas style)
			// 0.5 multiplier = tallest result of the three converging cases
			return MaxDesired + Pressure * 0.5;   // high mountain range

		if (bSubduction)
			// Oceanic plate (denser) dives under continental — continental side rises
			// 0.3 multiplier = coastal ranges, tall but less extreme than continent-continent
			// Oceanic trench on the other side is handled by the diverging case or post-pass
			return MaxDesired + Pressure * 0.3;   // coastal mountains (continental side)

		// bBothOceanic
		// bBothOceanic — one subducts, creating a volcanic island arc
		// MaxDesired ~ -0.3 (shallowest oceanic floor)
		// 0.8 multiplier strong enough to push above 0.0 at sufficient pressure → island chain
		return MaxDesired + Pressure * 0.8;       // modest island arc
	}

	// diverging — rift or mid-ocean ridge
	// Diverging — plates spreading apart, crust thins and sinks
	// Pressure is negative here, so this subtracts from MinDesired
	// Produces rift valleys on land or mid-ocean ridges underwater
	// 0.2 multiplier keeps rifts shallow — dips below plate floor but not drastically
	return MinDesired + Pressure * 0.2;           // Pressure < 0 → dips lower
}
#pragma endregion

#pragma region Plate boundary elevation - ComputeBoundaryElevation_Gainey
// Given one boundary edge and the two plates on either side,
// returns the elevation spike or trough at that boundary in [-1.0, +1.0].
// Smooth variant: removes the hard step at Pressure = 0 by blending the base
// elevation continuously between MinDesired (diverging) and MaxDesired (converging).
double UGeoDelaunatorComponent::ComputeBoundaryElevation_Gainey(
    const FPlateBoundary& Boundary,
    const FPlateData& PlateA,
    const FPlateData& PlateB)
{
    // Positive = converging (colliding), negative = diverging (spreading apart)
    const double Pressure = Boundary.Pressure;

    // Reference levels: crust piles toward the higher plate on collision,
    // crust thins toward the lower plate on divergence.
    const double MaxDesired = FMath::Max(PlateA.DesiredElevation, PlateB.DesiredElevation);
    const double MinDesired = FMath::Min(PlateA.DesiredElevation, PlateB.DesiredElevation);

    // Tectonic interaction style — picks the geological event multiplier.
    const bool bBothContinental = !PlateA.bIsOceanic && !PlateB.bIsOceanic;
    const bool bSubduction      = PlateA.bIsOceanic != PlateB.bIsOceanic;
    // bBothOceanic = the remaining case

    // Per-style multipliers (kept identical to the original step version):
    //   0.5 = continent-continent collision  → tallest mountains (Himalayas)
    //   0.3 = subduction                      → coastal ranges (continental side)
    //   0.8 = ocean-ocean                     → island arc, can rise above sea level
    //   0.2 = diverging                       → shallow rifts / mid-ocean ridges
    const double ConvergeCoeff = bBothContinental ? 0.5 : (bSubduction ? 0.3 : 0.8);
    const double DivergeCoeff  = 0.2;

    // Smooth blend factor in [0, 1]: 0 = fully diverging regime, 1 = fully converging.
    // At Pressure = 0 this is exactly 0.5 → Base = mean(Max, Min). No step.
    // TransitionWidth controls how wide the "transform / neutral" band is
    // (same units as Pressure). Smaller = sharper change, larger = softer blend.
    // Default 0.5 covers ~|Pressure| < 1 as the transition zone given drift speeds in [0.5, 1.5].
    constexpr double TransitionWidth = 0.5;
    const double x = Pressure / TransitionWidth;
    const double SmoothSign = x / FMath::Sqrt(1.0 + x * x);   // softsign ∈ (-1, +1)
    const double t = 0.5 + 0.5 * SmoothSign;                  // ∈ (0, 1)

    // Continuous base elevation — glides from MinDesired through the mean to MaxDesired
    // as Pressure goes from very negative to very positive. Removes the discontinuity.
    const double Base = FMath::Lerp(MinDesired, MaxDesired, t);

    // Continuous slope coefficient — also smooths the derivative kink at Pressure = 0,
    // so the rate of change doesn't jump from 0.2 to 0.5/0.3/0.8 across the neutral line.
    const double Coeff = FMath::Lerp(DivergeCoeff, ConvergeCoeff, t);

    // Final elevation:
    //  Pressure > 0 → Coeff * Pressure adds      → rises above the higher plate (mountain)
    //  Pressure < 0 → Coeff * Pressure subtracts → dips below the lower plate (rift)
    //  Pressure = 0 → returns the mean of the two plate floors (transform / neutral)
    return Base + Coeff * Pressure;
}
#pragma endregion

#pragma region Plate boundary elevation - ComputeBoundaryElevation_Hybrid
// Hybrid: Gainey's regime classification + convergence/shear/dormant branches,
// with the divergence branch anchored on MinElev so divergent boundaries depress
// below the lower plate floor (rift valleys, oceanic trenches).
//
// IMPORTANT: StressSaturation is tuned for raw inputs whose magnitude is at most
// |DriftSpeedA + DriftSpeedB| ≈ 3.0. If you change drift speeds, retune this.
// (Gainey used 30 because his raw stress was in the dozens; ours peaks at ~3.)
double UGeoDelaunatorComponent::ComputeBoundaryElevation_Hybrid(
    const FPlateBoundary& Boundary,
    const FPlateData& PlateA,
    const FPlateData& PlateB)
{
    // Tune to ~half your maximum raw pressure for a balanced response curve.
    // 1.0 maps raw |Pressure| = 3 → sigmoid ≈ ±0.9, raw |Pressure| = 1 → ±0.46.
    constexpr double StressSaturation = 1.5;

    auto Sigmoid = [](double Raw) -> double
    {
        return 2.0 / (1.0 + FMath::Exp(-Raw / StressSaturation)) - 1.0;
    };

    const double Pressure = Sigmoid(Boundary.Pressure);   // ∈ [-1, +1] (signed)
    const double Shear    = Sigmoid(Boundary.Shear);      // ∈ [ 0, +1] (magnitude)

    const double ElevA = PlateA.DesiredElevation;
    const double ElevB = PlateB.DesiredElevation;
    const double MaxElev = FMath::Max(ElevA, ElevB);
    const double MinElev = FMath::Min(ElevA, ElevB);

    // Strong convergence — Gainey: rises above MaxElev, linear in pressure.
    if (Pressure > 0.3)
        return MaxElev + Pressure;

    // Strong divergence — DEVIATION FROM GAINEY: anchor on MinElev with negative
    // offset so divergent boundaries actually depress below the lower plate floor.
    // Pressure < 0 here → MinElev + Pressure*0.25 < MinElev → trench / rift.
    if (Pressure < -0.3)
        return MinElev + Pressure * 0.25;

    // Transform / shearing — Gainey: small uplift along strike-slip faults.
    if (Shear > 0.3)
        return MaxElev + Shear * 0.125;

    // Dormant — Gainey: mean of the two plate floors.
    return (ElevA + ElevB) * 0.5;
}
#pragma endregion

#pragma region Plate boundary elevation - ComputeBoundaryElevation_Hybrid2
// Continuous version: no discrete regime branches. Same four "modes" as the hybrid,
// blended with smooth weights from pressure/shear so crossing a threshold is not a step.
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
	// Per-mode boundary elevation (same formulas as the old hybrid branches).
	const double EConv  = MaxElev + Pressure;              // convergent uplift
	const double EDiv   = MinElev + Pressure * 0.25;          // divergent trough (your deviation)
	const double EShear = MaxElev + Shear * 0.125;            // strike-slip bump (Gainey scale)
	const double EDorm  = MeanElev;
	// Soft weights: replace Gainey's |P|>0.3 checks with ramps. Tune Edge0/Edge1
	// to control how "wide" the transition band is (narrower = closer to sharp thresholds).
	const double WConvRaw  = FMath::SmoothStep(0.10, 0.40, Pressure);   // grows with +P
	const double WDivRaw   = FMath::SmoothStep(0.10, 0.40, -Pressure);  // grows with -P
	// Shear only contributes when pressure is not already picking conv or div strongly.
	const double WShearRaw = FMath::SmoothStep(0.12, 0.42, Shear)
		* (1.0 - FMath::Max(WConvRaw, WDivRaw));
	double Wc = WConvRaw;
	double Wd = WDivRaw;
	double Ws = WShearRaw;
	// If conv+div+shear sum past 1, scale them down so the blend stays convex.
	double Wsum = Wc + Wd + Ws;
	if (Wsum > 1.0 - KINDA_SMALL_NUMBER)
	{
		const double Inv = 1.0 / Wsum;
		Wc *= Inv;
		Wd *= Inv;
		Ws *= Inv;
	}
	const double W0 = 1.0 - Wc - Wd - Ws; // dormant / "quiet" contribution
	return W0 * EDorm + Wc * EConv + Wd * EDiv + Ws * EShear;
}
#pragma endregion

#pragma region Red Blob 1843 elevation (file-local noise + distance fields + site heightfield)
namespace
{
/** fBm matching planet-generation.js weights; uses smooth Perlin (Red Blob uses Simplex — same role). */
double RedBlobFbmNoiseOctaves(double X, double Y, double Z)
{
	constexpr double Persistence = 2.0 / 3.0;
	double Sum = 0.0;
	double SumAmp = 0.0;
	for (int32 Octave = 0; Octave < 5; ++Octave)
	{
		const double F = static_cast<double>(1 << Octave);
		const double Amp = FMath::Pow(Persistence, static_cast<double>(Octave));
		const FVector P(
			static_cast<float>(X * F),
			static_cast<float>(Y * F),
			static_cast<float>(Z * F));
		Sum += Amp * static_cast<double>(FMath::PerlinNoise3D(P));
		SumAmp += Amp;
	}
	return SumAmp > KINDA_SMALL_NUMBER ? (Sum / SumAmp) : 0.0;
}

static constexpr float RedBlobDistInf = 1.0e30f;

static bool RedBlobDistIsInf(float D) { return D > (RedBlobDistInf * 1.0e-6f); }
}

void UGeoDelaunatorComponent::RedBlobAssignDistanceField(
	const TSet<int32>& Seeds,
	const TSet<int32>& StopBlocks,
	TArray<float>& OutDist)
{
	const int32 NumSites = PlateIdPerSite.Num();
	if (NumSites == 0)
	{
		OutDist.Reset();
		return;
	}

	OutDist.Init(RedBlobDistInf, NumSites);
	TArray<int32> Queue;
	Queue.Reserve(NumSites / 8);
	for (const int32 Seed : Seeds)
	{
		if (!OutDist.IsValidIndex(Seed))
		{
			continue;
		}
		OutDist[Seed] = 0.f;
		Queue.Add(Seed);
	}

	TArray<int32> Neighbors;
	TArray<int32> HalfEdgeIdxUnused;
	int32 QOut = 0;
	while (QOut < Queue.Num())
	{
		const int32 Span = Queue.Num() - QOut;
		const int32 PickOffset = (Span > 1) ? RngStream.RandRange(0, Span - 1) : 0;
		const int32 Pick = QOut + PickOffset;
		const int32 Current = Queue[Pick];
		Queue[Pick] = Queue[QOut];
		++QOut;

		const float BaseDist = OutDist[Current];
		GetVoronoiNeighbors(Current, Neighbors, HalfEdgeIdxUnused);
		for (const int32 Neighbor : Neighbors)
		{
			if (StopBlocks.Contains(Neighbor))
			{
				continue;
			}
			if (RedBlobDistIsInf(OutDist[Neighbor]))
			{
				OutDist[Neighbor] = BaseDist + 1.f;
				Queue.Add(Neighbor);
			}
		}
	}
}

void UGeoDelaunatorComponent::AssignElevationFromRedBlob1843(TArray<float>& OutElevation)
{
	// Faithful to planet-generation.js assignRegionElevation + findCollisions + assignDistanceField:
	// https://github.com/redblobgames/1843-planet-generation/blob/main/planet-generation.js
	const int32 NumSites = PlateIdPerSite.Num();
	if (NumSites == 0)
	{
		OutElevation.Reset();
		return;
	}

	OutElevation.SetNumUninitialized(NumSites);

	// Plate motion MUST match JS generatePlates: unit chord from seed to first circulated neighbor,
	// then findCollisions moves positions by plate_vec[r_plate[r]] * deltaTime only (no extra speed).
	TArray<FVector> RedBlobPlateUnitAtSeed;
	RedBlobPlateUnitAtSeed.Init(FVector::ZeroVector, NumSites);
	TArray<int32> Neighbors;
	TArray<int32> HalfEdgeIdxUnused;
	for (int32 Pi = 0; Pi < Plates.Num(); ++Pi)
	{
		const int32 SeedR = Plates[Pi].SeedSite;
		if (!FibonacciPoints.IsValidIndex(SeedR))
		{
			continue;
		}
		GetVoronoiNeighbors(SeedR, Neighbors, HalfEdgeIdxUnused);
		if (Neighbors.Num() == 0)
		{
			continue;
		}
		const FVector Chord = FibonacciPoints[Neighbors[0]] - FibonacciPoints[SeedR];
		const FVector Dir = Chord.GetSafeNormal();
		if (!Dir.IsNearlyZero())
		{
			RedBlobPlateUnitAtSeed[SeedR] = Dir;
		}
	}

	constexpr double DeltaTime = 1.0e-2;
	constexpr double CollisionScale = 0.75;
	const double CollideThresh = CollisionScale * DeltaTime;

	TSet<int32> MountainSeedSites;
	TSet<int32> CoastlineSites;
	TSet<int32> OceanSites;

	for (int32 CurrentR = 0; CurrentR < NumSites; ++CurrentR)
	{
		const int32 PlateIdxA = PlateIdPerSite[CurrentR];
		double BestCompression = TNumericLimits<double>::Max();
		int32 BestR = -1;
		GetVoronoiNeighbors(CurrentR, Neighbors, HalfEdgeIdxUnused);
		for (const int32 NeighborR : Neighbors)
		{
			const int32 PlateIdxB = PlateIdPerSite[NeighborR];
			if (PlateIdxA == PlateIdxB)
			{
				continue;
			}

			const FVector& P0 = FibonacciPoints[CurrentR];
			const FVector& P1 = FibonacciPoints[NeighborR];
			const int32 SeedA = Plates[PlateIdxA].SeedSite;
			const int32 SeedB = Plates[PlateIdxB].SeedSite;
			const FVector U0 = RedBlobPlateUnitAtSeed[SeedA] * static_cast<float>(DeltaTime);
			const FVector U1 = RedBlobPlateUnitAtSeed[SeedB] * static_cast<float>(DeltaTime);
			const double D0 = FVector::Dist(P0, P1);
			const double D1 = FVector::Dist(P0 + U0, P1 + U1);
			const double Compression = D0 - D1;
			if (Compression < BestCompression)
			{
				BestCompression = Compression;
				BestR = NeighborR;
			}
		}

		if (BestR == -1)
		{
			continue;
		}

		const int32 PlateIdxB = PlateIdPerSite[BestR];
		const bool bCollided = BestCompression > CollideThresh;
		const bool bOceanA = Plates[PlateIdxA].bIsOceanic;
		const bool bOceanB = Plates[PlateIdxB].bIsOceanic;

		if (bOceanA && bOceanB)
		{
			if (bCollided) { CoastlineSites.Add(CurrentR); }
			else { OceanSites.Add(CurrentR); }
		}
		else if (!bOceanA && !bOceanB)
		{
			if (bCollided)
			{
				MountainSeedSites.Add(Plates[PlateIdxA].SeedSite);
			}
		}
		else
		{
			if (bCollided) { MountainSeedSites.Add(CurrentR); }
			else { CoastlineSites.Add(CurrentR); }
		}
	}

	for (int32 R = 0; R < NumSites; ++R)
	{
		const int32 PIdx = PlateIdPerSite[R];
		if (Plates[PIdx].SeedSite != R)
		{
			continue;
		}
		if (Plates[PIdx].bIsOceanic)
		{
			OceanSites.Add(R);
		}
		else
		{
			CoastlineSites.Add(R);
		}
	}

	TSet<int32> StopR;
	for (const int32 S : MountainSeedSites) { StopR.Add(S); }
	for (const int32 S : CoastlineSites) { StopR.Add(S); }
	for (const int32 S : OceanSites) { StopR.Add(S); }

	TArray<float> DistA, DistB, DistC;
	RedBlobAssignDistanceField(MountainSeedSites, OceanSites, DistA);
	RedBlobAssignDistanceField(OceanSites, CoastlineSites, DistB);
	RedBlobAssignDistanceField(CoastlineSites, StopR, DistC);

	constexpr double Epsilon = 1.0e-3;
	for (int32 R = 0; R < NumSites; ++R)
	{
		const double A = static_cast<double>(DistA[R]) + Epsilon;
		const double B = static_cast<double>(DistB[R]) + Epsilon;
		const double C = static_cast<double>(DistC[R]) + Epsilon;
		double E = 0.1;
		if (!(RedBlobDistIsInf(DistA[R]) && RedBlobDistIsInf(DistB[R])))
		{
			E = (1.0 / A - 1.0 / B) / (1.0 / A + 1.0 / B + 1.0 / C);
		}

		const FVector& P = FibonacciPoints[R];
		const double Noise = RedBlobFbmNoiseOctaves(static_cast<double>(P.X), static_cast<double>(P.Y), static_cast<double>(P.Z));
		E += 0.1 * Noise;
		OutElevation[R] = static_cast<float>(E);
	}
}

void UGeoDelaunatorComponent::FillDistanceToBoundaryBFS()
{
	const int32 NumSites = PlateIdPerSite.Num();
	DistanceToBoundary.Init(INT32_MAX, NumSites);
	TQueue<int32> Queue;

	auto TryEnqueueBoundarySite = [&](int32 Site)
		{
			if (!DistanceToBoundary.IsValidIndex(Site))
			{
				return;
			}
			if (DistanceToBoundary[Site] == INT32_MAX)
			{
				DistanceToBoundary[Site] = 0;
				Queue.Enqueue(Site);
			}
		};

	for (const FPlateBoundary& Boundary : PlateBoundaries)
	{
		TryEnqueueBoundarySite(Boundary.SiteA);
		TryEnqueueBoundarySite(Boundary.SiteB);
	}

	TArray<int32> Neighbors;
	TArray<int32> HalfEdgeIndicesUnused;
	int32 CurrentSite = 0;
	while (Queue.Dequeue(CurrentSite))
	{
		const int32 CurDist = DistanceToBoundary[CurrentSite];
		GetVoronoiNeighbors(CurrentSite, Neighbors, HalfEdgeIndicesUnused);
		for (const int32 Neighbor : Neighbors)
		{
			if (DistanceToBoundary[Neighbor] == INT32_MAX)
			{
				DistanceToBoundary[Neighbor] = CurDist + 1;
				Queue.Enqueue(Neighbor);
			}
		}
	}
}

void UGeoDelaunatorComponent::AssignElevations_RedBlob1843()
{
	const int32 NumSites = PlateIdPerSite.Num();
	ElevationPerSite.Init(0.0f, NumSites);

	//BlurBoundaryStress(/*Iterations=*/3, /*CenterWeight=*/0.4);
	AssignElevationFromRedBlob1843(ElevationPerSite);
	FillDistanceToBoundaryBFS();
}
#pragma endregion

#pragma region Plate elevation after boundary kernels (blur stress, distance BFS, assign entry points)
// ── Boundary stress smoothing ───────────────────────────────────────────────────
// Faithful port of Gainey's blurPlateBoundaryStress(boundaryCorners, 3, 0.4).
//
// Two FPlateBoundary entries are neighbours when they share a Voronoi site
// (i.e. they connect along the same plate-edge curve). Each iteration replaces
// every boundary's (Pressure, Shear) with a center-weighted blend of itself
// and the mean of its neighbours:
//
//     new = own * CenterWeight + mean(neighbours) * (1 - CenterWeight)
//
// Effect: high-frequency variation in BoundaryNormal direction (concave/convex
// pockets along curving borders) gets averaged out, so adjacent boundary cells
// no longer randomly straddle the |Pressure| > 0.3 regime threshold and no
// longer produce single-cell elevation spikes.
//
// Implementation note: scatter/gather with per-site accumulators — no nested
// loops, no TMap, no per-boundary neighbour list. The trick is that the sum of
// all boundaries touching either endpoint site of boundary B already equals
// (sum over neighbours of B) + 2*B (B contributes once at each of its sites).
// Subtract 2*B and you have the neighbour sum directly.
void UGeoDelaunatorComponent::BlurBoundaryStress(int32 Iterations, double CenterWeight)
{
	const int32 NumB = PlateBoundaries.Num();
	if (NumB == 0 || Iterations <= 0) return;

	const int32  NumSites       = PlateIdPerSite.Num();
	const double NeighborWeight = 1.0 - CenterWeight;

	// Per-site accumulators — reused each iteration via Memzero rather than reallocated
	TArray<double> SitePressureSum; SitePressureSum.SetNumZeroed(NumSites);
	TArray<double> SiteShearSum;    SiteShearSum   .SetNumZeroed(NumSites);
	TArray<int32>  SiteCount;       SiteCount      .SetNumZeroed(NumSites);

	// Output buffers — written in full each iteration so the pass is non-destructive
	TArray<double> NewPressure; NewPressure.SetNumUninitialized(NumB);
	TArray<double> NewShear;    NewShear   .SetNumUninitialized(NumB);

	for (int32 Iter = 0; Iter < Iterations; ++Iter)
	{
		// Reset accumulators in place — cheaper than reallocating
		FMemory::Memzero(SitePressureSum.GetData(), sizeof(double) * NumSites);
		FMemory::Memzero(SiteShearSum   .GetData(), sizeof(double) * NumSites);
		FMemory::Memzero(SiteCount      .GetData(), sizeof(int32)  * NumSites);

		// ── Scatter: every boundary deposits its values into its two endpoint sites ──
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

		// ── Gather: each boundary reads its neighbour mean from the site sums ──
		// Self appears once in each of its two site sums, so subtract 2*self to exclude it.
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
				// Isolated boundary edge — keep as-is
				NewPressure[i] = B.Pressure;
				NewShear   [i] = B.Shear;
			}
		}

		// ── Commit: copy this iteration's results back so the next iteration sees them ──
		for (int32 i = 0; i < NumB; ++i)
		{
			PlateBoundaries[i].Pressure = NewPressure[i];
			PlateBoundaries[i].Shear    = NewShear[i];
		}
	}
}

// 2ND PASS, BOUNDARY-TO-INTERIOR PROPAGATION BFS, ASSIGN ELEVATION, DISTANCE TO BOUNDARY, GEOLOGICAL PROVINCES
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
		const double BoundaryElev = ComputeBoundaryElevation(Boundary, PlateA, PlateB);

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

	// MAXDIST to calculate minecraft erosion control
	float MaxDist = 1.f;

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
				// double DistanceFactor = Sleef_exp_u10(-NewDist * DecayRate);

				// GAUSSIAN — narrow mountain ridge, steeper than exponential
				double DistanceFactor = Sleef_exp_u10(-(double)(NewDist * NewDist) * DecayRate);
				// DecayRate 0.08 = wide range, 0.25 = narrow alpine ridge

				// POWER CURVE — designer-tunable sharpness
				//const int32 MaxDist = 2; // hops before elevation hits floor
				//double t = FMath::Clamp((double)NewDist / MaxDist, 0.0, 1.0);
				//double DistanceFactor = FMath::Max(0.0, 1.0 - FMath::Pow(t, 2.5));
				// exponent 1.0 = linear, 2.0 = quadratic, 3.0+ = sharp peak

				// INVERSE SQUARE — fast near boundary, very long tail
				//const double k = 0.3;
				//double DistanceFactor = 1.0 / (1.0 + (double)(NewDist * NewDist) * k);

				DistanceToBoundary[NeighborSite] = NewDist;
				NearestBoundaryElevation[NeighborSite] = NearestElev;

				// for minecraft erosion control
				MaxDist = FMath::Max(MaxDist, static_cast<float>(NewDist));

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

	// invMaxDist for minecraft erosion control
	InvMaxDist = 1.0f / MaxDist;
}

// i need to find where to incorporate
void UGeoDelaunatorComponent::BuildErosionControlPerSite()
{
	const int32 NumSites = PlateIdPerSite.Num();
	ErosionControlPerSite.SetNumUninitialized(NumSites);
	for (int32 R = 0; R < NumSites; ++R)
	{
		const int32 d = DistanceToBoundary[R];
		const float t = (d == INT32_MAX) ? 0.0f : static_cast<float>(d) * InvMaxDist;

		const FVector& P = FibonacciPoints[R];
		const double n = RedBlobFbmNoiseOctaves(P.X * 0.01, P.Y * 0.01, P.Z * 0.01); //0.004

		float E = static_cast<float>(0.65 * t + 0.35 * n);
		ErosionControlPerSite[R] = FMath::Clamp(2.0f * E - 1.0f, -1.0f, 1.0f);
	}
}
#pragma endregion


uint32 UGeoDelaunatorComponent::BuildPackedColor(const int32 PlateIndex) const
{
	// Hash the index to get uncorrelated R, G, B
	uint32 H = (uint32)PlateIndex;
	H ^= H << 13; H ^= H >> 7; H ^= H << 17;  // xorshift

	const uint32 R = 30 + (H & 0xFF) % 225;
	const uint32 G = 30 + ((H >> 8) & 0xFF) % 225;
	const uint32 B = 30 + ((H >> 16) & 0xFF) % 225;

	return R | (G << 8) | (B << 16) | (255u << 24);
}

void UGeoDelaunatorComponent::BuildPlateDebugColors()
{
	VoronoiCellColors.Init(0, N);

	for (int32 Site = 0; Site < N; ++Site)
	{
		const int32 PlateIdx = PlateIdPerSite[Site];
		VoronoiCellColors[Site] = Plates.IsValidIndex(PlateIdx)
			? Plates[PlateIdx].PackedColor
			: 0xFFFFFFFF;
	}
}



// PLATES WITH WARP
//void UGeoDelaunatorComponent::GeneratePlates_NearestNeighbor()
//{
//	if (N <= 0)
//	{
//		return;
//	}
//
//	PlateIdPerSite.Init(-1, N);
//
//	PickRandomPlateSeeds(NumPlates, PlateSeeds);
//
//	// Cache normalized seed positions — avoids re-normalizing inside inner loop
//	TArray<FVector> SeedNormals;
//	SeedNormals.Reserve(PlateSeeds.Num());
//	for (int32 Seed : PlateSeeds)
//	{
//		SeedNormals.Add(FibonacciPoints[Seed].GetSafeNormal());
//	}
//
//	for (int32 Site = 0; Site < N; ++Site)
//	{
//		const FVector SiteNormal = FibonacciPoints[Site].GetSafeNormal();
//
//		float   BestDot = -2.f;
//		int32   BestSeed = PlateSeeds[0];
//
//		for (int32 i = 0; i < PlateSeeds.Num(); ++i)
//		{
//			// Dot product = cos(angle): higher = closer on sphere
//			const float Dot = FVector::DotProduct(SiteNormal, SeedNormals[i]);
//			if (Dot > BestDot)
//			{
//				BestDot = Dot;
//				BestSeed = PlateSeeds[i];
//			}
//		}
//
//		PlateIdPerSite[Site] = BestSeed;
//	}
//
//	BuildPlateDebugColors();
//}
//
//
//namespace PlanetNoise
//{
//	static uint32 Hash3(int32 X, int32 Y, int32 Z)
//	{
//		uint32 H = (uint32)(X * 1664525 + Y * 1013904223 + Z * 214013);
//		H ^= (H >> 16); H *= 0x45d9f3bU; H ^= (H >> 16);
//		return H;
//	}
//
//	static double Grad3(uint32 Hash, double Dx, double Dy, double Dz)
//	{
//		// 12 edge directions of a unit cube
//		static const double G[12][3] = {
//			{ 1, 1, 0}, {-1, 1, 0}, { 1,-1, 0}, {-1,-1, 0},
//			{ 1, 0, 1}, {-1, 0, 1}, { 1, 0,-1}, {-1, 0,-1},
//			{ 0, 1, 1}, { 0,-1, 1}, { 0, 1,-1}, { 0,-1,-1}
//		};
//		const double* Gv = G[Hash % 12];
//		return Gv[0] * Dx + Gv[1] * Dy + Gv[2] * Dz;
//	}
//
//	// Quintic fade — zero 1st and 2nd derivative at cell boundaries
//	static double Fade(double T) { return T * T * T * (T * (T * 6.0 - 15.0) + 10.0); }
//	static double Lrp(double A, double B, double T) { return A + T * (B - A); }
//
//	static double Noise3D(double X, double Y, double Z)
//	{
//		const int32 IX = FMath::FloorToInt(X);
//		const int32 IY = FMath::FloorToInt(Y);
//		const int32 IZ = FMath::FloorToInt(Z);
//		const double Dx = X - IX, Dy = Y - IY, Dz = Z - IZ;
//		const double U = Fade(Dx), V = Fade(Dy), W = Fade(Dz);
//
//		return Lrp(
//			Lrp(Lrp(Grad3(Hash3(IX, IY, IZ), Dx, Dy, Dz),
//				Grad3(Hash3(IX + 1, IY, IZ), Dx - 1, Dy, Dz), U),
//				Lrp(Grad3(Hash3(IX, IY + 1, IZ), Dx, Dy - 1, Dz),
//					Grad3(Hash3(IX + 1, IY + 1, IZ), Dx - 1, Dy - 1, Dz), U), V),
//			Lrp(Lrp(Grad3(Hash3(IX, IY, IZ + 1), Dx, Dy, Dz - 1),
//				Grad3(Hash3(IX + 1, IY, IZ + 1), Dx - 1, Dy, Dz - 1), U),
//				Lrp(Grad3(Hash3(IX, IY + 1, IZ + 1), Dx, Dy - 1, Dz - 1),
//					Grad3(Hash3(IX + 1, IY + 1, IZ + 1), Dx - 1, Dy - 1, Dz - 1), U), V), W);
//	}
//
//	// FBM — accumulate Octaves layers of noise
//	static double FBM(double X, double Y, double Z,
//		int32 Octaves, double Lacunarity, double Gain)
//	{
//		double Value = 0.0;
//		double Amplitude = 0.5;
//		double Frequency = 1.0;
//		for (int32 i = 0; i < Octaves; ++i)
//		{
//			Value += Amplitude * Noise3D(X * Frequency, Y * Frequency, Z * Frequency);
//			Frequency *= Lacunarity;
//			Amplitude *= Gain;
//		}
//		return Value;
//	}
//} // namespace PlanetNoise
//
//
//static FVector DomainWarpUnit(
//	const FVector& UnitPoint,      // FibonacciPoints[i] — already unit length
//	const double    PlanetRadius,   // scale up for noise domain
//	const double    WarpStrength,
//	const double    Frequency,
//	const int32     Octaves,
//	const double    Lacunarity,
//	const double    Gain)
//{
//	// Scale to world space so noise responds to planet size, not unit sphere
//	const double Px = (double)UnitPoint.X * PlanetRadius * Frequency;
//	const double Py = (double)UnitPoint.Y * PlanetRadius * Frequency;
//	const double Pz = (double)UnitPoint.Z * PlanetRadius * Frequency;
//
//	// Three decorrelated FBM channels — offsets break XYZ correlation
//	const double Wx = PlanetNoise::FBM(Px, Py, Pz, Octaves, Lacunarity, Gain);
//	const double Wy = PlanetNoise::FBM(Px + 5.2, Py + 1.3, Pz + 2.8, Octaves, Lacunarity, Gain);
//	const double Wz = PlanetNoise::FBM(Px + 9.3, Py + 7.8, Pz + 4.1, Octaves, Lacunarity, Gain);
//
//	// Displace in unit-sphere space, then reproject — no pre-normalization needed
//	const double NewX = (double)UnitPoint.X + Wx * WarpStrength;
//	const double NewY = (double)UnitPoint.Y + Wy * WarpStrength;
//	const double NewZ = (double)UnitPoint.Z + Wz * WarpStrength;
//
//	// Reproject back to sphere — magnitude is all that changes
//	const double InvLen = 1.0 / FMath::Sqrt(NewX * NewX + NewY * NewY + NewZ * NewZ);
//	return FVector((float)(NewX * InvLen), (float)(NewY * InvLen), (float)(NewZ * InvLen));
//}
//
//void UGeoDelaunatorComponent::GeneratePlates_NearestNeighbor_DomainWarped()
//{
//	if (N <= 0) return;
//
//	PlateIdPerSite.Init(-1, N);
//	PickRandomPlateSeeds(NumPlates, Plates);
//
//	// Warp seed positions by the SAME field as sites
//	TArray<FVector> WarpedSeedDirs;
//	WarpedSeedDirs.Reserve(Plates.Num());
//	for (const FPlateData& Plate : Plates)
//	{
//		WarpedSeedDirs.Add(DomainWarpUnit(
//			FibonacciPoints[Seed],
//			(double)PlanetRadius,
//			PlateWarpStrength,
//			PlateWarpFrequency,
//			PlateWarpOctaves,
//			PlateWarpLacunarity,
//			PlateWarpGain
//		));
//	}
//
//	for (int32 Site = 0; Site < N; ++Site)
//	{
//		// Warp site by same field
//		const FVector WarpedSite = DomainWarpUnit(
//			FibonacciPoints[Site],
//			(double)PlanetRadius,
//			PlateWarpStrength,
//			PlateWarpFrequency,
//			PlateWarpOctaves,
//			PlateWarpLacunarity,
//			PlateWarpGain
//		);
//
//		double BestDot = -2.0;
//		int32  BestSeed = PlateSeeds[0];
//
//		for (int32 i = 0; i < PlateSeeds.Num(); ++i)
//		{
//			const double Dot =
//				(double)WarpedSite.X * (double)WarpedSeedDirs[i].X +
//				(double)WarpedSite.Y * (double)WarpedSeedDirs[i].Y +
//				(double)WarpedSite.Z * (double)WarpedSeedDirs[i].Z;
//
//			if (Dot > BestDot)
//			{
//				BestDot = Dot;
//				BestSeed = PlateSeeds[i];
//			}
//		}
//
//		PlateIdPerSite[Site] = BestSeed;
//	}
//
//	BuildPlateDebugColors();
//}
