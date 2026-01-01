// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
// Constants so we don't sprinkle magic numbers
enum EHalfedgeField : uint32
{
    HE_Twin = 0,
    HE_Next = 1,
    HE_Prev = 2,
    HE_Vert = 3,
    HE_Edge = 4,
    HE_Face = 5,
    HE_Stride = 6  // number of ints per halfedge
};

 // For half-edge buffer
struct FHalfEdge_CBT
{
    int32 Twin;
    int32 Next;
    int32 Prev;
    int32 Vert;
    int32 Edge;
    int32 Face;
};

// Pointer to an invalid neighbor or index
#define INVALID_POINTER 2147483647

// Possible culling state
#define BACK_FACE_CULLED -3
#define FRUSTUM_CULLED -2
#define TOO_SMALL -1
#define UNCHANGED_ELEMENT 0
#define BISECT_ELEMENT 1
#define SIMPLIFY_ELEMENT 2
#define MERGED_ELEMENT 3

// Root Bisector Buffer
struct FRootBisector_CBT
{
public:

    FRootBisector_CBT() = default;
    FRootBisector_CBT(int32 InBisectorID, int32 InTwin, int32 InNext, int32 InPrev, int32 InBisectorCommand, int32 InChild0, int32 InChild1, int32 InChild2, int32 InChild3)
        : BisectorID(InBisectorID), Twin(InTwin), Next(InNext), Prev(InPrev), BisectorCommand(InBisectorCommand), Child0(InChild0), Child1(InChild1), Child2(InChild2), Child3(InChild3)
    {
    };
    FRootBisector_CBT(int32 InBisectorID, int32 InTwin, int32 InNext, int32 InPrev)
        : BisectorID(InBisectorID), Twin(InTwin), Next(InNext), Prev(InPrev), BisectorCommand(UNCHANGED_ELEMENT), Child0(INVALID_POINTER), Child1(INVALID_POINTER), Child2(INVALID_POINTER), Child3(INVALID_POINTER)
    {
    };

    int32 BisectorID;
    int32 Twin;
    int32 Next;
	int32 Prev;
    int32 BisectorCommand; // Split / Merge / Unchanged, stored as bitfield or enum
    int32 Child0;
	int32 Child1;
	int32 Child2;
	int32 Child3;
};

// Pointer Buffer
struct FPointer_CBT
{
    int32 IndexBuffer; // index buffer of bisectors located in the memory pool, Algorithm 7
    int32 AvailabeBlock; // the index buffer of the available blocks in the memory pool, which we use for allocations. Algorithm 8

};

// Triangle Descriptor for Indirect instancing and Vertex Factory, guaranteed 16-byte alignment
struct FTriangleDescriptor
{
    FVector4 VertA;
    FVector4 VertB;
    FVector4 VertC;
};