#pragma once
#include "CoreMinimal.h"
#include "FGInventoryComponent.h"
#include "TFTypes.generated.h"
class ATFStop;

USTRUCT(BlueprintType)
struct FTFStackReservation
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) TSubclassOf<UFGItemDescriptor> Resource;
    UPROPERTY(SaveGame) int32 Stacks = 0;
};

USTRUCT(BlueprintType)
struct FTFPortConfig
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) bool Input = true;
    UPROPERTY(SaveGame) int32 Compartment = 0;
    UPROPERTY(SaveGame) TSubclassOf<UFGItemDescriptor> Filter;
};

USTRUCT(BlueprintType)
struct FTFTank
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) TSubclassOf<UFGItemDescriptor> Resource;
    UPROPERTY(SaveGame) float Input = 0;
    UPROPERTY(SaveGame) float Output = 0;
};

USTRUCT(BlueprintType)
struct FTFRoute
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) TSubclassOf<UFGItemDescriptor> Resource;
    UPROPERTY(SaveGame) TObjectPtr<ATFStop> Destination;
    UPROPERTY(SaveGame) int32 Weight = 1;
    UPROPERTY(SaveGame) double Delivered = 0;
};

USTRUCT(BlueprintType)
struct FTFCargo
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) TSubclassOf<UFGItemDescriptor> Resource;
    UPROPERTY(SaveGame) TObjectPtr<ATFStop> Destination;
    UPROPERTY(SaveGame) float Amount = 0;
    UPROPERTY(SaveGame) int32 Compartment = 0;
};

// UI-only snapshot. Never replicate FInventoryItem's instance-state payload for an icon grid.
USTRUCT()
struct FTFInventorySlot
{
    GENERATED_BODY()
    UPROPERTY() TSubclassOf<UFGItemDescriptor> Resource;
    UPROPERTY() int32 NumItems = 0;
};

struct FTFStats
{
    int32 Stacks, Stops;
    float Fluid, Speed, ActiveMW, IdleMW, ItemRate, FluidRate;
    static FTFStats ForTier(int32 Tier)
    {
        if (Tier == 3) return {96,20,1500,1200,50,4,120,60};
        if (Tier == 2) return {48,12,800,800,25,2,60,30};
        return {24,6,300,400,10,1,20,10};
    }
};

namespace TF
{
    inline constexpr float DoorSeconds = .85f;
    inline float Ease(float Alpha) { const float A=FMath::Clamp(Alpha,0.f,1.f);return A*A*(3.f-2.f*A); }
    // Integrated trapezoidal velocity: zero speed at each landing, tier speed in cruise.
    inline float TravelPosition(float Elapsed,float Distance,float Speed)
    {
        if(Distance<=0 || Speed<=0)return 0;
        const float Ramp=FMath::Min(.8f,Distance/Speed),Duration=Distance/Speed+Ramp;
        const float T=FMath::Clamp(Elapsed,0.f,Duration);
        if(T<Ramp)return .5f*Speed*T*T/Ramp;
        if(T>Duration-Ramp){const float Left=Duration-T;return Distance-.5f*Speed*Left*Left/Ramp;}
        return Speed*(T-.5f*Ramp);
    }
    inline constexpr float Section = 400, StopSpacing = 800, MaxTravel = 24800, Epsilon = .0001f;
    inline int32 StackSize(TSubclassOf<UFGItemDescriptor> Resource) { return Resource ? FMath::Max(1,UFGItemDescriptor::GetStackSize(Resource)) : 1; }
    inline FString ResourceName(TSubclassOf<UFGItemDescriptor> Resource) { return Resource ? UFGItemDescriptor::GetItemName(Resource).ToString() : TEXT("Unassigned"); }
    inline bool Fluid(TSubclassOf<UFGItemDescriptor> Resource) { return Resource && UFGItemDescriptor::GetForm(Resource) == EResourceForm::RF_LIQUID; }
}
