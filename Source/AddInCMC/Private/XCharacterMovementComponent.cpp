// Fill out your copyright notice in the Description page of Project Settings.


#include "XCharacterMovementComponent.h"
#include "AddInCMCCharacter.h"
#include "Components/CapsuleComponent.h"

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

bool UXCharacterMovementComponent::IsMovingOnGround() const
{
	return Super::IsMovingOnGround() || IsCustomMovementMode(CMove_Slide);
}

bool UXCharacterMovementComponent::CanCrouchInCurrentState() const
{
	return Super::CanCrouchInCurrentState() && IsMovingOnGround();
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
	Safe_bPrevWantsToCrouch = bWantsToCrouch;
}

void UXCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	//Crouch will do in this Func, we need do slide befor crouch
	if (MovementMode == MOVE_Walking && !bWantsToCrouch && Safe_bPrevWantsToCrouch)
	{
		if (CanSlide())
		{
			SetMovementMode(MOVE_Custom, CMove_Slide);
		}
	}
	if (IsCustomMovementMode(CMove_Slide) && !bWantsToCrouch)
	{
		SetMovementMode(MOVE_Walking);
	}
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UXCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	Super::PhysCustom(deltaTime, Iterations);
	switch (CustomMovementMode)
	{
	case CMove_Slide:
		PhysSlide(deltaTime, Iterations);
		break;
	default:
		UE_LOG(LogTemp, Fatal, TEXT("Invalid Movement Mode!!!"));
		break;
	}
}

void UXCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMove_Slide) ExitSlide();

	if (IsCustomMovementMode(CMove_Slide)) EnterSlide(PreviousMovementMode, (ECustomMovementMode)PreviousCustomMode);
}

void UXCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
	XCharacter = Cast<AAddInCMCCharacter>(GetOwner());
}

void UXCharacterMovementComponent::EnterSlide(EMovementMode PrevMode, ECustomMovementMode PrevCustomMode)
{
	bWantsToCrouch = true;
	bOrientRotationToMovement = false;
	Velocity += Velocity.GetSafeNormal2D() * Slide_EnterImpulse;
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, true, NULL);
}

void UXCharacterMovementComponent::ExitSlide()
{
	bWantsToCrouch = false;
	bOrientRotationToMovement = true;
}

void UXCharacterMovementComponent::PhysSlide(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	RestorePreAdditiveRootMotionVelocity();

	//if can't slide exit and change physics which belong the new state
	if (!CanSlide())
	{
		SetMovementMode(MOVE_Walking);
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	bJustTeleported = false;
	bool bCheckedFall = false;
	bool bTriedLedgeMove = false;
	float remainingTime = deltaTime;


	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner && (CharacterOwner->Controller || bRunPhysicsWithNoController || (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)))
	{
		Iterations++;
		bJustTeleported = false;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		// Save current values
		UPrimitiveComponent* const OldBase = GetMovementBase();
		const FVector PreviousBaseLocation = (OldBase != NULL) ? OldBase->GetComponentLocation() : FVector::ZeroVector;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FFindFloorResult OldFloor = CurrentFloor;

		// Ensure velocity is horizontal.
		MaintainHorizontalGroundVelocity();
		const FVector OldVelocity = Velocity;

		FVector SlopeForce = CurrentFloor.HitResult.Normal;
		SlopeForce.Z = 0.f;
		Velocity += SlopeForce * Slide_GravityForce * deltaTime;

		Acceleration = Acceleration.ProjectOnTo(UpdatedComponent->GetRightVector().GetSafeNormal2D());

		// Apply acceleration
		CalcVelocity(timeTick, GroundFriction * Slide_Friction, false, GetMaxBrakingDeceleration());

		// Compute move parameters
		const FVector MoveVelocity = Velocity;
		const FVector Delta = timeTick * MoveVelocity;
		const bool bZeroDelta = Delta.IsNearlyZero();
		FStepDownResult StepDownResult;
		bool bFloorWalkable = CurrentFloor.IsWalkableFloor();

		if (bZeroDelta)
		{
			remainingTime = 0.f;
		}
		else
		{
			// try to move forward
			MoveAlongFloor(MoveVelocity, timeTick, &StepDownResult);

			if (IsFalling())
			{
				// pawn decided to jump up
				const float DesiredDist = Delta.Size();
				if (DesiredDist > KINDA_SMALL_NUMBER)
				{
					const float ActualDist = (UpdatedComponent->GetComponentLocation() - OldLocation).Size2D();
					remainingTime += timeTick * (1.f - FMath::Min(1.f, ActualDist / DesiredDist));
				}
				StartNewPhysics(remainingTime, Iterations);
				return;
			}
			else if (IsSwimming()) //just entered water
			{
				StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
				return;
			}
		}

		// Update floor.
		// StepUp might have already done it for us.
		if (StepDownResult.bComputedFloor)
		{
			CurrentFloor = StepDownResult.FloorResult;
		}
		else
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, bZeroDelta, NULL);
		}


		// check for ledges here
		const bool bCheckLedges = !CanWalkOffLedges();
		if (bCheckLedges && !CurrentFloor.IsWalkableFloor())
		{
			// calculate possible alternate movement
			const FVector GravDir = FVector(0.f, 0.f, -1.f);
			const FVector NewDelta = bTriedLedgeMove ? FVector::ZeroVector : GetLedgeMove(OldLocation, Delta, GravDir);
			if (!NewDelta.IsZero())
			{
				// first revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, false);

				// avoid repeated ledge moves if the first one fails
				bTriedLedgeMove = true;

				// Try new movement direction
				Velocity = NewDelta / timeTick;
				remainingTime += timeTick;
				continue;
			}
			else
			{
				// see if it is OK to jump
				// @todo collision : only thing that can be problem is that oldbase has world collision on
				bool bMustJump = bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump))
				{
					return;
				}
				bCheckedFall = true;

				// revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, true);
				remainingTime = 0.f;
				break;
			}
		}
		else
		{
			// Validate the floor check
			if (CurrentFloor.IsWalkableFloor())
			{
				if (ShouldCatchAir(OldFloor, CurrentFloor))
				{
					HandleWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, timeTick);
					if (IsMovingOnGround())
					{
						// If still walking, then fall. If not, assume the user set a different mode they want to keep.
						StartFalling(Iterations, remainingTime, timeTick, Delta, OldLocation);
					}
					return;
				}

				AdjustFloorHeight();
				SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
			}
			else if (CurrentFloor.HitResult.bStartPenetrating && remainingTime <= 0.f)
			{
				// The floor check failed because it started in penetration
				// We do not want to try to move downward because the downward sweep failed, rather we'd like to try to pop out of the floor.
				FHitResult Hit(CurrentFloor.HitResult);
				Hit.TraceEnd = Hit.TraceStart + FVector(0.f, 0.f, MAX_FLOOR_DIST);
				const FVector RequestedAdjustment = GetPenetrationAdjustment(Hit);
				ResolvePenetration(RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat());
				bForceNextFloorCheck = true;
			}

			// check if just entered water
			if (IsSwimming())
			{
				StartSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
				return;
			}

			// See if we need to start falling.
			if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
			{
				const bool bMustJump = bJustTeleported || bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump))
				{
					return;
				}
				bCheckedFall = true;
			}
		}

		// Allow overlap events and such to change physics state and velocity
		if (IsMovingOnGround() && bFloorWalkable)
		{
			// Make velocity reflect actual move
			if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && timeTick >= MIN_TICK_TIME)
			{
				// TODO-RootMotionSource: Allow this to happen during partial override Velocity, but only set allowed axes?
				Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick;
				MaintainHorizontalGroundVelocity();
			}
		}

		// If we didn't move at all this iteration then abort (since future iterations will also be stuck).
		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			remainingTime = 0.f;
			break;
		}
	}


	FHitResult Hit;
	FQuat NewRotation = FRotationMatrix::MakeFromXZ(Velocity.GetSafeNormal2D(), FVector::UpVector).ToQuat();
	SafeMoveUpdatedComponent(FVector::ZeroVector, NewRotation, false, Hit);
}


bool UXCharacterMovementComponent::CanSlide() const
{
	FVector Start = UpdatedComponent->GetComponentLocation();
	FVector End = Start + CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.5f * FVector::DownVector;
	FName ProfileName = TEXT("BlockAll");
	bool bValidSurface = GetWorld()->LineTraceTestByProfile(Start, End, ProfileName, XCharacter->GetIgnoreCharacterParams());
	bool bEnoughSpeed = Velocity.Size() > Slide_MinSpeed;//Velocity.SizeSquared() > pow(Slide_MinSpeed, 2);

	return bValidSurface && bEnoughSpeed;
}

UXCharacterMovementComponent::UXCharacterMovementComponent()
{
	NavAgentProps.bCanCrouch = true; // the ability crouch
}

void UXCharacterMovementComponent::SprintPressed()
{
	Safe_bWantsToSprint = true;
}

void UXCharacterMovementComponent::SprintReleased()
{
	Safe_bWantsToSprint = false;
}

void UXCharacterMovementComponent::CrouchPressed()
{
	bWantsToCrouch = !bWantsToCrouch;
}

bool UXCharacterMovementComponent::IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == InCustomMovementMode;
}


#pragma region Base
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
	Saved_bPrevWantsToCrouch = XCharacterMovement->Safe_bPrevWantsToCrouch;
}

void UXCharacterMovementComponent::FSavedMove_XCharacter::PrepMoveFor(ACharacter* C)
{
	FSavedMove_Character::PrepMoveFor(C);
	UXCharacterMovementComponent* XCharacterMovement = Cast<UXCharacterMovementComponent>(C->GetCharacterMovement());

	XCharacterMovement->Safe_bWantsToSprint = Saved_bWantsToSprint;
	XCharacterMovement->Safe_bPrevWantsToCrouch = Saved_bPrevWantsToCrouch;
}

UXCharacterMovementComponent::FNetworkPredictionData_Client_XCharacter::FNetworkPredictionData_Client_XCharacter(const UCharacterMovementComponent& ClientMovement):Super(ClientMovement)
{
}

FSavedMovePtr UXCharacterMovementComponent::FNetworkPredictionData_Client_XCharacter::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_XCharacter());
}
#pragma endregion
