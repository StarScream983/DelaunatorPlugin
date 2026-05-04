// Fill out your copyright notice in the Description page of Project Settings.


#include "CBTResource_Interface.h"
#include "RenderCore.h"
#include "RHI.h"

void FCBTResource_Interface::PrimeVoronoiBuffers(const TArray<FVector3_HighLow>& InVoronoiGeoMeshCenters, const TArray<FUintVector2>& InVoronoiGeoMesh_Ranges, const TArray<int32>& InVoronoiGeoMesh_Flat, const TArray<uint32>& InVoronoiCellColors)
{
	CPU_VoronoiGeoCenters_Buffer.Empty();
	CPU_VoronoiGeoMesh_Ranges_Buffer.Empty();
	CPU_VoronoiGeoMesh_Flat_Buffer.Empty();
	CPU_Voronoi_Cells_Color_Buffer.Empty();

	CPU_VoronoiGeoCenters_Buffer = InVoronoiGeoMeshCenters;
	CPU_VoronoiGeoMesh_Ranges_Buffer = InVoronoiGeoMesh_Ranges;
	CPU_VoronoiGeoMesh_Flat_Buffer = InVoronoiGeoMesh_Flat;
	CPU_Voronoi_Cells_Color_Buffer = InVoronoiCellColors;

	/*UE_LOG(LogTemp, Warning, TEXT("PrimeVoronoiBuffers: Centers=%d Ranges=%d Flat=%d Colors=%d"),
		InVoronoiGeoMeshCenters.Num(),
		InVoronoiGeoMesh_Ranges.Num(),
		InVoronoiGeoMesh_Flat.Num(),
		InVoronoiCellColors.Num());*/
}

void FCBTResource_Interface::PrimeTrianglesBuffers(const TArray<FVector3_HighLow>& InFibonacciPoints, const TArray<int32>& InSphericalTriangles, const TArray<int32>& InSphericalTrianglesHalfEdges)
{
    CPU_FibonacciPoints_Buffer.Empty();
    CPU_SphericalTriangles_Buffer.Empty();
    CPU_SphericalTriangles_HalfEdges_Buffer.Empty();

    CPU_FibonacciPoints_Buffer = InFibonacciPoints;
    CPU_SphericalTriangles_Buffer = InSphericalTriangles;
    CPU_SphericalTriangles_HalfEdges_Buffer = InSphericalTrianglesHalfEdges;
}

void FCBTResource_Interface::PrimeElevationPerSiteBuffer(const TArray<float>& InElevationPerSite)
{
    CPU_ElevationPerSite_Buffer.Empty();
    CPU_ElevationPerSite_Buffer = InElevationPerSite;
}

void FCBTResource_Interface::InitFromCPU(const int32 InD, const TArray<FHalfEdge_CBT>& InHalfEdges, const TArray<FVector3_HighLow>& InVertexBuffer, const TArray<FRootBisector_CBT>& InRootBisectors, const TArray<int32>& InCBTBuffer)
{

    CPUHalfEdge_Buffer.Empty();
    CPUVertex_Buffer.Empty();
    CPURootBisectors_Buffer.Empty();
    CPUCBT_Buffer.Empty();
    CPUPointer_Buffer.Empty();

    D = InD;
    CPUHalfEdge_Buffer = InHalfEdges;
    CPUVertex_Buffer = InVertexBuffer;
    CPURootBisectors_Buffer = InRootBisectors;
    CPUCBT_Buffer = InCBTBuffer;
    CPUPointer_Buffer.Init(FPointer_CBT(-1, -1), 1 << D);

    // Schedule InitRHI on the render thread
    BeginInitResource(this);

    /*UE_LOG(LogTemp, Warning, TEXT("HalfEdges: Num=%d, Size=%d, TotalBytes=%d"),
        CPUHalfEdge_Buffer.Num(),
        (int32)sizeof(FHalfEdge_CBT),
        CPUHalfEdge_Buffer.Num() * (int32)sizeof(FHalfEdge_CBT));*/
}

void FCBTResource_Interface::InitRHI(FRHICommandListBase& RHICmdList)
{
    // UPLOAD DELAUNAY TRIANGLES BUFFERS
    if (CPU_FibonacciPoints_Buffer.Num() > 0)
    {
        FibonacciPoints_Buffer.Initialize(
            RHICmdList,
            TEXT("LargeCBT_FibonacciPoints_Buffer"),     // Debug name
            sizeof(FVector3_HighLow),                 // BytesPerElement
            (uint32)CPU_FibonacciPoints_Buffer.Num(),      // NumElements
            BUF_ShaderResource,                    // Usage flags
            /*bUseUAVCounter=*/false,
            /*bAppendBuffer=*/false,
            ERHIAccess::SRVMask                    // Initial state
        );
        UploadFibonacciPointsToGPU();
    }

    if (CPU_SphericalTriangles_Buffer.Num() > 0)
    {
        SphericalTriangles_Buffer.Initialize(
            RHICmdList,
            TEXT("LargeCBT_SphericalTriangles_Buffer"),     // Debug name
            sizeof(int32),                                  // BytesPerElement
            (uint32)CPU_SphericalTriangles_Buffer.Num(),    // NumElements
            BUF_ShaderResource,                             // Usage flags
            /*bUseUAVCounter=*/false,
            /*bAppendBuffer=*/false,
            ERHIAccess::SRVMask                    // Initial state
        );
        UploadSphericalTrianglesToGPU();
    }

    if (CPU_SphericalTriangles_HalfEdges_Buffer.Num() > 0)
    {
        SphericalTriangles_HalfEdges_Buffer.Initialize(
            RHICmdList,
            TEXT("LargeCBT_SphericalTriangles_HalfEdges_Buffer"),     // Debug name
            sizeof(int32),                                  // BytesPerElement
            (uint32)CPU_SphericalTriangles_HalfEdges_Buffer.Num(),    // NumElements
            BUF_ShaderResource,                             // Usage flags
            /*bUseUAVCounter=*/false,
            /*bAppendBuffer=*/false,
            ERHIAccess::SRVMask                    // Initial state
        );
        UploadSphericalTrianglesHalfEdgesBufferToGPU();
    }

	// UPLOAD VORONOI BUFFERS

	if (CPU_VoronoiGeoCenters_Buffer.Num() > 0)
	{
		VoronoiGeoCenters_Buffer.Initialize(
			RHICmdList,
			TEXT("LargeCBT_VoronoiGeoCenters_Buffer"),
			sizeof(FVector3_HighLow),
			(uint32)CPU_VoronoiGeoCenters_Buffer.Num(),
			BUF_ShaderResource,
			/*bUseUAVCounter=*/false,
			/*bAppendBuffer=*/false,
			ERHIAccess::SRVMask
		);

		UploadVoronoiGeoCentersToGPU();
	}

	if (CPU_VoronoiGeoMesh_Ranges_Buffer.Num() > 0)
	{
		const uint32 NumBytes = CPU_VoronoiGeoMesh_Ranges_Buffer.Num() * sizeof(FUintVector2);
		FRHIResourceCreateInfo CreateInfo(TEXT("LargeCBT_VoronoiGeoMesh_Ranges_Buffer"));

		VoronoiGeoMesh_Ranges_Buffer = RHICmdList.CreateStructuredBuffer(
			sizeof(FUintVector2),
			NumBytes,
			BUF_ShaderResource | BUF_Static,
			ERHIAccess::SRVMask,
			CreateInfo);

		VoronoiGeoMesh_Ranges_SRV = RHICmdList.CreateShaderResourceView(VoronoiGeoMesh_Ranges_Buffer);

		/*UE_LOG(LogTemp, Warning, TEXT("Create VoronoiGeoMeshRanges: BufferValid=%d SRVValid=%d Num=%d Bytes=%u"),
			VoronoiGeoMesh_Ranges_Buffer.IsValid() ? 1 : 0,
			VoronoiGeoMesh_Ranges_SRV.IsValid() ? 1 : 0,
			CPU_VoronoiGeoMesh_Ranges_Buffer.Num(),
			NumBytes);*/

		UploadVoronoiGeoMeshRangesToGPU();
	}

	if (CPU_VoronoiGeoMesh_Flat_Buffer.Num() > 0)
	{
		const uint32 NumBytes = CPU_VoronoiGeoMesh_Flat_Buffer.Num() * sizeof(int32);
		FRHIResourceCreateInfo CreateInfo(TEXT("LargeCBT_VoronoiGeoMesh_Flat_Buffer"));

		VoronoiGeoMesh_Flat_Buffer = RHICmdList.CreateStructuredBuffer(
			sizeof(int32),
			NumBytes,
			BUF_ShaderResource | BUF_Static,
			ERHIAccess::SRVMask,
			CreateInfo);

		VoronoiGeoMesh_Flat_SRV = RHICmdList.CreateShaderResourceView(VoronoiGeoMesh_Flat_Buffer);

		/*UE_LOG(LogTemp, Warning, TEXT("Create VoronoiGeoMeshFlat: BufferValid=%d SRVValid=%d Num=%d Bytes=%u"),
			VoronoiGeoMesh_Flat_Buffer.IsValid() ? 1 : 0,
			VoronoiGeoMesh_Flat_SRV.IsValid() ? 1 : 0,
			CPU_VoronoiGeoMesh_Flat_Buffer.Num(),
			NumBytes);*/

		UploadVoronoiGeoMeshFlatToGPU();
	}

    if (CPU_ElevationPerSite_Buffer.Num() > 0)
    {
        const uint32 NumBytes = CPU_ElevationPerSite_Buffer.Num() * sizeof(float);
        FRHIResourceCreateInfo CreateInfo(TEXT("LargeCBT_ElevationPerSiteBuffer"));

        ElevationPerSiteBuffer_RHI = RHICmdList.CreateVertexBuffer(
            NumBytes,
            BUF_ShaderResource | BUF_Static,
            ERHIAccess::SRVMask,
            CreateInfo);

        ElevationPerSiteBuffer_SRV = RHICmdList.CreateShaderResourceView(
            ElevationPerSiteBuffer_RHI,
            sizeof(float),
            PF_R32_FLOAT);

        UploadElevationPerSiteBufferToGPU();
    }

	if (CPU_Voronoi_Cells_Color_Buffer.Num() > 0)
	{
		const uint32 NumBytes = CPU_Voronoi_Cells_Color_Buffer.Num() * sizeof(uint32);
		FRHIResourceCreateInfo CreateInfo(TEXT("LargeCBT_VoronoiCellColors_Buffer"));

		VoronoiCellColors_Buffer = RHICmdList.CreateVertexBuffer(
			NumBytes,
			BUF_ShaderResource | BUF_Static,
			ERHIAccess::SRVMask,
			CreateInfo);

		VoronoiCellColors_SRV = RHICmdList.CreateShaderResourceView(
			VoronoiCellColors_Buffer,
			sizeof(uint32),
			PF_R32_UINT);

		/*UE_LOG(LogTemp, Warning, TEXT("Create VoronoiCellColors: BufferValid=%d SRVValid=%d Num=%d Bytes=%u"),
			VoronoiCellColors_Buffer.IsValid() ? 1 : 0,
			VoronoiCellColors_SRV.IsValid() ? 1 : 0,
			CPU_Voronoi_Cells_Color_Buffer.Num(),
			NumBytes);*/

		UploadVoronoiCellColorsToGPU();
	}
    
    // UPLOAD CBT BUFFERS
    if (CPUHalfEdge_Buffer.Num() > 0)
    {
        HalfEdges_Buffer.Initialize(
            RHICmdList,
            TEXT("LargeCBT_HalfEdges_Buffer"),     // Debug name
            sizeof(FHalfEdge_CBT),                 // BytesPerElement
            (uint32)CPUHalfEdge_Buffer.Num(),      // NumElements
            BUF_ShaderResource | BUF_UnorderedAccess,                    // Usage flags
            /*bUseUAVCounter=*/false,
            /*bAppendBuffer=*/false,
            ERHIAccess::SRVMask                    // Initial state
        );
        UploadHalfEdgesToGPU();
    }

    if (CPUVertex_Buffer.Num() > 0)
    {
        Vertex_Buffer.Initialize(
            RHICmdList,
            TEXT("LargeCBT_Vertex_Buffer"),     // Debug name
            sizeof(FVector3_HighLow),                 // BytesPerElement
            (uint32)CPUVertex_Buffer.Num(),      // NumElements
            BUF_ShaderResource | BUF_UnorderedAccess,                    // Usage flags
            /*bUseUAVCounter=*/false,
            /*bAppendBuffer=*/false,
            ERHIAccess::SRVMask                    // Initial state
        );
        UploadVertexBufferToGPU();
    }

    if (CPURootBisectors_Buffer.Num() > 0)
    {
        RootBisectors_Buffer.Initialize(
            RHICmdList,
            TEXT("LargeCBT_RootBisectors_Buffer"),
            sizeof(FRootBisector_CBT),
            (uint32)CPURootBisectors_Buffer.Num(),
            BUF_ShaderResource | BUF_UnorderedAccess,
            false,
            false,
            ERHIAccess::UAVMask
        );
        UploadRootBisectorsToGPU();
    }

    if (CPUCBT_Buffer.Num() > 0)
    {
        CBT_Buffer.Initialize(
            RHICmdList,
            TEXT("LargeCBT_CBT_Buffer"),
            sizeof(int32),
            (uint32)CPUCBT_Buffer.Num(),
            BUF_ShaderResource | BUF_UnorderedAccess,
            false,
            false,
            ERHIAccess::UAVMask
        );
        UploadCBTToGPU();
    }

    // Allocation Counter Buffer
    {
        AllocationCounter_Buffer.Initialize(
            RHICmdList,
            TEXT("LargeCBT_AllocationCounter_Buffer"),
            sizeof(int32),
            1,
            BUF_ShaderResource | BUF_UnorderedAccess,
            false,
            false,
            ERHIAccess::UAVMask
        );

        check(AllocationCounter_Buffer.UAV);

        // DOWNCAST � this is intentional and correct
        FRHICommandListImmediate& RHICmdListImmediate =
            static_cast<FRHICommandListImmediate&>(RHICmdList);

        RHICmdListImmediate.ClearUAVUint(
            AllocationCounter_Buffer.UAV,
            FUintVector4(0xFFFFFFFF, 0, 0, 0)
        );
        // UploadAllocationCounterToGPU();
    }

    if (CPUPointer_Buffer.Num() > 0)
    {
        Pointer_Buffer.Initialize(
            RHICmdList,
            TEXT("LargeCBT_Pointer_Buffer"),
            sizeof(FPointer_CBT),
            (uint32)CPUPointer_Buffer.Num(),
            BUF_ShaderResource | BUF_UnorderedAccess,
            false,
            false,
            ERHIAccess::UAVMask
        );
        UploadPointerBufferToGPU();
    }

	/*UE_LOG(LogTemp, Warning, TEXT("InitRHI Voronoi Counts: Centers=%d Ranges=%d Flat=%d Colors=%d"),
		CPU_VoronoiGeoCenters_Buffer.Num(),
		CPU_VoronoiGeoMesh_Ranges_Buffer.Num(),
		CPU_VoronoiGeoMesh_Flat_Buffer.Num(),
		CPU_Voronoi_Cells_Color_Buffer.Num());*/
}

void FCBTResource_Interface::ReleaseRHI()
{
    FibonacciPoints_Buffer.Release();
    SphericalTriangles_Buffer.Release();
    SphericalTriangles_HalfEdges_Buffer.Release();

    VoronoiCellColors_SRV.SafeRelease();
    VoronoiCellColors_Buffer.SafeRelease();

	VoronoiGeoMesh_Ranges_SRV.SafeRelease();
	VoronoiGeoMesh_Ranges_Buffer.SafeRelease();

	VoronoiGeoMesh_Flat_SRV.SafeRelease();
	VoronoiGeoMesh_Flat_Buffer.SafeRelease();

	ElevationPerSiteBuffer_SRV.SafeRelease();
	ElevationPerSiteBuffer_RHI.SafeRelease();

	HalfEdges_Buffer.Release();
	Vertex_Buffer.Release();
	RootBisectors_Buffer.Release();
	CBT_Buffer.Release();
	AllocationCounter_Buffer.Release();
	Pointer_Buffer.Release();
}

void FCBTResource_Interface::UploadFibonacciPointsToGPU()
{
    if (!FibonacciPoints_Buffer.Buffer || CPU_FibonacciPoints_Buffer.Num() == 0)
        return;

    const uint32 NumBytes = CPU_FibonacciPoints_Buffer.Num() * sizeof(FVector3_HighLow);

    void* Dest = RHILockBuffer(
        FibonacciPoints_Buffer.Buffer,  // FRHIBuffer*
        0,                              // Offset
        NumBytes,                       // Size
        RLM_WriteOnly                   // Lock mode
    );

    FMemory::Memcpy(Dest, CPU_FibonacciPoints_Buffer.GetData(), NumBytes);
    RHIUnlockBuffer(FibonacciPoints_Buffer.Buffer);

    /*UE_LOG(LogTemp, Warning, TEXT("Uploaded %d Fibonacci points (%.2f MB) to GPU"),
        CPU_FibonacciPoints_Buffer.Num(), NumBytes / (1024.0f * 1024.0f));*/
}

void FCBTResource_Interface::UploadSphericalTrianglesToGPU()
{
    if (!SphericalTriangles_Buffer.Buffer || CPU_SphericalTriangles_Buffer.Num() == 0)
        return;

    const uint32 NumBytes = CPU_SphericalTriangles_Buffer.Num() * sizeof(int32);

    void* Dest = RHILockBuffer(
        SphericalTriangles_Buffer.Buffer,  // FRHIBuffer*
        0,                                 // Offset
        NumBytes,                          // Size
        RLM_WriteOnly                      // Lock mode
    );

    FMemory::Memcpy(Dest, CPU_SphericalTriangles_Buffer.GetData(), NumBytes);
    RHIUnlockBuffer(SphericalTriangles_Buffer.Buffer);

    /*UE_LOG(LogTemp, Warning, TEXT("Uploaded %d spherical triangle indices (%.2f MB) to GPU"),
        CPU_SphericalTriangles_Buffer.Num(), NumBytes / (1024.0f * 1024.0f));*/
}

void FCBTResource_Interface::UploadSphericalTrianglesHalfEdgesBufferToGPU()
{
    if (!SphericalTriangles_HalfEdges_Buffer.Buffer || CPU_SphericalTriangles_HalfEdges_Buffer.Num() == 0)
        return;

    const uint32 NumBytes = CPU_SphericalTriangles_HalfEdges_Buffer.Num() * sizeof(int32);

    void* Dest = RHILockBuffer(
        SphericalTriangles_HalfEdges_Buffer.Buffer,  // FRHIBuffer*
        0,                                           // Offset
        NumBytes,                                    // Size
        RLM_WriteOnly                                // Lock mode
    );

    FMemory::Memcpy(Dest, CPU_SphericalTriangles_HalfEdges_Buffer.GetData(), NumBytes);
    RHIUnlockBuffer(SphericalTriangles_HalfEdges_Buffer.Buffer);

    /*UE_LOG(LogTemp, Warning, TEXT("Uploaded %d half-edge indices (%.2f MB) to GPU"),
        CPU_SphericalTriangles_HalfEdges_Buffer.Num(), NumBytes / (1024.0f * 1024.0f));*/
}

void FCBTResource_Interface::UploadVoronoiGeoCentersToGPU()
{
	/*UE_LOG(LogTemp, Warning, TEXT("UploadVoronoiGeoCentersToGPU: BufferValid=%d Num=%d"),
		VoronoiGeoCenters_Buffer.IsValid() ? 1 : 0,
		CPU_VoronoiGeoCenters_Buffer.Num());*/

	if (!VoronoiGeoCenters_Buffer.Buffer || CPU_VoronoiGeoCenters_Buffer.Num() == 0)
	{
		return;
	}

	const uint32 NumBytes = CPU_VoronoiGeoCenters_Buffer.Num() * sizeof(FVector3_HighLow);

	void* Dest = RHILockBuffer(
		VoronoiGeoCenters_Buffer.Buffer,
		0,
		NumBytes,
		RLM_WriteOnly);

	FMemory::Memcpy(Dest, CPU_VoronoiGeoCenters_Buffer.GetData(), NumBytes);
	RHIUnlockBuffer(VoronoiGeoCenters_Buffer.Buffer);
}

void FCBTResource_Interface::UploadVoronoiGeoMeshRangesToGPU()
{
	/*UE_LOG(LogTemp, Warning, TEXT("UploadVoronoiGeoMeshRangesToGPU: BufferValid=%d Num=%d"),
		VoronoiGeoMesh_Ranges_Buffer.IsValid() ? 1 : 0,
		CPU_VoronoiGeoMesh_Ranges_Buffer.Num());*/

	if (!VoronoiGeoMesh_Ranges_Buffer.IsValid() || CPU_VoronoiGeoMesh_Ranges_Buffer.Num() == 0)
	{
		return;
	}

	const uint32 NumBytes = CPU_VoronoiGeoMesh_Ranges_Buffer.Num() * sizeof(FUintVector2);

	void* Dest = RHILockBuffer(
		VoronoiGeoMesh_Ranges_Buffer,
		0,
		NumBytes,
		RLM_WriteOnly);

	FMemory::Memcpy(Dest, CPU_VoronoiGeoMesh_Ranges_Buffer.GetData(), NumBytes);
	RHIUnlockBuffer(VoronoiGeoMesh_Ranges_Buffer);

	/*UE_LOG(LogTemp, Warning, TEXT("Uploaded VoronoiGeoMeshRanges: Num=%d Bytes=%u"),
		CPU_VoronoiGeoMesh_Ranges_Buffer.Num(),
		NumBytes);*/
}

void FCBTResource_Interface::UploadVoronoiGeoMeshFlatToGPU()
{
	/*UE_LOG(LogTemp, Warning, TEXT("UploadVoronoiGeoMeshFlatToGPU: BufferValid=%d Num=%d"),
		VoronoiGeoMesh_Flat_Buffer.IsValid() ? 1 : 0,
		CPU_VoronoiGeoMesh_Flat_Buffer.Num());*/

	if (!VoronoiGeoMesh_Flat_Buffer.IsValid() || CPU_VoronoiGeoMesh_Flat_Buffer.Num() == 0)
	{
		return;
	}

	const uint32 NumBytes = CPU_VoronoiGeoMesh_Flat_Buffer.Num() * sizeof(int32);

	void* Dest = RHILockBuffer(
		VoronoiGeoMesh_Flat_Buffer,
		0,
		NumBytes,
		RLM_WriteOnly);

	FMemory::Memcpy(Dest, CPU_VoronoiGeoMesh_Flat_Buffer.GetData(), NumBytes);
	RHIUnlockBuffer(VoronoiGeoMesh_Flat_Buffer);

	/*UE_LOG(LogTemp, Warning, TEXT("Uploaded VoronoiGeoMeshFlat: Num=%d Bytes=%u"),
		CPU_VoronoiGeoMesh_Flat_Buffer.Num(),
		NumBytes);*/
}

void FCBTResource_Interface::UploadVoronoiCellColorsToGPU()
{
	/*UE_LOG(LogTemp, Warning, TEXT("UploadVoronoiCellColorsToGPU: BufferValid=%d Num=%d"),
		VoronoiCellColors_Buffer.IsValid() ? 1 : 0,
		CPU_Voronoi_Cells_Color_Buffer.Num());*/

	if (!VoronoiCellColors_Buffer.IsValid() || CPU_Voronoi_Cells_Color_Buffer.Num() == 0)
	{
		return;
	}

	const uint32 NumBytes = CPU_Voronoi_Cells_Color_Buffer.Num() * sizeof(uint32);

	void* Dest = RHILockBuffer(
		VoronoiCellColors_Buffer,
		0,
		NumBytes,
		RLM_WriteOnly);

	FMemory::Memcpy(Dest, CPU_Voronoi_Cells_Color_Buffer.GetData(), NumBytes);
	RHIUnlockBuffer(VoronoiCellColors_Buffer);

	/*UE_LOG(LogTemp, Warning, TEXT("Uploaded VoronoiCellColors: Num=%d Bytes=%u"),
		CPU_Voronoi_Cells_Color_Buffer.Num(),
		NumBytes);*/
}

void FCBTResource_Interface::UploadHalfEdgesToGPU()
{
    if (!HalfEdges_Buffer.Buffer || CPUHalfEdge_Buffer.Num() == 0)
        return;

    const uint32 NumBytes = CPUHalfEdge_Buffer.Num() * sizeof(FHalfEdge_CBT);

    void* Dest = RHILockBuffer(
        HalfEdges_Buffer.Buffer, // FRHIBuffer*
        0,                       // Offset
        NumBytes,                // Size
        RLM_WriteOnly            // Lock mode
    );

    FMemory::Memcpy(Dest, CPUHalfEdge_Buffer.GetData(), NumBytes);
    RHIUnlockBuffer(HalfEdges_Buffer.Buffer);
}

void FCBTResource_Interface::UploadVertexBufferToGPU()
{
    if (!Vertex_Buffer.Buffer || CPUVertex_Buffer.Num() == 0)
        return;

    const uint32 NumBytes = CPUVertex_Buffer.Num() * sizeof(FVector3_HighLow);

    void* Dest = RHILockBuffer(
        Vertex_Buffer.Buffer, // FRHIBuffer*
        0,                       // Offset
        NumBytes,                // Size
        RLM_WriteOnly            // Lock mode
    );

    FMemory::Memcpy(Dest, CPUVertex_Buffer.GetData(), NumBytes);
    RHIUnlockBuffer(Vertex_Buffer.Buffer);
}

void FCBTResource_Interface::UploadRootBisectorsToGPU()
{
    if (!RootBisectors_Buffer.Buffer || CPURootBisectors_Buffer.Num() == 0)
        return;

    const uint32 NumBytes = CPURootBisectors_Buffer.Num() * sizeof(FRootBisector_CBT);

    void* Dest = RHILockBuffer(
        RootBisectors_Buffer.Buffer, // FRHIBuffer*
        0,                       // Offset
        NumBytes,                // Size
        RLM_WriteOnly            // Lock mode
    );

    FMemory::Memcpy(Dest, CPURootBisectors_Buffer.GetData(), NumBytes);
    RHIUnlockBuffer(RootBisectors_Buffer.Buffer);
}

void FCBTResource_Interface::UploadCBTToGPU()
{
    if (!CBT_Buffer.Buffer || CPUCBT_Buffer.Num() == 0)
        return;

    const uint32 NumBytes = CPUCBT_Buffer.Num() * sizeof(int32);

    void* Dest = RHILockBuffer(
        CBT_Buffer.Buffer, // FRHIBuffer*
        0,                       // Offset
        NumBytes,                // Size
        RLM_WriteOnly            // Lock mode
    );

    FMemory::Memcpy(Dest, CPUCBT_Buffer.GetData(), NumBytes);
    RHIUnlockBuffer(CBT_Buffer.Buffer);
}

void FCBTResource_Interface::UploadAllocationCounterToGPU()
{
    const uint32 NumBytes = 1 * sizeof(int32);

    void* Dest = RHILockBuffer(
        AllocationCounter_Buffer.Buffer, // FRHIBuffer*
        0,                       // Offset
        NumBytes,                // Size
        RLM_WriteOnly            // Lock mode
    );

    FMemory::Memcpy(Dest, &CPUAllocationCounter, NumBytes);
    RHIUnlockBuffer(AllocationCounter_Buffer.Buffer);
}

void FCBTResource_Interface::UploadPointerBufferToGPU()
{
    if (!Pointer_Buffer.Buffer || CPUPointer_Buffer.Num() == 0)
        return;

    const uint32 NumBytes = CPUPointer_Buffer.Num() * sizeof(FPointer_CBT);

    void* Dest = RHILockBuffer(
        Pointer_Buffer.Buffer, // FRHIBuffer*
        0,                       // Offset
        NumBytes,                // Size
        RLM_WriteOnly            // Lock mode
    );

    FMemory::Memcpy(Dest, CPUPointer_Buffer.GetData(), NumBytes);
    RHIUnlockBuffer(Pointer_Buffer.Buffer);
}

void FCBTResource_Interface::UploadElevationPerSiteBufferToGPU()
{
	if (!ElevationPerSiteBuffer_RHI.IsValid() || CPU_ElevationPerSite_Buffer.Num() == 0)
	{
		return;
	}

	const uint32 NumBytes = CPU_ElevationPerSite_Buffer.Num() * sizeof(float);

	void* Dest = RHILockBuffer(
		ElevationPerSiteBuffer_RHI,
		0,
		NumBytes,
		RLM_WriteOnly);

	FMemory::Memcpy(Dest, CPU_ElevationPerSite_Buffer.GetData(), NumBytes);
	RHIUnlockBuffer(ElevationPerSiteBuffer_RHI);
}
