#include "TFBuildingSnap.h"
#include "TFBuildables.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Buildables/FGBuildableWall.h"
#include "Hologram/FGFoundationHologram.h"
#include "Hologram/FGWallHologram.h"
#include "Patching/NativeHookManager.h"

namespace
{
    // Expose base-class member pointers without downcasting native hologram instances.
    struct FBuildingAccess : AFGBuildableHologram
    {
        static auto Building(){return &FBuildingAccess::mSnappedBuilding;}
        static auto Floor(){return &FBuildingAccess::mSnappedFloor;}
        static auto Wall(){return &FBuildingAccess::mSnappedWall;}
        static auto Direction(){return &FBuildingAccess::mWorldSpaceSnapDirection;}
        static auto Changed(){return &FBuildingAccess::SetIsChanged;}
    };
    struct FFoundationAccess : AFGFoundationHologram
    {
        static auto Vertical(){return &FFoundationAccess::mIsSnappedVertically;}
    };
    FDelegateHandle FoundationHook,WallHook,HitHook;

    bool Supported(const AFGBuildableHologram* H,FVector& Size,bool& Wall)
    {
        if(!H||!H->GetBuildClass())return false;
        const auto* Defaults=H->GetBuildClass()->GetDefaultObject();
        if(Cast<AFGFoundationHologram>(H))if(const auto* F=Cast<AFGBuildableFoundation>(Defaults))
        {
            if(!FMath::IsNearlyZero(F->mElevation))return false;
            Size=FVector(F->mWidth,F->mDepth,F->mHeight);Wall=false;return true;
        }
        if(Cast<AFGWallHologram>(H))if(const auto* W=Cast<AFGBuildableWall>(Defaults))
        {
            if(!FMath::IsNearlyZero(W->mElevation)||!FMath::IsNearlyZero(W->mAngularDepth)||!FMath::IsNearlyEqual(W->mWidth,800.f))return false;
            Size=FVector(W->mWidth,1,W->mHeight);Wall=true;return true;
        }
        return false;
    }
    void ClearFreightSnap(AFGBuildableHologram* H)
    {
        if(Cast<ATFPart>(H->GetSnappedBuilding()))
        {
            H->*FBuildingAccess::Building()=nullptr;
            H->*FBuildingAccess::Floor()=nullptr;
            H->*FBuildingAccess::Wall()=nullptr;
            H->*FBuildingAccess::Direction()=FVector::ZeroVector;
        }
    }
}

bool TFTryBuildingSnap(AFGBuildableHologram* H,const FHitResult& Hit)
{
    auto* Part=Cast<ATFPart>(Hit.GetActor());
    FVector Size;bool Wall=false;
    if(!Hit.bBlockingHit||!IsValid(Part)||Part->IsActorBeingDestroyed()||!Supported(H,Size,Wall)||H->IsHologramLocked())return false;
    auto* Drive=Part->Drive();
    if(!IsValid(Drive)||Part->GetBlueprintDesigner()!=H->GetBlueprintDesigner())return false;
    TArray<float> Landings;
    for(auto Stop:Drive->Stops)if(IsValid(Stop)&&!Stop->IsActorBeingDestroyed())Landings.Add(Stop->Height());
    FTransform Placement;
    if(!FTFBuildingSnap::Solve(Drive->GetActorTransform(),Drive->ShaftTop(),Landings,Hit.ImpactPoint,Hit.ImpactNormal,Wall,Size,H->GetScrollRotateValue(),Placement))return false;
    H->SetActorLocationAndRotation(Placement.GetLocation(),Placement.GetRotation());
    H->*FBuildingAccess::Building()=Part;
    H->*FBuildingAccess::Floor()=Wall?nullptr:Part;
    H->*FBuildingAccess::Wall()=Wall?Part:nullptr;
    H->*FBuildingAccess::Direction()=(Drive->GetActorLocation()-H->GetActorLocation()).GetSafeNormal2D();
    if(auto* Foundation=Cast<AFGFoundationHologram>(H))Foundation->*FFoundationAccess::Vertical()=false;
    (H->*FBuildingAccess::Changed())(true);
    return true;
}

void TFInstallBuildingSnaps()
{
#if !WITH_EDITOR
    FoundationHook=SUBSCRIBE_UOBJECT_METHOD(AFGFoundationHologram,TrySnapToActor,[](auto& Scope,AFGFoundationHologram* H,const FHitResult& Hit)
    {
        ClearFreightSnap(H);Scope(H,Hit);
        if(TFTryBuildingSnap(H,Hit))Scope.Override(true);
    });
    WallHook=SUBSCRIBE_UOBJECT_METHOD(AFGWallHologram,TrySnapToActor,[](auto& Scope,AFGWallHologram* H,const FHitResult& Hit)
    {
        ClearFreightSnap(H);Scope(H,Hit);
        if(TFTryBuildingSnap(H,Hit))Scope.Override(true);
    });
    HitHook=SUBSCRIBE_UOBJECT_METHOD(AFGBuildableHologram,IsValidHitResult,[](auto& Scope,const AFGBuildableHologram* H,const FHitResult& Hit)
    {
        FVector Size;bool Wall=false;
        if(Hit.bBlockingHit&&Cast<ATFPart>(Hit.GetActor())&&Supported(H,Size,Wall))Scope.Override(true);
    });
#endif
}
void TFRemoveBuildingSnaps()
{
#if !WITH_EDITOR
    UNSUBSCRIBE_UOBJECT_METHOD(AFGFoundationHologram,TrySnapToActor,FoundationHook);
    UNSUBSCRIBE_UOBJECT_METHOD(AFGWallHologram,TrySnapToActor,WallHook);
    UNSUBSCRIBE_UOBJECT_METHOD(AFGBuildableHologram,IsValidHitResult,HitHook);
#endif
}
