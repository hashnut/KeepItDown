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
#include "Engine/SkeletalMeshSocket.h"
#include "Sound/SoundCue.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Particles/ParticleSystemComponent.h"


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
	OverlappedItemCount(0),
	// Crosshair spread factors
	CrosshairSpreadMultiplier(0.f),
	CrosshairVelocityFactor(0.f),
	CrosshairInAirFactor(0.f),
	CrosshairAimFactor(0.f),
	CrosshairShootingFactor(0.f),
	// Bullet fire timer variables
	ShootTimeDuration(0.05f),
	bFiringBullet(false),
	// Automatic fire variables
	AutomaticFireRate(0.8f),
	bShouldFire(true),
	bAttackButtonPressed(false),
	StartingPistolAmmo(13),
	StartingARAmmo(130),
	// Combat variables
	CombatState(ECombatState::ECS_Unoccupied),
	// Stamina Variables
	bIsRunning(false),
	bIsHoldingBreath(false),
	bShouldHoldBreathAgain(false),
	// Set default stamina to MAX
	MaxStamina(10.f),
	MinStamina(0.f),
	CurrentStamina(10.f)
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

	// Make Mesh1P children of FirstPersonCamera
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
	Mesh1P->SetupAttachment(FirstPersonCamera);
	//static ConstructorHelpers::FObjectFinder<USkeletalMesh> SK_ARMS(TEXT("/Game/FirstPerson/Character/Mesh/SK_Mannequin_Arms.SK_Mannequin_Arms"));
	//if (SK_ARMS.Succeeded())
	//{
	//	Mesh1P->SetSkeletalMesh(SK_ARMS.Object);
	//}
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

	InitializeAmmoMap();
}

void AMainCharacter::Jump()
{
	if (CurrentWeaponType == EWeaponType::EWT_HoldBreath) return;

	Super::Jump();
}

void AMainCharacter::StopJumping()
{
	if (CurrentWeaponType == EWeaponType::EWT_HoldBreath) return;

	Super::StopJumping();
}

// Called every frame
void AMainCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Handle interpolation when camera zoomed
	CameraInterpZoom(DeltaTime);

	// Change look sensitivity based on aiming
	SetLookRates();

	// Calculate crosshair spread multiplier
	CalculateCrosshairSpread(DeltaTime);

	// Check OverlappedItemCount, then trace for items
	TraceForItems();

	// Check manage stamina (HoldBreath / Running)
	ManageStamina(DeltaTime);

}

// Called to bind functionality to input
void AMainCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AMainCharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AMainCharacter::StopJumping);

	// Aiming events
	PlayerInputComponent->BindAction("AimingButton", IE_Pressed, this, &AMainCharacter::AimingButtonPressed);
	PlayerInputComponent->BindAction("AimingButton", IE_Released, this, &AMainCharacter::AimingButtonReleased);

	// Equip knife and gun
	PlayerInputComponent->BindAction("Knife", IE_Pressed, this, &AMainCharacter::EquipKnife);
	PlayerInputComponent->BindAction("Gun", IE_Pressed, this, &AMainCharacter::EquipGun);

	// Interact with objects
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &AMainCharacter::Interact);

	//PlayerInputComponent->BindAction("Attack", IE_Pressed, this, &AMainCharacter::Attack);
	PlayerInputComponent->BindAction("Attack", IE_Pressed, this, &AMainCharacter::AttackButtonPressed);
	PlayerInputComponent->BindAction("Attack", IE_Released, this, &AMainCharacter::AttackButtonReleased);

	// Reload Weapon
	PlayerInputComponent->BindAction("ReloadButton", IE_Pressed, this, &AMainCharacter::ReloadButtonPressed);

	// Run
	PlayerInputComponent->BindAction("Run", IE_Pressed, this, &AMainCharacter::StartRunning);
	PlayerInputComponent->BindAction("Run", IE_Released, this, &AMainCharacter::EndRunning);

	// HoldBreath
	PlayerInputComponent->BindAction("HoldBreath", IE_Pressed, this, &AMainCharacter::StartHoldBreath);
	PlayerInputComponent->BindAction("HoldBreath", IE_Released, this, &AMainCharacter::FinishHoldBreath);

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

void AMainCharacter::FinishReloading()
{
	// Update the Combat State
	CombatState = ECombatState::ECS_Unoccupied;

	if (MainGun == nullptr) return;

	const auto AmmoType{ MainGun->GetAmmoType() };

	// Update the AmmoMap
	if (AmmoMap.Contains(AmmoType))
	{
		// Amount of ammo the Character is carrying of the EquippedWeapon type
		int32 CarriedAmmo = AmmoMap[AmmoType];

		// Space left in the magazine of EquippedWeapon
		const int32 MagEmptySpace = MainGun->GetMagazineCapacity() - MainGun->GetAmmo();

		if (MagEmptySpace > CarriedAmmo)
		{
			// Reload the magazine with all the ammo we are caryying
			MainGun->ReloadAmmo(CarriedAmmo);
			CarriedAmmo = 0;
			AmmoMap.Add(AmmoType, CarriedAmmo);
		}
		else
		{
			// fill the magazine
			MainGun->ReloadAmmo(MagEmptySpace);
			CarriedAmmo -= MagEmptySpace;
			AmmoMap.Add(AmmoType, CarriedAmmo);
		}
	}
}

void AMainCharacter::FinishSlashing()
{
	// Update the Combat State
	CombatState = ECombatState::ECS_Unoccupied;

	if (MainKnife == nullptr) return;
}

void AMainCharacter::ManageStamina(float DeltaTime)
{
	if (/*!bIsRunning && */!bIsHoldingBreath)
	{
		//UE_LOG(LogTemp, Warning, TEXT("Increase Stamina"));
		IncrementStamina(DeltaTime);
	}
	else
	{
		//UE_LOG(LogTemp, Warning, TEXT("Decrease Stamina"));
		if (CurrentStamina <= MinStamina && CombatState == ECombatState::ECS_HoldBreath)
		{
			UE_LOG(LogTemp, Warning, TEXT("StartHoldBreath -> Stamina all used up"));

			CombatState = ECombatState::ECS_Unoccupied;
			CurrentWeaponType = EWeaponType::EWT_Unarmed;

			bIsHoldingBreath = false;
			bShouldHoldBreathAgain = false;

			PlayEndHoldBreathSound();
		}

		DecrementStamina(DeltaTime);
	}
}

void AMainCharacter::IncrementStamina(float DeltaTime)
{
	CurrentStamina = FMath::Clamp<float>(CurrentStamina + DeltaTime, MinStamina, MaxStamina);
	//UE_LOG(LogTemp, Warning, TEXT("DeltaTime : %f"), DeltaTime);
	//UE_LOG(LogTemp, Warning, TEXT("CurrentStamina : %f"), CurrentStamina);
}

void AMainCharacter::DecrementStamina(float DeltaTime)
{
	CurrentStamina = FMath::Clamp<float>(CurrentStamina - DeltaTime, MinStamina, MaxStamina);

	//UE_LOG(LogTemp, Warning, TEXT("DeltaTime : %f"), DeltaTime);
	//UE_LOG(LogTemp, Warning, TEXT("CurrentStamina : %f"), CurrentStamina);
}

float AMainCharacter::GetCrosshairSpreadMultipllier() const
{
	return CrosshairSpreadMultiplier;
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
	/*auto Weapon = Cast<AItem>(Item);*/
	if (Item)
	{
		//SwapWeapon(Weapon);
		TraceHitItem = nullptr;
		TraceHitItemLastFrame = nullptr;

		UE_LOG(LogTemp, Warning, TEXT("Name of the Weapon class : %s"), *(Item->GetClass()->GetName()));
		
		// if (Weapon->GetClass()->GetName().StartsWith(TEXT("Gun")))
		if (Item->GetItemCategory() == EItemCategory::EIC_Gun)
		{
			// Downcasting can cause errors?
			//MainGun = Cast<AGun>(Weapon);
			MainGun = Cast<AGun>(Item);
		}
		else if (Item->GetItemCategory() == EItemCategory::EIC_Knife)
		{
			//MainKnife = Cast<AKnife>(Weapon);
			MainKnife = Cast<AKnife>(Item);
		}
		else if (Item->GetItemCategory() == EItemCategory::EIC_Ammo)
		{
			// TODO : Add Ammo
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Weird weapon class other than Gun, Knife and Ammo"));
		}

		// Vanish WeaponToEquip
		Item->SetItemState(EItemState::EIS_Obtained);
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
	// Cannot run while holding breath
	if (CombatState == ECombatState::ECS_HoldBreath) return;

	// Cannot run when using pistol
	if (CurrentWeaponType != EWeaponType::EWT_Gun)
	{
		bIsRunning = true;
		GetCharacterMovement()->MaxWalkSpeed = 750.f;
	}
}

void AMainCharacter::EndRunning()
{
	bIsRunning = false;
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

bool AMainCharacter::GetBeamEndLocation(const FVector& MuzzleSocketLocation, FVector& OutBeamLocation)
{
	FHitResult CrosshairHitResult;
	bool bCrosshairHit = TraceUnderCrosshairs(CrosshairHitResult, OutBeamLocation);

	if (bCrosshairHit)
	{
		// Tentative beam location - still need to trace from gun
		OutBeamLocation = CrosshairHitResult.Location;
	}
	else // no crosshair trace hit
	{
		// OutBeamLocation is the End location for the line trace
	}

	// Perform a second trace, this time from the gun barrel
	FHitResult WeaponTraceHit;
	const FVector WeaponTraceStart{ MuzzleSocketLocation };
	const FVector StartToEnd{ OutBeamLocation - MuzzleSocketLocation };
	const FVector WeaponTraceEnd{ MuzzleSocketLocation + StartToEnd * 1.25f };
	GetWorld()->LineTraceSingleByChannel(
		WeaponTraceHit,
		WeaponTraceStart,
		WeaponTraceEnd,
		ECollisionChannel::ECC_Visibility);
	if (WeaponTraceHit.bBlockingHit) // Object between barrel and BeamEndPoint?
	{
		OutBeamLocation = WeaponTraceHit.Location;
		return true;
	}
	return false;
}

void AMainCharacter::CalculateCrosshairSpread(float DeltaTime)
{
	FVector2D WalkSpeedRange{ 0.f, 750.f };
	FVector2D VelocityMultiplierRange{ 0.f, 1.f };
	FVector Velocity{ GetVelocity() };
	Velocity.Z = 0.f;

	// Calculate crosshair velocity factor
	CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(
		WalkSpeedRange,
		VelocityMultiplierRange,
		Velocity.Size());

	// Calculate crosshair in air factor
	if (GetCharacterMovement()->IsFalling()) // is in air?
	{
		// Spread the crosshairs slowly while in air
		CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f);
	}
	else // Character is on the ground
	{
		// Shrink the crosshairs rapidly while on the ground
		CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 30.f);
	}

	// Calculate crosshair aim factor
	if (bAiming) // Are we aiming?
	{
		// Shrink crosshairs a small amount very quickly
		CrosshairAimFactor = FMath::FInterpTo(
			CrosshairAimFactor,
			0.6f,
			DeltaTime,
			30.f);
	}
	else // Not aiming
	{
		// Spread back to normal very quickly
		CrosshairAimFactor = FMath::FInterpTo(
			CrosshairAimFactor,
			0.f,
			DeltaTime,
			30.f);
	}

	// True 0.0f second after firing
	if (bFiringBullet)
	{
		CrosshairShootingFactor = FMath::FInterpTo(
			CrosshairShootingFactor,
			0.3f,
			DeltaTime,
			60.f);
	}
	else
	{
		CrosshairShootingFactor = FMath::FInterpTo(
			CrosshairShootingFactor,
			0.f,
			DeltaTime,
			60.f
		);
	}

	CrosshairSpreadMultiplier =
		0.5f +
		CrosshairVelocityFactor +
		CrosshairInAirFactor -
		CrosshairAimFactor +
		CrosshairShootingFactor;
}

void AMainCharacter::StartCrosshairBulletFire()
{
	bFiringBullet = true;

	GetWorldTimerManager().SetTimer(
		CrosshairShootTimer,
		this,
		&AMainCharacter::FinishCrosshairBulletFire,
		ShootTimeDuration);
}

void AMainCharacter::FinishCrosshairBulletFire()
{
	bFiringBullet = false;
}

void AMainCharacter::AttackButtonPressed()
{
	bAttackButtonPressed = true;
	Attack();
}

void AMainCharacter::AttackButtonReleased()
{
	bAttackButtonPressed = false;
}

void AMainCharacter::StartFireTimer()
{
	CombatState = ECombatState::ECS_FireTimerInProgress;

	GetWorldTimerManager().SetTimer(
		AutoFireTimer,
		this,
		&AMainCharacter::AutoFireReset,
		AutomaticFireRate);
}

void AMainCharacter::AutoFireReset()
{
	CombatState = ECombatState::ECS_Unoccupied;

	if (WeaponHasAmmo())
	{
		if (bAttackButtonPressed)
		{
			FireWeapon();
		}
	}
	else
	{
		// Reload Weapon
		ReloadWeapon();
	}
}

void AMainCharacter::InitializeAmmoMap()
{
	AmmoMap.Add(EAmmoType::EAT_Pistol, StartingPistolAmmo);
	AmmoMap.Add(EAmmoType::EAT_AR, StartingARAmmo);
}

bool AMainCharacter::WeaponHasAmmo()
{
	if (MainGun == nullptr) return false;

	return MainGun->GetAmmo() > 0;
}

void AMainCharacter::PlayFireSound()
{
	// Play fire sound
	if (FireSound)
	{
		UGameplayStatics::PlaySound2D(this, FireSound);
	}
}

void AMainCharacter::SendBullet()
{
	// Send bullet
	const USkeletalMeshSocket* BarrelSocket = MainGun->GetItemMesh()->GetSocketByName("MuzzleFlash");
	if (BarrelSocket)
	{
		const FTransform SocketTransform = BarrelSocket->GetSocketTransform(MainGun->GetItemMesh());

		if (MuzzleFlash)
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, SocketTransform);
		}

		FVector BeamEnd;
		bool bBeamEnd = GetBeamEndLocation(
			SocketTransform.GetLocation(),
			BeamEnd);
		if (bBeamEnd)
		{
			if (ImpactParticles)
			{
				UGameplayStatics::SpawnEmitterAtLocation(
					GetWorld(),
					ImpactParticles,
					BeamEnd);
			}

			UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(),
				BeamParticles,
				SocketTransform);

			if (Beam)
			{
				Beam->SetVectorParameter(FName("Target"), BeamEnd);
			}

		}
	}
}

void AMainCharacter::PlayPistolFireMontage()
{
	// Play Pistol Fire Montage
	UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
	if (AnimInstance && PistolFireMontage)
	{
		AnimInstance->Montage_Play(PistolFireMontage);
		AnimInstance->Montage_JumpToSection(FName("StartFire"));
	}
}

void AMainCharacter::PlayClickSound()
{
	if (ClickSound)
	{
		UGameplayStatics::PlaySound2D(this, ClickSound);
	}
}

void AMainCharacter::PlayReloadSound()
{
	if (ReloadSound)
	{
		UGameplayStatics::PlaySound2D(this, ReloadSound);
	}
}

void AMainCharacter::PlaySlashSound()
{
	// Play slash sound
	if (SlashSound)
	{
		UGameplayStatics::PlaySound2D(this, SlashSound);
	}
}

void AMainCharacter::PlayKnifeSlashMontage()
{
	// Play Knife Slash Montage
	UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
	if (AnimInstance && KnifeSlashMontage)
	{
		AnimInstance->Montage_Play(KnifeSlashMontage);
		AnimInstance->Montage_JumpToSection(FName("StartSlash"));
	}
}

void AMainCharacter::ReloadButtonPressed()
{
	ReloadWeapon();
}

void AMainCharacter::ReloadWeapon()
{
	if (CombatState != ECombatState::ECS_Unoccupied) return;

	// Do we have ammo of the correct type?
	// TODO : Check if current ammo is full (== MagazineCapacity)
	if (CarryingAmmo())
	{
		CombatState = ECombatState::ECS_Reloading;

		PlayReloadSound();

		UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
		if (AnimInstance && ReloadMontage)
		{
			UE_LOG(LogTemp, Warning, TEXT("Reload Montage is called"));

			AnimInstance->Montage_Play(ReloadMontage);
			AnimInstance->Montage_JumpToSection(MainGun->GetReloadMontageSection());
		}
	}
}

bool AMainCharacter::CarryingAmmo()
{
	if (MainGun == nullptr) return false;

	auto AmmoType = MainGun->GetAmmoType();

	if (AmmoMap.Contains(AmmoType))
	{
		return AmmoMap[AmmoType] > 0;
	}

	return false;
}

void AMainCharacter::StartHoldBreath()
{
	if (CombatState != ECombatState::ECS_Unoccupied) return;
	if (CurrentWeaponType != EWeaponType::EWT_Unarmed) return;

	UE_LOG(LogTemp, Warning, TEXT("StartHoldBreath Called"));

	if (CurrentStamina > MinStamina && bShouldHoldBreathAgain == false)
	{
		if (bIsHoldingBreath == false)
		{
			PlayStartHoldBreathSound();
		}

		UE_LOG(LogTemp, Warning, TEXT("StartHoldBreath -> Change States"));

		CombatState = ECombatState::ECS_HoldBreath;
		CurrentWeaponType = EWeaponType::EWT_HoldBreath;

		bIsHoldingBreath = true;
	}
	//else // if (CurrentStamina <= MinStamina || bShouldHoldBreathAgain == false)
	//{
	//	UE_LOG(LogTemp, Warning, TEXT("StartHoldBreath -> Stamina all used up"));

	//	CombatState = ECombatState::ECS_Unoccupied;
	//	CurrentWeaponType = EWeaponType::EWT_Unarmed;

	//	bIsHoldingBreath = false;
	//	bShouldHoldBreathAgain = true;

	//	PlayEndHoldBreathSound();
	//}
}

void AMainCharacter::FinishHoldBreath()
{
	if (CombatState != ECombatState::ECS_HoldBreath) return;
	if (CurrentWeaponType != EWeaponType::EWT_HoldBreath) return;

	CombatState = ECombatState::ECS_Unoccupied;
	CurrentWeaponType = EWeaponType::EWT_Unarmed;

	bIsHoldingBreath = false;
	bShouldHoldBreathAgain = false;

	PlayEndHoldBreathSound();
}

void AMainCharacter::PlayStartHoldBreathSound()
{
	if (HoldBreathStartSound)
	{
		UGameplayStatics::PlaySound2D(this, HoldBreathStartSound);
	}
}

void AMainCharacter::PlayEndHoldBreathSound()
{
	if (HoldBreathEndSound)
	{
		UGameplayStatics::PlaySound2D(this, HoldBreathEndSound);
	}
}

void AMainCharacter::EquipKnife()
{
	if (MainKnife)
	{
		if (CurrentWeaponType == EWeaponType::EWT_Unarmed)
		{
			CurrentWeaponType = EWeaponType::EWT_Knife;

			// Attach Knife to KnifeSocket
			AttachKnife();
			CurrentWeapon = MainKnife;
		}
		else if (CurrentWeaponType == EWeaponType::EWT_Knife)
		{
			CurrentWeaponType = EWeaponType::EWT_Unarmed;
			
			// Detach Knife from KnifeSocket
			DetachKnife();
			CurrentWeapon = nullptr;
		}
		else if (CurrentWeaponType == EWeaponType::EWT_Gun)
		{
			CurrentWeaponType = EWeaponType::EWT_Knife;

			// Detach Gun from GunSocket
			DetachGun();

			// Attach Knife to KnifeSocket
			AttachKnife();
			CurrentWeapon = MainKnife;
		}
	}
}

void AMainCharacter::EquipGun()
{
	if (MainGun)
	{
		if (CurrentWeaponType == EWeaponType::EWT_Unarmed)
		{
			CurrentWeaponType = EWeaponType::EWT_Gun;

			UE_LOG(LogTemp, Warning, TEXT("Attach Gun!"));

			// Attach Gun to PistolSocket
			AttachGun();
			CurrentWeapon = MainGun;
		}
		else if (CurrentWeaponType == EWeaponType::EWT_Knife)
		{
			CurrentWeaponType = EWeaponType::EWT_Gun;

			UE_LOG(LogTemp, Warning, TEXT("Detach Knife!"));
			// Detach Knife from KnifeSocket
			DetachKnife();

			UE_LOG(LogTemp, Warning, TEXT("Attach Gun!"));
			// Attach Gun to PistolSocket
			AttachGun();
			CurrentWeapon = MainGun;
		}
		else if (CurrentWeaponType == EWeaponType::EWT_Gun)
		{
			CurrentWeaponType = EWeaponType::EWT_Unarmed;

			UE_LOG(LogTemp, Warning, TEXT("Detach Gun!"));
			// Detach Gun from GunSocket
			DetachGun();
			CurrentWeapon = nullptr;
		}
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

void AMainCharacter::AttachKnife()
{
	if (MainKnife)
	{
		const USkeletalMeshSocket* KnifeSocket = Mesh1P->GetSocketByName(
			FName("KnifeSocket"));
		if (KnifeSocket)
		{
			KnifeSocket->AttachActor(MainKnife, Mesh1P);
		}
		MainKnife->SetItemState(EItemState::EIS_Equipped);
	}
}

void AMainCharacter::DetachKnife()
{
	if (MainKnife)
	{
		FDetachmentTransformRules DetachmentTransformRules(EDetachmentRule::KeepWorld, true);
		MainKnife->GetItemMesh()->DetachFromComponent(DetachmentTransformRules);
		MainKnife->SetItemState(EItemState::EIS_Obtained);
	}
}

void AMainCharacter::AttachGun()
{
	if (MainGun)
	{
		const USkeletalMeshSocket* PistolSocket = Mesh1P->GetSocketByName(
			FName("PistolSocket"));
		if (PistolSocket)
		{
			PistolSocket->AttachActor(MainGun, Mesh1P);
		}
		MainGun->SetItemState(EItemState::EIS_Equipped);
	}
}

void AMainCharacter::DetachGun()
{
	if (MainGun)
	{
		FDetachmentTransformRules DetachmentTransformRules(EDetachmentRule::KeepWorld, true);
		MainGun->GetItemMesh()->DetachFromComponent(DetachmentTransformRules);
		MainGun->SetItemState(EItemState::EIS_Obtained);
	}
}

void AMainCharacter::Attack()
{
	if (CurrentWeaponType == EWeaponType::EWT_Unarmed) return;
	if (CombatState != ECombatState::ECS_Unoccupied) return;

	if (CurrentWeaponType == EWeaponType::EWT_Gun)
	{
		FireWeapon();
	}
	else if (CurrentWeaponType == EWeaponType::EWT_Knife)
	{
		SlashKnife();
	}

}

void AMainCharacter::FireWeapon()
{
	if (WeaponHasAmmo())
	{
		PlayFireSound();
		SendBullet();
		PlayPistolFireMontage();

		// Start bullet fire timer for crosshairs
		// StartCrosshairBulletFire();

		// Subtract 1 from the Weapon's Ammo
		MainGun->DecrementAmmo();

		StartFireTimer();
	}
	else
	{
		PlayClickSound();
	}
}

void AMainCharacter::SlashKnife()
{
	CombatState = ECombatState::ECS_SlashTimerInProgress;

	PlaySlashSound();
	PlayKnifeSlashMontage();
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