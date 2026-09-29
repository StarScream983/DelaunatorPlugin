// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshWeaverSceneProxy.h"
#include "MeshWeaverSceneViewExtension.h"
#include "MeshWeaverCull.h"
#include "MeshWeaverDrawBuffers.h"
#include "CBTResource_Interface.h"
#include "Components/PrimitiveComponent.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "SceneViewExtension.h"

const static FName NAME_MeshWeaver(TEXT("MeshWeaver"));

FMeshWeaverSceneProxy::FMeshWeaverSceneProxy(
	UPrimitiveComponent* InComponent,
	const TSharedPtr<FCBTResource_Interface>& InCBTResources,
	float InPlanetRadius)
	: FPrimitiveSceneProxy(InComponent, NAME_MeshWeaver)
	, PlanetRadius(InPlanetRadius)
	, CBTResources(InCBTResources)
{
	bHasDeformableMesh = false;
	// Opaque GBuffer inject; no material shading model from a UMaterial.
	bVerifyUsedMaterials = false;
	GetMeshWeaverCullExtension().RegisterExtension();

	UE_LOG(LogTemp, Warning, TEXT("FMeshWeaverSceneProxy ctor, CBTResources valid=%d"),
		CBTResources.IsValid() ? 1 : 0);
}

SIZE_T FMeshWeaverSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<SIZE_T>(&UniquePointer);
}

uint32 FMeshWeaverSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize();
}

void FMeshWeaverSceneProxy::OnTransformChanged()
{
}

void FMeshWeaverSceneProxy::CreateRenderThreadResources()
{
	UE_LOG(LogTemp, Warning, TEXT("FMeshWeaverSceneProxy::CreateRenderThreadResources"));
	ViewExtension = FSceneViewExtensions::NewExtension<FMeshWeaverSceneViewExtension>(this);
}

void FMeshWeaverSceneProxy::DestroyRenderThreadResources()
{
	ViewExtension.Reset();
}

FPrimitiveViewRelevance FMeshWeaverSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	const bool bValid = CBTResources.IsValid() && CBTResources->IsGPUReady();

	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = bValid && IsShown(View);
	Result.bShadowRelevance = false; // no shadow-depth pass yet
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bOpaque = true;
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = false;
	Result.bVelocityRelevance = false;
	return Result;
}

void FMeshWeaverSceneProxy::GetDynamicMeshElements(
	const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap,
	FMeshElementCollector& Collector) const
{
	check(IsInRenderingThread());
	(void)Collector;

	if (!ViewFamily.Views.IsValidIndex(0) || GetMeshWeaverCullExtension().IsInFrame())
	{
		return;
	}

	FSceneView const* MainView = ViewFamily.Views[0];
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		if ((VisibilityMap & (1u << ViewIndex)) == 0u)
		{
			continue;
		}

		CulledDrawBuffers = &GetMeshWeaverCullExtension().AddWork(this, Views[ViewIndex]);
		(void)MainView;
	}
}
