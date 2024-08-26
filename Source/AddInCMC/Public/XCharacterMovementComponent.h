// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "XCharacterMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class ADDINCMC_API UXCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	/*** Safe Param ***/
	// These Params Will Replicate in Server
	//And When Client Update These Params will be in Current State and Replicate
public:

	//A snapshot when Character Move per frame
	class FSavedMove_XCharacter : public FSavedMove_Character
	{
		typedef FSavedMove_Character Super;

		//The Flag Use To send and rec Server When Safe_bWantsToSprint Changged the param will change
		//and the replicate from sever will change the Safe_bWantsToSprint
		uint8 Saved_bWantsToSprint : 1;

		//Check The OldMove can or not Combine with NewMove
		//if true will combine two Move
		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
		
		//Reset The All States In SavedMove
		virtual void Clear() override;

		//SavedMove Flags
		//The Flags always use to control or show the States
		virtual uint8 GetCompressedFlags() const;

		//From The State To Set The SavedFlag
		virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData);
		
		//From The SaveFlag To Set The State
		virtual void PrepMoveFor(ACharacter* C);
	};

	//The Class To Tell The CharacterMovement use the custom SavedMove
	class FNetworkPredictionData_Client_XCharacter : public FNetworkPredictionData_Client_Character
	{
	public:
		FNetworkPredictionData_Client_XCharacter(const UCharacterMovementComponent& ClientMovement);
		typedef FNetworkPredictionData_Client_Character Super;
		virtual FSavedMovePtr AllocateNewMove() override;
	};

	/*** FUNCTION ***/
public:
	//To Tell this class Use the custom FNetworkPredictionData_Client_XCharacter
	//return the class obj use new FNetworkPredictionData_Client_XCharacter()
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:

	//Update State Form the Flags In SavedMove
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	//Update after pre move and will do any logic don't care about the Movemode
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

	/*** Variable ***/
public:
	//Dash State Set On Client Wher Character want to Dash
	//Working Variable, the logic base on the variable
	bool Safe_bWantsToSprint;

	//Speed
	UPROPERTY(EditDefaultsOnly)
	float Sprint_MaxWalkSpeed;
	UPROPERTY(EditDefaultsOnly)
	float Walk_MaxWalkSpeed;

public:
	UXCharacterMovementComponent();

	/*** FUNCTION ***/

	//Change The Safe_bWantsToSprint value
	//these func not safe don't use these func change safe variable in server
	//you can only use these func in owing client to change the safe variable
	UFUNCTION(BlueprintCallable)
	void SprintPressed();
	UFUNCTION(BlueprintCallable)
	void SprintReleased();
};
