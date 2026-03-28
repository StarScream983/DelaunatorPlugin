// Fill out your copyright notice in the Description page of Project Settings.


#include "PlanetPrototype.h"
#include "GeoDelaunatorComponent.h"
#include "Explorer/Explorer.h"
#include "Explorer/PawnInterface.h"

// Sets default values
APlanetPrototype::APlanetPrototype()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bGenerateOverlapEventsDuringLevelStreaming = true;

	//RootComponent = CreateDefaultSubobject<USceneComponent>(FName("RootComponent"));
	RootComponent = VoronoiPlanet = CreateDefaultSubobject<UGeoDelaunatorComponent>(FName("VoronoiPlanet"));
	
	float radius = (float)GravityVolumeRadius;
	if(VoronoiPlanet)
	{
		radius = VoronoiPlanet->GetPlanetRadius() * 2.f;
	}

	GravityVolume = CreateDefaultSubobject<USphereComponent>("GravityVolume");
	GravityVolume->InitSphereRadius(6000.f);
	GravityVolume->SetupAttachment(RootComponent);
	GravityVolume->SetHiddenInGame(false);
	GravityVolume->SetComponentTickEnabled(false);
	GravityVolume->CanCharacterStepUpOn = ECB_No;
	GravityVolume->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	GravityVolume->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	GravityVolume->OnComponentBeginOverlap.AddDynamic(this, &APlanetPrototype::OnOverlapBegin);
	GravityVolume->OnComponentEndOverlap.AddDynamic(this, &APlanetPrototype::OnOverlapEnd);
}

// Called when the game starts or when spawned
void APlanetPrototype::BeginPlay()
{
	Super::BeginPlay();
	
}

void APlanetPrototype::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

// Called every frame
void APlanetPrototype::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void APlanetPrototype::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if ((OtherActor != nullptr) && (OtherActor != this) && (OtherComp != nullptr) && OtherActor->GetClass()->ImplementsInterface(UPawnInterface::StaticClass()))
	{
		Explorer = Cast<IPawnInterface>(OtherActor);
		if (Explorer && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Explorer entered Gravity Volume"));
		}
	}
}

void APlanetPrototype::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if ((OtherActor != nullptr) && (OtherActor != this) && (OtherComp != nullptr) && OtherActor->GetClass()->ImplementsInterface(UPawnInterface::StaticClass()))
	{
		Explorer = nullptr;
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Explorer exited Gravity Volume"));
		}
	}
}

