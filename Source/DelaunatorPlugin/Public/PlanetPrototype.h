// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "PlanetPrototype.generated.h"

class AExplorer;
class IPawnInterface;
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

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "PrototypePlanet")
	int32 GravityVolumeRadius{ 10000 };

	IPawnInterface* Explorer;
	
public:	
	// Sets default values for this actor's properties
	APlanetPrototype();

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

public:	

};
