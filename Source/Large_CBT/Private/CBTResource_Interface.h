// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RenderResource.h"
#include "CBTStructs.h"

/**
 * 
 */
class LARGE_CBT_API FCBTResource_Interface : public FRenderResource
{
public:
	FCBTResource_Interface() = default;

private:

	// CPU TRIANGLES BUFFERS
	TArray<FVector3_HighLow> CPU_FibonacciPoints_Buffer;
	TArray<int32> CPU_SphericalTriangles_Buffer; // flat array
	TArray<int32> CPU_SphericalTriangles_HalfEdges_Buffer;

	// CPU VORONOI BUFFERS
	TArray<FVector3_HighLow> CPU_VoronoiGeoCenters_Buffer;
	TArray<FUintVector2> CPU_VoronoiGeoMesh_Ranges_Buffer;
	TArray<int32> CPU_VoronoiGeoMesh_Flat_Buffer;
	TArray<uint32> CPU_Voronoi_Cells_Color_Buffer;
	TArray<float> CPU_ElevationPerSite_Buffer;
	TArray<float> CPU_DistanceToBoundaryNormPerSite_Buffer;
	TArray<float> CPU_ErosionControlPerSite_Buffer;
	TArray<uint32> CPU_LandDistanceField_Buffer;
	float CPU_MaxLandDistance = 1.0f;

	// CPU CBT BUFFERS
	int32 D{ 0 };
	TArray<FHalfEdge_CBT> CPUHalfEdge_Buffer;
	TArray<FVector3_HighLow> CPUVertex_Buffer;
	TArray<FRootBisector_CBT> CPURootBisectors_Buffer;
	TArray<int32> CPUCBT_Buffer;
	int32 CPUAllocationCounter = -1;
	TArray<FPointer_CBT> CPUPointer_Buffer;

	// GPU TRIANGLES BUFFERS
	FRWBufferStructured FibonacciPoints_Buffer;
	FRWBufferStructured SphericalTriangles_Buffer;
	FRWBufferStructured SphericalTriangles_HalfEdges_Buffer;

	// GPU VORONOI BUFFERS
	FRWBufferStructured VoronoiGeoCenters_Buffer;

	FBufferRHIRef VoronoiGeoMesh_Ranges_Buffer;
	FShaderResourceViewRHIRef VoronoiGeoMesh_Ranges_SRV;

	FBufferRHIRef VoronoiGeoMesh_Flat_Buffer;
	FShaderResourceViewRHIRef VoronoiGeoMesh_Flat_SRV;

	FBufferRHIRef VoronoiCellColors_Buffer;
	FShaderResourceViewRHIRef VoronoiCellColors_SRV;

	FBufferRHIRef ElevationPerSiteBuffer_RHI;
	FShaderResourceViewRHIRef ElevationPerSiteBuffer_SRV;

	FBufferRHIRef DistanceToBoundaryNormPerSiteBuffer_RHI;
	FShaderResourceViewRHIRef DistanceToBoundaryNormPerSiteBuffer_SRV;

	FBufferRHIRef ErosionControlPerSiteBuffer_RHI;
	FShaderResourceViewRHIRef ErosionControlPerSiteBuffer_SRV;

	FBufferRHIRef LandDistanceFieldBuffer_RHI;
	FShaderResourceViewRHIRef LandDistanceFieldBuffer_SRV;

	// GPU CBT buffers
	FRWBufferStructured HalfEdges_Buffer;     // StructuredBuffer<FHalfEdge_CBT>
	FRWBufferStructured Vertex_Buffer;
	FRWBufferStructured RootBisectors_Buffer; // StructuredBuffer<FRootBisector>
	FRWBufferStructured CBT_Buffer;				// StructuredBuffer<int>
	FRWBufferStructured AllocationCounter_Buffer;
	FRWBufferStructured Pointer_Buffer;

public:
	/** Returns true if InitRHI has been called (render resource is initialized). */
	bool IsInitialized() const { return IsGPUReady(); }

	// GPU Buffer Accessors
	FORCEINLINE FShaderResourceViewRHIRef GetFibonacciPointsSRV() const { return FibonacciPoints_Buffer.SRV; }
	FORCEINLINE FShaderResourceViewRHIRef GetSphericalTrianglesSRV() const { return SphericalTriangles_Buffer.SRV; }
	FORCEINLINE FShaderResourceViewRHIRef GetVoronoiGeoCentersSRV() const { return VoronoiGeoCenters_Buffer.SRV; }
	FORCEINLINE FShaderResourceViewRHIRef GetVoronoiGeoMeshRangesSRV() const { return VoronoiGeoMesh_Ranges_SRV; }
	FORCEINLINE FShaderResourceViewRHIRef GetVoronoiGeoMeshFlatSRV() const { return VoronoiGeoMesh_Flat_SRV; }
	FORCEINLINE FShaderResourceViewRHIRef GetVoronoiCellColorsSRV() const { return VoronoiCellColors_SRV; }
	FORCEINLINE FShaderResourceViewRHIRef GetElevationPerSiteSRV() const { return ElevationPerSiteBuffer_SRV; }
	FORCEINLINE FShaderResourceViewRHIRef GetDistanceToBoundaryNormPerSiteSRV() const { return DistanceToBoundaryNormPerSiteBuffer_SRV; }
	FORCEINLINE FShaderResourceViewRHIRef GetErosionControlPerSiteSRV() const { return ErosionControlPerSiteBuffer_SRV; }
	FORCEINLINE FShaderResourceViewRHIRef GetLandDistanceFieldSRV() const { return LandDistanceFieldBuffer_SRV; }
	FORCEINLINE float GetMaxLandDistance() const { return CPU_MaxLandDistance; }

	/** Returns true if the two required GPU buffers have been initialized. */
	FORCEINLINE bool IsGPUReady() const { return FibonacciPoints_Buffer.SRV.IsValid() && SphericalTriangles_Buffer.SRV.IsValid(); }

	/** Returns the number of Fibonacci sphere vertices uploaded to the GPU. */
	FORCEINLINE uint32 GetNumFibonacciPoints() const { return static_cast<uint32>(CPU_FibonacciPoints_Buffer.Num()); }
	/** Returns the number of spherical triangles (flat indices / 3). */
	FORCEINLINE uint32 GetNumTriangles() const { return static_cast<uint32>(CPU_SphericalTriangles_Buffer.Num() / 3); }
	/** Returns number of Voronoi Geo Centers */
	FORCEINLINE uint32 GetNumVoronoiGeoCenters() const { return static_cast<uint32>(CPU_VoronoiGeoCenters_Buffer.Num()); }
	/**Returns the logical element count of the flat Voronoi polygon index buffer.
	*This is passed to HLSL because StructuredBuffer does not expose a .Num() there.*/
	FORCEINLINE uint32 GetNumVoronoiGeoMeshFlat() const	{ return static_cast<uint32>(CPU_VoronoiGeoMesh_Flat_Buffer.Num());	}

	// Push Colors to buffer for Voronoi cells
	void PrimeVoronoiBuffers(const TArray<FVector3_HighLow>& InVoronoiGeoMeshCenters, const TArray<FUintVector2>& InVoronoiGeoMesh_Ranges, const TArray<int32>& InVoronoiGeoMesh_Flat, const TArray<uint32>& InVoronoiCellColors);
	// Prime Triangle CPU Buffers
	void PrimeTrianglesBuffers(const TArray<FVector3_HighLow>& InFibonacciPoints, const TArray<int32>& InSphericalTriangles, const TArray<int32>& InSphericalTrianglesHalfEdges);
	// Init from CPU arrays (call from game thread)
	void InitFromCPU(const int32 InD,
		const TArray<FHalfEdge_CBT>& InHalfEdges,
		const TArray<FVector3_HighLow>& InVertexBuffer,
		const TArray<FRootBisector_CBT>& InRootBisectors,
		const TArray<int32>& InCBTBuffer);

	void PrimeElevationPerSiteBuffer(const TArray<float>& InElevationPerSite);
	void PrimeDistanceToBoundaryNormPerSiteBuffer(const TArray<float>& InDistanceNormPerSite);
	void PrimeErosionControlPerSiteBuffer(const TArray<float>& InErosionControlPerSite);
	void PrimeLandDistanceFieldBuffer(const TArray<uint32>& InLandDistanceField, float InMaxLandDistance);

private:

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	void UploadFibonacciPointsToGPU();
	void UploadSphericalTrianglesToGPU();
	void UploadSphericalTrianglesHalfEdgesBufferToGPU();

	void UploadVoronoiGeoCentersToGPU();
	void UploadVoronoiGeoMeshRangesToGPU();
	void UploadVoronoiGeoMeshFlatToGPU();
	void UploadVoronoiCellColorsToGPU();

	void UploadHalfEdgesToGPU();
	void UploadVertexBufferToGPU();
	void UploadRootBisectorsToGPU();
	void UploadCBTToGPU();
	void UploadAllocationCounterToGPU();
	void UploadPointerBufferToGPU();
	void UploadElevationPerSiteBufferToGPU();
	void UploadDistanceToBoundaryNormPerSiteBufferToGPU();
	void UploadErosionControlPerSiteBufferToGPU();
	void UploadLandDistanceFieldBufferToGPU();
};
