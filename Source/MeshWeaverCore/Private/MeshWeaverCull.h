// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "MeshWeaverDrawBuffers.h"

class FMeshWeaverSceneProxy;
class FSceneView;
class FRDGBuilder;

class FMeshWeaverCullExtension : public FRenderResource
{
public:
	bool IsInFrame() const { return bInFrame; }
	void RegisterExtension();

	FMeshWeaverDrawBuffers& AddWork(FMeshWeaverSceneProxy const* InProxy, FSceneView const* InView);
	void SubmitWork(FRDGBuilder& GraphBuilder);

protected:
	virtual void ReleaseRHI() override;

private:
	void BeginFrame(FRDGBuilder& GraphBuilder);
	void EndFrame(FRDGBuilder& GraphBuilder);
	void EndFrame();

	bool bInFrame = false;
	uint32 DiscardId = 0;

	TArray<FMeshWeaverDrawBuffers> Buffers;
	TArray<uint32> DiscardIds;
	TArray<FMeshWeaverSceneProxy const*> SceneProxies;
	TArray<FSceneView const*> Views;

	struct FWorkDesc
	{
		int32 ProxyIndex = 0;
		int32 ViewIndex = 0;
		int32 BufferIndex = INDEX_NONE;
	};
	TArray<FWorkDesc> WorkDescs;
};

FMeshWeaverCullExtension& GetMeshWeaverCullExtension();
