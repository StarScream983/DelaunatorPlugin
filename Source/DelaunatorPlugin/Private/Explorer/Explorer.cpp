// Fill out your copyright notice in the Description page of Project Settings.


#include "Explorer/Explorer.h"
#include "Engine/LocalPlayer.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/Controller.h"

// Sets default values
AExplorer::AExplorer()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	CapsuleComponent->InitCapsuleSize(5.0f, 5.0f);
	CapsuleComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	CapsuleComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	CapsuleComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Ignore);
	CapsuleComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);

	CapsuleComponent->CanCharacterStepUpOn = ECB_No;
	CapsuleComponent->SetShouldUpdatePhysicsVolume(true);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->bDynamicObstacle = false;
	RootComponent = CapsuleComponent;

	MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MovementComponent"));
	MovementComponent->UpdatedComponent = RootComponent;

	MovementComponent->MaxSpeed = 3000.f;
	MovementComponent->Acceleration = 8000.f;
	MovementComponent->Deceleration = 8000.f;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bInheritPitch = true;
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritRoll = true;

	SpringArm->bEnableCameraLag = false;
	SpringArm->bEnableCameraRotationLag = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
}

// Called when the game starts or when spawned
void AExplorer::BeginPlay()
{
	Super::BeginPlay();
	
	//Add Input Mapping Context
	if (Controller)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Cast<APlayerController>(Controller)->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

// Called every frame
void AExplorer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	/*DrawDebugCoordinateSystem(
		GetWorld(),
		SpringArm->GetComponentLocation(),
		SpringArm->GetComponentRotation(),
		200.f,
		false,
		0.f,
		0,
		2.f
	);*/
}

// Called to bind functionality to input
void AExplorer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	//Super::SetupPlayerInputComponent(PlayerInputComponent);
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AExplorer::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AExplorer::Look);
		EnhancedInputComponent->BindAction(RollAction, ETriggerEvent::Triggered, this, &AExplorer::Roll);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AExplorer::Move(const FInputActionValue& Value)
{
	FVector MovementVector = Value.Get<FVector>();
	//const AController* LocalController = GetController();
	if (Controller)
	{

		const FQuat ControlQuat = Controller->GetControlRotation().Quaternion();
		const FQuat ActorQuat = GetActorQuat();

		const FVector Forward = ActorQuat.GetForwardVector();
		const FVector Right = ActorQuat.GetRightVector();
		const FVector Up = ActorQuat.GetUpVector();

		AddMovementInput(Forward, MovementVector.X);
		AddMovementInput(Right, MovementVector.Y);
		AddMovementInput(Up, MovementVector.Z);
	}
}

void AExplorer::Look(const FInputActionValue& Value)
{
	//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("LOOK: %s"), *LookAxis.ToString()));
	
    const FVector2D Look = Value.Get<FVector2D>();
    if (Look.IsNearlyZero())
        return;

    constexpr float Sensitivity = 0.015f; // radians per pixel

    FQuat Current = GetActorQuat();

    const FVector Up    = Current.GetUpVector();
    const FVector Right = Current.GetRightVector();

    const FQuat Yaw   = FQuat(Up,    Look.X * Sensitivity);
    const FQuat Pitch = FQuat(Right, -Look.Y * Sensitivity);

    FQuat NewQuat = Yaw * Pitch * Current;
    NewQuat.Normalize();

    SetActorRotation(NewQuat);
	//Controller->SetControlRotation(NewQuat.Rotator());
}

void AExplorer::Roll(const FInputActionValue& Value)
{
	const float RollInput = Value.Get<float>();
	if (FMath::IsNearlyZero(RollInput))
		return;

	constexpr float RollSpeed = 1.5f; // radians/sec

	FQuat Current = GetActorQuat();
	const FVector Forward = Current.GetForwardVector();

	const FQuat RollQuat = FQuat(Forward, RollInput * RollSpeed * GetWorld()->GetDeltaSeconds());

	FQuat NewQuat = RollQuat * Current;
	NewQuat.Normalize();

	SetActorRotation(NewQuat);
}

FVector AExplorer::GetExplorerLocation() const
{
	return GetActorLocation();
}

