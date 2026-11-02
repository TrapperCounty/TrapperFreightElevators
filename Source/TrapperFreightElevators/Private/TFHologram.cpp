#include "TFHologram.h"
#include "TFBuildables.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

ATFHologram::ATFHologram() = default;

ATFShaftHologram::ATFShaftHologram()
{
    static ConstructorHelpers::FClassFinder<UFGHologramBuildModeDescriptor> Zoop(
        TEXT("/Game/FactoryGame/Buildable/Factory/-Shared/BuildGunModes/BuildMode_Zoop"));
    mBuildModeZoop = Zoop.Class;
    mDefaultBuildMode = Zoop.Class;
    mMaxZoopAmount = MaxSectionsPerBuild - 1;
    mUseDefaultZoopImplementation = false;
}

void ATFShaftHologram::GetSupportedBuildModes_Implementation(TArray<TSubclassOf<UFGBuildGunModeDescriptor>>& Modes) const
{
    Modes.AddUnique(mBuildModeZoop);
}

ATFStopHologram::ATFStopHologram()
{
    mDefaultBuildMode = UTFStopSnap1m::StaticClass();
}

void ATFStopHologram::GetSupportedBuildModes_Implementation(TArray<TSubclassOf<UFGBuildGunModeDescriptor>>& Modes) const
{
    Modes.AddUnique(UTFStopSnap1m::StaticClass());
    Modes.AddUnique(UTFStopSnap2m::StaticClass());
    Modes.AddUnique(UTFStopSnap4m::StaticClass());
}

float ATFStopHologram::StopSnapStep() const
{
    if (IsCurrentBuildMode(UTFStopSnap4m::StaticClass())) return 400.f;
    if (IsCurrentBuildMode(UTFStopSnap2m::StaticClass())) return 200.f;
    return 100.f;
}
bool ATFHologram::IsShaft() const {return GetBuildClass()&&GetBuildClass()->IsChildOf(ATFShaft::StaticClass());}
bool ATFHologram::IsValidHitResult(const FHitResult& Hit) const
{
    const auto* Defaults=GetBuildClass()?Cast<ATFPart>(GetBuildClass()->GetDefaultObject()):nullptr;
    if(Defaults&&Defaults->Kind!=0)
    {
        if(IsShaft()&&ShaftPositionLocked)return true;
        if(Hit.bBlockingHit&&Cast<ATFPart>(Hit.GetActor()))return true;
    }
    return Super::IsValidHitResult(Hit);
}
ATFDrive* ATFHologram::ShaftDrive() const
{
    for(TActorIterator<ATFDrive> It(GetWorld());It;++It)
    {
        if(It->IsActorBeingDestroyed())continue;
        const FVector P=It->GetActorTransform().InverseTransformPosition(GetActorLocation());
        if(FMath::Abs(P.X)<2&&FMath::Abs(P.Y)<2&&FMath::Abs(P.Z-It->ShaftTop())<1)return *It;
    }
    return nullptr;
}
void ATFHologram::SetShaftSections(int32 Count)
{
    int32 Limit = MaxSectionsPerBuild;
    if (const auto* Tower = ShaftDrive())
        Limit = FMath::Clamp(FMath::FloorToInt((25600 - Tower->ShaftTop()) / TF::Section), 1, Limit);
    const int32 Selected = FMath::Clamp(Count, 1, Limit);
    if (ShaftSections == Selected && GetZoopInstanceTransforms().Num() == Selected - 1) return;
    ShaftSections = Selected;
    SetZoopAmount(FIntVector(0, 0, ShaftSections - 1));
    RegenerateZoopInstances();
    SetIsChanged(true);
}

void ATFHologram::Scroll(int32 Delta)
{
    if (!UsesShaftSizing()) { Super::Scroll(Delta); return; }
    SetShaftSections(ShaftSections + Delta);
    if (AimOriginZ.IsSet()) AimOriginZ = LastAimZ;
    AimOriginCount = ShaftSections;
}

void ATFHologram::ScrollRotate(int32 Delta, int32 Step)
{
    if (UsesShaftSizing()) Scroll(Delta);
    else Super::ScrollRotate(Delta, Step);
}
bool ATFHologram::CanBeZooped() const{return IsShaft()||Super::CanBeZooped();}
bool ATFHologram::IsInZoopBuildMode() const{return IsShaft()||Super::IsInZoopBuildMode();}
bool ATFHologram::TryZoop(const FHitResult& Hit)
{
    if (!UsesShaftSizing()) return Super::TryZoop(Hit);
    if (ShaftPositionLocked) SetZoopFromHitresult(Hit);
    return ShaftPositionLocked;
}

void ATFHologram::SetZoopFromHitresult(const FHitResult& Hit)
{
    if (!UsesShaftSizing()) { Super::SetZoopFromHitresult(Hit); return; }
    if (!ShaftPositionLocked) return;
    // Project the sight line onto the vertical plane through the tower. Aiming
    // at sky still gives a usable ray when the preview extends above the roof.
    const FVector Direction = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal();
    const FVector ToTower = GetActorLocation() - Hit.TraceStart;
    const double Horizontal = Direction.SizeSquared2D();
    if (Horizontal < UE_SMALL_NUMBER) return;
    const double Distance = (ToTower.X * Direction.X + ToTower.Y * Direction.Y) / Horizontal;
    if (Distance <= 0) return;
    LastAimZ = Hit.TraceStart.Z + Distance * Direction.Z;
    if (!AimOriginZ.IsSet())
    {
        AimOriginZ = LastAimZ;
        AimOriginCount = ShaftSections;
    }
    const double Offset = FMath::Clamp((LastAimZ - AimOriginZ.GetValue()) / TF::Section,
        -double(MaxSectionsPerBuild), double(MaxSectionsPerBuild));
    const int32 Count = FMath::Clamp(AimOriginCount + FMath::RoundToInt(Offset), MinimumSections(), MaxSectionsPerBuild);
    if (Count != ShaftSections) SetShaftSections(Count);
}

void ATFHologram::CreateZoopInstances(const FIntVector& Desired)
{
    if(!IsShaft()){Super::CreateZoopInstances(Desired);return;}
    // Native construction always builds the root section; zoop contains only extras.
    for(int32 I=1;I<=FMath::Clamp(Desired.Z,0,MaxSectionsPerBuild-1);++I)GenerateZoopInstance(FTransform(FVector(0,0,I*400)));
}
int32 ATFHologram::GetBaseCostMultiplier() const{return IsShaft()?FMath::Clamp(ShaftSections,1,MaxSectionsPerBuild):Super::GetBaseCostMultiplier();}
bool ATFHologram::DoMultiStepPlacement(bool IsRelease)
{
    if(!UsesShaftSizing())return Super::DoMultiStepPlacement(IsRelease);
    if(IsRelease)return false;
    if(!ShaftPositionLocked){ShaftPositionLocked=true;AimOriginZ.Reset();SetShaftSections(ShaftSections);return false;}
    return true;
}
bool ATFHologram::HandleReset(){if(UsesShaftSizing()&&ShaftPositionLocked){ShaftPositionLocked=false;AimOriginZ.Reset();SetShaftSections(MinimumSections());return true;}return Super::HandleReset();}
AActor* ATFHologram::Construct(TArray<AActor*>& OutChildren,FNetConstructionID ID)
{
    if(IsShaft())
    {
        auto* D=ShaftDrive();if(!D||ShaftSections<1||ShaftSections>MaxSectionsPerBuild||D->ShaftTop()+ShaftSections*400>25600)return nullptr;
        SetShaftSections(ShaftSections);
    }
    if (GetBuildClass() && GetBuildClass()->IsChildOf(ATFStop::StaticClass()))
    {
        const auto* Stop = GetBuildClass()->GetDefaultObject<ATFStop>();
        bool Supported = false;
        for (TActorIterator<ATFDrive> It(GetWorld()); It; ++It)
            if (It->IsFluid == Stop->IsFluid && It->CanAttach(GetActorLocation())) { Supported = true; break; }
        if (!Supported) return nullptr;
    }
    return Super::Construct(OutChildren,ID);
}
void ATFHologram::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ATFHologram,UpgradeTarget);DOREPLIFETIME(ATFHologram,ShaftSections);DOREPLIFETIME(ATFHologram,ShaftPositionLocked);}
AActor* ATFHologram::GetUpgradedActor() const {return UpgradeTarget.Get();}
void ATFHologram::SerializeConstructMessage(FArchive& Ar,FNetConstructionID ID) {Super::SerializeConstructMessage(Ar,ID);Ar<<UpgradeTarget;Ar<<ShaftSections;Ar<<ShaftPositionLocked;if(Ar.IsLoading()&&IsShaft())SetShaftSections(ShaftSections);}
bool ATFHologram::TryUpgrade(const FHitResult& Hit)
{
    UpgradeTarget=nullptr;const auto* Defaults=Cast<ATFDrive>(GetBuildClass()->GetDefaultObject());auto* Existing=Cast<ATFDrive>(Hit.GetActor());
    if(Defaults && Existing && Defaults->IsFluid==Existing->IsFluid && Defaults->Tier>Existing->Tier)
    {UpgradeTarget=Existing;SetActorLocationAndRotation(Existing->GetActorLocation(),Existing->GetActorRotation());SetIsChanged(true);return true;}
    return false;
}

bool ATFHologram::TrySnapToActor(const FHitResult& Hit)
{
    if(IsShaft()&&ShaftPositionLocked)return true;
    const auto* Defaults=Cast<ATFPart>(GetBuildClass()->GetDefaultObject());
    auto* Part=Cast<ATFPart>(Hit.GetActor());
    if(Defaults && Part && Defaults->Kind!=0)
    {
        FVector P=Part->GetActorLocation();
        if(Defaults->Kind==2){auto* D=Part->Drive();if(!D)return false;P=D->GetActorLocation()+FVector(0,0,D->ShaftTop());}
        else {auto* D=Part->Drive();if(!D||D->IsFluid!=Defaults->IsFluid)return false;P=D->GetActorLocation();P.Z+=FMath::GridSnap(Hit.ImpactPoint.Z-P.Z,StopSnapStep());}
        SetActorLocationAndRotation(P,Part->GetActorRotation());SetIsChanged(true);return true;
    }
    return Super::TrySnapToActor(Hit);
}
void ATFHologram::SetHologramLocationAndRotation(const FHitResult& Hit)
{
    if(IsShaft()&&ShaftPositionLocked)return;
    Super::SetHologramLocationAndRotation(Hit);
    const auto* Defaults=Cast<ATFPart>(GetBuildClass()->GetDefaultObject());
    if(!Defaults || Defaults->Kind==0) return;
    ATFDrive* Best=nullptr; float Distance=600.f;
    for(TActorIterator<ATFDrive> It(GetWorld());It;++It)
    {
        if(Defaults->Kind==1 && Defaults->IsFluid!=It->IsFluid) continue;
        const float D=FVector::Dist2D(GetActorLocation(),It->GetActorLocation());
        if(D<Distance) { Best=*It;Distance=D; }
    }
    if(Best)
    {
        FVector P=Best->GetActorLocation();P.Z+=Defaults->Kind==2?Best->ShaftTop():FMath::GridSnap(GetActorLocation().Z-P.Z,StopSnapStep());
        SetActorLocationAndRotation(P,Best->GetActorRotation());SetIsChanged(true);
        if(IsShaft())SetShaftSections(ShaftSections);
    }
}
void ATFHologram::CheckValidPlacement()
{
    const auto* Defaults=Cast<ATFPart>(GetBuildClass()->GetDefaultObject());
    // Shafts and landings are supported by the validated tower, not a floor below.
    if(Defaults&&Defaults->Kind!=0)SetNeedsValidFloor(false);
    Super::CheckValidPlacement();
    if(!Defaults || Defaults->Kind==0) return;
    if(Defaults->Kind==1)
    {
        bool Valid=false;
        for(TActorIterator<ATFDrive> It(GetWorld());It;++It)
            if(It->IsFluid==Defaults->IsFluid && It->CanAttach(GetActorLocation())) {Valid=true;break;}
        if(!Valid) AddConstructDisqualifier(UTFPlacementError::StaticClass());
    }
    else
    {
        const auto* D=ShaftDrive();const auto* Parent=Cast<ATFTowerHologram>(GetParentHologram());
        const bool Valid=ShaftSections>=1 && ShaftSections<=MaxSectionsPerBuild &&
            ((D && D->ShaftTop()+ShaftSections*400<=25600) || (Parent && Parent->SupportsNewShaft(this)));
        if(!Valid) AddConstructDisqualifier(UTFPlacementError::StaticClass());
    }
}
void ATFHologram::ConfigureActor(AFGBuildable* Buildable) const
{
    Super::ConfigureActor(Buildable);
    if(auto* D=Cast<ATFDrive>(Buildable))D->ShaftBuiltFirst=true;
    if(auto* Stop=Cast<ATFStop>(Buildable))
        for(TActorIterator<ATFDrive> It(GetWorld());It;++It)
            if(It->IsFluid==Stop->IsFluid && It->CanAttach(Stop->GetActorLocation(),Stop)) {Stop->Elevator=*It;Stop->Tier=It->Tier;break;}
}
TSubclassOf<UFGConstructDisqualifier> ATFHologram::GetConstructDisqualifierFromClearanceOverlap(const EClearanceOverlapResult& Result,AActor* Other) const
{
    // Shafts, stops and the drive intentionally share the same modular envelope.
    if(auto* Part=Cast<ATFPart>(Other))
        if(FVector::DistSquared2D(Part->GetActorLocation(),GetActorLocation())<4 && !IsHologramIdenticalToActor(Other,GetActorTransform())) return nullptr;
    return Super::GetConstructDisqualifierFromClearanceOverlap(Result,Other);
}
