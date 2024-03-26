// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Delaunator.h"
#include <functional>
#include <array>
#include "GeoDelaunatorComponent.generated.h"


#define _PI UE_DOUBLE_PI
#define _TWO_PI UE_DOUBLE_TWO_PI
#define DEGREES (180. / UE_DOUBLE_PI)
#define RADIANS (UE_DOUBLE_PI / 180.)

struct FCompose
{
public:
	static std::function<FVector2D(FVector2D)> Compose(
		std::function<FVector2D(FVector2D)> a,
		std::function<FVector2D(FVector2D)> b) {

		return[a, b](FVector2D coordinates) {
			FVector2D result = b(coordinates);
			return a(result);
			};
	}
};

struct FGeoStereographic
{
public:
};

struct FGeoRotation
{
public:
	// Pivot in degrees
	FGeoRotation(FVector2D pivot){
		Rotate = RotateRadians(pivot.X, pivot.Y);
	}

	std::function<FVector2D(FVector2D)> Rotate;

	// the the main function, will not be called
	FVector2D Forward(FVector2D coordinates)
	{
		// call Rotate with "coordinates"
		return FVector2D();
	}

	// the invert of the main function, this one will be called
	FVector2D Invert(FVector2D coordinates)
	{
		UE_LOG(LogTemp, Warning, TEXT("GEO INVERT !!"));
		coordinates = Rotate(coordinates);
		return coordinates * DEGREES;
	}

	std::function<FVector2D(FVector2D)> RotateRadians(double deltaLambda, double deltaPhi, double deltaGamma = 0.)
	{
		deltaLambda = FMath::Fmod(deltaLambda, _TWO_PI);
		if (deltaLambda != 0.)
		{
			if(deltaPhi != 0. || deltaGamma != 0.)
			{
				return FCompose::Compose(RotationLambda(-deltaLambda), RotationPhiGamma(deltaPhi, deltaGamma));
			}
			else
			{
				return RotationLambda(-deltaLambda);
			}
		}
		else
		{
			if (deltaPhi != 0. || deltaGamma != 0.)
			{
				return RotationLambda(-deltaLambda);
			}
			else
			{
				return RotationIdentity();
			}
		}
	}

	std::function<FVector2D(FVector2D)> RotationIdentity()
	{
		return [](FVector2D lambdaPhi) {
				double lambda = lambdaPhi.X;
				if (FMath::Abs(lambda) > _PI) lambda -= std::round(lambda / _TWO_PI) * _TWO_PI;
				return FVector2D(lambda, lambdaPhi.Y);
			};
	}

	std::function<FVector2D(FVector2D)> RotationLambda(double deltaLambda)
	{
		return [deltaLambda](FVector2D coordinates) {
			double lambda = coordinates.X;
			lambda += deltaLambda;
			if (FMath::Abs(lambda) > _PI) lambda -= std::round(lambda / _TWO_PI) * _TWO_PI;
				return FVector2D(lambda, coordinates.Y);
			};
	}

	std::function<FVector2D(FVector2D)> RotationPhiGamma(double deltaPhi, double deltaGamma = 0.)
	{
		return [deltaPhi, deltaGamma](FVector2D coordinates) {
			double cosDeltaPhi = FMath::Cos(deltaPhi),
				sinDeltaPhi = FMath::Sin(deltaPhi),
				cosDeltaGamma = FMath::Cos(deltaGamma),
				sinDeltaGamma = FMath::Sin(deltaGamma);

			double lambda = coordinates.X, phi = coordinates.Y;

			double cosPhi = FMath::Cos(phi),
				x = FMath::Cos(lambda) * cosPhi,
				y = FMath::Sin(lambda) * cosPhi,
				z = FMath::Sin(phi),
				k = z * cosDeltaGamma - y * sinDeltaGamma;
			return FVector2D(FMath::Atan2(y * cosDeltaGamma + z * sinDeltaGamma, x * cosDeltaPhi + k * sinDeltaPhi),
				FMath::Asin(k * cosDeltaPhi - x * sinDeltaPhi));
		};
	}
};


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

protected:

	UPROPERTY()
	UDelaunator* Delaunator = nullptr;

public:

	void GeoDelauny(std::vector<FVector2D> inPoints);
	UDelaunator* GeoDelaunayFrom(std::vector<FVector2D> inPoints);
};
