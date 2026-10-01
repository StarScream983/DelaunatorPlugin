// Fill out your copyright notice in the Description page of Project Settings.


#include "PlanetPrototype.h"
#include "GeoDelaunatorComponent.h"
#include "Explorer/PawnInterface.h"
#include "UObject/UnrealType.h"

// Sets default values
APlanetPrototype::APlanetPrototype()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bGenerateOverlapEventsDuringLevelStreaming = true;

	//RootComponent = CreateDefaultSubobject<USceneComponent>(FName("RootComponent"));
	RootComponent = VoronoiPlanet = CreateDefaultSubobject<UGeoDelaunatorComponent>(FName("VoronoiPlanet"));

	GravityVolume = CreateDefaultSubobject<USphereComponent>("GravityVolume");
	GravityVolume->InitSphereRadius((float)(PlanetRadius * 2.0));
	GravityVolume->SetupAttachment(RootComponent);
	GravityVolume->SetHiddenInGame(false);
	GravityVolume->SetComponentTickEnabled(false);
	GravityVolume->CanCharacterStepUpOn = ECB_No;
	GravityVolume->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	GravityVolume->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	GravityVolume->SetCollisionResponseToChannel(ECollisionChannel::ECC_PhysicsBody, ECollisionResponse::ECR_Overlap);
	GravityVolume->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Overlap);
	GravityVolume->SetCollisionResponseToChannel(ECollisionChannel::ECC_Vehicle, ECollisionResponse::ECR_Overlap);
	GravityVolume->OnComponentBeginOverlap.AddDynamic(this, &APlanetPrototype::OnOverlapBegin);
	GravityVolume->OnComponentEndOverlap.AddDynamic(this, &APlanetPrototype::OnOverlapEnd);
}

// Called when the game starts or when spawned
void APlanetPrototype::BeginPlay()
{
	Super::BeginPlay();
	ApplyPlanetRadius();
}

void APlanetPrototype::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPlanetRadius();
}

void APlanetPrototype::ApplyPlanetRadius()
{
	if (VoronoiPlanet)
	{
		VoronoiPlanet->SetPlanetRadius(PlanetRadius);
	}
	if (GravityVolume)
	{
		GravityVolume->SetSphereRadius((float)(PlanetRadius * 2.0), false);
	}
}

#if WITH_EDITOR
void APlanetPrototype::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(APlanetPrototype, PlanetRadius))
	{
		ApplyPlanetRadius();
	}
}
#endif

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
		IPawnInterface* OverlapPawn = Cast<IPawnInterface>(OtherActor);
		if (OverlapPawn && VoronoiPlanet)
		{
			VoronoiPlanet->RegisterPawn(OverlapPawn);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Pawn entered Gravity Volume"));
			}
		}
	}
}

void APlanetPrototype::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if ((OtherActor != nullptr) && (OtherActor != this) && (OtherComp != nullptr) && OtherActor->GetClass()->ImplementsInterface(UPawnInterface::StaticClass()))
	{
		IPawnInterface* OverlapPawn = Cast<IPawnInterface>(OtherActor);
		if (VoronoiPlanet)
		{
			VoronoiPlanet->UnregisterPawn(OverlapPawn);
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Pawn exited Gravity Volume"));
		}
	}
}
