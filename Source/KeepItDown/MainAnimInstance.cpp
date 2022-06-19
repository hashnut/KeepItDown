// Fill out your copyright notice in the Description page of Project Settings.


#include "MainAnimInstance.h"
#include "MainCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UMainAnimInstance::UpdateAnimationProperties(float DeltaTime)
{
	if (MainCharacter == nullptr)
	{
		MainCharacter = Cast<AMainCharacter>(TryGetPawnOwner());
	}

	if (MainCharacter)
	{
		// Get the lateral speed of the character from velocity
		FVector Velocity{ MainCharacter->GetVelocity() };
		Velocity.Z = 0;
		Speed = Velocity.Size();

		// Is the character in the air?
		bIsInAir = MainCharacter->GetCharacterMovement()->IsFalling();

		// Is the character accelerating?
		if (MainCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f)
		{
			bIsAccelerating = true;
		}
		else
		{
			bIsAccelerating = false;
		}

		FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(
			MainCharacter->GetVelocity());

		FRotator AimRotation = MainCharacter->GetBaseAimRotation();

		MovementOffsetYaw = UKismetMathLibrary::NormalizedDeltaRotator(
			MovementRotation,
			AimRotation).Yaw;

		if (MainCharacter->GetVelocity().Size() > 0.f)
		{
			LastMovementOffsetYaw = MovementOffsetYaw;
		}

		bAiming = MainCharacter->GetAiming();

		//FString RotationMessage = FString::Printf(
		//	TEXT("Base Aim Rotation: %f"),
		//	AimRotation.Yaw);

		//FString MovementRotationMessage = FString::Printf(
		//	TEXT("MovementRotation: %f"), 
		//	MovementRotation.Yaw);

		//FString OffsetMessage = FString::Printf(
		//	TEXT("Movement Offset Yaw: %f"),
		//	AimRotation.Yaw);

		//if (GEngine)
		//{
		//	GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::White, OffsetMessage);
		//}
	}
}

void UMainAnimInstance::NativeInitializeAnimation()
{
	MainCharacter = Cast<AMainCharacter>(TryGetPawnOwner());
}

