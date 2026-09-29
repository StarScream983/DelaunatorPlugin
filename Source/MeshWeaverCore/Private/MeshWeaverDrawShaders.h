// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "ShaderParameterStruct.h"

class FMeshWeaverSceneProxy;
struct FRenderTargetBindingSlots;
class FSceneView;

void AddMeshWeaverDrawPass(
	FRDGBuilder& GraphBuilder,
	FSceneView& View,
	const FRenderTargetBindingSlots& RenderTargets,
	FMeshWeaverSceneProxy* Proxy);
