// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#include "IndirectInstancingVertexFactory.h"

#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Materials/Material.h"
#include "MeshMaterialShader.h"
#include "RHIStaticStates.h"
#include "ShaderParameters.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "MaterialDomain.h"
#include "MeshDrawShaderBindings.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FGeoVoronoiIndirectInstancingParameters, "GeoVoronoiIndirectInstancingParams");

namespace FGeoVoronoiIndirectInstancingUtil
{
	template <typename T>
	FBufferRHIRef CreateIndexBuffer(FRHICommandListBase &RHICmdList)
	{
		TResourceArray<T, INDEXBUFFER_ALIGNMENT> Indices;

		// Allocate room for indices
		Indices.Reserve(3);

		// CCW triangle winding order
		Indices.Add(1);
		Indices.Add(0);
		Indices.Add(2);

		const uint32 Size = Indices.GetResourceDataSize();
		const uint32 Stride = sizeof(T);

		FRHIResourceCreateInfo CreateInfo(TEXT("GeoVoronoiIndirectInstancingIndexBuffer"), &Indices);
		return RHICmdList.CreateIndexBuffer(Stride, Size, BUF_Static, CreateInfo);
	}
}

void FGeoVoronoiIndirectInstancingIndexBuffer::InitRHI(FRHICommandListBase &RHICmdList)
{
	IndexBufferRHI = FGeoVoronoiIndirectInstancingUtil::CreateIndexBuffer<uint16>(RHICmdList);
}

/**
 * Shader parameters for vertex factory.
 */
class FGeoVoronoiIndirectInstancingShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FGeoVoronoiIndirectInstancingShaderParameters, NonVirtual);

public:
	void Bind(const FShaderParameterMap &ParameterMap)
	{
		InstanceBufferParameter.Bind(ParameterMap, TEXT("InstanceBuffer"));
		LodViewOriginParameter.Bind(ParameterMap, TEXT("LodViewOrigin"));
		CBT_FibonacciPointsParameter.Bind(ParameterMap, TEXT("CBT_FibonacciPoints"));
		CBT_SphericalTrianglesParameter.Bind(ParameterMap, TEXT("CBT_SphericalTriangles"));
	}

	void GetElementShaderBindings(
			const class FSceneInterface *Scene,
			const class FSceneView *View,
			const class FMeshMaterialShader *Shader,
			const EVertexInputStreamType InputStreamType,
			ERHIFeatureLevel::Type FeatureLevel,
			const class FVertexFactory *InVertexFactory,
			const struct FMeshBatchElement &BatchElement,
			class FMeshDrawSingleShaderBindings &ShaderBindings,
			FVertexInputStreamArray &VertexStreams) const
	{
		FGeoVoronoiIndirectInstancingVertexFactory *VertexFactory = (FGeoVoronoiIndirectInstancingVertexFactory *)InVertexFactory;
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FGeoVoronoiIndirectInstancingParameters>(), VertexFactory->UniformBuffer);

		FGeoVoronoiIndirectInstancingUserData *UserData = (FGeoVoronoiIndirectInstancingUserData *)BatchElement.UserData;
		ShaderBindings.Add(InstanceBufferParameter, UserData->InstanceBufferSRV);
		ShaderBindings.Add(LodViewOriginParameter, UserData->LodViewOrigin);

		if (CBT_FibonacciPointsParameter.IsBound() && UserData->CBT_FibonacciPointsSRV)
		{
			ShaderBindings.Add(CBT_FibonacciPointsParameter, UserData->CBT_FibonacciPointsSRV);
		}
		if (CBT_SphericalTrianglesParameter.IsBound() && UserData->CBT_SphericalTrianglesSRV)
		{
			ShaderBindings.Add(CBT_SphericalTrianglesParameter, UserData->CBT_SphericalTrianglesSRV);
		}
	}

protected:
	LAYOUT_FIELD(FShaderResourceParameter, InstanceBufferParameter);
	LAYOUT_FIELD(FShaderParameter, LodViewOriginParameter);
	LAYOUT_FIELD(FShaderResourceParameter, CBT_FibonacciPointsParameter);
	LAYOUT_FIELD(FShaderResourceParameter, CBT_SphericalTrianglesParameter);
};

IMPLEMENT_TYPE_LAYOUT(FGeoVoronoiIndirectInstancingShaderParameters);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGeoVoronoiIndirectInstancingVertexFactory, SF_Vertex, FGeoVoronoiIndirectInstancingShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGeoVoronoiIndirectInstancingVertexFactory, SF_Pixel, FGeoVoronoiIndirectInstancingShaderParameters);

FGeoVoronoiIndirectInstancingVertexFactory::FGeoVoronoiIndirectInstancingVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const FGeoVoronoiIndirectInstancingParameters &InParams)
		: FVertexFactory(InFeatureLevel), Params(InParams)
{
	IndexBuffer = new FGeoVoronoiIndirectInstancingIndexBuffer();
}

FGeoVoronoiIndirectInstancingVertexFactory::~FGeoVoronoiIndirectInstancingVertexFactory()
{
	delete IndexBuffer;
}

void FGeoVoronoiIndirectInstancingVertexFactory::InitRHI(FRHICommandListBase &RHICmdList)
{
	UniformBuffer = FGeoVoronoiIndirectInstancingBufferRef::CreateUniformBufferImmediate(Params, UniformBuffer_MultiFrame);

	IndexBuffer->InitResource(RHICmdList);

	FVertexStream NullVertexStream;
	NullVertexStream.VertexBuffer = nullptr;
	NullVertexStream.Stride = 0;
	NullVertexStream.Offset = 0;
	NullVertexStream.VertexStreamUsage = EVertexStreamUsage::ManualFetch;

	check(Streams.Num() == 0);
	Streams.Add(NullVertexStream);

	FVertexDeclarationElementList Elements;

	InitDeclaration(Elements);
}

void FGeoVoronoiIndirectInstancingVertexFactory::ReleaseRHI()
{
	UniformBuffer.SafeRelease();

	if (IndexBuffer)
	{
		IndexBuffer->ReleaseResource();
	}

	FVertexFactory::ReleaseRHI();
}

bool FGeoVoronoiIndirectInstancingVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters &Parameters)
{
	if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5))
	{
		return false;
	}
	// TODO
	return (Parameters.MaterialParameters.MaterialDomain == MD_Surface && Parameters.MaterialParameters.bIsUsedWithVirtualHeightfieldMesh) || Parameters.MaterialParameters.bIsSpecialEngineMaterial;
}

void FGeoVoronoiIndirectInstancingVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters &Parameters, FShaderCompilerEnvironment &OutEnvironment)
{
	// TODO
}

void FGeoVoronoiIndirectInstancingVertexFactory::ValidateCompiledResult(const FVertexFactoryType *Type, EShaderPlatform Platform, const FShaderParameterMap &ParameterMap, TArray<FString> &OutErrors)
{
}

// TODO update shader path when you create GeoVoronoi shader files
IMPLEMENT_VERTEX_FACTORY_TYPE(FGeoVoronoiIndirectInstancingVertexFactory, "/GeoVoronoiIndirectInstancing/GeoVoronoiIndirectInstancingVertexFactory.ush",
															EVertexFactoryFlags::UsedWithMaterials | EVertexFactoryFlags::SupportsDynamicLighting | EVertexFactoryFlags::SupportsPrimitiveIdStream);
