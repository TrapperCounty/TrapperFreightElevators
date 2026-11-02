#pragma once
#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"
#include "FGBuildableSubsystem.h"
#include "FGPipeConnectionFactory.h"
#include "TFTypes.h"
#include "TFShaftLayout.h"
#include "TFBuildables.generated.h"
class UStaticMeshComponent;
class UTextRenderComponent;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UFGPowerConnectionComponent;
class UFGPowerInfoComponent;
class ATFDrive;

UCLASS()
class TRAPPERFREIGHTELEVATORS_API UTFPipePort : public UFGPipeConnectionFactory
{
    GENERATED_BODY()
public:
    UTFPipePort();
    float Content() const { return mFluidBox.Content; }
    float Take(float Amount);
    float Give(float Amount);
    void Configure(bool Input);
};

/** Stable native components are serialized by the game and copied to holograms. */
UCLASS(Abstract)
class TRAPPERFREIGHTELEVATORS_API ATFPart : public AFGBuildable, public IFGFactoryTickHandlerInterface
{
    GENERATED_BODY()
public:
    ATFPart();
    UPROPERTY(EditDefaultsOnly) int32 Kind = 2; // drive, stop, shaft
    UPROPERTY(EditDefaultsOnly,SaveGame,Replicated) int32 Tier = 1;
    UPROPERTY(EditDefaultsOnly) bool IsFluid = false;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> FreightRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Display;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> FreightCollision;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> FreightSelection;
    void ConfigureCollision(float Height);
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float Delta) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PostSerializedFromBlueprint(bool IsBlueprintWorld = false) override;
    virtual void UpdateUseState_Implementation(AFGCharacterPlayer*,const FVector&,UPrimitiveComponent*,FUseState&) override;
    virtual void OnUse_Implementation(AFGCharacterPlayer*,const FUseState&) override;
    virtual bool IsUseable_Implementation() const override { return Kind != 2; }
    virtual FText GetLookAtDecription_Implementation(AFGCharacterPlayer*,const FUseState&) const override;
    virtual bool DoesContainLightweightInstances_Native() const override { return false; }
    virtual bool SupportsDismantleDisqualifiers_Implementation() const override { return true; }
    virtual void GetDismantleDisqualifiers_Implementation(TArray<TSubclassOf<UFGConstructDisqualifier>>& Out,const TArray<AActor*>& Selected) const override;
    ATFDrive* Drive() const;
    UStaticMeshComponent* Box(FName Name,FVector Position,FVector Size,const TCHAR* Material,bool Collision = true,USceneComponent* Parent = nullptr);
    UStaticMeshComponent* Cylinder(FName Name,FVector Position,FVector Size,const TCHAR* Material,USceneComponent* Parent = nullptr);
    UStaticMeshComponent* Ironworks(FName Name,const TCHAR* Asset,USceneComponent* Parent = nullptr);
    void HideLegacyArt();
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Art;
};

UCLASS()
class TRAPPERFREIGHTELEVATORS_API ATFShaft : public ATFPart
{
    GENERATED_BODY()
public:
    ATFShaft();
    virtual void Tick(float Delta) override;
    virtual bool CanDismantle_Implementation() const override;
    virtual void GetDismantleDependencies_Implementation(TArray<AActor*>& Out) const override;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Screens;
};

UCLASS(Abstract)
class TRAPPERFREIGHTELEVATORS_API ATFStop : public ATFPart
{
    GENERATED_BODY()
public:
    ATFStop();
    UPROPERTY(SaveGame,Replicated) TObjectPtr<ATFDrive> Elevator;
    UPROPERTY(SaveGame,Replicated) FString FloorName;
    UPROPERTY(SaveGame,Replicated) bool Enabled = true;
    UPROPERTY(SaveGame,Replicated) bool WaitFull = false;
    UPROPERTY(SaveGame,Replicated) float MaxWait = 30;
    UPROPERTY(SaveGame,ReplicatedUsing=OnRep_Config) TArray<FTFPortConfig> Ports;
    UPROPERTY(SaveGame,Replicated) TArray<FTFTank> Tanks;
    UPROPERTY(SaveGame,Replicated) TArray<FTFStackReservation> IncomingReservations;
    UPROPERTY(SaveGame,Replicated) TArray<FTFStackReservation> OutgoingReservations;
    UPROPERTY(VisibleAnywhere,SaveGame) TObjectPtr<UFGInventoryComponent> Incoming;
    UPROPERTY(VisibleAnywhere,SaveGame) TObjectPtr<UFGInventoryComponent> Outgoing;
    UPROPERTY(Replicated) TArray<FTFInventorySlot> IncomingView;
    UPROPERTY(Replicated) TArray<FTFInventorySlot> OutgoingView;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UFGFactoryConnectionComponent>> Belts;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UTFPipePort>> Pipes;
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual void PostFactoryTick(AFGBuildableSubsystem* Subsystem,float Delta) override;
    virtual bool Factory_PeekOutput_Implementation(const UFGFactoryConnectionComponent*,TArray<FInventoryItem>&,TSubclassOf<UFGItemDescriptor>) const override;
    virtual bool Factory_GrabOutput_Implementation(UFGFactoryConnectionComponent*,FInventoryItem&,float&,TSubclassOf<UFGItemDescriptor>) override;
    virtual bool CanDismantle_Implementation() const override;
    virtual uint8 MaxNumGrab(float Delta) const override {return IsFluid?0:255;}
    virtual uint8 EstimatedMaxNumGrab_Threadsafe(float Delta) const override {return IsFluid?0:255;}
    UFUNCTION() void OnRep_Config();
    void ApplyConfig();
    void Link();
    bool HasContents() const;
    float Height() const;
    FString Label() const;
    float FluidFree(int32 Tank,bool Input) const;
    float ItemFree(TSubclassOf<UFGItemDescriptor> Resource,bool IncludeReservations = true) const;
    float Reserved(TSubclassOf<UFGItemDescriptor> Resource = nullptr,int32 Tank = INDEX_NONE) const;
    TSubclassOf<UFGItemDescriptor> AcceptFilter(const UFGFactoryConnectionComponent* Port) const;
    double WaitingSince = 0;
    double ReceivedItems = 0, ReceivedFluid = 0;
    bool PendingFlush = false;
    float ScanTimer = 0;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Gates;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> BufferHousing;
    UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> PortNumbers;
    UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> PortDirections;
    int32 MoveStorageStack(UFGInventoryComponent* PlayerInventory,bool Deposit,int32 Buffer,int32 Slot,TSubclassOf<UFGItemDescriptor> Expected);
    void UpdateStorageViews();
    bool SetIncomingReservation(TSubclassOf<UFGItemDescriptor> Resource,int32 Stacks);
    int32 ReservedIncomingStacks(TSubclassOf<UFGItemDescriptor> Resource=nullptr) const;
    bool SetStorageReservation(int32 Buffer,TSubclassOf<UFGItemDescriptor> Resource,int32 Stacks);
    int32 ReservedStorageStacks(int32 Buffer,TSubclassOf<UFGItemDescriptor> Resource=nullptr) const;
    int32 IncomingRoom(TSubclassOf<UFGItemDescriptor> Resource,bool HasItemState=false) const;
    bool CanReceiveIncoming(const FInventoryItem& Item) const;
    float GateOpen = 0;
};

UCLASS()
class TRAPPERFREIGHTELEVATORS_API ATFItemStop : public ATFStop
{
    GENERATED_BODY()
public: ATFItemStop();
};

UCLASS()
class TRAPPERFREIGHTELEVATORS_API ATFFluidStop : public ATFStop
{
    GENERATED_BODY()
public: ATFFluidStop();
};

UCLASS()
class TRAPPERFREIGHTELEVATORS_API ATFDrive : public ATFPart
{
    GENERATED_BODY()
public:
    ATFDrive();
    UPROPERTY(SaveGame,Replicated) bool ShaftBuiltFirst = false;
    UPROPERTY(SaveGame,Replicated) float PreservedShaftHeight = 800;
    float ShaftTop(const ATFShaft* Ignore = nullptr) const;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Cabin;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UFGPowerConnectionComponent> Power;
    UPROPERTY(VisibleAnywhere,SaveGame) TObjectPtr<UFGPowerInfoComponent> PowerInfo;
    UPROPERTY(VisibleAnywhere,SaveGame) TObjectPtr<UFGInventoryComponent> CabinInventory;
    UPROPERTY(SaveGame,Replicated) float CabinHeight = 0;
    UPROPERTY(SaveGame,Replicated) TObjectPtr<ATFStop> Target;
    UPROPERTY(SaveGame,Replicated) TObjectPtr<ATFStop> Docked;
    UPROPERTY(SaveGame,Replicated) TObjectPtr<ATFStop> GateStop;
    UPROPERTY(SaveGame,Replicated) float DoorPhase = 0;
    UPROPERTY(SaveGame) float TravelStart = 0;
    UPROPERTY(SaveGame) float TravelElapsed = 0;
    UPROPERTY(SaveGame) bool TravelActive = false;
    UPROPERTY(SaveGame,Replicated) int32 Direction = 1;
    UPROPERTY(SaveGame,Replicated) bool Paused = false;
    UPROPERTY(SaveGame,Replicated) int32 Compartments = 1;
    UPROPERTY(SaveGame,Replicated) TArray<TSubclassOf<UFGItemDescriptor>> TankResources;
    UPROPERTY(SaveGame,Replicated) TArray<FTFRoute> Routes;
    UPROPERTY(SaveGame,Replicated) TArray<FTFCargo> Cargo;
    UPROPERTY(Replicated) TArray<TObjectPtr<ATFStop>> Stops;
    UPROPERTY(Replicated) FString Status;
    UPROPERTY(Replicated) bool Powered = false;
    UPROPERTY(SaveGame) TArray<FInventoryStack> Snapshot;
    UPROPERTY(SaveGame) TArray<float> FluidSnapshot;
    UPROPERTY(SaveGame) float Dwell = 0;
    UPROPERTY(SaveGame) bool DepartRequested = false;
    UPROPERTY(SaveGame) float SweepEnd = 0;
    UPROPERTY(SaveGame) bool HasSweep = false;
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual void PreFactoryTick(AFGBuildableSubsystem* Subsystem,float Delta) override;
    virtual void PostFactoryTick(AFGBuildableSubsystem* Subsystem,float Delta) override;
    virtual bool CanDismantle_Implementation() const override;
    virtual void Upgrade_Implementation(AActor* NewActor) override;
    virtual void GetDismantleDependencies_Implementation(TArray<AActor*>& Out) const override;
    void RefreshStops();
    void RefreshAutoShaft();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> AutoShaftFrames;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> AutoShaftScreens;
    FTFShaftLayout AutoShaftLayout;
    float AutoShaftTimer = 0;
    bool CanAttach(const FVector& Position,const ATFStop* Ignore = nullptr) const;
    void Arrive(ATFStop* Stop);
    ATFStop* NextStop();
    bool HasPickup(ATFStop* Stop) const;
    int32 ChooseRoute(TSubclassOf<UFGItemDescriptor> Resource,ATFStop* Source,float& Room,int32 Compartment = INDEX_NONE) const;
    int32 TankFor(TSubclassOf<UFGItemDescriptor> Resource) const;
    float CargoAmount(int32 Tank = INDEX_NONE) const;
    int32 CargoSlots() const;
    float CabinRoom(TSubclassOf<UFGItemDescriptor> Resource,int32 Tank) const;
    bool AtCapacity() const;
    bool Unload(float& Budget);
    bool Load(float& Budget);
    bool HasLoadingWork() const;
    bool SafeCompartments(int32 Count);
    bool SetTankResource(int32 Index,TSubclassOf<UFGItemDescriptor> Resource);
    void UpdateDisplay();
    float DiscoveryTimer = 0, VisualTimer = 0, TransferCredit = 0;
    bool QABypassPower = false; // Only set by explicit opt-in QA fixtures.
    UPROPERTY() TObjectPtr<UStaticMeshComponent> HoistHead;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> HoistDrum;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LiftCables;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Gates;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Screens;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> PressureTanks;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> FreightCrates;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> TankLevels;
    TArray<float> VisualTankFill;
    float GateOpen = 0;
};
