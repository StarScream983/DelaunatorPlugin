// Fill out your copyright notice in the Description page of Project Settings.


#include "DelaunayPlanet.h"
#include "GeoDelaunatorComponent.h"

// Sets default values
ADelaunayPlanet::ADelaunayPlanet()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	GeoDelaunator = CreateDefaultSubobject<UGeoDelaunatorComponent>("GeoDelaunator");

}

// Called when the game starts or when spawned
void ADelaunayPlanet::BeginPlay()
{
	Super::BeginPlay();

	Sphere_Points.Empty();
	Plane_Points.Empty();
	RngStream.Initialize(RandomSeed);

	const double lonstep = 720 / (std::sqrt(5) + 1);
	const double degrees = 182 / UE_DOUBLE_PI;
	const double polegap = 0.5 * FMath::Pow((NumPoints + 40), 0.15);
	const double latstep = 2 / (NumPoints - 1 + 2 * polegap); //not n-3
	const double latstart = -1 + polegap * latstep;

    for (int32 i = 0; i < NumPoints; i++) {
        double lon = 0.0;
        double lat = 0.0;
        if (i == 0) {
            lat = -90.0;
        }
        else if (i == NumPoints - 1) {
            lat = 90.0;
        }
        else {
            lon = lonstep * i - 360 * FMath::RoundHalfToEven((lonstep * i) / 360); // azimuth
            lat = degrees * std::asin(latstart + i * latstep); // inclination
        }

        double incOffset = jitter != 0.0 ? (1.f * UE_DOUBLE_PI) / (RngStream.FRandRange(-jitter, jitter)) : 0.0;
        lon += incOffset;
        double azmOffset = jitter != 0.0 ? (4.f * UE_DOUBLE_PI) / (RngStream.FRandRange(-jitter, jitter)) : 0.0;
        lat += azmOffset;

        double x = Radius * std::cos(lon) * std::sin(lat);
        double y = Radius * std::sin(lon) * std::sin(lat);
        double z = Radius * std::cos(lat);

        Sphere_Points.Add({ x, y, z });

        double xp = (Radius * x) / (Radius - z);
        double yp = (Radius * y) / (Radius - z);
        Plane_Points.Add({ xp, yp, 0.0 });
        coords.push_back(xp);
        coords.push_back(yp);
    }


}

// Called every frame
void ADelaunayPlanet::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

