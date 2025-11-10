// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DelaunayPlanet.generated.h"

class UGeoDelaunatorComponent;

UCLASS()
class DELAUNATORPLUGIN_API ADelaunayPlanet : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ADelaunayPlanet();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY(BlueprintReadOnly)
	UGeoDelaunatorComponent* GeoDelaunator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fibonacci")
	int64 RandomSeed{ 2236 };
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fibonacci")
	FRandomStream RngStream;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fibonacci")
	int32 NumPoints{ 1000 };
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fibonacci")
	double Radius{ 5000.0 };
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fibonacci")
	double jitter{ 1000.0 };

	std::vector<double> coords;
	std::vector<FVector2D> sphere_coords;

	TArray<FVector3d> Sphere_Points;
	std::vector<FVector3d> SphereCircumCenters;
	std::vector<int32> SphereVLines;

	TArray<FVector3d> Plane_Points;
	std::vector<FVector3d> PlaneCircumCenters;
	std::vector<int32> PlaneVLines;
};
