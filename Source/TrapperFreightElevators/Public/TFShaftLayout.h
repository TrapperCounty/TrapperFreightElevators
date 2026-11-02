#pragma once
#include "CoreMinimal.h"
#include "TFTypes.h"

// Derived geometry only: no buildable actors, inventory, construction cost or save data.
struct FTFShaftLayout
{
    float Height = 800;
    TArray<FVector2D> Frames; // Local base height and length, in centimetres.
    TArray<FVector2D> Screens;

    bool operator==(const FTFShaftLayout& Other) const
    { return Height==Other.Height && Frames==Other.Frames && Screens==Other.Screens; }

    static FTFShaftLayout Make(const TArray<float>& Stops,const TArray<float>& ManualShafts,float BuiltHeight=800)
    {
        FTFShaftLayout Result;
        Result.Height=FMath::Clamp(BuiltHeight,800.f,25600.f);
        for(float H:Stops)if(FMath::IsFinite(H)&&H>=0&&H<=TF::MaxTravel)Result.Height=FMath::Max(Result.Height,H+800);
        // Use the finest stop grid, merging adjacent slices into full meshes where possible.
        auto AddSlice = [](TArray<FVector2D>& Spans, float Z, float MaxLength)
        {
            if (!Spans.IsEmpty() && Spans.Last().X + Spans.Last().Y == Z && Spans.Last().Y < MaxLength)
                Spans.Last().Y += 100;
            else
                Spans.Emplace(Z, 100);
        };
        auto Covered = [](float Mid, const TArray<float>& Starts, float Length)
        {
            for (float H : Starts)
                if (Mid >= H && Mid < H + Length) return true;
            return false;
        };
        for (float Z = 800; Z < Result.Height; Z += 100)
        {
            if (Covered(Z + 50, ManualShafts, 400) || Covered(Z + 50, Stops, 600)) continue;
            AddSlice(Result.Frames, Z, 400);
            if (Z < Result.Height - 200) AddSlice(Result.Screens, Z, 200);
        }
        return Result;
    }
};
