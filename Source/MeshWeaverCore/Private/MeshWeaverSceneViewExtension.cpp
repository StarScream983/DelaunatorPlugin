// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshWeaverSceneViewExtension.h"
#include "MeshWeaverSceneProxy.h"
#include "MeshWeaverDrawShaders.h"
#include "CBTResource_Interface.h"
#include "SceneView.h"

FMeshWeaverSceneViewExtension::FMeshWeaverSceneViewExtension(
	const FAutoRegister& AutoRegister,
	FMeshWeaverSceneProxy* InProxy)
	: FSceneViewExtensionBase(AutoRegister)
	, Proxy(InProxy)
{
	UE_LOG(LogTemp, Warning, TEXT("FMeshWeaverSceneViewExtension ctor"));
}

void FMeshWeaverSceneViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	(void)InViewFamily;
}

void FMeshWeaverSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	(void)InViewFamily;
	(void)InView;
}

void FMeshWeaverSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	(void)InViewFamily;
}

bool FMeshWeaverSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	(void)Context;
	return Proxy != nullptr
		&& Proxy->CBTResources.IsValid()
		&& Proxy->CBTResources->IsGPUReady();
}

void FMeshWeaverSceneViewExtension::PostRenderBasePassDeferred_RenderThread(
	FRDGBuilder& GraphBuilder,
	FSceneView& InView,
	const FRenderTargetBindingSlots& RenderTargets,
	TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	(void)InView;
	(void)RenderTargets;
	(void)SceneTextures;

	if (!Proxy)
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "MeshWeaver");
	// DrawIndexedPrimitiveIndirect + EncodeGBuffer
	AddMeshWeaverDrawPass(GraphBuilder, InView, RenderTargets, Proxy);
}
