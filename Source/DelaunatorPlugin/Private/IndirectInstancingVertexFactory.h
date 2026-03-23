// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#pragma once

#include "CoreMinimal.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "RenderResource.h"
#include "RHI.h"
#include "SceneManagement.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"

/**
 * Uniform buffer to hold parameters specific to this vertex factory. Only set up once.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FGeoVoronoiIndirectInstancingParameters, )
// SHADER_PARAMETER_TEXTURE(Texture2D<uint4>, PageTableTexture)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FGeoVoronoiIndirectInstancingParameters> FGeoVoronoiIndirectInstancingBufferRef;

/**
 * Per frame UserData to pass to the vertex shader.
 */
struct FGeoVoronoiIndirectInstancingUserData : public FOneFrameResource
{
	FRHIShaderResourceView *InstanceBufferSRV;
	FVector3f LodViewOrigin;

	// CBT buffers needed by the vertex factory to reconstruct triangle vertices
	FRHIShaderResourceView* CBT_FibonacciPointsSRV = nullptr;
	FRHIShaderResourceView* CBT_SphericalTrianglesSRV = nullptr;
};

/*
 * Index buffer to provide indices for the mesh we're rendering.
 */
class FGeoVoronoiIndirectInstancingIndexBuffer : public FIndexBuffer
{
public:
	FGeoVoronoiIndirectInstancingIndexBuffer()
	{
	}

	virtual void InitRHI(FRHICommandListBase &RHICmdList) override;

	int32 GetIndexCount() const { return NumIndices; }

private:
	int32 NumIndices = 0;
};

class FGeoVoronoiIndirectInstancingVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FGeoVoronoiIndirectInstancing);

public:
	FGeoVoronoiIndirectInstancingVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const FGeoVoronoiIndirectInstancingParameters &InParams);

	~FGeoVoronoiIndirectInstancingVertexFactory();

	virtual void InitRHI(FRHICommandListBase &RHICmdList) override;
	virtual void ReleaseRHI() override;

	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters &Parameters);
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters &Parameters, FShaderCompilerEnvironment &OutEnvironment);
	static void ValidateCompiledResult(const FVertexFactoryType *Type, EShaderPlatform Platform, const FShaderParameterMap &ParameterMap, TArray<FString> &OutErrors);

	FIndexBuffer const *GetIndexBuffer() const { return IndexBuffer; }

private:
	FGeoVoronoiIndirectInstancingParameters Params;
	FGeoVoronoiIndirectInstancingBufferRef UniformBuffer;
	FGeoVoronoiIndirectInstancingIndexBuffer *IndexBuffer = nullptr;

	// Shader parameters is the data passed to our vertex shader
	friend class FGeoVoronoiIndirectInstancingShaderParameters;
};
