// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshWeaverCull.h"
#include "MeshWeaverSceneProxy.h"
#include "CBTResource_Interface.h"
#include "Engine/Engine.h"
#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderUtils.h"
#include "SceneView.h"
#include "ShaderParameterStruct.h"
#include "DataDrivenShaderPlatformInfo.h"

namespace MeshWeaverCull
{
	static const uint32 MaxSupportedInstances = 1u << 21;

	class FInitInstanceBufferCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FInitInstanceBufferCS);
		SHADER_USE_PARAMETER_STRUCT(FInitInstanceBufferCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(int32, NumIndices)
			SHADER_PARAMETER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};
	IMPLEMENT_GLOBAL_SHADER(FInitInstanceBufferCS, "/MeshWeaverCoreShaders/MeshWeaverCull.usf", "InitInstanceBufferCS", SF_Compute);

	class FCullInstancesCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FCullInstancesCS);
		SHADER_USE_PARAMETER_STRUCT(FCullInstancesCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(uint32, MaxInstances)
			SHADER_PARAMETER(uint32, NumSites)
			SHADER_PARAMETER(uint32, NumVoronoiCenters)
			SHADER_PARAMETER(uint32, NumVoronoiGeoMeshFlat)
			SHADER_PARAMETER_SRV(StructuredBuffer<uint2>, VoronoiGeoMeshRanges)
			SHADER_PARAMETER_SRV(StructuredBuffer<int>, VoronoiGeoMeshFlat)
			SHADER_PARAMETER_UAV(RWStructuredBuffer<FMeshWeaverRenderInstance>, RWInstanceBuffer)
			SHADER_PARAMETER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};
	IMPLEMENT_GLOBAL_SHADER(FCullInstancesCS, "/MeshWeaverCoreShaders/MeshWeaverCull.usf", "CullInstancesCS", SF_Compute);

	void InitializeInstanceBuffers(FRHICommandListImmediate& RHICmdList, FMeshWeaverDrawBuffers& InBuffers)
	{
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("MeshWeaver.InstanceBuffer"));
			const int32 InstanceSize = sizeof(FMeshWeaverRenderInstance);
			const int32 InstanceBufferSize = int32(MaxSupportedInstances) * InstanceSize;
			InBuffers.InstanceBuffer = RHICmdList.CreateStructuredBuffer(
				InstanceSize, InstanceBufferSize, BUF_UnorderedAccess | BUF_ShaderResource, ERHIAccess::SRVMask, CreateInfo);
			InBuffers.InstanceBufferUAV = RHICmdList.CreateUnorderedAccessView(InBuffers.InstanceBuffer, false, false);
			InBuffers.InstanceBufferSRV = RHICmdList.CreateShaderResourceView(InBuffers.InstanceBuffer);
		}
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("MeshWeaver.IndirectArgsBuffer"));
			InBuffers.IndirectArgsBuffer = RHICmdList.CreateVertexBuffer(
				5 * sizeof(uint32), BUF_UnorderedAccess | BUF_DrawIndirect, ERHIAccess::IndirectArgs, CreateInfo);
			InBuffers.IndirectArgsBufferUAV = RHICmdList.CreateUnorderedAccessView(InBuffers.IndirectArgsBuffer, PF_R32_UINT);
		}
	}

	void ReleaseInstanceBuffers(FMeshWeaverDrawBuffers& InBuffers)
	{
		InBuffers.InstanceBuffer.SafeRelease();
		InBuffers.InstanceBufferUAV.SafeRelease();
		InBuffers.InstanceBufferSRV.SafeRelease();
		InBuffers.IndirectArgsBuffer.SafeRelease();
		InBuffers.IndirectArgsBufferUAV.SafeRelease();
	}

	void AddPass_InitInstanceBuffer(FRDGBuilder& GraphBuilder, FMeshWeaverDrawBuffers& Output)
	{
		TShaderMapRef<FInitInstanceBufferCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FInitInstanceBufferCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitInstanceBufferCS::FParameters>();
		PassParameters->NumIndices = 3;
		PassParameters->RWIndirectArgsBuffer = Output.IndirectArgsBufferUAV;
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("MeshWeaver.InitInstanceBuffer"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	void AddPass_CullInstances(FRDGBuilder& GraphBuilder, FMeshWeaverSceneProxy const* Proxy, FMeshWeaverDrawBuffers& Output)
	{
		if (!Proxy || !Proxy->CBTResources.IsValid() || !Proxy->CBTResources->IsGPUReady())
		{
			return;
		}

		const TSharedPtr<FCBTResource_Interface>& CBT = Proxy->CBTResources;
		const uint32 NumSites = CBT->GetNumFibonacciPoints();
		if (NumSites == 0u)
		{
			return;
		}

		TShaderMapRef<FCullInstancesCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FCullInstancesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCullInstancesCS::FParameters>();
		PassParameters->MaxInstances = MaxSupportedInstances;
		PassParameters->NumSites = NumSites;
		PassParameters->NumVoronoiCenters = CBT->GetNumVoronoiGeoCenters();
		PassParameters->NumVoronoiGeoMeshFlat = CBT->GetNumVoronoiGeoMeshFlat();
		PassParameters->VoronoiGeoMeshRanges = CBT->GetVoronoiGeoMeshRangesSRV();
		PassParameters->VoronoiGeoMeshFlat = CBT->GetVoronoiGeoMeshFlatSRV();
		PassParameters->RWInstanceBuffer = Output.InstanceBufferUAV;
		PassParameters->RWIndirectArgsBuffer = Output.IndirectArgsBufferUAV;

		const FIntVector GroupCount(FMath::DivideAndRoundUp<int32>((int32)NumSites, 64), 1, 1);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("MeshWeaver.CullInstances"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	void AddPass_TransitionDrawBuffers(FRDGBuilder& GraphBuilder, TArray<FMeshWeaverDrawBuffers> const& InBuffers, TArrayView<int32> const& BufferIndices, bool bToWrite)
	{
		TArray<FRHIUnorderedAccessView*> OverlapUAVs;
		TArray<FRHITransitionInfo> TransitionInfos;
		for (int32 BufferIndex : BufferIndices)
		{
			OverlapUAVs.Add(InBuffers[BufferIndex].IndirectArgsBufferUAV);
			TransitionInfos.Add(FRHITransitionInfo(
				InBuffers[BufferIndex].IndirectArgsBufferUAV,
				bToWrite ? ERHIAccess::IndirectArgs : ERHIAccess::UAVMask,
				bToWrite ? ERHIAccess::UAVMask : ERHIAccess::IndirectArgs));
			TransitionInfos.Add(FRHITransitionInfo(
				InBuffers[BufferIndex].InstanceBufferUAV,
				bToWrite ? ERHIAccess::SRVMask : ERHIAccess::UAVMask,
				bToWrite ? ERHIAccess::UAVMask : ERHIAccess::SRVMask));
		}

		AddPass(GraphBuilder, RDG_EVENT_NAME("MeshWeaver.TransitionDrawBuffers"),
			[bToWrite, OverlapUAVs, TransitionInfos](FRHICommandList& RHICmdList)
			{
				if (!bToWrite)
				{
					RHICmdList.EndUAVOverlap(OverlapUAVs);
				}
				RHICmdList.Transition(TransitionInfos);
				if (bToWrite)
				{
					RHICmdList.BeginUAVOverlap(OverlapUAVs);
				}
			});
	}
}

TGlobalResource<FMeshWeaverCullExtension> GMeshWeaverCullExtension;

FMeshWeaverCullExtension& GetMeshWeaverCullExtension()
{
	return GMeshWeaverCullExtension;
}

void FMeshWeaverCullExtension::RegisterExtension()
{
	static bool bInit = false;
	if (!bInit)
	{
		GEngine->GetPreRenderDelegateEx().AddRaw(this, &FMeshWeaverCullExtension::BeginFrame);
		GEngine->GetPostRenderDelegateEx().AddRaw(this, &FMeshWeaverCullExtension::EndFrame);
		bInit = true;
	}
}

void FMeshWeaverCullExtension::ReleaseRHI()
{
	for (FMeshWeaverDrawBuffers& Buffer : Buffers)
	{
		MeshWeaverCull::ReleaseInstanceBuffers(Buffer);
	}
	Buffers.Reset();
	DiscardIds.Reset();
	SceneProxies.Reset();
	Views.Reset();
	WorkDescs.Reset();
	bInFrame = false;
	DiscardId = 0;
}

FMeshWeaverDrawBuffers& FMeshWeaverCullExtension::AddWork(FMeshWeaverSceneProxy const* InProxy, FSceneView const* InView)
{
	if (!ensure(!bInFrame))
	{
		EndFrame();
	}

	FWorkDesc WorkDesc;
	WorkDesc.ProxyIndex = SceneProxies.AddUnique(InProxy);
	WorkDesc.ViewIndex = Views.AddUnique(InView);
	WorkDesc.BufferIndex = INDEX_NONE;

	for (const FWorkDesc& Existing : WorkDescs)
	{
		if (Existing.ProxyIndex == WorkDesc.ProxyIndex && Existing.ViewIndex == WorkDesc.ViewIndex)
		{
			return Buffers[Existing.BufferIndex];
		}
	}

	for (int32 BufferIndex = 0; BufferIndex < Buffers.Num(); ++BufferIndex)
	{
		if (DiscardIds[BufferIndex] < DiscardId)
		{
			DiscardIds[BufferIndex] = DiscardId;
			WorkDesc.BufferIndex = BufferIndex;
			WorkDescs.Add(WorkDesc);
			return Buffers[BufferIndex];
		}
	}

	DiscardIds.Add(DiscardId);
	WorkDesc.BufferIndex = Buffers.AddDefaulted();
	WorkDescs.Add(WorkDesc);
	MeshWeaverCull::InitializeInstanceBuffers(GetImmediateCommandList_ForRenderCommand(), Buffers[WorkDesc.BufferIndex]);
	return Buffers[WorkDesc.BufferIndex];
}

void FMeshWeaverCullExtension::BeginFrame(FRDGBuilder& GraphBuilder)
{
	if (!ensure(!bInFrame))
	{
		EndFrame();
	}
	bInFrame = true;

	if (WorkDescs.Num() > 0)
	{
		SubmitWork(GraphBuilder);
	}
}

void FMeshWeaverCullExtension::EndFrame(FRDGBuilder& GraphBuilder)
{
	(void)GraphBuilder;
	EndFrame();
}

void FMeshWeaverCullExtension::EndFrame()
{
	if (!bInFrame)
	{
		return;
	}
	bInFrame = false;
	SceneProxies.Reset();
	Views.Reset();
	WorkDescs.Reset();
	++DiscardId;
	for (int32 Index = 0; Index < DiscardIds.Num();)
	{
		if (DiscardId - DiscardIds[Index] > 4u)
		{
			MeshWeaverCull::ReleaseInstanceBuffers(Buffers[Index]);
			Buffers.RemoveAtSwap(Index);
			DiscardIds.RemoveAtSwap(Index);
			continue;
		}
		++Index;
	}
}

void FMeshWeaverCullExtension::SubmitWork(FRDGBuilder& GraphBuilder)
{
	TArray<int32, TInlineAllocator<8>> UsedBufferIndices;
	for (const FWorkDesc& WorkDesc : WorkDescs)
	{
		UsedBufferIndices.AddUnique(WorkDesc.BufferIndex);
	}
	if (UsedBufferIndices.Num() == 0)
	{
		return;
	}

	MeshWeaverCull::AddPass_TransitionDrawBuffers(GraphBuilder, Buffers, UsedBufferIndices, true);

	for (const FWorkDesc& WorkDesc : WorkDescs)
	{
		MeshWeaverCull::AddPass_InitInstanceBuffer(GraphBuilder, Buffers[WorkDesc.BufferIndex]);
		MeshWeaverCull::AddPass_CullInstances(GraphBuilder, SceneProxies[WorkDesc.ProxyIndex], Buffers[WorkDesc.BufferIndex]);
	}

	MeshWeaverCull::AddPass_TransitionDrawBuffers(GraphBuilder, Buffers, UsedBufferIndices, false);
}
