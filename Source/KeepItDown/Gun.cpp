// Fill out your copyright notice in the Description page of Project Settings.

#include "Gun.h"

AGun::AGun() :
	Ammo(13),
	MagazineCapacity(13),
	GunType(EGunType::EGT_Pistol),
	AmmoType(EAmmoType::EAT_Pistol),
	ReloadMontageSection(FName(TEXT("StartReload")))
{
	PrimaryActorTick.bCanEverTick = true;
	ItemCategory = EItemCategory::EIC_Gun;
}

void AGun::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Keep the Weapon upright
	if (GetItemState() == EItemState::EIS_Falling)
	{
		FRotator MeshRotation{ 0.f, GetItemMesh()->GetComponentRotation().Yaw, 0.f };
		GetItemMesh()->SetWorldRotation(
			MeshRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
}

void AGun::DecrementAmmo()
{
	if (Ammo - 1 <= 0)
	{
		Ammo = 0;
	}
	else
	{
		--Ammo;
	}
}

void AGun::ReloadAmmo(int32 Amount)
{
	checkf(Ammo + Amount <= MagazineCapacity, TEXT("Attempted to reload with more than magazine capacity"));
	Ammo += Amount;
}
