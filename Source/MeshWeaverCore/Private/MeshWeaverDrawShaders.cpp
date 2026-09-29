// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshWeaverDrawShaders.h"
#include "MeshWeaverCull.h"
#include "MeshWeaverDrawBuffers.h"
#include "MeshWeaverSceneProxy.h"
#include "CBTResource_Interface.h"
#include "CBTStructs.h"
#include "CommonRenderResources.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "MeshPassProcessor.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "ShaderParameterStruct.h"

class FMeshWeaverIndexBuffer : public FIndexBuffer
{
public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> Indices;
		Indices.Add(0);
		Indices.Add(1);
		Indices.Add(2);
		FRHIResourceCreateInfo CreateInfo(TEXT("MeshWeaver.IndexBuffer"), &Indices);
		IndexBufferRHI = RHICmdList.CreateIndexBuffer(sizeof(uint16), Indices.GetResourceDataSize(), BUF_Static, CreateInfo);
	}
};

TGlobalResource<FMeshWeaverIndexBuffer> GMeshWeaverIndexBuffer;

BEGIN_SHADER_PARAMETER_STRUCT(FMeshWeaverDrawParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_SRV(StructuredBuffer<FMeshWeaverRenderInstance>, InstanceBuffer)
	SHADER_PARAMETER_SRV(StructuredBuffer<FVector3_HighLow>, VoronoiGeoCenters)
	SHADER_PARAMETER_SRV(StructuredBuffer<FVector3_HighLow>, CBT_FibonacciPoints)
	SHADER_PARAMETER_SRV(Buffer<uint>, VoronoiCellColors)
	SHADER_PARAMETER_SRV(Buffer<float>, ElevationPerSite)
	SHADER_PARAMETER(float, PlanetRadius)
	SHADER_PARAMETER(FMatrix44f, LocalToWorld)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FMeshWeaverVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMeshWeaverVS);
	SHADER_USE_PARAMETER_STRUCT(FMeshWeaverVS, FGlobalShader);
	using FParameters = FMeshWeaverDrawParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FMeshWeaverVS, "/MeshWeaverCoreShaders/MeshWeaverDraw.usf", "MainVS", SF_Vertex);

class FMeshWeaverPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMeshWeaverPS);
	SHADER_USE_PARAMETER_STRUCT(FMeshWeaverPS, FGlobalShader);
	using FParameters = FMeshWeaverDrawParameters;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FMeshWeaverPS, "/MeshWeaverCoreShaders/MeshWeaverDraw.usf", "MainPS", SF_Pixel);

void AddMeshWeaverDrawPass(
	FRDGBuilder& GraphBuilder,
	FSceneView& View,
	const FRenderTargetBindingSlots& RenderTargets,
	FMeshWeaverSceneProxy* Proxy)
{
	if (!Proxy || !Proxy->CBTResources.IsValid() || !Proxy->CBTResources->IsGPUReady())
	{
		return;
	}

	const FMeshWeaverDrawBuffers* DrawBuffers = Proxy->GetCulledDrawBuffers();
	if (!DrawBuffers || !DrawBuffers->InstanceBufferSRV || !DrawBuffers->IndirectArgsBuffer)
	{
		return;
	}

	const TSharedPtr<FCBTResource_Interface>& CBT = Proxy->CBTResources;
	if (!CBT->GetVoronoiGeoCentersSRV() || !CBT->GetFibonacciPointsSRV()
		|| !CBT->GetVoronoiCellColorsSRV() || !CBT->GetElevationPerSiteSRV())
	{
		return;
	}

	FMeshWeaverDrawParameters* PassParameters = GraphBuilder.AllocParameters<FMeshWeaverDrawParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->InstanceBuffer = DrawBuffers->InstanceBufferSRV;
	PassParameters->VoronoiGeoCenters = CBT->GetVoronoiGeoCentersSRV();
	PassParameters->CBT_FibonacciPoints = CBT->GetFibonacciPointsSRV();
	PassParameters->VoronoiCellColors = CBT->GetVoronoiCellColorsSRV();
	PassParameters->ElevationPerSite = CBT->GetElevationPerSiteSRV();
	PassParameters->PlanetRadius = Proxy->PlanetRadius;
	PassParameters->LocalToWorld = FMatrix44f(Proxy->GetLocalToWorld());
	PassParameters->RenderTargets = RenderTargets;

	TShaderMapRef<FMeshWeaverVS> VertexShader(GetGlobalShaderMap(View.GetFeatureLevel()));
	TShaderMapRef<FMeshWeaverPS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
	FRHIBuffer* IndexBufferRHI = GMeshWeaverIndexBuffer.IndexBufferRHI;
	FRHIBuffer* IndirectArgsRHI = DrawBuffers->IndirectArgsBuffer;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("MeshWeaver.Draw"),
		PassParameters,
		ERDGPassFlags::Raster,
		[PassParameters, VertexShader, PixelShader, IndexBufferRHI, IndirectArgsRHI](FRHICommandList& RHICmdList)
		{
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			// Index buffer is CCW (0,1,2). UE opaque default is CM_CW (cull clockwise = keep CCW).
			// CM_CCW shows the inner shell of the far hemisphere.
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), *PassParameters);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

			RHICmdList.SetStreamSource(0, nullptr, 0);
			RHICmdList.DrawIndexedPrimitiveIndirect(IndexBufferRHI, IndirectArgsRHI, 0);
		});
}
