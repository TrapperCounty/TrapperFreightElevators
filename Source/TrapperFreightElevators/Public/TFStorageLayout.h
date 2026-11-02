#pragma once
#include "CoreMinimal.h"
#include "TFTypes.h"

// A display permutation only. Transfers always use SourceIndex, never the tile position.
struct FTFStorageTile
{
    int32 SourceIndex = INDEX_NONE;
    TSubclassOf<UFGItemDescriptor> ReservedFor;
    bool WaitingForSpace = false;
    bool InTransit = false;
};

inline TArray<FTFStorageTile> TFStorageLayout(const TArray<FInventoryStack>& Stacks, int32 Capacity,
    const TArray<FTFStackReservation>& Rules, const TMap<TSubclassOf<UFGItemDescriptor>, int32>& Transit,
    TFunctionRef<int32(TSubclassOf<UFGItemDescriptor>)> StackSize = TF::StackSize)
{
    TArray<FTFStorageTile> Result;
    TSet<int32> Used;
    auto Has = [&](int32 I) { return Stacks.IsValidIndex(I) && Stacks[I].HasItems(); };
    // Claim matching occupied stacks for every rule before assigning empty/shared slots.
    for (const auto& Rule : Rules)
    {
        if (!Rule.Resource) continue;
        for (int32 N = 0; N < Rule.Stacks && Result.Num() < Capacity; ++N)
        {
            FTFStorageTile Tile; Tile.ReservedFor = Rule.Resource;
            for (int32 I = 0; I < Capacity; ++I)
                if (!Used.Contains(I) && Has(I) && Stacks[I].Item.GetItemClass() == Rule.Resource)
                { Tile.SourceIndex = I; Used.Add(I); break; }
            Result.Add(Tile);
        }
    }
    TMap<TSubclassOf<UFGItemDescriptor>, int32> PendingSlots;
    for (const auto& Rule : Rules)
    {
        int32 Items = 0, Occupied = 0;
        for (const auto& Stack : Stacks) if (Stack.HasItems() && Stack.Item.GetItemClass() == Rule.Resource)
        { Items += Stack.NumItems; ++Occupied; }
        if (Rule.Resource)
            PendingSlots.Add(Rule.Resource, FMath::Max(0, FMath::DivideAndRoundUp(Items + Transit.FindRef(Rule.Resource), FMath::Max(1,StackSize(Rule.Resource))) - Occupied));
    }
    for (auto& Tile : Result) if (Tile.SourceIndex == INDEX_NONE)
    {
        // Empty slots first; occupied conflicts remain visible and can still be withdrawn.
        for (int32 Pass = 0; Pass < 2 && Tile.SourceIndex == INDEX_NONE; ++Pass)
            for (int32 I = 0; I < Capacity; ++I)
                if (!Used.Contains(I) && (Pass == 1 || !Has(I)))
                { Tile.SourceIndex = I; Tile.WaitingForSpace = Has(I); Used.Add(I); break; }
        int32& Pending = PendingSlots.FindOrAdd(Tile.ReservedFor);
        Tile.InTransit = Pending > 0;
        if (Tile.InTransit) --Pending;
    }
    for (int32 I = 0; I < Capacity; ++I) if (!Used.Contains(I))
    { FTFStorageTile Tile; Tile.SourceIndex = I; Result.Add(Tile); }
    return Result;
}
