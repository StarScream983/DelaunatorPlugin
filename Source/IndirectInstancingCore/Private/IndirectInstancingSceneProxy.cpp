// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#include "IndirectInstancingSceneProxy.h"

#include "CommonRenderResources.h"
#include "EngineModule.h"
#include "Engine/Engine.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderUtils.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GeoDelaunatorComponent.h"
#include "CBTResource_Interface.h"
#include "IndirectInstancingVertexFactory.h"

namespace GeoVoronoiIndirectInstancingMesh
{
	/** Initialize the FDrawInstanceBuffers objects. */
	void InitializeInstanceBuffers(FRHICommandListImmediate& InRHICmdList, FDrawInstanceBuffers& InBuffers);

	/** Release the FDrawInstanceBuffers objects. */
	void ReleaseInstanceBuffers(FDrawInstanceBuffers& InBuffers)
	{
		InBuffers.InstanceBuffer.SafeRelease();
		InBuffers.InstanceBufferUAV.SafeRelease();
		InBuffers.InstanceBufferSRV.SafeRelease();
		InBuffers.IndirectArgsBuffer.SafeRelease();
		InBuffers.IndirectArgsBufferUAV.SafeRelease();
	}
}

/** Renderer extension to manage the buffer pool and add hooks for GPU culling passes. */
class FGeoVoronoiIndirectInstancingRendererExtension : public FRenderResource
{
public:
	FGeoVoronoiIndirectInstancingRendererExtension()
		: bInFrame(false), DiscardId(0)
	{
	}

	virtual ~FGeoVoronoiIndirectInstancingRendererExtension()
	{
	}

	bool IsInFrame() { return bInFrame; }

	/** Call once to register this extension. */
	void RegisterExtension();

	/** Call once per frame for each mesh/view that has relevance. */
	GeoVoronoiIndirectInstancingMesh::FDrawInstanceBuffers& AddWork(FGeoVoronoiIndirectInstancingSceneProxy const* InProxy, FSceneView const* InMainView, FSceneView const* InCullView);
	/** Submit all the work added by AddWork(). */
	void SubmitWork(FRDGBuilder& GraphBuilder);

protected:
	//~ Begin FRenderResource Interface
	virtual void ReleaseRHI() override;
	//~ End FRenderResource Interface

private:
	/** Called by renderer at start of render frame. */
	void BeginFrame(FRDGBuilder& GraphBuilder);
	/** Called by renderer at end of render frame. */
	void EndFrame(FRDGBuilder& GraphBuilder);
	void EndFrame();

	/** Flag for frame validation. */
	bool bInFrame;

	/** Buffers to fill. */
	TArray<GeoVoronoiIndirectInstancingMesh::FDrawInstanceBuffers> Buffers;
	/** Per buffer frame time stamp of last usage. */
TArray<uint32> DiscardIds;
	/** Current frame time stamp. */
	uint32 DiscardId;

	/** Array of unique scene proxies to render this frame. */
	TArray<FGeoVoronoiIndirectInstancingSceneProxy const*> SceneProxies;
	/** Array of unique main views to render this frame. */
	TArray<FSceneView const*> MainViews;
	/** Array of unique culling views to render this frame. */
	TArray<FSceneView const*> CullViews;

	/** Key for each buffer we need to generate. */
	struct FWorkDesc
	{
		int32 ProxyIndex;
		int32 MainViewIndex;
		int32 CullViewIndex;
		int32 BufferIndex;
	};

	/** Keys specifying what to render. */
	TArray<FWorkDesc> WorkDescs;

	/** Sort predicate for FWorkDesc. */
	struct FWorkDescSort
	{
		uint32 SortKey(FWorkDesc const& WorkDesc) const
		{
			return (WorkDesc.ProxyIndex << 24) | (WorkDesc.MainViewIndex << 16) | (WorkDesc.CullViewIndex << 8) | WorkDesc.BufferIndex;
		}

		bool operator()(FWorkDesc const& A, FWorkDesc const& B) const
		{
			return SortKey(A) < SortKey(B);
		}
	};
};

/** Single global instance of the renderer extension. */
TGlobalResource<FGeoVoronoiIndirectInstancingRendererExtension> GeoVoronoiIndirectInstancingRendererExtension;

void FGeoVoronoiIndirectInstancingRendererExtension::RegisterExtension()
{
	static bool bInit = false;
	if (!bInit)
	{
		GEngine->GetPreRenderDelegateEx().AddRaw(this, &FGeoVoronoiIndirectInstancingRendererExtension::BeginFrame);
		GEngine->GetPostRenderDelegateEx().AddRaw(this, &FGeoVoronoiIndirectInstancingRendererExtension::EndFrame);
		bInit = true;
	}
}

void FGeoVoronoiIndirectInstancingRendererExtension::ReleaseRHI()
{
	for (GeoVoronoiIndirectInstancingMesh::FDrawInstanceBuffers& Buffer : Buffers)
	{
		GeoVoronoiIndirectInstancingMesh::ReleaseInstanceBuffers(Buffer);
	}

	Buffers.Reset();
	DiscardIds.Reset();
	SceneProxies.Reset();
	MainViews.Reset();
	CullViews.Reset();
	WorkDescs.Reset();

	bInFrame = false;
	DiscardId = 0;
}

GeoVoronoiIndirectInstancingMesh::FDrawInstanceBuffers& FGeoVoronoiIndirectInstancingRendererExtension::AddWork(
	FGeoVoronoiIndirectInstancingSceneProxy const* InProxy,
	FSceneView const* InMainView,
	FSceneView const* InCullView)
{
	if (!ensure(!bInFrame))
	{
		EndFrame();
	}

	FWorkDesc WorkDesc;
	WorkDesc.ProxyIndex = SceneProxies.AddUnique(InProxy);
	WorkDesc.MainViewIndex = MainViews.AddUnique(InMainView);
	WorkDesc.CullViewIndex = CullViews.AddUnique(InCullView);
	WorkDesc.BufferIndex = INDEX_NONE;

	for (const FWorkDesc& ExistingWork : WorkDescs)
	{
		if (ExistingWork.ProxyIndex == WorkDesc.ProxyIndex
			&& ExistingWork.MainViewIndex == WorkDesc.MainViewIndex
			&& ExistingWork.CullViewIndex == WorkDesc.CullViewIndex
			&& ExistingWork.BufferIndex != INDEX_NONE)
		{
			return Buffers[ExistingWork.BufferIndex];
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

	GeoVoronoiIndirectInstancingMesh::InitializeInstanceBuffers(
		GetImmediateCommandList_ForRenderCommand(),
		Buffers[WorkDesc.BufferIndex]);

	return Buffers[WorkDesc.BufferIndex];
}

void FGeoVoronoiIndirectInstancingRendererExtension::BeginFrame(FRDGBuilder& GraphBuilder)
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

void FGeoVoronoiIndirectInstancingRendererExtension::EndFrame(FRDGBuilder& GraphBuilder)
{
	EndFrame();
}

void FGeoVoronoiIndirectInstancingRendererExtension::EndFrame()
{
	if (!bInFrame)
	{
		return;
	}

	bInFrame = false;

	SceneProxies.Reset();
	MainViews.Reset();
	CullViews.Reset();
	WorkDescs.Reset();

	++DiscardId;

	for (int32 Index = 0; Index < DiscardIds.Num();)
	{
		if (DiscardId - DiscardIds[Index] > 4u)
		{
			GeoVoronoiIndirectInstancingMesh::ReleaseInstanceBuffers(Buffers[Index]);
			Buffers.RemoveAtSwap(Index);
			DiscardIds.RemoveAtSwap(Index);
			continue;
		}

		++Index;
	}
}

const static FName NAME_GeoVoronoiIndirectInstancing(TEXT("GeoVoronoiIndirectInstancing"));

FGeoVoronoiIndirectInstancingSceneProxy::FGeoVoronoiIndirectInstancingSceneProxy(UGeoDelaunatorComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent, NAME_GeoVoronoiIndirectInstancing), VertexFactory(nullptr)
{
	UE_LOG(LogTemp, Warning, TEXT("SceneProxy ctor, CBTResources valid=%d"),
		InComponent->GetCBTResources().IsValid() ? 1 : 0);

	GeoVoronoiIndirectInstancingRendererExtension.RegisterExtension();
	bHasDeformableMesh = false;

	UMaterialInterface* ComponentMaterial = InComponent->GetMaterial();
	const bool bValidMaterial = ComponentMaterial != nullptr && ComponentMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_VirtualHeightfieldMesh);
	Material = bValidMaterial ? ComponentMaterial->GetRenderProxy() : UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	MaterialRelevance = Material->GetMaterialInterface()->GetRelevance_Concurrent(GetScene().GetFeatureLevel());

	// Capture the CBT GPU resource pointer (lifetime owned by UGeoDelaunatorComponent)
	CBTResources = InComponent->GetCBTResources();
}

SIZE_T FGeoVoronoiIndirectInstancingSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FGeoVoronoiIndirectInstancingSceneProxy::GetMemoryFootprint() const
{
	return (sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize());
}

void FGeoVoronoiIndirectInstancingSceneProxy::OnTransformChanged()
{
	// TODO
}

void FGeoVoronoiIndirectInstancingSceneProxy::CreateRenderThreadResources()
{
	UE_LOG(LogTemp, Warning, TEXT("SceneProxy::CreateRenderThreadResources"));
	// Gather vertex factory uniform parameters.
	FGeoVoronoiIndirectInstancingParameters UniformParams;
	// TODO UNIFORM INIT

	// Create vertex factory.
	VertexFactory = new FGeoVoronoiIndirectInstancingVertexFactory(GetScene().GetFeatureLevel(), UniformParams);
	VertexFactory->InitResource(FRHICommandListImmediate::Get());
}

void FGeoVoronoiIndirectInstancingSceneProxy::DestroyRenderThreadResources()
{
	if (VertexFactory != nullptr)
	{
		VertexFactory->ReleaseResource();
		delete VertexFactory;
		VertexFactory = nullptr;
	}
}

FPrimitiveViewRelevance FGeoVoronoiIndirectInstancingSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	// Do not draw at all until the CBT GPU buffers are uploaded and ready.
	const bool bHasResources = CBTResources.IsValid();
	const bool bGPUReady = bHasResources && CBTResources->IsGPUReady();

	/*UE_LOG(LogTemp, Warning, TEXT("GetViewRelevance: bHasResources=%d bGPUReady=%d"),
		bHasResources ? 1 : 0, bGPUReady ? 1 : 0);*/

	const bool bValid = CBTResources.IsValid() && CBTResources->IsGPUReady();
	const bool bIsHiddenInEditor = bHiddenInEditor && View->Family->EngineShowFlags.Editor;

	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = bValid && IsShown(View) && !bIsHiddenInEditor;
	Result.bShadowRelevance = bValid && IsShadowCast(View) && ShouldRenderInMainPass() && !bIsHiddenInEditor;
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = false;
	Result.bVelocityRelevance = false;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	return Result;
}

void FGeoVoronoiIndirectInstancingSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	check(IsInRenderingThread());

	if (VertexFactory == nullptr || Material == nullptr || !ViewFamily.Views.IsValidIndex(0))
	{
		return;
	}

	if (GeoVoronoiIndirectInstancingRendererExtension.IsInFrame())
	{
		return;
	}

	FSceneView const* MainView = ViewFamily.Views[0];

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		if ((VisibilityMap & (1u << ViewIndex)) == 0u)
		{
			continue;
		}

		GeoVoronoiIndirectInstancingMesh::FDrawInstanceBuffers& DrawBuffers =
			GeoVoronoiIndirectInstancingRendererExtension.AddWork(this, MainView, Views[ViewIndex]);

		FMeshBatch& Mesh = Collector.AllocateMesh();
		Mesh.bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
		Mesh.bUseWireframeSelectionColoring = IsSelected();
		Mesh.VertexFactory = VertexFactory;
		Mesh.MaterialRenderProxy = Material;
		Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = SDPG_World;
		Mesh.bCanApplyViewModeOverrides = true;
		Mesh.bUseForMaterial = true;
		Mesh.CastShadow = true;
		Mesh.bUseForDepthPass = true;

		Mesh.Elements.SetNumZeroed(1);

		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		BatchElement.IndexBuffer = VertexFactory->GetIndexBuffer();
		BatchElement.IndirectArgsBuffer = DrawBuffers.IndirectArgsBuffer;
		BatchElement.IndirectArgsOffset = 0;
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = 0;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = 2;

		BatchElement.PrimitiveIdMode = PrimID_ForceZero;
		BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();

		FGeoVoronoiIndirectInstancingUserData* UserData =
			&Collector.AllocateOneFrameResource<FGeoVoronoiIndirectInstancingUserData>();
		BatchElement.UserData = UserData;

		UserData->InstanceBufferSRV = DrawBuffers.InstanceBufferSRV;
		UserData->CBT_FibonacciPointsSRV = nullptr;
		UserData->CBT_SphericalTrianglesSRV = nullptr;
		UserData->VoronoiGeoCentersSRV = nullptr;
		UserData->VoronoiGeoMeshRangesSRV = nullptr;
		UserData->VoronoiGeoMeshFlatSRV = nullptr;
		UserData->VoronoiCellColorsSRV = nullptr;

		if (CBTResources.IsValid() && CBTResources->IsGPUReady())
		{
			UserData->CBT_FibonacciPointsSRV = CBTResources->GetFibonacciPointsSRV();
			UserData->CBT_SphericalTrianglesSRV = CBTResources->GetSphericalTrianglesSRV();
			UserData->VoronoiGeoCentersSRV = CBTResources->GetVoronoiGeoCentersSRV();
			UserData->VoronoiGeoMeshRangesSRV = CBTResources->GetVoronoiGeoMeshRangesSRV();
			UserData->VoronoiGeoMeshFlatSRV = CBTResources->GetVoronoiGeoMeshFlatSRV();
			UserData->VoronoiCellColorsSRV = CBTResources->GetVoronoiCellColorsSRV();
		}

		UserData->LodViewOrigin = (FVector3f)MainView->ViewMatrices.GetViewOrigin();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const FViewMatrices* FrozenViewMatrices =
			MainView->State != nullptr ? MainView->State->GetFrozenViewMatrices() : nullptr;
		if (FrozenViewMatrices != nullptr)
		{
			UserData->LodViewOrigin = (FVector3f)FrozenViewMatrices->GetViewOrigin();
		}
#endif

		Collector.AddMesh(ViewIndex, Mesh);
	}
}

namespace GeoVoronoiIndirectInstancingMesh
{
	/* Keep indirect args offsets in sync with ISM.usf. */
	static const int32 IndirectArgsByteOffset_FinalCull = 0;
	static const int32 IndirectArgsByteSize = 4 * sizeof(uint32);
	static const uint32 MaxSupportedInstances = 1u << 18;

	struct WorkerQueueInfo
	{
		uint32 Read;
		uint32 Write;
		int32 NumActive;
	};

	struct FGeoVoronoiIndirectInstancingRenderInstance
	{
		//float Position[3];
		float UVTransform[3]; // float3
		uint32 TriangleId;    // uint PosLevelPacked — stores the triangle index
	};

	/** Compute shader to initialize all buffers. */
	class FInitBuffersVHM_CS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FInitBuffersVHM_CS);
		SHADER_USE_PARAMETER_STRUCT(FInitBuffersVHM_CS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(uint32, MaxLevel)
			SHADER_PARAMETER(uint32, NumForceLoadLods)
			SHADER_PARAMETER(uint32, PageTableFeedbackId)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<WorkerQueueInfo>, RWQueueInfo)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWQueueBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, RWQuadBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWFeedbackBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FInitBuffersVHM_CS, "/IndirectInstancingCoreShaders/GeoVoronoiIndirectInstancingCompute.usf", "InitBuffersCS", SF_Compute);

	/** Compute shader to collect quads. */
	class FCollectQuadsVHM_CS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FCollectQuadsVHM_CS);
		SHADER_USE_PARAMETER_STRUCT(FCollectQuadsVHM_CS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_TEXTURE(Texture2D, HeightMinMaxTexture)
			SHADER_PARAMETER_SAMPLER(SamplerState, MinMaxTextureSampler)
			SHADER_PARAMETER(int32, MinMaxLevelOffset)
			SHADER_PARAMETER_TEXTURE(Texture2D, LodBiasMinMaxTexture)
			SHADER_PARAMETER_TEXTURE(Texture2D<float>, OcclusionTexture)
			SHADER_PARAMETER(int32, OcclusionLevelOffset)
			SHADER_PARAMETER_TEXTURE(Texture2D<uint> , PageTableTexture)
			SHADER_PARAMETER(uint32, MaxLevel)
			SHADER_PARAMETER(FVector4f, PageTableSize)
			SHADER_PARAMETER(uint32, PageTableFeedbackId)
			SHADER_PARAMETER(FVector4f, LodDistances)
			SHADER_PARAMETER(float, LodBiasScale)
			SHADER_PARAMETER(FVector3f, ViewOrigin)
			SHADER_PARAMETER_ARRAY(FVector4f, FrustumPlanes, [5])
			SHADER_PARAMETER(FMatrix44f, UVToWorld)
			SHADER_PARAMETER(FVector3f, UVToWorldScale)
			SHADER_PARAMETER(uint32, QueueBufferSizeMask)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<WorkerQueueInfo>, RWQueueInfo)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWQueueBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, RWQuadBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWFeedbackBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FCollectQuadsVHM_CS, "/IndirectInstancingCoreShaders/GeoVoronoiIndirectInstancingCompute.usf", "CollectQuadsCS", SF_Compute);

	/** InitInstanceBuffer compute shader. */
	class FInitInstanceBufferVHM_CS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FInitInstanceBufferVHM_CS);
		SHADER_USE_PARAMETER_STRUCT(FInitInstanceBufferVHM_CS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(int32, NumIndices)
			SHADER_PARAMETER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FInitInstanceBufferVHM_CS, "/IndirectInstancingCoreShaders/GeoVoronoiIndirectInstancingCompute.usf", "InitInstanceBufferCS", SF_Compute);

	/** CullInstances compute shader. */
	class FCullInstancesVHM_CS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FCullInstancesVHM_CS);
		SHADER_USE_PARAMETER_STRUCT(FCullInstancesVHM_CS, FGlobalShader);

		class FReuseCullDim : SHADER_PERMUTATION_BOOL("REUSE_CULL");

		using FPermutationDomain = TShaderPermutationDomain<FReuseCullDim>;

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_TEXTURE(Texture2D, HeightMinMaxTexture)
			SHADER_PARAMETER_SAMPLER(SamplerState, MinMaxTextureSampler)
			SHADER_PARAMETER(int32, MinMaxLevelOffset)
			SHADER_PARAMETER_TEXTURE(Texture2D, PageTableTexture)
			SHADER_PARAMETER(FVector4f, PageTableSize)
			SHADER_PARAMETER_ARRAY(FVector4f, FrustumPlanes, [5])
			SHADER_PARAMETER(FVector4f, PhysicalPageTransform)
			SHADER_PARAMETER(uint32, NumPhysicalAddressBits)
			SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint2>, QuadBuffer)
			SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, IndirectArgsBufferSRV)
			SHADER_PARAMETER_UAV(RWStructuredBuffer<GeoVoronoiIndirectInstancingMesh::FGeoVoronoiIndirectInstancingRenderInstance>, RWInstanceBuffer)
			SHADER_PARAMETER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
			RDG_BUFFER_ACCESS(IndirectArgsBuffer, ERHIAccess::IndirectArgs)
			SHADER_PARAMETER(uint32, MaxInstances)
			// CBT buffers
			SHADER_PARAMETER(uint32, NumSites)
			SHADER_PARAMETER(uint32, NumVoronoiCenters)
			SHADER_PARAMETER_SRV(StructuredBuffer<FVector3_HighLow>, CBT_FibonacciPoints)
			SHADER_PARAMETER_SRV(StructuredBuffer<int>, CBT_SphericalTriangles)
			SHADER_PARAMETER_SRV(StructuredBuffer<FVector3_HighLow>, VoronoiGeoCenters)
			SHADER_PARAMETER_SRV(StructuredBuffer<uint2>, VoronoiGeoMeshRanges)
			SHADER_PARAMETER_SRV(StructuredBuffer<int>, VoronoiGeoMeshFlat)
		END_SHADER_PARAMETER_STRUCT()
	};

	IMPLEMENT_GLOBAL_SHADER(FCullInstancesVHM_CS, "/IndirectInstancingCoreShaders/GeoVoronoiIndirectInstancingCompute.usf", "CullInstancesCS", SF_Compute);

	/** Default Min/Max texture has the fixed maximum [0,1]. */
	class FHeightMinMaxDefaultTexture : public FTexture
	{
	public:
		virtual void InitRHI(FRHICommandListBase& RHICmdList) override
		{
			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create2D(TEXT("GeoVoronoi.MinMaxDefaultTexture"), 1, 1, PF_B8G8R8A8)
				.SetFlags(ETextureCreateFlags::ShaderResource);
			TextureRHI = RHICreateTexture(Desc);

			uint32 DestStride;
			FColor* DestBuffer = (FColor*)RHILockTexture2D(TextureRHI, 0, RLM_WriteOnly, DestStride, false);
			*DestBuffer = FColor(0, 0, 255, 255);
			RHIUnlockTexture2D(TextureRHI, 0, false);

			FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point, AM_Clamp, AM_Clamp, AM_Clamp);
			SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);
		}

		virtual uint32 GetSizeX() const override { return 1; }
		virtual uint32 GetSizeY() const override { return 1; }
	};

	FTexture* GHeightMinMaxDefaultTexture = new TGlobalResource<FHeightMinMaxDefaultTexture>;

	struct FViewData
	{
		FVector ViewOrigin;
		FMatrix ProjectionMatrix;
		FConvexVolume ViewFrustum;
		bool bViewFrozen;
	};

	void GetViewData(FSceneView const* InSceneView, FViewData& OutViewData)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const FViewMatrices* FrozenViewMatrices = InSceneView->State != nullptr ? InSceneView->State->GetFrozenViewMatrices() : nullptr;
		if (FrozenViewMatrices != nullptr)
		{
			OutViewData.ViewOrigin = FrozenViewMatrices->GetViewOrigin();
			OutViewData.ProjectionMatrix = FrozenViewMatrices->GetProjectionMatrix();
			GetViewFrustumBounds(OutViewData.ViewFrustum, FrozenViewMatrices->GetViewProjectionMatrix(), true);
			OutViewData.bViewFrozen = true;
		}
		else
#endif
		{
			OutViewData.ViewOrigin = InSceneView->ViewMatrices.GetViewOrigin();
			OutViewData.ProjectionMatrix = InSceneView->ViewMatrices.GetProjectionMatrix();
			OutViewData.ViewFrustum = InSceneView->ViewFrustum;
			OutViewData.bViewFrozen = false;
		}
	}

	struct FProxyDesc
	{
		FProxyDesc()
			: PageTableTexture(nullptr)
			, HeightMinMaxTexture(nullptr)
			, LodBiasMinMaxTexture(nullptr)
			, MinMaxLevelOffset(0)
			, MaxLevel(0)
			, NumForceLoadLods(0)
			, PageTableFeedbackId(0)
			, NumPhysicalAddressBits(0)
			, PageTableSize(0.0, 0.0, 0.0, 0.0)
			, PhysicalPageTransform(0.0, 0.0, 0.0, 0.0)
			, UVToWorld(FMatrix::Identity)
			, UVToWorldScale(FVector::ZeroVector)
			, NumQuadsPerTileSide(0)
			, MaxPersistentQueueItems(0)
			, MaxRenderItems(0)
			, MaxFeedbackItems(0)
			, NumCollectPassWavefronts(0)
		{
		}

		FRHITexture* PageTableTexture;
		FRHITexture* HeightMinMaxTexture;
		FRHITexture* LodBiasMinMaxTexture;
		int32 MinMaxLevelOffset;

		uint32 MaxLevel;
		uint32 NumForceLoadLods;
		uint32 PageTableFeedbackId;
		uint32 NumPhysicalAddressBits;
		FVector4 PageTableSize;
		FVector4 PhysicalPageTransform;
		FMatrix UVToWorld;
		FVector UVToWorldScale;
		uint32 NumQuadsPerTileSide;

		int32 MaxPersistentQueueItems;
		int32 MaxRenderItems;
		int32 MaxFeedbackItems;
		int32 NumCollectPassWavefronts;

		TSharedPtr<FCBTResource_Interface> CBTResources;
	};

	struct FMainViewDesc
	{
		FMainViewDesc()
			: ViewDebug(nullptr)
			, ViewOrigin(FVector::ZeroVector)
			, LodDistances(0.0, 0.0, 0.0, 0.0)
			, LodBiasScale(0.0f)
			, OcclusionLevelOffset(0)
		{
			for (int32 PlaneIndex = 0; PlaneIndex < 5; ++PlaneIndex)
			{
				Planes[PlaneIndex] = FVector4(0.0, 0.0, 0.0, 0.0);
			}
		}

		FSceneView const* ViewDebug;
		FVector ViewOrigin;
		FVector4 LodDistances;
		float LodBiasScale;
		FVector4 Planes[5];
		FTextureRHIRef OcclusionTexture;
		int32 OcclusionLevelOffset;
	};

	struct FChildViewDesc
	{
		FChildViewDesc()
			: ViewDebug(nullptr)
			, bIsMainView(false)
		{
			for (int32 PlaneIndex = 0; PlaneIndex < 5; ++PlaneIndex)
			{
				Planes[PlaneIndex] = FVector4(0.0, 0.0, 0.0, 0.0);
			}
		}

		FSceneView const* ViewDebug;
		bool bIsMainView;
		FVector4 Planes[5];
	};

	struct FVolatileResources
	{
		FRDGBufferRef QueueInfo;
		FRDGBufferUAVRef QueueInfoUAV;
		FRDGBufferRef QueueBuffer;
		FRDGBufferUAVRef QueueBufferUAV;

		FRDGBufferRef QuadBuffer;
		FRDGBufferUAVRef QuadBufferUAV;
		FRDGBufferSRVRef QuadBufferSRV;

		FRDGBufferRef FeedbackBuffer;
		FRDGBufferUAVRef FeedbackBufferUAV;

		FRDGBufferRef IndirectArgsBuffer;
		FRDGBufferUAVRef IndirectArgsBufferUAV;
		FRDGBufferSRVRef IndirectArgsBufferSRV;
	};

	void InitializeInstanceBuffers(FRHICommandListImmediate& InRHICmdList, FDrawInstanceBuffers& InBuffers)
	{
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FGeoVoronoiIndirectInstancing.InstanceBuffer"));
			const int32 InstanceSize = sizeof(GeoVoronoiIndirectInstancingMesh::FGeoVoronoiIndirectInstancingRenderInstance);
			const int32 InstanceBufferSize = int32(GeoVoronoiIndirectInstancingMesh::MaxSupportedInstances) * InstanceSize;
			InBuffers.InstanceBuffer = InRHICmdList.CreateStructuredBuffer(InstanceSize, InstanceBufferSize, BUF_UnorderedAccess | BUF_ShaderResource, ERHIAccess::SRVMask, CreateInfo);
			InBuffers.InstanceBufferUAV = InRHICmdList.CreateUnorderedAccessView(InBuffers.InstanceBuffer, false, false);
			InBuffers.InstanceBufferSRV = InRHICmdList.CreateShaderResourceView(InBuffers.InstanceBuffer);
		}
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FGeoVoronoiIndirectInstancing.InstanceIndirectArgsBuffer"));
			InBuffers.IndirectArgsBuffer = InRHICmdList.CreateVertexBuffer(5 * sizeof(uint32), BUF_UnorderedAccess | BUF_DrawIndirect, ERHIAccess::IndirectArgs, CreateInfo);
			InBuffers.IndirectArgsBufferUAV = InRHICmdList.CreateUnorderedAccessView(InBuffers.IndirectArgsBuffer, PF_R32_UINT);
		}
	}

	void InitializeResources(FRDGBuilder& GraphBuilder, FProxyDesc const& InDesc, FMainViewDesc const& InMainViewDesc, FVolatileResources& OutResources)
	{
		OutResources.QueueInfo = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(WorkerQueueInfo), 1), TEXT("GeoVoronoiIndirectInstancingMesh.QueueInfo"));
		OutResources.QueueInfoUAV = GraphBuilder.CreateUAV(OutResources.QueueInfo);
		OutResources.QueueBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), InDesc.MaxPersistentQueueItems), TEXT("GeoVoronoiIndirectInstancingMesh.QuadQueue"));
		OutResources.QueueBufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutResources.QueueBuffer, PF_R32_UINT));

		OutResources.QuadBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32) * 2, InDesc.MaxRenderItems), TEXT("GeoVoronoiIndirectInstancingMesh.QuadBuffer"));
		OutResources.QuadBufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutResources.QuadBuffer, PF_R32G32_UINT));
		OutResources.QuadBufferSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(OutResources.QuadBuffer, PF_R32G32_UINT));

		FRDGBufferDesc FeedbackBufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), InDesc.MaxFeedbackItems + 1);
		FeedbackBufferDesc.Usage = EBufferUsageFlags(FeedbackBufferDesc.Usage | BUF_SourceCopy);
		OutResources.FeedbackBuffer = GraphBuilder.CreateBuffer(FeedbackBufferDesc, TEXT("GeoVoronoiIndirectInstancingMesh.FeedbackBuffer"));
		OutResources.FeedbackBufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutResources.FeedbackBuffer, PF_R32_UINT));

		OutResources.IndirectArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(IndirectArgsByteSize), TEXT("GeoVoronoiIndirectInstancingMesh.IndirectArgsBuffer"));
		OutResources.IndirectArgsBufferUAV = GraphBuilder.CreateUAV(OutResources.IndirectArgsBuffer);
		OutResources.IndirectArgsBufferSRV = GraphBuilder.CreateSRV(OutResources.IndirectArgsBuffer);
	}

	void AddPass_TransitionAllDrawBuffers(FRDGBuilder& GraphBuilder, TArray<GeoVoronoiIndirectInstancingMesh::FDrawInstanceBuffers> const& Buffers, TArrayView<int32> const& BufferIndices, bool bToWrite)
	{
		TArray<FRHIUnorderedAccessView*> OverlapUAVs;
		OverlapUAVs.Reserve(BufferIndices.Num());

		TArray<FRHITransitionInfo> TransitionInfos;
		TransitionInfos.Reserve(BufferIndices.Num() * 2);

		for (int32 BufferIndex : BufferIndices)
		{
			FRHIUnorderedAccessView* IndirectArgsBufferUAV = Buffers[BufferIndex].IndirectArgsBufferUAV;
			FRHIUnorderedAccessView* InstanceBufferUAV = Buffers[BufferIndex].InstanceBufferUAV;

			OverlapUAVs.Add(IndirectArgsBufferUAV);

			TransitionInfos.Add(FRHITransitionInfo(IndirectArgsBufferUAV, bToWrite ? ERHIAccess::IndirectArgs : ERHIAccess::UAVMask, bToWrite ? ERHIAccess::UAVMask : ERHIAccess::IndirectArgs));
			TransitionInfos.Add(FRHITransitionInfo(InstanceBufferUAV, bToWrite ? ERHIAccess::SRVMask : ERHIAccess::UAVMask, bToWrite ? ERHIAccess::UAVMask : ERHIAccess::SRVMask));
		}

		AddPass(GraphBuilder, RDG_EVENT_NAME("TransitionAllDrawBuffers"), [bToWrite, OverlapUAVs, TransitionInfos](FRHICommandList& InRHICmdList)
			{
				if (!bToWrite)
				{
					InRHICmdList.EndUAVOverlap(OverlapUAVs);
				}

				InRHICmdList.Transition(TransitionInfos);

				if (bToWrite)
				{
					InRHICmdList.BeginUAVOverlap(OverlapUAVs);
				}
			});
	}

	void AddPass_InitBuffers(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InGlobalShaderMap, FProxyDesc const& InDesc, FVolatileResources& InVolatileResources)
	{
		TShaderMapRef<FInitBuffersVHM_CS> ComputeShader(InGlobalShaderMap);

		FInitBuffersVHM_CS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitBuffersVHM_CS::FParameters>();
		PassParameters->MaxLevel = InDesc.MaxLevel;
		PassParameters->NumForceLoadLods = InDesc.NumForceLoadLods;
		PassParameters->PageTableFeedbackId = InDesc.PageTableFeedbackId;
		PassParameters->RWQueueInfo = InVolatileResources.QueueInfoUAV;
		PassParameters->RWQueueBuffer = InVolatileResources.QueueBufferUAV;
		PassParameters->RWQuadBuffer = InVolatileResources.QuadBufferUAV;
		PassParameters->RWIndirectArgsBuffer = InVolatileResources.IndirectArgsBufferUAV;
		PassParameters->RWFeedbackBuffer = InVolatileResources.FeedbackBufferUAV;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("InitBuffers"),
			PassParameters,
			ERDGPassFlags::Compute,
			[PassParameters, ComputeShader](FRHICommandList& RHICmdList)
			{
				RHICmdList.ClearUAVUint(PassParameters->RWFeedbackBuffer->GetRHI(), FUintVector4(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff));
				FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, FIntVector(1, 1, 1));
			});
	}

	void AddPass_CollectQuads(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InGlobalShaderMap, FProxyDesc const& InDesc, FVolatileResources& InVolatileResources, FMainViewDesc const& InViewDesc)
	{
		TShaderMapRef<FCollectQuadsVHM_CS> ComputeShader(InGlobalShaderMap);

		FCollectQuadsVHM_CS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCollectQuadsVHM_CS::FParameters>();
		PassParameters->HeightMinMaxTexture = InDesc.HeightMinMaxTexture;
		PassParameters->LodBiasMinMaxTexture = InDesc.LodBiasMinMaxTexture;
		PassParameters->MinMaxTextureSampler = TStaticSamplerState<SF_Point>::GetRHI();
		PassParameters->MinMaxLevelOffset = InDesc.MinMaxLevelOffset;
		PassParameters->OcclusionTexture = InViewDesc.OcclusionTexture;
		PassParameters->OcclusionLevelOffset = InViewDesc.OcclusionLevelOffset;
		PassParameters->PageTableTexture = InDesc.PageTableTexture;
		PassParameters->MaxLevel = InDesc.MaxLevel;
		PassParameters->PageTableSize = FVector4f(InDesc.PageTableSize);
		PassParameters->PageTableFeedbackId = InDesc.PageTableFeedbackId;
		PassParameters->UVToWorld = FMatrix44f(InDesc.UVToWorld);
		PassParameters->UVToWorldScale = (FVector3f)InDesc.UVToWorldScale;
		PassParameters->ViewOrigin = (FVector3f)InViewDesc.ViewOrigin;
		PassParameters->LodDistances = FVector4f(InViewDesc.LodDistances);
		PassParameters->LodBiasScale = InViewDesc.LodBiasScale;
		for (int32 PlaneIndex = 0; PlaneIndex < 5; ++PlaneIndex)
		{
			PassParameters->FrustumPlanes[PlaneIndex] = FVector4f(InViewDesc.Planes[PlaneIndex]);
		}
		PassParameters->QueueBufferSizeMask = InDesc.MaxPersistentQueueItems - 1;
		PassParameters->RWQueueInfo = InVolatileResources.QueueInfoUAV;
		PassParameters->RWQueueBuffer = InVolatileResources.QueueBufferUAV;
		PassParameters->RWQuadBuffer = InVolatileResources.QuadBufferUAV;
		PassParameters->RWIndirectArgsBuffer = InVolatileResources.IndirectArgsBufferUAV;
		PassParameters->RWFeedbackBuffer = InVolatileResources.FeedbackBufferUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CollectQuads"),
			ComputeShader, PassParameters, FIntVector(InDesc.NumCollectPassWavefronts, 1, 1));
	}

	void AddPass_InitInstanceBuffer(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InGlobalShaderMap, FDrawInstanceBuffers& InOutputResources)
	{
		TShaderMapRef<FInitInstanceBufferVHM_CS> ComputeShader(InGlobalShaderMap);

		FInitInstanceBufferVHM_CS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitInstanceBufferVHM_CS::FParameters>();
		PassParameters->NumIndices = 3;
		PassParameters->RWIndirectArgsBuffer = InOutputResources.IndirectArgsBufferUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitInstanceBuffer"),
			ComputeShader, PassParameters, FIntVector(1, 1, 1));
	}

	void AddPass_CullInstances(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InGlobalShaderMap, FProxyDesc const& InDesc, FVolatileResources& InVolatileResources, FDrawInstanceBuffers& InOutputResources, FChildViewDesc const& InViewDesc, TSharedPtr<FCBTResource_Interface> InCBTResources)
	{
		if (!InCBTResources.IsValid() || !InCBTResources->IsGPUReady())
		{
			return;
		}

		FCullInstancesVHM_CS::FParameters* PassParameters =
			GraphBuilder.AllocParameters<FCullInstancesVHM_CS::FParameters>();

		PassParameters->HeightMinMaxTexture =
			InDesc.HeightMinMaxTexture != nullptr
				? InDesc.HeightMinMaxTexture
				: GHeightMinMaxDefaultTexture->TextureRHI.GetReference();
		PassParameters->MinMaxTextureSampler = TStaticSamplerState<SF_Point>::GetRHI();
		PassParameters->MinMaxLevelOffset = InDesc.MinMaxLevelOffset;
		PassParameters->PageTableTexture =
			InDesc.PageTableTexture != nullptr
				? InDesc.PageTableTexture
				: GBlackTexture->TextureRHI.GetReference();
		PassParameters->PageTableSize = FVector4f(InDesc.PageTableSize);
		PassParameters->PhysicalPageTransform = FVector4f(InDesc.PhysicalPageTransform);
		PassParameters->NumPhysicalAddressBits = InDesc.NumPhysicalAddressBits;

		for (int32 PlaneIndex = 0; PlaneIndex < 5; ++PlaneIndex)
		{
			PassParameters->FrustumPlanes[PlaneIndex] = FVector4f(InViewDesc.Planes[PlaneIndex]);
		}

		PassParameters->QuadBuffer = InVolatileResources.QuadBufferSRV;
		PassParameters->IndirectArgsBuffer = InVolatileResources.IndirectArgsBuffer;
		PassParameters->IndirectArgsBufferSRV = InVolatileResources.IndirectArgsBufferSRV;
		PassParameters->RWInstanceBuffer = InOutputResources.InstanceBufferUAV;
		PassParameters->RWIndirectArgsBuffer = InOutputResources.IndirectArgsBufferUAV;
		PassParameters->MaxInstances = GeoVoronoiIndirectInstancingMesh::MaxSupportedInstances;

		// Bind CBT GPU buffers
		PassParameters->NumSites = InCBTResources->GetNumFibonacciPoints();
		PassParameters->NumVoronoiCenters = InCBTResources->GetNumVoronoiGeoCenters();
		PassParameters->CBT_FibonacciPoints = InCBTResources->GetFibonacciPointsSRV();
		PassParameters->CBT_SphericalTriangles = InCBTResources->GetSphericalTrianglesSRV();
		PassParameters->VoronoiGeoCenters = InCBTResources->GetVoronoiGeoCentersSRV();
		PassParameters->VoronoiGeoMeshRanges = InCBTResources->GetVoronoiGeoMeshRangesSRV();
		PassParameters->VoronoiGeoMeshFlat = InCBTResources->GetVoronoiGeoMeshFlatSRV();

		FCullInstancesVHM_CS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FCullInstancesVHM_CS::FReuseCullDim>(InViewDesc.bIsMainView);

		const uint32 NumSites = InCBTResources->GetNumFibonacciPoints();
		if (NumSites == 0u)
		{
			return;
		}
		const FIntVector GroupCount(FMath::DivideAndRoundUp<int32>((int32)NumSites, 64), 1, 1);
		TShaderMapRef<FCullInstancesVHM_CS> ComputeShader(InGlobalShaderMap, PermutationVector);
		/*FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CullInstances"),
			ComputeShader, PassParameters,
			InVolatileResources.IndirectArgsBuffer,
			IndirectArgOffset);*/

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CullInstances"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}
}

void FGeoVoronoiIndirectInstancingRendererExtension::SubmitWork(FRDGBuilder& GraphBuilder)
{
	WorkDescs.Sort(FWorkDescSort());

	TArray<int32, TInlineAllocator<8>> UsedBufferIndices;
	for (const FWorkDesc& WorkDesc : WorkDescs)
	{
		UsedBufferIndices.AddUnique(WorkDesc.BufferIndex);
	}

	if (UsedBufferIndices.Num() == 0)
	{
		return;
	}

	GeoVoronoiIndirectInstancingMesh::AddPass_TransitionAllDrawBuffers(GraphBuilder, Buffers, UsedBufferIndices, true);

	for (const FWorkDesc& WorkDesc : WorkDescs)
	{
		GeoVoronoiIndirectInstancingMesh::AddPass_InitInstanceBuffer(
			GraphBuilder,
			GetGlobalShaderMap(GMaxRHIFeatureLevel),
			Buffers[WorkDesc.BufferIndex]);
	}

	const int32 NumWorkItems = WorkDescs.Num();
	int32 WorkIndex = 0;

	while (WorkIndex < NumWorkItems)
	{
		const FGeoVoronoiIndirectInstancingSceneProxy* Proxy = SceneProxies[WorkDescs[WorkIndex].ProxyIndex];

		GeoVoronoiIndirectInstancingMesh::FProxyDesc ProxyDesc;
		ProxyDesc.PageTableTexture = GBlackTexture->TextureRHI.GetReference();
		ProxyDesc.HeightMinMaxTexture = GeoVoronoiIndirectInstancingMesh::GHeightMinMaxDefaultTexture->TextureRHI.GetReference();
		ProxyDesc.LodBiasMinMaxTexture = GeoVoronoiIndirectInstancingMesh::GHeightMinMaxDefaultTexture->TextureRHI.GetReference();
		ProxyDesc.MinMaxLevelOffset = 0;
		ProxyDesc.MaxLevel = 0;
		ProxyDesc.NumForceLoadLods = 0;
		ProxyDesc.PageTableFeedbackId = 0;
		ProxyDesc.NumPhysicalAddressBits = 0;
		ProxyDesc.PageTableSize = FVector4(0.0, 0.0, 0.0, 0.0);
		ProxyDesc.PhysicalPageTransform = FVector4(0.0, 0.0, 0.0, 0.0);
		ProxyDesc.UVToWorld = FMatrix::Identity;
		ProxyDesc.UVToWorldScale = FVector::OneVector;
		ProxyDesc.NumQuadsPerTileSide = 0;
		ProxyDesc.MaxPersistentQueueItems = 1 << FMath::CeilLogTwo(1024 * 4);
		ProxyDesc.MaxRenderItems = 1024 * 4;
		ProxyDesc.MaxFeedbackItems = 1024 * 4;
		ProxyDesc.NumCollectPassWavefronts = 16;
		ProxyDesc.CBTResources = Proxy != nullptr ? Proxy->CBTResources : nullptr;

		while (WorkIndex < NumWorkItems && SceneProxies[WorkDescs[WorkIndex].ProxyIndex] == Proxy)
		{
			FSceneView const* MainView = MainViews[WorkDescs[WorkIndex].MainViewIndex];

			GeoVoronoiIndirectInstancingMesh::FViewData MainViewData;
			GeoVoronoiIndirectInstancingMesh::GetViewData(MainView, MainViewData);

			GeoVoronoiIndirectInstancingMesh::FMainViewDesc MainViewDesc = {};
			MainViewDesc.ViewDebug = MainView;
			MainViewDesc.ViewOrigin = MainViewData.ViewOrigin;

			const int32 NumMainPlanes = FMath::Min(MainViewData.ViewFrustum.Planes.Num(), 5);
			for (int32 PlaneIndex = 0; PlaneIndex < NumMainPlanes; ++PlaneIndex)
			{
				const FPlane& Plane = MainViewData.ViewFrustum.Planes[PlaneIndex];
				MainViewDesc.Planes[PlaneIndex] = FVector4(Plane.X, Plane.Y, Plane.Z, Plane.W);
			}

			// Build only the volatile buffers CullInstances needs (QuadBuffer, IndirectArgs).
			// Skip InitBuffers and CollectQuads — not needed for CBT direct triangle dispatch.
			GeoVoronoiIndirectInstancingMesh::FVolatileResources VolatileResources;
			GeoVoronoiIndirectInstancingMesh::InitializeResources(GraphBuilder, ProxyDesc, MainViewDesc, VolatileResources);

			while (WorkIndex < NumWorkItems && MainViews[WorkDescs[WorkIndex].MainViewIndex] == MainView)
			{
				FSceneView const* CullView = CullViews[WorkDescs[WorkIndex].CullViewIndex];

				GeoVoronoiIndirectInstancingMesh::FViewData CullViewData;
				GeoVoronoiIndirectInstancingMesh::GetViewData(CullView, CullViewData);

				GeoVoronoiIndirectInstancingMesh::FChildViewDesc ChildViewDesc;
				ChildViewDesc.ViewDebug = CullView;
				ChildViewDesc.bIsMainView = (CullView == MainView);

				const int32 NumCullPlanes = FMath::Min(CullViewData.ViewFrustum.Planes.Num(), 5);
				for (int32 PlaneIndex = 0; PlaneIndex < NumCullPlanes; ++PlaneIndex)
				{
					const FPlane& Plane = CullViewData.ViewFrustum.Planes[PlaneIndex];
					ChildViewDesc.Planes[PlaneIndex] = FVector4(Plane.X, Plane.Y, Plane.Z, Plane.W);
				}

				GeoVoronoiIndirectInstancingMesh::AddPass_CullInstances(
					GraphBuilder,
					GetGlobalShaderMap(GMaxRHIFeatureLevel),
					ProxyDesc,
					VolatileResources,
					Buffers[WorkDescs[WorkIndex].BufferIndex],
					ChildViewDesc,
					ProxyDesc.CBTResources);

				++WorkIndex;
			}
		}
	}

	GeoVoronoiIndirectInstancingMesh::AddPass_TransitionAllDrawBuffers(GraphBuilder, Buffers, UsedBufferIndices, false);
}