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
#include "RHIResources.h"
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
	SHADER_PARAMETER_SRV(Buffer<float>, DistanceToBoundaryNormPerSite)
	SHADER_PARAMETER_SRV(Buffer<float>, ErosionControlPerSite)
	SHADER_PARAMETER_SRV(Buffer<uint>, LandDistanceField)
	SHADER_PARAMETER(float, PlanetRadiusHigh)
	SHADER_PARAMETER(float, PlanetRadiusLow)
	// Rotation and scale only. The origin is the high/low pair below.
	SHADER_PARAMETER(FMatrix44f, LocalToWorld)
	SHADER_PARAMETER(FVector3f, WorldOriginHigh)
	SHADER_PARAMETER(FVector3f, WorldOriginLow)
	SHADER_PARAMETER(uint32, Unlit)
	SHADER_PARAMETER(uint32, ColorViewMode)
	SHADER_PARAMETER(float, MaxLandDistance)
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
		|| !CBT->GetVoronoiCellColorsSRV() || !CBT->GetElevationPerSiteSRV()
		|| !CBT->GetDistanceToBoundaryNormPerSiteSRV() || !CBT->GetErosionControlPerSiteSRV()
		|| !CBT->GetLandDistanceFieldSRV())
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
	PassParameters->DistanceToBoundaryNormPerSite = CBT->GetDistanceToBoundaryNormPerSiteSRV();
	PassParameters->ErosionControlPerSite = CBT->GetErosionControlPerSiteSRV();
	PassParameters->LandDistanceField = CBT->GetLandDistanceFieldSRV();
	const double Radius = Proxy->PlanetRadius;
	const float RadiusHigh = static_cast<float>(Radius);
	PassParameters->PlanetRadiusHigh = RadiusHigh;
	PassParameters->PlanetRadiusLow = static_cast<float>(Radius - static_cast<double>(RadiusHigh));

	const FMatrix LocalToWorld = Proxy->GetLocalToWorld();
	const FVector Origin = LocalToWorld.GetOrigin();
	const float OriginHighX = static_cast<float>(Origin.X);
	const float OriginHighY = static_cast<float>(Origin.Y);
	const float OriginHighZ = static_cast<float>(Origin.Z);
	PassParameters->WorldOriginHigh = FVector3f(OriginHighX, OriginHighY, OriginHighZ);
	PassParameters->WorldOriginLow = FVector3f(
		static_cast<float>(Origin.X - static_cast<double>(OriginHighX)),
		static_cast<float>(Origin.Y - static_cast<double>(OriginHighY)),
		static_cast<float>(Origin.Z - static_cast<double>(OriginHighZ)));
	FMatrix44f LocalToWorldNoOrigin(LocalToWorld);
	LocalToWorldNoOrigin.M[3][0] = 0.f;
	LocalToWorldNoOrigin.M[3][1] = 0.f;
	LocalToWorldNoOrigin.M[3][2] = 0.f;
	PassParameters->LocalToWorld = LocalToWorldNoOrigin;
	PassParameters->Unlit = Proxy->GetUnlit();
	PassParameters->ColorViewMode = Proxy->GetColorViewMode();
	PassParameters->MaxLandDistance = CBT->GetMaxLandDistance();
	PassParameters->RenderTargets = RenderTargets;
	// BasePass may bind depth as read-only (Nanite / DBuffer). Lighting reconstructs P from
	// SceneDepth — without a write the pawn stays in the depth buffer and shows through, and
	// N·L / specular use far-plane positions (X-shaped lighting).
	if (PassParameters->RenderTargets.DepthStencil.GetTexture())
	{
		PassParameters->RenderTargets.DepthStencil.SetDepthStencilAccess(
			FExclusiveDepthStencil::DepthWrite_StencilWrite);
	}

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
