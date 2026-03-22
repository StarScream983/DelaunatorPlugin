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

	// GPU CBT buffers
	FRWBufferStructured HalfEdges_Buffer;     // StructuredBuffer<FHalfEdge_CBT>
	FRWBufferStructured Vertex_Buffer;
	FRWBufferStructured RootBisectors_Buffer; // StructuredBuffer<FRootBisector>
	FRWBufferStructured CBT_Buffer;				// StructuredBuffer<int>
	FRWBufferStructured AllocationCounter_Buffer;
	FRWBufferStructured Pointer_Buffer;

public:

	// Prime Triangle CPU Buffers
	void PrimeTrianglesBuffers(const TArray<FVector3_HighLow>& InFibonacciPoints, const TArray<int32>& InSphericalTriangles, const TArray<int32>& InSphericalTrianglesHalfEdges);
	// Init from CPU arrays (call from game thread)
	void InitFromCPU(const int32 InD,
		const TArray<FHalfEdge_CBT>& InHalfEdges,
		const TArray<FVector3_HighLow>& InVertexBuffer,
		const TArray<FRootBisector_CBT>& InRootBisectors,
		const TArray<int32>& InCBTBuffer);

private:

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	void UploadFibonacciPointsToGPU();
	void UploadSphericalTrianglesToGPU();
	void UploadSphericalTrianglesHalfEdgesBufferToGPU();

	void UploadHalfEdgesToGPU();
	void UploadVertexBufferToGPU();
	void UploadRootBisectorsToGPU();
	void UploadCBTToGPU();
	void UploadAllocationCounterToGPU();
	void UploadPointerBufferToGPU();

};
