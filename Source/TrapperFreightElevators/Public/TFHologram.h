#pragma once
#include "CoreMinimal.h"
#include "Hologram/FGBuildableHologram.h"
#include "FGConstructDisqualifier.h"
#include "TFHologram.generated.h"
UCLASS() class TRAPPERFREIGHTELEVATORS_API UTFPlacementError : public UFGConstructDisqualifier
{
    GENERATED_BODY()
public: UTFPlacementError() { mDisqfualifyingText=FText::FromString(TEXT("Build the shaft first. Snap extensions to its top; stops need 8m headroom, a whole-metre height and 8m spacing. Maximum tower height: 256m.")); }
};
UCLASS() class TRAPPERFREIGHTELEVATORS_API UTFDismantleCargo : public UFGConstructDisqualifier
{
    GENERATED_BODY()
public: UTFDismantleCargo() { mDisqfualifyingText=FText::FromString(TEXT("Unload the elevator's onboard cargo before dismantling its base.")); }
};
UCLASS() class TRAPPERFREIGHTELEVATORS_API UTFDismantleStorage : public UFGConstructDisqualifier
{
    GENERATED_BODY()
public: UTFDismantleStorage() { mDisqfualifyingText=FText::FromString(TEXT("Empty this floor stop's item storage and fluid tanks before dismantling it.")); }
};
UCLASS() class TRAPPERFREIGHTELEVATORS_API UTFDismantleStops : public UFGConstructDisqualifier
{
    GENERATED_BODY()
public: UTFDismantleStops() { mDisqfualifyingText=FText::FromString(TEXT("Select or remove the attached floor stops first. The ground-floor stop is separate from the base.")); }
};
UCLASS() class TRAPPERFREIGHTELEVATORS_API UTFDismantleShafts : public UFGConstructDisqualifier
{
    GENERATED_BODY()
public: UTFDismantleShafts() { mDisqfualifyingText=FText::FromString(TEXT("Select or remove the shaft sections above this part first.")); }
};
UCLASS() class TRAPPERFREIGHTELEVATORS_API ATFHologram : public AFGBuildableHologram
{
    GENERATED_BODY()
public:
    ATFHologram();
    UPROPERTY(Replicated) int32 ShaftSections = 1;
    UPROPERTY(Replicated) bool ShaftPositionLocked = false;
    static constexpr int32 MaxSectionsPerBuild = 10;
    bool IsShaft() const;
    virtual float StopSnapStep() const { return 100.f; }
    virtual bool IsValidHitResult(const FHitResult& Hit) const override;
    class ATFDrive* ShaftDrive() const;
    virtual void SetShaftSections(int32 Count);
    virtual void Scroll(int32 Delta) override;
    virtual void ScrollRotate(int32 Delta,int32 Step) override;
    virtual bool DoMultiStepPlacement(bool IsRelease) override;
    virtual bool HandleReset() override;
    virtual bool CanBeZooped() const override;
    virtual bool IsInZoopBuildMode() const override;
    virtual bool TryZoop(const FHitResult& Hit) override;
    virtual void CreateZoopInstances(const FIntVector& Desired) override;
    virtual void SetZoopFromHitresult(const FHitResult& Hit) override;
    virtual int32 GetBaseCostMultiplier() const override;
    virtual AActor* Construct(TArray<AActor*>& OutChildren,FNetConstructionID ID) override;
    UPROPERTY(Replicated) TObjectPtr<class ATFDrive> UpgradeTarget;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual bool TryUpgrade(const FHitResult& Hit) override;
    virtual AActor* GetUpgradedActor() const override;
    virtual bool TrySnapToActor(const FHitResult& Hit) override;
    virtual void SetHologramLocationAndRotation(const FHitResult& Hit) override;
protected:
    virtual void SerializeConstructMessage(FArchive& Ar,FNetConstructionID ID) override;
    virtual void CheckValidPlacement() override;
    virtual bool UsesShaftSizing() const { return IsShaft(); }
    virtual int32 MinimumSections() const { return 1; }
    // Aim height at the tower centre line, measured relative to the first anchored ray.
    TOptional<double> AimOriginZ;
    double LastAimZ = 0;
    int32 AimOriginCount = 1;
    virtual void ConfigureActor(AFGBuildable* Buildable) const override;
    virtual TSubclassOf<UFGConstructDisqualifier> GetConstructDisqualifierFromClearanceOverlap(const EClearanceOverlapResult&,AActor*) const override;
};

UCLASS()
class TRAPPERFREIGHTELEVATORS_API ATFShaftHologram : public ATFHologram
{
    GENERATED_BODY()
public:
    ATFShaftHologram();
    virtual void GetSupportedBuildModes_Implementation(TArray<TSubclassOf<UFGBuildGunModeDescriptor>>& Modes) const override;
};

UCLASS()
class TRAPPERFREIGHTELEVATORS_API UTFStopSnap1m : public UFGHologramBuildModeDescriptor
{
    GENERATED_BODY()
public:
    UTFStopSnap1m() { mDisplayName = NSLOCTEXT("Freight", "Snap1", "1 m snap"); }
};
UCLASS()
class TRAPPERFREIGHTELEVATORS_API UTFStopSnap2m : public UFGHologramBuildModeDescriptor
{
    GENERATED_BODY()
public:
    UTFStopSnap2m() { mDisplayName = NSLOCTEXT("Freight", "Snap2", "2 m snap"); }
};
UCLASS()
class TRAPPERFREIGHTELEVATORS_API UTFStopSnap4m : public UFGHologramBuildModeDescriptor
{
    GENERATED_BODY()
public:
    UTFStopSnap4m() { mDisplayName = NSLOCTEXT("Freight", "Snap4", "4 m snap"); }
};

UCLASS()
class TRAPPERFREIGHTELEVATORS_API ATFStopHologram : public ATFHologram
{
    GENERATED_BODY()
public:
    ATFStopHologram();
    virtual void GetSupportedBuildModes_Implementation(TArray<TSubclassOf<UFGBuildGunModeDescriptor>>& Modes) const override;
    virtual float StopSnapStep() const override;
};

/** Drive recipe with an optional, separately costed shaft preview. */
UCLASS()
class TRAPPERFREIGHTELEVATORS_API ATFTowerHologram : public ATFShaftHologram
{
    GENERATED_BODY()
public:
    ATFTowerHologram();
    virtual bool CanBeZooped() const override { return true; }
    virtual bool IsInZoopBuildMode() const override { return !IsUpgrade(); }
    virtual void SpawnChildren(AActor* HologramOwner, FVector Location, APawn* InstigatorPawn) override;
    virtual bool IsValidHitResult(const FHitResult& Hit) const override;
    virtual bool TryUpgrade(const FHitResult& Hit) override;
    virtual bool TrySnapToActor(const FHitResult& Hit) override;
    virtual void SetHologramLocationAndRotation(const FHitResult& Hit) override;
    virtual void PostHologramPlacement(const FHitResult& Hit, bool ChildrenToo) override;
    virtual void SetShaftSections(int32 Count) override;
    virtual void Scroll(int32 Delta) override;
    virtual void ScrollRotate(int32 Delta,int32 Step) override;
    virtual TArray<FItemAmount> GetBaseCost() const override;
    virtual TArray<FItemAmount> GetCost(bool IncludeChildren) const override;
    virtual AActor* Construct(TArray<AActor*>& ChildrenBuilt, FNetConstructionID ID) override;
    ATFHologram* ShaftPreview() const;
    bool SupportsNewShaft(const ATFHologram* Shaft) const;

protected:
    virtual bool UsesShaftSizing() const override { return !IsUpgrade(); }
    virtual int32 MinimumSections() const override { return Extending ? 1 : 0; }
    virtual void CheckValidPlacement() override;
    virtual void CheckClearance() override;
    virtual void SerializeConstructMessage(FArchive& Ar, FNetConstructionID ID) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Props) const override;
private:
    UPROPERTY(Replicated) TObjectPtr<class ATFDrive> Extending;
    TArray<TWeakObjectPtr<UStaticMeshComponent>> HiddenDriveMeshes;
    void SetTarget(ATFDrive* Drive);
    void SyncPreview();
    bool LayoutIsValid() const;
};
