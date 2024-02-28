﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "GMCAbilityEffect.h"

#include "GMCAbilitySystem.h"
#include "Components/GMCAbilityComponent.h"


void UGMCAbilityEffect::InitializeEffect(UGMC_AbilityComponent* AbilityComponent, bool bServerApplied, FGMCAbilityEffectData InitializationData)
{
	if (AbilityComponent)
	{
		OwnerAbilityComponent = AbilityComponent;
	}
	else
	{
		UE_LOG(LogGMCAbilitySystem, Error, TEXT("Invalid AbilityComponent Passed to UGMCAbilityEffect"));
	}

	// Add any programmatically generated modifiers
	for (FGMCAttributeModifier Modifier : InitializationData.Modifiers)
	{
		if (InitializationData.bOverwriteExistingModifiers)
		{
			TArray<int> ExistingModifiersToRemove;
			// Find all matching with the same name
			for (int i = 0; i < Modifiers.Num(); i++)
			{
				if (Modifiers[i].AttributeName == Modifier.AttributeName)
				{
					ExistingModifiersToRemove.Push(i);
				}
			}
			// Purge them from the existing modifiers array
			for (int i : ExistingModifiersToRemove)
			{
				Modifiers.RemoveAt(i);
			}
		}
		
		Modifiers.Add(Modifier);
	}

	if (AbilityComponent->HasAuthority())
	{
		// Server confirms its self!
		bServerConfirmed = true;
		// If applied by server, for the client to be able to actually predict this, the start of the ability must be delayed
		// on the server long enough for the client to receive the effect. 
		if (bServerApplied)
		{
			// .25 is a buffer that should capture most players. If you have over .25 seconds of ping, you're probably not playing
			// Todo: Make this a setting
			StartTime = OwnerAbilityComponent->ActionTimer + .25;
			EndTime = OwnerAbilityComponent->ActionTimer + Duration + .25;
		}
		else
		{
			StartTime = OwnerAbilityComponent->ActionTimer;
			EndTime = OwnerAbilityComponent->ActionTimer + Duration;
		}
		return;
	}
	
	// This is a client prediction
	if (InitializationData == FGMCAbilityEffectData{} && !bServerApplied)
	{
		StartTime = OwnerAbilityComponent->ActionTimer;
		EndTime = OwnerAbilityComponent->ActionTimer + Duration;
	}
	// Data from the server about this effect has been received
	else
	{
		// Actual delay is a bit longer than RTT, hence the 30% increase
		StartTime = InitializationData.ServerStartTime;
		// UE_LOG(LogGMCAbilitySystem, Warning, TEXT("Time Until Client Start: %lf"), StartTime - OwnerAbilityComponent->ActionTimer )
		EndTime = InitializationData.ServerEndTime;
		Id = InitializationData.EffectID;
		bServerConfirmed = true;
	}
}

void UGMCAbilityEffect::EndEffect()
{
	bCompleted = true;

	if (CurrentState != EEffectState::Ended)
	{
		UpdateState(EEffectState::Ended, true);
	}

	// Only remove tags and abilities if the effect has started
	if (!bHasStarted) return;
	
	RemoveTagsFromOwner();
	RemoveAbilitiesFromOwner();
	
	if (EffectType != EEffectType::Instant && Period == 0)
	{
		OwnerAbilityComponent->RemoveActiveAbilityModifiers(this);
	}
}

void UGMCAbilityEffect::Tick(float DeltaTime)
{
	// Ensure tag requirements are met before applying the effect
	if( !CheckMustHaveTags() || !CheckMustNotHaveTags() )
	{
		UpdateState(EEffectState::Ended, true);
	}
	
	TickPeriodicEffects(DeltaTime);
	CheckState();
}

void UGMCAbilityEffect::TickPeriodicEffects(float DeltaTime)
{
	// If there's a period, check to see if it needs to apply its modifiers
	if (Period != 0 && CurrentState == EEffectState::Started)
	{
		PeriodicApplicationTimer += DeltaTime;
		if (PeriodicApplicationTimer >= Period)
		{
			OwnerAbilityComponent->ApplyAbilityEffectModifiers(this);
			PeriodicApplicationTimer = 0;
		}
	}
}

void UGMCAbilityEffect::UpdateState(EEffectState State, bool Force)
{
	if (State == EEffectState::Ended)
	{
		// UE_LOG(LogGMCAbilitySystem, Warning, TEXT("Effect Ended"));
	}

	CurrentState = State;
}

void UGMCAbilityEffect::AddTagsToOwner()
{
	for (const FGameplayTag Tag : GrantedTags)
	{
		OwnerAbilityComponent->AddActiveTag(Tag);
	}
}

void UGMCAbilityEffect::RemoveTagsFromOwner()
{
	for (const FGameplayTag Tag : GrantedTags)
	{
		OwnerAbilityComponent->RemoveActiveTag(Tag);
	}
}

void UGMCAbilityEffect::AddAbilitiesToOwner()
{
	for (const FGameplayTag Tag : GrantedAbilities)
	{
		OwnerAbilityComponent->GrantAbilityByTag(Tag);
	}
}

void UGMCAbilityEffect::RemoveAbilitiesFromOwner()
{
	for (const FGameplayTag Tag : GrantedAbilities)
	{
		OwnerAbilityComponent->RemoveGrantedAbilityByTag(Tag);
	}
}

bool UGMCAbilityEffect::CheckMustHaveTags()
{
	for (const FGameplayTag Tag : MustHaveTags)
	{
		if (!OwnerAbilityComponent->HasActiveTag(Tag))
		{
			return false;
		}
	}
	return true;
}

bool UGMCAbilityEffect::CheckMustNotHaveTags()
{
	for (const FGameplayTag Tag : MustNotHaveTags)
	{
		if (OwnerAbilityComponent->HasActiveTag(Tag))
		{
			return false;
		}
	}
	return true;
}

bool UGMCAbilityEffect::DuplicateEffectAlreadyApplied()
{
	if (EffectTag == FGameplayTag::EmptyTag)
	{
		return false;
	}
	
	for (const TPair<int, UGMCAbilityEffect*> Effect : OwnerAbilityComponent->GetActiveEffects())
	{
		if (Effect.Value->EffectTag == this->EffectTag && Effect.Value->bHasStarted)
		{
			return true;
		}
	}

	return false;
}

bool UGMCAbilityEffect::CompletedAndServerConfirmed()
{
	return bCompleted && bServerConfirmed;
}

void UGMCAbilityEffect::CheckState()
{
	switch (CurrentState)
	{
		case EEffectState::Initialized:
			if (OwnerAbilityComponent->ActionTimer >= StartTime)
			{
				// Ensure tag requirements are met before applying the effect
				if( !CheckMustHaveTags() || !CheckMustNotHaveTags() || DuplicateEffectAlreadyApplied() )
				{
					UpdateState(EEffectState::Ended, true);
					return;
				}

				bHasStarted = true;
				
				AddTagsToOwner();
				AddAbilitiesToOwner();
				
				// Instant/Duration effects that apply immediately
				if (Period == 0)
				{
					OwnerAbilityComponent->ApplyAbilityEffectModifiers(this);
				}

				// Instant effects instantly end
				if (EffectType == EEffectType::Instant)
				{
					UpdateState(EEffectState::Ended, true);
				}
				
				else
				{
					UpdateState(EEffectState::Started, true);
				}
			}
			break;
		case EEffectState::Started:
			if (EffectType == EEffectType::Duration && OwnerAbilityComponent->ActionTimer >= EndTime)
			{
				UpdateState(EEffectState::Ended, true);
			}
			break;
		case EEffectState::Ended:
			EndEffect();
			bCompleted = true;
			break;
	default: break;
	}
}
