// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Item.h"
#include "AmmoType.h"
#include "Gun.generated.h"

/**
 * 
 */

UENUM(BlueprintType)
enum class EGunType : uint8
{
	EGT_Pistol UMETA(DisplayName = "Pistol"),
	EGT_Rifle UMETA(DisplayName = "AssaultRifle"),

	EGT_MAX UMETA(DisplayName = "DefaultMAX")
};

UCLASS()
class KEEPITDOWN_API AGun : public AItem
{
	GENERATED_BODY()
	
public:
	AGun();

	virtual void Tick(float DeltaTime) override;

private:

	/* Ammo count for this Weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Properties", meta = (AllowPrivateAccess = "true"))
	int32 Ammo;

	/* Maximum ammo that our weapon can hold */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Properties", meta = (AllowPrivateAccess = "true"))
	int32 MagazineCapacity;

	/* The type of weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Properties", meta = (AllowPrivateAccess = "true"))
	EGunType GunType;

	/* The type of ammo */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Properties", meta = (AllowPrivateAccess = "true"))
	EAmmoType AmmoType;

	/* FName for the Reload Montage Section */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Properties", meta = (AllowPrivateAccess = "true"))
	FName ReloadMontageSection;

public:

	FORCEINLINE int32 GetAmmo() const { return Ammo; }
	FORCEINLINE int32 GetMagazineCapacity() const { return MagazineCapacity; }

	/* Called when decrementing ammo */
	void DecrementAmmo();

	FORCEINLINE EGunType GetWeaponType() const { return GunType; }
	FORCEINLINE EAmmoType GetAmmoType() const { return AmmoType; }
	FORCEINLINE FName GetReloadMontageSection() const { return ReloadMontageSection; }

	void ReloadAmmo(int32 Amount);
};
