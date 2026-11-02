#include "TFHologram.h"
#include "TFBuildables.h"
#include "FGRecipe.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace
{
    const FName ShaftChildName(TEXT("TowerShaft"));
    constexpr float DriveHeight = 800.f;
    constexpr float MaxHeight = TF::MaxTravel + DriveHeight;
}

ATFTowerHologram::ATFTowerHologram()
{
    ShaftSections = 0;
}

void ATFTowerHologram::SpawnChildren(AActor* HologramOwner, FVector Location, APawn* InstigatorPawn)
{
    Super::SpawnChildren(HologramOwner, Location, InstigatorPawn);
    const auto Recipe = LoadClass<UFGRecipe>(nullptr, TEXT("/TrapperFreightElevators/Recipes/Recipe_Shaft.Recipe_Shaft_C"));
    if (Recipe && !ShaftPreview())
    {
        auto* Child = SpawnChildHologram<ATFHologram>(ShaftChildName, Recipe, HologramOwner, Location);
        Child->SetDisabled(true);
    }
}

ATFHologram* ATFTowerHologram::ShaftPreview() const
{
    return Cast<ATFHologram>(FindChildHologramByName(ShaftChildName));
}

bool ATFTowerHologram::IsValidHitResult(const FHitResult& Hit) const
{
    if (ShaftPositionLocked) return true;
    if (Hit.bBlockingHit)
        if (const auto* Part = Cast<ATFPart>(Hit.GetActor()))
        {
            const auto* Drive = Part->Drive();
            return Drive && GetBuildClass()->GetDefaultObject<ATFDrive>()->IsFluid == Drive->IsFluid;
        }
    return Super::IsValidHitResult(Hit);
}

void ATFTowerHologram::SetTarget(ATFDrive* Drive)
{
    if (Extending == Drive) return;
    Extending = Drive;
    ShaftSections = MinimumSections();
    if (Drive)
    {
        TInlineComponentArray<UStaticMeshComponent*> Meshes(this);
        for (auto* Mesh : Meshes)
            if (Mesh->IsVisible())
            {
                HiddenDriveMeshes.Add(Mesh);
                Mesh->SetVisibility(false);
            }
    }
    else
    {
        for (const auto& Mesh : HiddenDriveMeshes)
            if (Mesh.IsValid()) Mesh->SetVisibility(true);
        HiddenDriveMeshes.Reset();
    }
    SetIsChanged(true);
}

bool ATFTowerHologram::TryUpgrade(const FHitResult& Hit)
{
    if (ShaftPositionLocked) return false;
    // Directly targeting the base with a higher tier still performs a drive upgrade.
    const auto* Base = Cast<ATFDrive>(Hit.GetActor());
    if (Base && Hit.ImpactPoint.Z < Base->GetActorLocation().Z + DriveHeight && Super::TryUpgrade(Hit))
    {
        SetTarget(nullptr);
        ShaftSections = 0;
        SyncPreview();
        return true;
    }
    UpgradeTarget = nullptr;
    return false;
}

bool ATFTowerHologram::TrySnapToActor(const FHitResult& Hit)
{
    if (ShaftPositionLocked) return true;
    const auto* Part = Cast<ATFPart>(Hit.GetActor());
    auto* Drive = Part ? Part->Drive() : nullptr;
    if (Drive && Drive->IsFluid == GetBuildClass()->GetDefaultObject<ATFDrive>()->IsFluid)
    {
        SetTarget(Drive);
        SetActorLocationAndRotation(Drive->GetActorLocation() + FVector::UpVector * Drive->ShaftTop(), Drive->GetActorRotation());
        SyncPreview();
        return true;
    }
    SetTarget(nullptr);
    return Super::TrySnapToActor(Hit);
}

void ATFTowerHologram::SetHologramLocationAndRotation(const FHitResult& Hit)
{
    if (ShaftPositionLocked) return;
    SetTarget(nullptr);
    Super::SetHologramLocationAndRotation(Hit);
    SyncPreview();
}

void ATFTowerHologram::PostHologramPlacement(const FHitResult& Hit, bool ChildrenToo)
{
    Super::PostHologramPlacement(Hit, ChildrenToo);
    SyncPreview();
}

void ATFTowerHologram::SetShaftSections(int32 Count)
{
    const int32 Limit = Extending ? FMath::Clamp(FMath::FloorToInt((MaxHeight-Extending->ShaftTop())/TF::Section),1,MaxSectionsPerBuild) : MaxSectionsPerBuild;
    ShaftSections = FMath::Clamp(Count, MinimumSections(), Limit);
    SyncPreview();
    SetIsChanged(true);
}

void ATFTowerHologram::SyncPreview()
{
    auto* Shaft = ShaftPreview();
    if (!Shaft) return;
    const bool Disabled = ShaftSections == 0 || IsUpgrade();
    if (Shaft->IsDisabled() != Disabled) Shaft->SetDisabled(Disabled);
    if (Disabled) return;
    const FVector Location = GetActorLocation() + FVector::UpVector * (Extending ? 0.f : DriveHeight);
    Shaft->SetActorLocationAndRotation(Location, GetActorRotation());
    Shaft->SetShaftSections(ShaftSections);
}

void ATFTowerHologram::Scroll(int32 Delta)
{
    if (ShaftPositionLocked) ATFHologram::Scroll(Delta);
    else AFGBuildableHologram::Scroll(Delta);
}

void ATFTowerHologram::ScrollRotate(int32 Delta, int32 Step)
{
    if (ShaftPositionLocked) ATFHologram::ScrollRotate(Delta, Step);
    else AFGBuildableHologram::ScrollRotate(Delta, Step);
}

bool ATFTowerHologram::SupportsNewShaft(const ATFHologram* Shaft) const
{
    return !Extending && !IsUpgrade() && Shaft && Shaft == ShaftPreview() &&
        Shaft->GetActorLocation().Equals(GetActorLocation() + FVector::UpVector * DriveHeight, 1.f);
}

bool ATFTowerHologram::LayoutIsValid() const
{
    if (ShaftSections < MinimumSections() || ShaftSections > MaxSectionsPerBuild) return false;
    if (ShaftSections && !ShaftPreview()) return false;
    if (Extending)
    {
        if (!IsValid(Extending) || Extending->IsActorBeingDestroyed() ||
            Extending->IsFluid != GetBuildClass()->GetDefaultObject<ATFDrive>()->IsFluid) return false;
        const float Top = Extending->ShaftTop();
        return Top + ShaftSections*TF::Section <= MaxHeight &&
            GetActorLocation().Equals(Extending->GetActorLocation() + FVector::UpVector*Top, 1.f);
    }
    if (IsUpgrade()) return ShaftSections == 0;
    // Two confirmations in the same space must never create coincident drives.
    for (TActorIterator<ATFDrive> It(GetWorld()); It; ++It)
    {
        if (It->IsActorBeingDestroyed() || FVector::DistSquared2D(GetActorLocation(),It->GetActorLocation())>4) continue;
        const double Z = GetActorLocation().Z-It->GetActorLocation().Z;
        if (Z <= It->ShaftTop()+1 && Z+DriveHeight+ShaftSections*TF::Section>=0) return false;
    }
    return true;
}

void ATFTowerHologram::CheckClearance()
{
    if (!Extending) Super::CheckClearance();
}

void ATFTowerHologram::CheckValidPlacement()
{
    SetNeedsValidFloor(!Extending && !IsUpgrade());
    Super::CheckValidPlacement();
    if (!LayoutIsValid()) AddConstructDisqualifier(UTFPlacementError::StaticClass());
}

TArray<FItemAmount> ATFTowerHologram::GetBaseCost() const
{
    return Extending ? TArray<FItemAmount>() : Super::GetBaseCost();
}

TArray<FItemAmount> ATFTowerHologram::GetCost(bool IncludeChildren) const
{
    if (Extending)
    {
        const auto* Shaft = ShaftPreview();
        return IncludeChildren && Shaft ? Shaft->GetCost(true) : TArray<FItemAmount>();
    }
    return Super::GetCost(IncludeChildren);
}

AActor* ATFTowerHologram::Construct(TArray<AActor*>& ChildrenBuilt, FNetConstructionID ID)
{
    if (!LayoutIsValid()) return nullptr;
    SyncPreview();
    if (Extending) return ShaftPreview()->Construct(ChildrenBuilt, ID);
    return Super::Construct(ChildrenBuilt, ID);
}

void ATFTowerHologram::SerializeConstructMessage(FArchive& Ar, FNetConstructionID ID)
{
    Super::SerializeConstructMessage(Ar, ID);
    Ar << Extending;
    if (Ar.IsLoading()) SyncPreview();
}

void ATFTowerHologram::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATFTowerHologram, Extending);
}
