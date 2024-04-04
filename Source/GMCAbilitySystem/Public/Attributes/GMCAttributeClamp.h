﻿#pragma once
#include "GameplayTagContainer.h"
#include "GMCAttributeClamp.generated.h"


class UGMC_AbilitySystemComponent;

USTRUCT()
struct GMCABILITYSYSTEM_API FAttributeClamp
{
	GENERATED_BODY()

	// Minimum attribute value
	UPROPERTY(EditDefaultsOnly)
	float Min;

	// Value will be clamped to the value of this attribute
	// If set, this will take priority over Min
	UPROPERTY(EditDefaultsOnly)
	FGameplayTag MinAttributeTag;

	// Maximum attribute value
	UPROPERTY(EditDefaultsOnly)
	float Max;

	// Value will be clamped to the value of this attribute
	// If set, this will take priority over Max
	UPROPERTY(EditDefaultsOnly)
	FGameplayTag MaxAttributeTag;

	UPROPERTY()
	UGMC_AbilitySystemComponent* AbilityComponent;

	bool operator==(const FAttributeClamp* Other) const {return Other->Min == Min && Other->Max == Max && Other->MinAttributeTag == MinAttributeTag && Other->MaxAttributeTag == MaxAttributeTag;}

	float ClampValue(float Value) const;
};