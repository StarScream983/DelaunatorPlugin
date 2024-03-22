// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Delaunator.h"
#include <functional>
#include <array>
#include "GeoDelaunatorComponent.generated.h"


#define UE_DOUBLE_DEGREES (180. / UE_DOUBLE_PI)
#define UE_DOUBLE_RADIANS (UE_DOUBLE_PI / 180.)


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DELAUNATORPLUGIN_API UGeoDelaunatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UGeoDelaunatorComponent();

protected:

	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:	

protected:

	UPROPERTY()
	UDelaunator* Delaunator = nullptr;
};
