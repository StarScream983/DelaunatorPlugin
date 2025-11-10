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
	std::vector<FVector2D> Points = {FVector2D(25., 90.), FVector2D(21., 91.)};
	GeoDelaunayFrom(Points);
}

void UGeoDelaunatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
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
	// while (std::isnan(inPoints[pivot][0] + inPoints[pivot][1]) && pivot++ < inPoints.size()); works in JS only
	while (pivot < (int)inPoints.size() &&
		std::isnan(inPoints[pivot][0] + inPoints[pivot][1]))
	{
		++pivot;
	}

	//// we have only Longitude and Latitude, so [lambda, phi, gamma] parameters will receive gamma = 0
	FGeoRotation r = FGeoRotation(inPoints[pivot]);
	FVector2D result = r.Invert(FVector2D(180., 0.));

	FGeoStereographic StereoGraphic;
	StereoGraphic = StereoGraphic.Translate(FVector2D(0.)).Scale(1.).Rotate(result);
	std::function<FVector2D(FVector2D)> projection = StereoGraphic._projection();
	std::vector<FVector2D> outPoints(inPoints.size());
	std::transform(inPoints.begin(), inPoints.end(), outPoints.begin(), projection);

	//std::vector<int32> zeros;
	//double max2 = 1.0;

	//for (int i = 0, n = outPoints.size(); i < n; ++i) {
	//	double m = outPoints[i][0] * outPoints[i][0] + outPoints[i][1] * outPoints[i][1];
	//	if (!std::isfinite(m) || m > 1e32) {
	//		zeros.push_back(i);
	//	}
	//	else if (m > max2) {
	//		max2 = m;
	//	}
	//}

	//const double FAR = 1e6 * std::sqrt(max2);

	//for (int i : zeros) {
	//	outPoints[i] = FVector2D(FAR, 0.0);
	//}
	//// Add infinite horizon points
	//outPoints.push_back(FVector2D(0, FAR));
	//outPoints.push_back(FVector2D(-FAR, 0));
	//outPoints.push_back(FVector2D(0, -FAR));

	//int32 npoints = outPoints.size();
	//for (int32 i = 0; i < npoints; i++) {
	//	coords.push_back(outPoints[i].X);
	//	coords.push_back(outPoints[i].Y);
	//}

	//UDelaunator* Del = NewObject<UDelaunator>(this);
	//if(Del) Del->InitDelaunator(coords);

	/*std::function<FVector2D(FVector2D)> projection = [](FVector2D point) {
		return point + FVector2D(1., 2.);
	};
	std::vector<FVector2D> outPoints(inPoints.size());
	std::transform(inPoints.begin(), inPoints.end(), inPoints.begin(), projection);*/
	//TEST
	//UE_LOG(LogTemp, Warning, TEXT("Result: %s"), *result.ToString());

	//*******************************************************************
	//TEST for lambda function capture of inner parameters with [=, this]
	/*MyStruct TestStruct;
	TestStruct.a = 2;
	TestStruct.b = 5.6;
	auto func = TestStruct.getFunction();
	func();*/
	//*******************************************************************

	return nullptr;
}