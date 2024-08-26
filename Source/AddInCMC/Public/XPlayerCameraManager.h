// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "XPlayerCameraManager.generated.h"

/**
 * 
 */
UCLASS()
class ADDINCMC_API AXPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()
public:
	AXPlayerCameraManager();

public:
	//The Params use to Crouch Blend
	UPROPERTY(EditDefaultsOnly)
	float CrouchBlendDuration = 0.5f;
	float CrouchBlendTime;

public:
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;
	
};
