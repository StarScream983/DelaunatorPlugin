// Fill out your copyright notice in the Description page of Project Settings.


#include "GeoDelaunatorComponent.h"
#include <string>
#include <iostream>

// Sets default values for this component's properties
UGeoDelaunatorComponent::UGeoDelaunatorComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}


// Called when the game starts
void UGeoDelaunatorComponent::BeginPlay()
{
	Super::BeginPlay();

	//TEST
	std::vector<FVector2D> Points = {FVector2D(45., 60.), FVector2D(21., 91.)};
	GeoDelaunayFrom(Points);
}


// Called every frame
void UGeoDelaunatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UGeoDelaunatorComponent::GeoDelauny(std::vector<FVector2D> inPoints)
{
	Delaunator = GeoDelaunayFrom(inPoints);
}

UDelaunator* UGeoDelaunatorComponent::GeoDelaunayFrom(std::vector<FVector2D> inPoints)
{
	if (inPoints.size() < 2) return nullptr;

	// find a valid point to send to infinity
	int32 pivot = 0;

	// we have only Longitude and Latitude, so [lambda, phi, gamma] parameters will receive gamma = 0
	FGeoRotation r = FGeoRotation(inPoints[pivot]);
	FVector2D result = r.Invert(FVector2D(180., 0.));

	//TEST
	UE_LOG(LogTemp, Warning, TEXT("Result: %s"), *result.ToString());

	return nullptr;
}

