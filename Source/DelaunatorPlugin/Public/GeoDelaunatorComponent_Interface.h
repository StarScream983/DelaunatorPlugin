// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GeoDelaunatorComponent_Interface.generated.h"

/** UObject glue for `IGeoDelaunatorComponent_Interface`. */
UINTERFACE(MinimalAPI)
class UGeoDelaunatorComponent_Interface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Narrow surface exposed to rendering (scene proxy) so IndirectInstancingCore
 * does not depend on concrete `UGeoDelaunatorComponent` beyond CreateSceneProxy.
 */
class DELAUNATORPLUGIN_API IGeoDelaunatorComponent_Interface
{
	GENERATED_IINTERFACE_BODY()

public:
	/** Matches `EGeoVoronoiPlanetColorDebug`; read on render thread from `TAtomic` mirror (not RenderTarget). */
	virtual uint32 GetPlanetColorDebugShaderValue_RenderThread() const = 0;
};
