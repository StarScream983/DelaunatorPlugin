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