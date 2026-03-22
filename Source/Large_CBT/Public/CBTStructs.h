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

// ============================================================================
// DOUBLE-PRECISION COORDINATE PAIRS FOR GPU PROCESSING
// ============================================================================
// 
// These structures enable double-precision coordinates on GPU hardware that
// lacks native 64-bit float support. We split double values into high/low
// float pairs, allowing the GPU to reconstruct ~48-bit precision values.
//
// Usage Flow:
// 1. CPU: Convert FVector/double ? FVector3_HighLow (split into high/low)
// 2. GPU: Load high + low buffers separately
// 3. GPU: Reconstruct value = (double)High + (double)Low
// 4. GPU: Perform math in double precision (float math with extended mantissa)
// 5. GPU: For readback, split result back to high/low pairs
// 6. CPU: Readback high/low ? reconstruct to double precision FVector
//
// Performance: ~48-bit effective precision at planet scales (6000+ km radius)
// with negligible performance overhead (~1 extra addition per vertex).
// ============================================================================

/**
 * Single-axis double-precision coordinate stored as high/low float pair
 * 
 * Stores one component of a 3D coordinate in split form:
 * - High: Contains the "big" part of the number (aligned to 2^16 boundary)
 * - Low: Contains the "fractional" remainder
 * 
 * Reconstruction: (double)High + (double)Low = original double value
 * 
 * Example: Value 1234567.89
 *   High = 1200000.0 (rounded to 2^16 boundary)
 *   Low = 34567.89
 *   Result: 1200000.0 + 34567.89 = 1234567.89
 */
struct FHighLow_Pair
{
    float High;
    float Low;
    
    FHighLow_Pair() : High(0.0f), Low(0.0f) {}
    
    /** Create from double-precision value */
    explicit FHighLow_Pair(double Value)
    {
        const double Stride = 65536.0;  // 2^16 - optimal split point
        double HighDouble = floor(Value / Stride) * Stride;
        double LowDouble = Value - HighDouble;
        
        High = (float)HighDouble;
        Low = (float)LowDouble;
    }
    
    /** Reconstruct to double precision */
    double Reconstruct() const
    {
        return (double)High + (double)Low;
    }
};

/**
 * 3D coordinate in double precision stored as three high/low pairs
 * 
 * Represents a 3D point (X, Y, Z) where each axis is split into high/low floats.
 * Used for base triangle vertices and Voronoi circumcenters that need precision
 * at large scales (6000+ km planet radii).
 * 
 * GPU Buffer Layout:
 *   - Separate buffer for all High.X values
 *   - Separate buffer for all Low.X values
 *   - (And Y, Z similarly)
 * 
 * This allows GPU compute shaders to reconstruct: Final = High + Low
 * with full double-precision math while storing as float pairs.
 */
struct FVector3_HighLow
{
    FHighLow_Pair X;
    FHighLow_Pair Y;
    FHighLow_Pair Z;
    
    FVector3_HighLow() = default;
    
    /** Create from FVector (double precision) */
    explicit FVector3_HighLow(const FVector& Value)
        : X(Value.X), Y(Value.Y), Z(Value.Z)
    {
    }
    
    /** Reconstruct to full double-precision FVector */
    FVector Reconstruct() const
    {
        return FVector(X.Reconstruct(), Y.Reconstruct(), Z.Reconstruct());
    }
};


