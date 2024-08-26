// Fill out your copyright notice in the Description page of Project Settings.


#include "XPlayerCameraManager.h"
#include "XCharacterMovementComponent.h"
#include "AddInCMCCharacter.h"
#include "Components/CapsuleComponent.h"

AXPlayerCameraManager::AXPlayerCameraManager()
{
}

void AXPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);

	AAddInCMCCharacter* XCharacter = Cast<AAddInCMCCharacter>(GetOwningPlayerController()->GetPawn());
		
	if (XCharacter)
	{
		UXCharacterMovementComponent* XMC = XCharacter->GetXCharacterMovementComponent();
		//Target
		FVector TargetCrouchOffset = FVector(
			0.0f,
			0.0f,
			XMC->CrouchedHalfHeight - XCharacter->GetClass()->GetDefaultObject<ACharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		);
		//per frame need offset
		FVector Offset = FMath::Lerp(
			FVector::ZeroVector,
			TargetCrouchOffset,
			FMath::Clamp(CrouchBlendTime / CrouchBlendDuration, 0.0f, 1.0f)
		);

		if (XMC->IsCrouching())
		{
			//now frame blendtime
			CrouchBlendTime = FMath::Clamp(
				CrouchBlendTime + DeltaTime,
				0.0f,
				CrouchBlendDuration
			);
			//the offset  change relative from idle to crouch 
			Offset -= TargetCrouchOffset;
		}
		else
		{
			CrouchBlendTime = FMath::Clamp(
				CrouchBlendTime - DeltaTime,
				0.0f,
				CrouchBlendDuration
			);
		}
		if (XMC->IsMovingOnGround())
		{
			OutVT.POV.Location += Offset;
		}
	}
}
