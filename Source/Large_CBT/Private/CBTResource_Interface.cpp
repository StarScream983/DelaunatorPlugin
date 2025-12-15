// Fill out your copyright notice in the Description page of Project Settings.


#include "CBTResource_Interface.h"
#include "RenderCore.h"
#include "RHI.h"

void FCBTResource_Interface::PrimeTrianglesBuffers(const TArray<FVector>& InFibonacciPoints, const TArray<int32>& InSphericalTriangles, const TArray<int32>& InSphericalTrianglesHalfEdges)
{
    CPU_FibonacciPoints_Buffer.Empty();
    CPU_SphericalTriangles_Buffer.Empty();
    CPU_SphericalTriangles_HalfEdges_Buffer.Empty();

    CPU_FibonacciPoints_Buffer = InFibonacciPoints;
    CPU_SphericalTriangles_Buffer = InSphericalTriangles;
    CPU_SphericalTriangles_HalfEdges_Buffer = InSphericalTrianglesHalfEdges;
}

void FCBTResource_Interface::InitFromCPU(const int32 InD, const TArray<FHalfEdge_CBT>& InHalfEdges, const TArray<FVector>& InVertexBuffer, const TArray<FRootBisector_CBT>& InRootBisectors, const TArray<int32>& InCBTBuffer)
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
            sizeof(FVector),                 // BytesPerElement
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
            sizeof(FVector),                 // BytesPerElement
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

        // DOWNCAST — this is intentional and correct
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
}

void FCBTResource_Interface::ReleaseRHI()
{
    FibonacciPoints_Buffer.Release();
    SphericalTriangles_Buffer.Release();
    SphericalTriangles_HalfEdges_Buffer.Release();

	HalfEdges_Buffer.Release();
	Vertex_Buffer.Release();
	RootBisectors_Buffer.Release();
	CBT_Buffer.Release();
	AllocationCounter_Buffer.Release();
	Pointer_Buffer.Release();
}

void FCBTResource_Interface::UploadFibonacciPointsToGPU()
{
}

void FCBTResource_Interface::UploadSphericalTrianglesToGPU()
{
}

void FCBTResource_Interface::UploadSphericalTrianglesHalfEdgesBufferToGPU()
{
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

    const uint32 NumBytes = CPUVertex_Buffer.Num() * sizeof(FHalfEdge_CBT);

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

    const uint32 NumBytes = CPURootBisectors_Buffer.Num() * sizeof(FHalfEdge_CBT);

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
