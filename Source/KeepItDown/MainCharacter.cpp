// Fill out your copyright notice in the Description page of Project Settings.


#include "MainCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Item.h"
#include "Gun.h"
#include "Knife.h"
#include "Components/WidgetComponent.h"


// Sets default values
AMainCharacter::AMainCharacter() :
	// Base rates for turning/looking up
	BaseTurnRate(45.f),
	BaseLookUpRate(45.f),
	// Aiming/Not Aiming rates for turning/looking up
	HipTurnRate(45.f),
	HipLookUpRate(45.f),
	AimingTurnRate(15.f),
	AimingLookUpRate(15.f),
	// Mouse look sensitivity scale factors
	MouseHipTurnRate(1.0f),
	MouseHipLookUpRate(1.0f),
	MouseAimingTurnRate(0.2f),
	MouseAimingLookUpRate(0.2f),
	// True if Aiming
	bAiming(false),
	// Camera field of view values
	CameraDefaultFOV(100.f), // set in BeginPlay
	CameraZoomedFOV(35.f),
	CameraCurrentFOV(100.f),
	ZoomInterpSpeed(35.f),
	// Set Default CurrentWeaponType to Unarmed
	CurrentWeaponType(EWeaponType::EWT_Unarmed),
	// Item Trace variables
	bShouldTraceForItems(false),
	// CameraInterpLocation
	CameraInterpDistance(250.f),
	CameraInterpElevation(65.f),
	OverlappedItemCount(0)
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(34.f, 96.0f);

	// Create FirstPersonCamera
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCamera->bUsePawnControlRotation = true;

	// Don't rotate when the controller rotates. Let the controller only affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f); // ... at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

}

// Called when the game starts or when spawned
void AMainCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (FirstPersonCamera)
	{
		CameraDefaultFOV = GetFirstPersonCamera()->FieldOfView;
		CameraCurrentFOV = CameraDefaultFOV;
	}


}

// Called every frame
void AMainCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Handle interpolation when camera zoomed
	CameraInterpZoom(DeltaTime);

	// Change look sensitivity based on aiming
	SetLookRates();

	// Check OverlappedItemCount, then trace for items
	TraceForItems();
}

// Called to bind functionality to input
void AMainCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Aiming events
	PlayerInputComponent->BindAction("AimingButton", IE_Pressed, this, &AMainCharacter::AimingButtonPressed);
	PlayerInputComponent->BindAction("AimingButton", IE_Released, this, &AMainCharacter::AimingButtonReleased);

	// Equip knife and gun
	PlayerInputComponent->BindAction("Knife", IE_Pressed, this, &AMainCharacter::EquipKnife);
	PlayerInputComponent->BindAction("Gun", IE_Pressed, this, &AMainCharacter::EquipGun);

	// Interact with objects
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &AMainCharacter::Interact);

	// Run
	PlayerInputComponent->BindAction("Run", IE_Pressed, this, &AMainCharacter::StartRunning);
	PlayerInputComponent->BindAction("Run", IE_Released, this, &AMainCharacter::EndRunning);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &AMainCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AMainCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AMainCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AMainCharacter::LookUpAtRate);
}

void AMainCharacter::IncrementOverlappedItemCount(int8 Amount)
{
	if (OverlappedItemCount + Amount <= 0)
	{
		OverlappedItemCount = 0;
		bShouldTraceForItems = false;
	}
	else
	{
		OverlappedItemCount += Amount;
		bShouldTraceForItems = true;
	}
}

FVector AMainCharacter::GetCameraInterpLocation()
{
	const FVector CameraWorldLocation{ FirstPersonCamera->GetComponentLocation() };
	const FVector CameraForward{ FirstPersonCamera->GetForwardVector() };
	// Desired = CameraWorldLocation + Forward * A + Up * B
	return CameraWorldLocation + CameraForward * CameraInterpDistance
		+ FVector(0.f, 0.f, CameraInterpElevation);
}

void AMainCharacter::GetPickupItem(AItem* Item)
{
	auto Weapon = Cast<AItem>(Item);
	if (Weapon)
	{
		//SwapWeapon(Weapon);
		TraceHitItem = nullptr;
		TraceHitItemLastFrame = nullptr;

		UE_LOG(LogTemp, Warning, TEXT("Name of the Weapon class : %s"), *(Weapon->GetClass()->GetName()));

		if (Weapon->GetClass()->GetName().StartsWith(TEXT("Gun")))
		{
			// Downcasting can cause errors?
			MainGun = Cast<AGun>(Weapon); 

		}
		else if (Weapon->GetClass()->GetName().StartsWith(TEXT("Knife")))
		{
			MainKnife = Cast<AKnife>(Weapon);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Weird weapon class other than Gun and Knife"));
		}

		// Vanish WeaponToEquip
		Weapon->SetItemState(EItemState::EIS_Equipped);
	}
}

void AMainCharacter::TurnAtRate(float Rate)
{
	//  calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds()); // deg/sec * sec/frame
}

void AMainCharacter::LookUpAtRate(float Rate)
{
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds()); // deg/sec * sec/frame
}

void AMainCharacter::Turn(float Value)
{
	float TurnScaleFactor{};

	if (bAiming)
	{
		TurnScaleFactor = MouseAimingTurnRate;
	}
	else
	{
		TurnScaleFactor = MouseHipTurnRate;
	}

	AddControllerYawInput(Value * TurnScaleFactor); // Mouse X
}

void AMainCharacter::LookUp(float Value)
{
	float LookUpScaleFactor{};

	if (bAiming)
	{
		LookUpScaleFactor = MouseAimingLookUpRate;
	}
	else
	{
		LookUpScaleFactor = MouseHipLookUpRate;
	}

	AddControllerPitchInput(Value * LookUpScaleFactor); // Mouse Y
}

void AMainCharacter::StartRunning()
{
	if (CurrentWeaponType != EWeaponType::EWT_Gun)
	{
		GetCharacterMovement()->MaxWalkSpeed = 750.f;
	}
}

void AMainCharacter::EndRunning()
{
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
}


void AMainCharacter::AimingButtonPressed()
{
	bAiming = true;
}

void AMainCharacter::AimingButtonReleased()
{
	bAiming = false;
}

void AMainCharacter::CameraInterpZoom(float DeltaTime)
{
	// Set current camera field of view
	if (bAiming)
	{
		// Interpolate to zoomed FOV
		CameraCurrentFOV = FMath::FInterpTo(CameraCurrentFOV, CameraZoomedFOV, DeltaTime, ZoomInterpSpeed);
	}
	else
	{
		// Interpolate to default FOV
		CameraCurrentFOV = FMath::FInterpTo(CameraCurrentFOV, CameraDefaultFOV, DeltaTime, ZoomInterpSpeed);
	}

	GetFirstPersonCamera()->SetFieldOfView(CameraCurrentFOV);
}

void AMainCharacter::SetLookRates()
{
	if (bAiming)
	{
		BaseTurnRate = AimingTurnRate;
		BaseLookUpRate = AimingLookUpRate;
	}
	else
	{
		BaseTurnRate = HipTurnRate;
		BaseLookUpRate = HipLookUpRate;
	}
}

void AMainCharacter::CalculateCrosshairSpread(float DeltaTime)
{

}

void AMainCharacter::EquipKnife()
{
	if (CurrentWeaponType == EWeaponType::EWT_Knife)
	{
		CurrentWeaponType = EWeaponType::EWT_Unarmed;
	}
	else
	{
		CurrentWeaponType = EWeaponType::EWT_Knife;
	}

	if (MainKnife)
	{

	}
}

void AMainCharacter::EquipGun()
{
	if (CurrentWeaponType == EWeaponType::EWT_Gun)
	{
		CurrentWeaponType = EWeaponType::EWT_Unarmed;
	}
	else
	{
		CurrentWeaponType = EWeaponType::EWT_Gun;
	}
}

void AMainCharacter::Interact()
{
	UE_LOG(LogTemp, Warning, TEXT("TraceHitItem Called!"));

	if (TraceHitItem)
	{
		UE_LOG(LogTemp, Warning, TEXT("TraceHitItem Succeeded!"));
		TraceHitItem->StartItemCurve(this);

		//auto TraceHitWeapon = Cast<AWeapon>(TraceHitItem);
		//SwapWeapon(TraceHitWeapon);
	}
}

bool AMainCharacter::TraceUnderCrosshairs(FHitResult& OutHitResult, FVector& OutHitLocation)
{
	// Get Viewport Size
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	// Get screen space location of crosshairs
	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	//CrosshairLocation.Y -= 50.f;
	FVector CrosshairWorldPosition;
	FVector CrosshairWorldDirection;

	// Get world position and direction of the crosshair
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
		UGameplayStatics::GetPlayerController(this, 0),
		CrosshairLocation,
		CrosshairWorldPosition,
		CrosshairWorldDirection);

	if (bScreenToWorld)
	{
		// Trace from Crosshair world location outward
		const FVector Start{ CrosshairWorldPosition };
		const FVector End{ Start + CrosshairWorldDirection * 50'000.f };
		OutHitLocation = End;
		GetWorld()->LineTraceSingleByChannel(
			OutHitResult,
			Start,
			End,
			ECollisionChannel::ECC_Visibility);

		if (OutHitResult.bBlockingHit)
		{
			OutHitLocation = OutHitResult.Location;
			return true;
		}
	}

	return false;
}

void AMainCharacter::TraceForItems()
{
	if (bShouldTraceForItems)
	{
		FHitResult ItemTraceResult;
		FVector OutHitLocation;
		TraceUnderCrosshairs(ItemTraceResult, OutHitLocation);
		if (ItemTraceResult.bBlockingHit)
		{
			TraceHitItem = Cast<AItem>(ItemTraceResult.Actor);
			if (TraceHitItem && TraceHitItem->GetPickupWidget())
			{
				// Show Item's Pickup Widget
				TraceHitItem->GetPickupWidget()->SetVisibility(true);
			}

			// We hit an AItem last frame
			if (TraceHitItemLastFrame)
			{
				if (TraceHitItem != TraceHitItemLastFrame)
				{
					// We are hitting a different AItem this frame last frame
					// Or AItem is null
					TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);
				}
			}

			// Store a reference to HitItem for next frame
			TraceHitItemLastFrame = TraceHitItem;
		}
	}
	else if (TraceHitItemLastFrame)
	{
		// No longer overlapping any items,
		// Item last frame should not show widget
		TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);
	}
}

void AMainCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		const FRotator Rotation{ Controller->GetControlRotation() };
		const FRotator YawRotation{ 0, Rotation.Yaw, 0 };

		const FVector Direction{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::X) };
		AddMovementInput(Direction, Value);
	}
}

void AMainCharacter::MoveRight(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		const FRotator Rotation{ Controller->GetControlRotation() };
		const FRotator YawRotation{ 0, Rotation.Yaw, 0 };

		const FVector Direction{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::Y) };
		AddMovementInput(Direction, Value);
	}
}