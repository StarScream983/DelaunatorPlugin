// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "RenderGraphBuilder.h"

class FMeshWeaverSceneProxy;

/**
 * Hooks PostRenderBasePassDeferred: cull fills instance buffers, then we draw GBuffer.
 */
class FMeshWeaverSceneViewExtension final : public FSceneViewExtensionBase
{
public:
	FMeshWeaverSceneViewExtension(const FAutoRegister& AutoRegister, FMeshWeaverSceneProxy* InProxy);

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;

	virtual void PostRenderBasePassDeferred_RenderThread(
		FRDGBuilder& GraphBuilder,
		FSceneView& InView,
		const FRenderTargetBindingSlots& RenderTargets,
		TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override;

protected:
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	FMeshWeaverSceneProxy* Proxy = nullptr;
};
