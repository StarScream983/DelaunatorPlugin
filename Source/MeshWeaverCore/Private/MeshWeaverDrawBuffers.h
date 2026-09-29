// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHI.h"
#include "RHIResources.h"

/** GPU instance written by MeshWeaver CullInstancesCS. Matches QuadRenderInstance in MeshWeaverCommon.ush. */
struct FMeshWeaverRenderInstance
{
	uint32 GeoCenterA;
	uint32 GeoCenterB;
	uint32 SiteId;
	uint32 Padding;
};

struct FMeshWeaverDrawBuffers
{
	FBufferRHIRef InstanceBuffer;
	FUnorderedAccessViewRHIRef InstanceBufferUAV;
	FShaderResourceViewRHIRef InstanceBufferSRV;

	FBufferRHIRef IndirectArgsBuffer;
	FUnorderedAccessViewRHIRef IndirectArgsBufferUAV;
};
