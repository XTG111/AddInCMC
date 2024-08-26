// Fill out your copyright notice in the Description page of Project Settings.


#include "XCharacterMovementComponent.h"
#include "GameFramework/Character.h"

FNetworkPredictionData_Client* UXCharacterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr);
	if (ClientPredictionData == nullptr)
	{
		UXCharacterMovementComponent* Mutablethis = const_cast<UXCharacterMovementComponent*>(this);
		Mutablethis->ClientPredictionData = new FNetworkPredictionData_Client_XCharacter(*this);
		Mutablethis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.0f;
		Mutablethis->ClientPredictionData->NoSmoothNetUpdateDist = 140.0f;
	}
	return ClientPredictionData;
}

void UXCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	Safe_bWantsToSprint = (Flags & FSavedMove_XCharacter::FLAG_Custom_0) != 0;
}

void UXCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	//Do Some Change when the State Change
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	
	if (MovementMode == MOVE_Walking)
	{
		if (Safe_bWantsToSprint)
		{
			MaxWalkSpeed = Sprint_MaxWalkSpeed;
		}
		else
		{
			MaxWalkSpeed = Walk_MaxWalkSpeed;
		}
	}
}

UXCharacterMovementComponent::UXCharacterMovementComponent()
{
}

void UXCharacterMovementComponent::SprintPressed()
{
	Safe_bWantsToSprint = true;
}

void UXCharacterMovementComponent::SprintReleased()
{
	Safe_bWantsToSprint = false;
}

bool UXCharacterMovementComponent::FSavedMove_XCharacter::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	FSavedMove_XCharacter* NewXMove = static_cast<FSavedMove_XCharacter*>(NewMove.Get());
	if (Saved_bWantsToSprint != NewXMove->Saved_bWantsToSprint)
	{
		//if OldMove SprintState not equal NewMove SprintState can't Combine Return false
		return false;
	}

	return FSavedMove_Character::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void UXCharacterMovementComponent::FSavedMove_XCharacter::Clear()
{
	FSavedMove_Character::Clear();
	Saved_bWantsToSprint = 0;
}

uint8 UXCharacterMovementComponent::FSavedMove_XCharacter::GetCompressedFlags() const
{
	uint8 Result = FSavedMove_Character::GetCompressedFlags();

	if (Saved_bWantsToSprint)
	{
		Result |= FLAG_Custom_0;
	}

	return Result;
}

void UXCharacterMovementComponent::FSavedMove_XCharacter::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	UXCharacterMovementComponent* XCharacterMovement = Cast<UXCharacterMovementComponent>(C->GetCharacterMovement());
	
	Saved_bWantsToSprint = XCharacterMovement->Safe_bWantsToSprint;
}

void UXCharacterMovementComponent::FSavedMove_XCharacter::PrepMoveFor(ACharacter* C)
{
	FSavedMove_Character::PrepMoveFor(C);
	UXCharacterMovementComponent* XCharacterMovement = Cast<UXCharacterMovementComponent>(C->GetCharacterMovement());

	XCharacterMovement->Safe_bWantsToSprint = Saved_bWantsToSprint;
}

UXCharacterMovementComponent::FNetworkPredictionData_Client_XCharacter::FNetworkPredictionData_Client_XCharacter(const UCharacterMovementComponent& ClientMovement):Super(ClientMovement)
{
}

FSavedMovePtr UXCharacterMovementComponent::FNetworkPredictionData_Client_XCharacter::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_XCharacter());
}
