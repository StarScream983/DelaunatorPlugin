// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"

class FCBTResource_Interface;
class FMeshWeaverSceneViewExtension;
class UPrimitiveComponent;
struct FMeshWeaverDrawBuffers;

/**
 * Scene proxy for the Mesh Weaver draw path.
 * No material vertex factory and no FMeshBatch. SceneViewExtension is created in CreateRenderThreadResources.
 * Lifetime of CBTResources is owned by UGeoDelaunatorComponent.
 */
class MESHWEAVERCORE_API FMeshWeaverSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FMeshWeaverSceneProxy(
		UPrimitiveComponent* InComponent,
		const TSharedPtr<FCBTResource_Interface>& InCBTResources,
		float InPlanetRadius,
		bool bInUnlit = true);

	virtual ~FMeshWeaverSceneProxy() = default;

protected:
	//~ Begin FPrimitiveSceneProxy Interface
	virtual SIZE_T GetTypeHash() const override;
	virtual uint32 GetMemoryFootprint() const override;
	virtual void CreateRenderThreadResources() override;
	virtual void DestroyRenderThreadResources() override;
	virtual void OnTransformChanged() override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override;
	//~ End FPrimitiveSceneProxy Interface

public:
	float PlanetRadius = 1.0f;
	TSharedPtr<FCBTResource_Interface> CBTResources;

	const FMeshWeaverDrawBuffers* GetCulledDrawBuffers() const { return CulledDrawBuffers; }

	void SetUnlit(bool bInUnlit) { UnlitFlag.Store(bInUnlit ? 1u : 0u); }
	uint32 GetUnlit() const { return UnlitFlag.Load(); }
	void SetColorViewMode(uint32 InMode) { ColorViewFlag.Store(InMode); }
	uint32 GetColorViewMode() const { return ColorViewFlag.Load(); }

private:
	TSharedPtr<FMeshWeaverSceneViewExtension, ESPMode::ThreadSafe> ViewExtension;
	mutable const FMeshWeaverDrawBuffers* CulledDrawBuffers = nullptr;
	TAtomic<uint32> UnlitFlag;
	TAtomic<uint32> ColorViewFlag;
};
