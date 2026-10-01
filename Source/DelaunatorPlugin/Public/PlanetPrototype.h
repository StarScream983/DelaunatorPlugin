// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "PlanetPrototype.generated.h"

class UGeoDelaunatorComponent;

UCLASS()
class DELAUNATORPLUGIN_API APlanetPrototype : public AActor
{
	GENERATED_BODY()

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"), Category = "PrototypePlanet")
	UGeoDelaunatorComponent* VoronoiPlanet;

	// Gravity Volume to detect the character
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "PrototypePlanet")
	USphereComponent* GravityVolume;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "100.0", UIMin = "100.0"), Category = "Rendering")
	double PlanetRadius = 3000.0;
	
public:	
	// Sets default values for this actor's properties
	APlanetPrototype();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void ApplyPlanetRadius();

public:	

};
