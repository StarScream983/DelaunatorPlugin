// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "Materials/MaterialRenderProxy.h"
#include "UObject/WeakInterfacePtr.h"

#include "GeoDelaunatorComponent_Interface.h"

class FCBTResource_Interface;

namespace GeoVoronoiIndirectInstancingMesh
{
	/** Buffers filled by GPU culling. */
	struct FDrawInstanceBuffers
	{
		/* Culled instance buffer. */
		FBufferRHIRef InstanceBuffer;
		FUnorderedAccessViewRHIRef InstanceBufferUAV;
		FShaderResourceViewRHIRef InstanceBufferSRV;

		/* IndirectArgs buffer for final DrawInstancedIndirect. */
		FBufferRHIRef IndirectArgsBuffer;
		FUnorderedAccessViewRHIRef IndirectArgsBufferUAV;
	};
}

class INDIRECTINSTANCINGCORE_API FGeoVoronoiIndirectInstancingSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FGeoVoronoiIndirectInstancingSceneProxy(class UGeoDelaunatorComponent* InComponent);

protected:
	//~ Begin FPrimitiveSceneProxy Interface
	virtual SIZE_T GetTypeHash() const override;
	virtual uint32 GetMemoryFootprint() const override;
	virtual void CreateRenderThreadResources() override;
	virtual void DestroyRenderThreadResources() override;
	virtual void OnTransformChanged() override;
	// virtual bool HasSubprimitiveOcclusionQueries() const override;
	// virtual const TArray<FBoxSphereBounds>* GetOcclusionQueries(const FSceneView* View) const override;
	// virtual void AcceptOcclusionResults(const FSceneView* View, TArray<bool>* Results, int32 ResultsStart, int32 NumResults) override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	//~ End FPrimitiveSceneProxy Interface

private:
	void BuildOcclusionVolumes(TArrayView<FVector2D> const& InMinMaxData, FIntPoint const& InMinMaxSize, TArrayView<int32> const& InMinMaxMips, int32 InNumLods);

public:
	bool bHiddenInEditor;

	class FMaterialRenderProxy* Material;
	FMaterialRelevance MaterialRelevance;

	bool bCallbackRegistered;

	class FGeoVoronoiIndirectInstancingVertexFactory* VertexFactory;
	float PlanetRadius = 1.0f;

	/** Render-thread pointer to CBT GPU resources. Lifetime owned by UGeoDelaunatorComponent. */
	TSharedPtr<FCBTResource_Interface> CBTResources;

	/** Weak ref to render-facing subset of the owner (`IGeoDelaunatorComponent_Interface`). */
	TWeakInterfacePtr<IGeoDelaunatorComponent_Interface> GeoOwner;
};

//  Notes: Looks like GetMeshShaderMap is returning nullptr during the DepthPass