#include "TFBuildables.h"
#include "TFHologram.h"
#include "TFPanel.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "FGPowerInfoComponent.h"
#include "FGPowerCircuit.h"
#include "FGGameState.h"
#include "FGPipeSubsystem.h"
#include "Buildables/FGBuildableWire.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    bool FreightPresent(const AFGBuildable* Part)
    {return IsValid(Part)&&!Part->IsActorBeingDestroyed()&&!Part->GetIsDismantled();}
    const TCHAR* Steel=TEXT("/TrapperFreightElevators/Art/M_Steel.M_Steel");
    const TCHAR* Orange=TEXT("/TrapperFreightElevators/Art/M_Orange.M_Orange");
    const TCHAR* Dark=TEXT("/TrapperFreightElevators/Art/M_Dark.M_Dark");
    const TCHAR* Light=TEXT("/TrapperFreightElevators/Art/M_Light.M_Light");
    FVector PortLocation(int32 I) { const FVector P[]={FVector(400,0,100),FVector(0,400,100),FVector(-400,0,100),FVector(0,-400,100)}; return P[I]; }
    bool ScreenAtStop(const ATFPart* Part,float Z)
    {
        const FVector Position=Part->GetActorTransform().TransformPosition(FVector(0,0,Z));
        if(auto* D=Part->Drive())
        {const float Top=D->GetActorLocation().Z+D->ShaftTop();if(Position.Z>Top-200)return true;}
        for(TActorIterator<ATFStop> It(Part->GetWorld());It;++It)
            if(FVector::DistSquared2D(It->GetActorLocation(),Position)<100 && Position.Z+100>It->GetActorLocation().Z && Position.Z-100<It->GetActorLocation().Z+600) return true;
        return false;
    }
    void AnimateGates(const TArray<TObjectPtr<UStaticMeshComponent>>& Gates,float Open,float RadialOffset,float WidthScale)
    {
        for(int32 I=0;I<Gates.Num();++I)if(Gates[I])
        {
            const FRotator R(0,(I/2)*90,0);const float Sign=(I%2)?1.f:-1.f;
            Gates[I]->SetRelativeLocation(R.RotateVector(FVector(RadialOffset,Sign*Open*95*WidthScale,0)));
        }
    }
}

UTFPipePort::UTFPipePort() { mFluidBoxVolume=5; mFluidBox.MaxContent=5; mFluidBox.MaxOverfillPct=.01f; }
float UTFPipePort::Take(float Amount) { const float N=FMath::Min(FMath::Max(0.f,Amount),FMath::Max(0.f,mFluidBox.Content)); mFluidBox.Content-=N; return N; }
float UTFPipePort::Give(float Amount) { const float N=FMath::Min(FMath::Max(0.f,Amount),FMath::Max(0.f,mFluidBox.MaxContent-mFluidBox.Content)); mFluidBox.Content+=N; return N; }
void UTFPipePort::Configure(bool Input) { SetPipeConnectionType(Input?EPipeConnectionType::PCT_CONSUMER:EPipeConnectionType::PCT_PRODUCER); mFluidBox.AddedPressure=Input?0.f:10.f; }

UStaticMeshComponent* ATFPart::Box(FName Name,FVector Position,FVector Size,const TCHAR* Material,bool Collision,USceneComponent* Parent)
{
    auto* C=CreateDefaultSubobject<UStaticMeshComponent>(Name);
    C->SetupAttachment(Parent?Parent:FreightRoot.Get());
    C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    C->SetRelativeLocation(Position); C->SetRelativeScale3D(Size/100.f);
    if (auto* M=LoadObject<UMaterialInterface>(nullptr,Material,nullptr,LOAD_NoWarn)) C->SetMaterial(0,M);
    C->SetMobility(EComponentMobility::Movable);
    C->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));
    Art.Add(C); return C;
}
UStaticMeshComponent* ATFPart::Cylinder(FName Name,FVector Position,FVector Size,const TCHAR* Material,USceneComponent* Parent)
{
    auto* C=Box(Name,Position,Size,Material,false,Parent);
    C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
    return C;
}
UStaticMeshComponent* ATFPart::Ironworks(FName Name,const TCHAR* Asset,USceneComponent* Parent)
{
    auto* C=CreateDefaultSubobject<UStaticMeshComponent>(Name);C->SetupAttachment(Parent?Parent:FreightRoot.Get());
    const FString Path=FString(TEXT("/TrapperFreightElevators/Art/Ironworks/SM_IW_"))+Asset+TEXT(".SM_IW_")+Asset;
    C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*Path,nullptr,LOAD_NoWarn));C->SetMobility(EComponentMobility::Movable);C->SetCollisionProfileName(TEXT("NoCollision"));
    C->SetCanEverAffectNavigation(false);Art.Add(C);return C;
}
void ATFPart::HideLegacyArt()
{
    for(auto C:Art)if(C&&!C->GetName().StartsWith(TEXT("IW_")))
    {
        C->SetVisibility(false);C->SetHiddenInGame(true);
        // Keep only the original solid deck, corner and docking collision shapes.
        const FString N=C->GetName();if(N!=TEXT("CabinDeck")&&!N.StartsWith(TEXT("BasePillar"))&&!N.StartsWith(TEXT("Column"))&&!N.StartsWith(TEXT("DockFrame"))&&N!=TEXT("FrontConsole")&&N!=TEXT("ControlPanel"))C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}
ATFPart::ATFPart()
{
    bReplicates=true; SetNetDormancy(DORM_Awake); PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickInterval=.05f; SetNetUpdateFrequency(8);
    FreightRoot=CreateDefaultSubobject<USceneComponent>(TEXT("FreightRoot")); SetRootComponent(FreightRoot);
    FreightCollision=CreateDefaultSubobject<UBoxComponent>(TEXT("FreightBodyCollision"));FreightCollision->SetupAttachment(FreightRoot);
    FreightCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);FreightCollision->SetCanEverAffectNavigation(false);FreightCollision->SetHiddenInGame(true);
    FreightSelection=CreateDefaultSubobject<UBoxComponent>(TEXT("FreightSelection"));FreightSelection->SetupAttachment(FreightRoot);
    FreightSelection->SetCollisionEnabled(ECollisionEnabled::NoCollision);FreightSelection->SetCanEverAffectNavigation(false);FreightSelection->SetHiddenInGame(true);
    mHologramClass=ATFHologram::StaticClass();
    Display=CreateDefaultSubobject<UTextRenderComponent>(TEXT("FreightDisplay")); Display->SetupAttachment(FreightRoot);
    Display->SetRelativeLocation(FVector(395,0,250)); Display->SetRelativeRotation(FRotator(0,0,0));
    Display->SetHorizontalAlignment(EHTA_Center); Display->SetWorldSize(25); Display->SetTextRenderColor(FColor(90,230,235));
    Display->SetText(FText::GetEmpty());
}
void ATFPart::ConfigureCollision(float Height)
{
    // Freight machinery occupies this volume even while its cargo gates are open.
    // Keep connection visibility traces unobstructed; the native build gun must
    // also be able to aim at the tower when adding shaft sections or landings.
    FreightCollision->SetRelativeLocation(FVector(0,0,Height*.5f));FreightCollision->SetBoxExtent(FVector(395,395,Height*.5f));
    FreightCollision->SetCollisionObjectType(ECC_WorldStatic);FreightCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    FreightCollision->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
    FreightCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    FreightCollision->SetGenerateOverlapEvents(false);
    // A drive's player barrier spans the tower, but must never intercept traces
    // aimed at separately built shaft sections. Legacy automatic shaft belongs
    // to the drive and remains selectable up to its preserved height.
    const auto* D=Cast<ATFDrive>(this);
    const float SelectionHeight=D?FMath::Clamp(D->PreservedShaftHeight,800.f,Height):Height;
    FreightSelection->SetRelativeLocation(FVector(0,0,SelectionHeight*.5f));
    FreightSelection->SetBoxExtent(FVector(395,395,SelectionHeight*.5f));
    FreightSelection->SetCollisionObjectType(ECC_WorldStatic);
    FreightSelection->SetCollisionResponseToAllChannels(ECR_Ignore);
    FreightSelection->SetCollisionResponseToChannel(ECC_GameTraceChannel5,ECR_Block);
    FreightSelection->SetCollisionEnabled(Kind==1?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryOnly);
    FreightSelection->SetGenerateOverlapEvents(false);
}
void ATFPart::BeginPlay() { Super::BeginPlay(); HideLegacyArt();ConfigureCollision(Kind==0?800.f:Kind==1?600.f:400.f); if(Kind==2&&Display)Display->SetVisibility(false); SetNetDormancy(DORM_Awake); SetActorTickEnabled(GetNetMode()!=NM_DedicatedServer); if(HasAuthority()&&Kind!=2)if(auto* BS=AFGBuildableSubsystem::Get(GetWorld()))BS->AddFactoryTickHandler(this); }
void ATFPart::EndPlay(const EEndPlayReason::Type Reason) {if(HasAuthority()&&Kind!=2)if(auto* BS=AFGBuildableSubsystem::Get(GetWorld()))BS->RemoveFactoryTickHandler(this);Super::EndPlay(Reason);}
void ATFPart::Tick(float Delta) { Super::Tick(Delta); }
void ATFPart::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ATFPart,Tier); }
void ATFPart::PostSerializedFromBlueprint(bool IsBlueprintWorld)
{
    Super::PostSerializedFromBlueprint(IsBlueprintWorld);
    // A blueprint carries configuration, never copies a live freight shipment.
    auto Empty=[](UFGInventoryComponent* Inventory){if(!Inventory)return;TArray<FInventoryStack> Stacks;Inventory->GetInventoryStacks(Stacks);for(const auto& S:Stacks)Inventory->Remove(S.Item.GetItemClass(),S.NumItems);};
    if(auto* D=Cast<ATFDrive>(this))
    {
        D->Cargo.Reset();Empty(D->CabinInventory);D->Target=nullptr;D->Docked=nullptr;D->CabinHeight=0;D->Snapshot.Reset();D->FluidSnapshot.Reset();D->HasSweep=false;D->DepartRequested=false;D->GateStop=nullptr;D->DoorPhase=0;D->TravelActive=false;D->TravelElapsed=0;
        for(auto& R:D->Routes) R.Delivered=0;
    }
    if(auto* S=Cast<ATFStop>(this))
    {
        Empty(S->Incoming);Empty(S->Outgoing);for(auto& T:S->Tanks){T.Input=0;T.Output=0;}for(auto P:S->Pipes)P->Take(P->Content());
        if(S->Elevator && S->Elevator->GetBlueprintDesigner()!=GetBlueprintDesigner()) S->Elevator=nullptr;
    }
}
void ATFPart::UpdateUseState_Implementation(AFGCharacterPlayer*,const FVector&,UPrimitiveComponent*,FUseState& State) { State.SetUseState(UFGUseState_Valid::StaticClass()); }
void ATFPart::OnUse_Implementation(AFGCharacterPlayer* Player,const FUseState&)
{
    UE_LOG(LogTemp,Display,TEXT("TFFreight panel interaction: %s local=%d"),*GetName(),Player&&Player->IsLocallyControlled());
    if (Player && Player->IsLocallyControlled()) if(auto* PC=Cast<AFGPlayerController>(Player->GetController())) UTFPanel::Open(PC,this);
}
FText ATFPart::GetLookAtDecription_Implementation(AFGCharacterPlayer*,const FUseState&) const { return FText::FromString(TEXT("Configure freight elevator")); }
ATFDrive* ATFPart::Drive() const
{
    if (auto* D=Cast<ATFDrive>(const_cast<ATFPart*>(this))) return D;
    if (auto* S=Cast<ATFStop>(const_cast<ATFPart*>(this))) return S->Elevator;
    for (TActorIterator<ATFDrive> It(GetWorld());It;++It)
        if (FVector::DistSquared2D(GetActorLocation(),It->GetActorLocation())<100 && GetActorLocation().Z>=It->GetActorLocation().Z-1 && GetActorLocation().Z-It->GetActorLocation().Z<=TF::MaxTravel) return *It;
    return nullptr;
}
void ATFPart::GetDismantleDisqualifiers_Implementation(TArray<TSubclassOf<UFGConstructDisqualifier>>& Out,const TArray<AActor*>& Selected) const
{
    Super::GetDismantleDisqualifiers_Implementation(Out,Selected);
    if(const auto* D=Cast<ATFDrive>(this))if(!D->Cargo.IsEmpty())Out.AddUnique(UTFDismantleCargo::StaticClass());
    if(const auto* S=Cast<ATFStop>(this))if(S->HasContents())Out.AddUnique(UTFDismantleStorage::StaticClass());
    TArray<AActor*> Dependencies;GetDismantleDependencies_Implementation(Dependencies);
    for(auto* Part:Dependencies)if(!Selected.Contains(Part))
    {
        if(Cast<ATFStop>(Part))Out.AddUnique(UTFDismantleStops::StaticClass());
        if(Cast<ATFShaft>(Part))Out.AddUnique(UTFDismantleShafts::StaticClass());
    }
}
ATFShaft::ATFShaft()
{
    mHologramClass = ATFShaftHologram::StaticClass();
    Kind=2; PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=2.f;
    for(int32 I=0;I<4;++I)
    {
        const float X=(I&1)?365:-365,Y=(I&2)?365:-365;
        Box(*FString::Printf(TEXT("Column%d"),I),FVector(X,Y,200),FVector(35,35,400),Steel);
        Box(*FString::Printf(TEXT("Brace%d"),I),FVector(X,0,380),FVector(45,730,30),Orange,false);
    }
    for(int32 I=0;I<2;++I) Box(*FString::Printf(TEXT("Guide%d"),I),FVector(0,I?315:-315,200),FVector(18,18,400),Light,false);
    Ironworks(TEXT("IW_Shaft"),TEXT("Shaft"));for(int32 I=0;I<2;++I){auto* C=Ironworks(*FString::Printf(TEXT("IW_Screen%d"),I),TEXT("Screen"));C->SetRelativeLocation(FVector(0,0,I*200));Screens.Add(C);}HideLegacyArt();
    FFGClearanceData C; C.ClearanceBox=FBox(FVector(-390,-390,0),FVector(390,390,400)); mClearanceData.Add(C);
}
void ATFShaft::Tick(float Delta)
{
    Super::Tick(Delta);if(GetNetMode()==NM_DedicatedServer){SetActorTickEnabled(false);return;}
    for(int32 I=0;I<Screens.Num();++I)if(Screens[I])Screens[I]->SetVisibility(!ScreenAtStop(this,I*200+100));
}
bool ATFShaft::CanDismantle_Implementation() const
{
    TArray<AActor*> Dependencies;GetDismantleDependencies_Implementation(Dependencies);
    return Dependencies.IsEmpty();
}
void ATFShaft::GetDismantleDependencies_Implementation(TArray<AActor*>& Out) const
{
    Super::GetDismantleDependencies_Implementation(Out);
    const auto* D=Drive();if(!D)return;
    const float Remaining=D->ShaftTop(this);
    if(Remaining>=D->ShaftTop())return;
    for(auto S:D->Stops)if(FreightPresent(S)&&S->Height()+800>Remaining+.1f)Out.AddUnique(S);
    for(TActorIterator<ATFShaft> It(GetWorld());It;++It)
        if(*It!=this&&FreightPresent(*It)&&It->Drive()==D&&It->GetActorLocation().Z+400>D->GetActorLocation().Z+Remaining+.1f)Out.AddUnique(*It);
}
ATFStop::ATFStop()
{
    mHologramClass = ATFStopHologram::StaticClass();
    Kind=1; PrimaryActorTick.TickInterval=0; Ports.SetNum(4); Ports[2].Input=false; Ports[3].Input=false; Tanks.SetNum(4);
    Incoming=CreateDefaultSubobject<UFGInventoryComponent>(TEXT("Incoming"));
    Outgoing=CreateDefaultSubobject<UFGInventoryComponent>(TEXT("Outgoing"));
    for(int32 I=0;I<4;++I)
    {
        const bool AlongX=(I%2)==0;
        const FVector P=PortLocation(I);
        Box(*FString::Printf(TEXT("DockFrame%d"),I),FVector(P.X*.92,P.Y*.92,20),AlongX?FVector(60,790,40):FVector(790,60,40),Orange);
        Box(*FString::Printf(TEXT("DockBuffer%d"),I),FVector(P.X*.82,P.Y*.82,115),AlongX?FVector(120,200,150):FVector(200,120,150),Dark);
        Box(*FString::Printf(TEXT("PortFrame%d"),I),FVector(P.X*.97,P.Y*.97,100),AlongX?FVector(25,170,110):FVector(170,25,110),Steel,false);
        Box(*FString::Printf(TEXT("PortLight%d"),I),FVector(P.X*.98,P.Y*.98,175),AlongX?FVector(20,150,10):FVector(150,20,10),Light,false);
    }
    Box(TEXT("ControlPanel"),FVector(385,175,205),FVector(30,110,100),Dark);
    Ironworks(TEXT("IW_Stop"),TEXT("Stop"));
    BufferHousing=Ironworks(TEXT("IW_Buffer"),TEXT("Buffer"));
    Ironworks(TEXT("IW_PortSigns"),TEXT("PortSigns"));
    for(int32 I=0;I<4;++I)
    {
        const FRotator R(0,I*90,0);
        auto* N=CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("PortNumber%d"),I));N->SetupAttachment(FreightRoot);
        N->SetRelativeLocation(R.RotateVector(FVector(405,-66,282)));N->SetRelativeRotation(R);N->SetHorizontalAlignment(EHTA_Center);N->SetVerticalAlignment(EVRTA_TextCenter);N->SetWorldSize(42);N->SetText(FText::AsNumber(I+1));N->SetTextRenderColor(FColor::Black);PortNumbers.Add(N);
        auto* T=CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("PortDirection%d"),I));T->SetupAttachment(FreightRoot);
        T->SetRelativeLocation(R.RotateVector(FVector(405,32,282)));T->SetRelativeRotation(R);T->SetHorizontalAlignment(EHTA_Center);T->SetVerticalAlignment(EVRTA_TextCenter);T->SetWorldSize(21);PortDirections.Add(T);
    }
    for(int32 I=0;I<8;++I){auto* C=Ironworks(*FString::Printf(TEXT("IW_StationGate%d"),I),TEXT("Gate"));C->SetRelativeRotation(FRotator(0,(I/2)*90,0));C->SetRelativeScale3D(FVector(1,(I%2)?-1.65f:1.65f,1));Gates.Add(C);}AnimateGates(Gates,0,70,1.65f);HideLegacyArt();
    Display->SetRelativeLocation(FVector(407,217,280));Display->SetWorldSize(13);Display->SetTextRenderColor(FColor(255,196,104));
    FFGClearanceData C; C.ClearanceBox=FBox(FVector(-395,-395,0),FVector(395,395,250)); mClearanceData.Add(C);
}
ATFItemStop::ATFItemStop()
{
    for(int32 I=0;I<4;++I)
    {
        auto* P=CreateDefaultSubobject<UFGFactoryConnectionComponent>(*FString::Printf(TEXT("Belt%d"),I));
        P->SetupAttachment(FreightRoot); P->SetRelativeLocation(PortLocation(I)); P->SetRelativeRotation(FRotator(0,I*90,0));
        P->SetDirection(I<2?EFactoryConnectionDirection::FCD_INPUT:EFactoryConnectionDirection::FCD_OUTPUT);
        P->SetConnectorClearance(100); P->SetForwardPeekAndGrabToBuildable(true); Belts.Add(P);
    }
}
ATFFluidStop::ATFFluidStop()
{
    IsFluid=true;
    for(int32 I=0;I<4;++I)
    {
        auto* P=CreateDefaultSubobject<UTFPipePort>(*FString::Printf(TEXT("Pipe%d"),I)); P->SetupAttachment(FreightRoot);
        P->SetRelativeLocation(PortLocation(I)); P->SetRelativeRotation(FRotator(0,I*90,0)); P->SetConnectorClearance(100); P->Configure(I<2); Pipes.Add(P);
        auto* Ring=Cylinder(*FString::Printf(TEXT("PipeFlange%d"),I),PortLocation(I),FVector(100,100,30),Steel);
        Ring->SetRelativeRotation(FRotator(90,I*90,0));
    }
    Ironworks(TEXT("IW_Flanges"),TEXT("Flanges"));HideLegacyArt();
}
void ATFStop::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority()) { Incoming->Resize(FTFStats::ForTier(Tier).Stacks); Outgoing->Resize(FTFStats::ForTier(Tier).Stacks); Link(); }
    ApplyConfig();
}
void ATFStop::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATFStop,Elevator); DOREPLIFETIME(ATFStop,FloorName); DOREPLIFETIME(ATFStop,Enabled); DOREPLIFETIME(ATFStop,WaitFull);
    DOREPLIFETIME(ATFStop,MaxWait); DOREPLIFETIME(ATFStop,Ports); DOREPLIFETIME(ATFStop,Tanks);
    DOREPLIFETIME(ATFStop,IncomingView); DOREPLIFETIME(ATFStop,OutgoingView);
    DOREPLIFETIME(ATFStop,IncomingReservations);
    DOREPLIFETIME(ATFStop,OutgoingReservations);
}
void ATFStop::OnRep_Config() { ApplyConfig(); }
void ATFStop::ApplyConfig()
{
    auto* LabelMaterial=GetNetMode()!=NM_DedicatedServer?LoadObject<UMaterialInterface>(nullptr,TEXT("/TrapperFreightElevators/Art/Ironworks/M_IW_Label.M_IW_Label"),nullptr,LOAD_NoWarn):nullptr;
    if(BufferHousing){const TCHAR* Name=IsFluid?TEXT("FluidBuffer"):TEXT("Buffer");const FString Path=FString(TEXT("/TrapperFreightElevators/Art/Ironworks/SM_IW_"))+Name+TEXT(".SM_IW_")+Name;BufferHousing->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*Path));}
    if(Ports.Num()!=4) Ports.SetNum(4);
    for(int32 I=0;I<4;++I)
    {
        if(PortNumbers.IsValidIndex(I)&&IsValid(PortNumbers[I]))PortNumbers[I]->SetTextRenderColor(FColor::White);
        if(LabelMaterial){if(PortNumbers.IsValidIndex(I)&&IsValid(PortNumbers[I]))PortNumbers[I]->SetTextMaterial(LabelMaterial);if(PortDirections.IsValidIndex(I)&&IsValid(PortDirections[I]))PortDirections[I]->SetTextMaterial(LabelMaterial);}
        if(Belts.IsValidIndex(I)) { Belts[I]->SetDirection(Ports[I].Input?EFactoryConnectionDirection::FCD_INPUT:EFactoryConnectionDirection::FCD_OUTPUT); Belts[I]->SetInventory(Ports[I].Input?Incoming:Outgoing); }
        if(Pipes.IsValidIndex(I)) Pipes[I]->Configure(Ports[I].Input);
        if(PortDirections.IsValidIndex(I)&&IsValid(PortDirections[I])){PortDirections[I]->SetText(FText::FromString(Ports[I].Input?TEXT("INPUT"):TEXT("OUTPUT")));PortDirections[I]->SetTextRenderColor(Ports[I].Input?FColor(110,235,180):FColor(255,185,75));}
    }
}
void ATFStop::UpdateStorageViews()
{
    if(IsFluid){IncomingView.Reset();OutgoingView.Reset();return;}
    auto Snapshot=[](UFGInventoryComponent* Inventory,TArray<FTFInventorySlot>& View)
    {
        TArray<FInventoryStack> Stacks;Inventory->GetInventoryStacks(Stacks,true);View.SetNum(Stacks.Num());
        for(int32 I=0;I<Stacks.Num();++I){const bool Has=Stacks[I].HasItems();View[I].Resource=Has?Stacks[I].Item.GetItemClass():nullptr;View[I].NumItems=Has?Stacks[I].NumItems:0;}
    };
    Snapshot(Incoming,IncomingView);Snapshot(Outgoing,OutgoingView);
}
int32 ATFStop::ReservedIncomingStacks(TSubclassOf<UFGItemDescriptor> Resource) const
{
    return ReservedStorageStacks(0,Resource);
}
bool ATFStop::SetIncomingReservation(TSubclassOf<UFGItemDescriptor> Resource,int32 Stacks)
{
    return SetStorageReservation(0,Resource,Stacks);
}
int32 ATFStop::ReservedStorageStacks(int32 Buffer,TSubclassOf<UFGItemDescriptor> Resource) const
{
    if(Buffer!=0&&Buffer!=1)return 0;
    const auto& Rules=Buffer==0?IncomingReservations:OutgoingReservations;
    int32 N=0;for(const auto& R:Rules)if(!Resource||R.Resource==Resource)N+=R.Stacks;return N;
}
bool ATFStop::SetStorageReservation(int32 Buffer,TSubclassOf<UFGItemDescriptor> Resource,int32 Stacks)
{
    if(!HasAuthority()||IsFluid||(Buffer!=0&&Buffer!=1)||!Resource||UFGItemDescriptor::GetForm(Resource)!=EResourceForm::RF_SOLID)return false;
    auto* Storage=Buffer==0?Incoming.Get():Outgoing.Get();
    if(Stacks<0||Stacks>Storage->GetSizeLinear())return false;
    const int32 Total=ReservedStorageStacks(Buffer)-ReservedStorageStacks(Buffer,Resource)+Stacks;
    if(Total>Storage->GetSizeLinear())return false;
    auto& Rules=Buffer==0?IncomingReservations:OutgoingReservations;
    auto* Existing=Rules.FindByPredicate([&](const FTFStackReservation& R){return R.Resource==Resource;});
    if(Stacks==0)Rules.RemoveAll([&](const FTFStackReservation& R){return R.Resource==Resource;});
    else if(Existing)Existing->Stacks=Stacks;
    else {FTFStackReservation R;R.Resource=Resource;R.Stacks=Stacks;Rules.Add(R);}
    // Configuration never moves or removes cargo. Existing excess drains normally.
    ForceNetUpdate();return true;
}
int32 ATFStop::IncomingRoom(TSubclassOf<UFGItemDescriptor> Resource,bool HasItemState) const
{
    if(IsFluid||!Resource||UFGItemDescriptor::GetForm(Resource)!=EResourceForm::RF_SOLID)return 0;
    TArray<FInventoryStack> Existing;Incoming->GetInventoryStacks(Existing,true);
    TMap<TSubclassOf<UFGItemDescriptor>,int32> Occupied;int32 Empty=0,PartialRoom=0;
    for(int32 I=0;I<Existing.Num();++I)
    {
        const auto& S=Existing[I];if(!S.HasItems()){++Empty;continue;}
        ++Occupied.FindOrAdd(S.Item.GetItemClass());
        if(!HasItemState&&!S.Item.HasState()&&S.Item.GetItemClass()==Resource)
            PartialRoom+=FMath::Max(0,Incoming->GetSlotSize(I,Resource)-S.NumItems);
    }
    int32 Protected=0;
    for(const auto& R:IncomingReservations)if(R.Resource!=Resource)
        Protected+=FMath::Max(0,R.Stacks-Occupied.FindRef(R.Resource));
    // Filling a partial stack never consumes another empty slot.
    return PartialRoom+FMath::Max(0,Empty-Protected)*(HasItemState?1:TF::StackSize(Resource));
}
bool ATFStop::CanReceiveIncoming(const FInventoryItem& Item) const
{
    return IncomingRoom(Item.GetItemClass(),Item.HasState())>0&&Incoming->HasEnoughSpaceForItem(Item);
}
int32 ATFStop::MoveStorageStack(UFGInventoryComponent* PlayerInventory,bool Deposit,int32 Buffer,int32 Slot,TSubclassOf<UFGItemDescriptor> Expected)
{
    if(!HasAuthority()||IsFluid||!PlayerInventory||!Expected||(Buffer!=0&&Buffer!=1)||(Deposit&&Buffer!=0))return 0;
    auto* Storage=Buffer==0?Incoming.Get():Outgoing.Get();auto* Source=Deposit?PlayerInventory:Storage;auto* Destination=Deposit?Storage:PlayerInventory;
    if(Source==Destination)return 0;
    FInventoryStack Stack;if(!Source->GetStackFromIndex(Slot,Stack)||!Stack.HasItems()||Stack.Item.GetItemClass()!=Expected)return 0;
    if(Deposit&&(Stack.Item.HasState()||UFGItemDescriptor::GetForm(Expected)!=EResourceForm::RF_SOLID))return 0;
    if(Deposit){Stack.NumItems=FMath::Min(Stack.NumItems,IncomingRoom(Expected));if(Stack.NumItems<=0)return 0;}
    const int32 Moved=Destination->AddStack(Stack,true);
    if(Moved>0)Source->RemoveFromIndex(Slot,Moved,Destination);
    UpdateStorageViews();ForceNetUpdate();return Moved;
}
void ATFStop::Link()
{
    if (IsValid(Elevator)) return;
    Elevator=nullptr;
    for(TActorIterator<ATFDrive> It(GetWorld());It;++It)
        if (It->IsFluid==IsFluid && It->CanAttach(GetActorLocation(),this)) { Elevator=*It; Tier=It->Tier; Incoming->Resize(FTFStats::ForTier(Tier).Stacks); Outgoing->Resize(FTFStats::ForTier(Tier).Stacks); It->RefreshStops(); break; }
}
FString ATFStop::Label() const { return FloorName.IsEmpty()?FString::Printf(TEXT("Floor %.0fm"),Height()/100.f):FloorName; }
float ATFStop::Height() const { return Elevator?GetActorLocation().Z-Elevator->GetActorLocation().Z:0; }
bool ATFStop::HasContents() const
{
    if(!Incoming->IsEmpty()||!Outgoing->IsEmpty()||Reserved()>TF::Epsilon) return true;
    for(const auto& T:Tanks) if(T.Input+T.Output>TF::Epsilon) return true;
    for(const auto& P:Pipes) if(P->Content()>TF::Epsilon) return true;
    return false;
}
bool ATFStop::CanDismantle_Implementation() const { return !HasContents(); }
float ATFStop::Reserved(TSubclassOf<UFGItemDescriptor> Resource,int32 Tank) const
{
    float N=0;
    if(IsValid(Elevator)) for(const auto& C:Elevator->Cargo) if(C.Destination==this && (!Resource||C.Resource==Resource) && (Tank==INDEX_NONE || C.Compartment==Tank)) N+=C.Amount;
    return N;
}
float ATFStop::FluidFree(int32 Tank,bool Input) const
{
    if(!IsValid(Elevator)||!Tanks.IsValidIndex(Tank)||Tank>=Elevator->Compartments) return 0;
    const float Max=FTFStats::ForTier(Tier).Fluid/Elevator->Compartments;
    return FMath::Max(0.f,Max-(Input?Tanks[Tank].Input:Tanks[Tank].Output+Reserved(nullptr,Tank)));
}
float ATFStop::ItemFree(TSubclassOf<UFGItemDescriptor> Resource,bool Reservations) const
{
    if(!Resource) return 0;
    TMap<TSubclassOf<UFGItemDescriptor>,int32> Counts;
    TArray<FInventoryStack> Existing; Outgoing->GetInventoryStacks(Existing);
    for(const auto& S:Existing) if(S.HasItems()) Counts.FindOrAdd(S.Item.GetItemClass())+=S.NumItems;
    if(Reservations && IsValid(Elevator)) for(const auto& C:Elevator->Cargo) if(C.Destination==this) Counts.FindOrAdd(C.Resource)+=FMath::RoundToInt(C.Amount);
    int32 OtherSlots=0;
    for(const auto& Pair:Counts) if(Pair.Key!=Resource) OtherSlots+=FMath::DivideAndRoundUp(Pair.Value,TF::StackSize(Pair.Key));
    // Stored and already assigned cargo both satisfy reservations. Protect only
    // the unused part of another item's allocation before choosing a route.
    for(const auto& Rule:OutgoingReservations) if(Rule.Resource!=Resource)
        OtherSlots+=FMath::Max(0,Rule.Stacks-FMath::DivideAndRoundUp(Counts.FindRef(Rule.Resource),TF::StackSize(Rule.Resource)));
    return FMath::Max(0,(Outgoing->GetSizeLinear()-OtherSlots)*TF::StackSize(Resource)-Counts.FindRef(Resource));
}
TSubclassOf<UFGItemDescriptor> ATFStop::AcceptFilter(const UFGFactoryConnectionComponent* Port) const
{
    const int32 I=Belts.IndexOfByKey(Port); return Ports.IsValidIndex(I)?Ports[I].Filter:nullptr;
}
bool ATFStop::Factory_PeekOutput_Implementation(const UFGFactoryConnectionComponent* Port,TArray<FInventoryItem>& Items,TSubclassOf<UFGItemDescriptor> Type) const
{
    const int32 I=Belts.IndexOfByKey(Port);
    if(!Ports.IsValidIndex(I)||Ports[I].Input) return false;
    auto Filter=AcceptFilter(Port); if(Filter && Type && Filter!=Type) return false;
    TArray<FInventoryStack> Stacks; Outgoing->GetInventoryStacks(Stacks);
    for(const auto& S:Stacks) if(S.HasItems() && (!Type || Type==S.Item.GetItemClass()) && (!Filter || Filter==S.Item.GetItemClass())) { Items.Add(S.Item); return true; }
    return false;
}
bool ATFStop::Factory_GrabOutput_Implementation(UFGFactoryConnectionComponent* Port,FInventoryItem& Item,float& Offset,TSubclassOf<UFGItemDescriptor> Type)
{
    TArray<FInventoryItem> Items; if(!HasAuthority() || !Factory_PeekOutput_Implementation(Port,Items,Type)) return false;
    Item=Items[0]; Offset=0; Outgoing->Remove(Item.GetItemClass(),1); return true;
}
void ATFStop::Tick(float Delta)
{
    Super::Tick(Delta);
    if(GetNetMode()!=NM_DedicatedServer)
    {
        const float Phase=IsValid(Elevator)&&Elevator->GateStop==this?Elevator->DoorPhase:0.f;
        GateOpen=FMath::FInterpConstantTo(GateOpen,Phase,Delta,1.f/TF::DoorSeconds);
        AnimateGates(Gates,TF::Ease(GateOpen),70,1.65f);
    }
    if(!HasAuthority()){ScanTimer+=Delta;if(ScanTimer>=1){ScanTimer=0;if(Display)Display->SetText(FText::FromString(Label()+TEXT("\nFLOOR BUFFER")));}}
}
void ATFStop::PostFactoryTick(AFGBuildableSubsystem*,float Delta)
{
    if(!HasAuthority()) return;
    if(PendingFlush){PendingFlush=false;for(auto& T:Tanks){T.Input=0;T.Output=0;}for(auto P:Pipes)P->Take(P->Content());}
    ScanTimer+=Delta;
    if(ScanTimer>=1) { ScanTimer=0; Link(); UpdateStorageViews(); if(GetNetMode()!=NM_DedicatedServer&&Display)Display->SetText(FText::FromString(Label()+TEXT("\n")+(Elevator?TEXT("FLOOR BUFFER"):TEXT("NO DRIVE")))); }
    if(!IsValid(Elevator)) return;
    if(!IsFluid)
    {
        for(int32 I=0;I<Belts.Num();++I)
        {
            if(!Ports[I].Input || !Belts[I]->IsConnected()) continue;
            auto* Other=Belts[I]->GetConnection(); if(!Other) continue;
            for(int32 Grab=0;Grab<MaxNumGrab(Delta);++Grab)
            {
                TArray<FInventoryItem> Peek;
                if(!Other->Factory_PeekOutput(Peek,Ports[I].Filter)||Peek.IsEmpty()||!CanReceiveIncoming(Peek[0])) break;
                FInventoryItem Item; float Offset=0;
                if(!Other->Factory_GrabOutput(Item,Offset,Ports[I].Filter)) break;
                if(!Incoming->AddItem(Item)) { UE_LOG(LogTemp,Error,TEXT("Freight input insertion failed after preflight")); break; }
                ++ReceivedItems;
            }
        }
    }
    else
    {
        for(int32 I=0;I<Pipes.Num();++I)
        {
            const int32 K=Ports[I].Compartment;
            if(!Tanks.IsValidIndex(K)||K>=Elevator->Compartments) continue;
            auto Resource=Elevator->TankResources.IsValidIndex(K)?Elevator->TankResources[K]:nullptr;
            if(!Resource) continue;
            auto* P=Pipes[I].Get(); const auto NetworkFluid=P->GetFluidDescriptor();
            if(NetworkFluid && NetworkFluid!=Resource) continue;
            if(!NetworkFluid && P->IsConnected())
            {
                if(auto* Subsystem=AFGPipeSubsystem::Get(GetWorld())) Subsystem->TrySetNetworkFluidDescriptor(P->GetPipeNetworkID(),Resource);
                continue;
            }
            Tanks[K].Resource=Resource;
            if(Ports[I].Input){const float N=P->Take(FMath::Min(FluidFree(K,true),10.f*Delta));Tanks[K].Input+=N;ReceivedFluid+=N;}
            else { const float N=P->Give(FMath::Min(Tanks[K].Output,10.f*Delta)); Tanks[K].Output-=N; }
        }
    }
    if(Elevator->HasPickup(this)) { if(WaitingSince==0) WaitingSince=GetWorld()->GetTimeSeconds(); } else WaitingSince=0;
}

ATFDrive::ATFDrive()
{
    mHologramClass = ATFTowerHologram::StaticClass();
    Kind=0; TankResources.SetNum(4);
    AutoShaftFrames=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("AutoShaftFrames"));
    AutoShaftScreens=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("AutoShaftScreens"));
    for(auto* Component:{AutoShaftFrames.Get(),AutoShaftScreens.Get()})
    {
        Component->SetupAttachment(FreightRoot);Component->SetMobility(EComponentMobility::Movable);
        Component->SetCollisionProfileName(TEXT("NoCollision"));Component->SetCanEverAffectNavigation(false);
    }
    AutoShaftFrames->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/TrapperFreightElevators/Art/Ironworks/SM_IW_Shaft.SM_IW_Shaft"),nullptr,LOAD_NoWarn));
    AutoShaftScreens->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/TrapperFreightElevators/Art/Ironworks/SM_IW_Screen.SM_IW_Screen"),nullptr,LOAD_NoWarn));
    PowerInfo=CreateDefaultSubobject<UFGPowerInfoComponent>(TEXT("FreightPowerInfo"));
    CabinInventory=CreateDefaultSubobject<UFGInventoryComponent>(TEXT("CabinInventory"));
    Power=CreateDefaultSubobject<UFGPowerConnectionComponent>(TEXT("FreightPower")); Power->SetupAttachment(FreightRoot); Power->SetRelativeLocation(FVector(390,280,300));
    Cabin=CreateDefaultSubobject<USceneComponent>(TEXT("Cabin")); Cabin->SetupAttachment(FreightRoot);
    Box(TEXT("CabinDeck"),FVector(0,0,20),FVector(560,560,40),Steel,true,Cabin);
    Box(TEXT("CabinRoof"),FVector(0,0,580),FVector(560,560,40),Orange,false,Cabin);
    for(int32 I=0;I<4;++I)
    {
        Box(*FString::Printf(TEXT("CabinPillar%d"),I),FVector((I&1)?265:-265,(I&2)?265:-265,300),FVector(25,25,550),Steel,false,Cabin);
        Box(*FString::Printf(TEXT("BasePillar%d"),I),FVector((I&1)?365:-365,(I&2)?365:-365,400),FVector(45,45,800),Steel);
    }
    Box(TEXT("DriveRoof"),FVector(0,0,770),FVector(790,790,60),Dark);
    Box(TEXT("MotorHousing"),FVector(0,0,690),FVector(400,500,100),Orange);
    Cylinder(TEXT("Winch"),FVector(0,0,705),FVector(190,190,280),Steel)->SetRelativeRotation(FRotator(90,0,0));
    Box(TEXT("FrontConsole"),FVector(385,-200,270),FVector(30,200,180),Dark);
    Display->SetRelativeLocation(FVector(405,-200,300)); Display->SetWorldSize(20);
    for(int32 I=0;I<3;++I) Box(*FString::Printf(TEXT("CargoBox%d"),I),FVector(0,(I-1)*160,110),FVector(320,140,140),Dark,false,Cabin);
    for(int32 I=0;I<4;++I) Cylinder(*FString::Printf(TEXT("CargoTank%d"),I),FVector((I&1)?130:-130,(I&2)?130:-130,250),FVector(190,190,400),Orange,Cabin);
    Ironworks(TEXT("IW_Base"),TEXT("Base"));Ironworks(TEXT("IW_Cabin"),TEXT("Cabin"),Cabin);
    HoistHead=Ironworks(TEXT("IW_Head"),TEXT("Head"));HoistHead->SetRelativeLocation(FVector(0,0,610));
    HoistDrum=Ironworks(TEXT("IW_Drum"),TEXT("Drum"));HoistDrum->SetRelativeLocation(FVector(0,0,705));
    LiftCables=Ironworks(TEXT("IW_Cables"),TEXT("Cables"));
    FreightCrates=Ironworks(TEXT("IW_Cargo"),TEXT("Cargo"),Cabin);
    for(int32 I=0;I<4;++I){auto* C=Ironworks(*FString::Printf(TEXT("IW_Tank%d"),I),TEXT("Tank"),Cabin);PressureTanks.Add(C);auto* Level=Ironworks(*FString::Printf(TEXT("IW_TankLevel%d"),I),TEXT("Level"),C);TankLevels.Add(Level);}
    VisualTankFill.Init(0.f,4);
    for(int32 I=0;I<8;++I){auto* C=Ironworks(*FString::Printf(TEXT("IW_CabinGate%d"),I),TEXT("Gate"),Cabin);C->SetRelativeRotation(FRotator(0,(I/2)*90,0));C->SetRelativeScale3D(FVector(1,(I%2)?-1.f:1.f,1));Gates.Add(C);}
    for(int32 I=0;I<4;++I){auto* C=Ironworks(*FString::Printf(TEXT("IW_BaseScreen%d"),I),TEXT("Screen"));C->SetRelativeLocation(FVector(0,0,I*200));Screens.Add(C);}
    HideLegacyArt();Display->SetRelativeLocation(FVector(407,-215,290));Display->SetWorldSize(12);Display->SetTextRenderColor(FColor(255,196,104));
    FFGClearanceData C; C.ClearanceBox=FBox(FVector(-395,-395,0),FVector(395,395,800)); mClearanceData.Add(C);
}
void ATFDrive::BeginPlay()
{
    Super::BeginPlay(); Power->SetPowerInfo(PowerInfo);
    Cabin->SetRelativeLocation(FVector(0,0,CabinHeight));GateOpen=DoorPhase;
    if(IsValid(Docked)&&!GateStop)GateStop=Docked;
    if(HasAuthority()) { CabinInventory->Resize(IsFluid?1:FTFStats::ForTier(Tier).Stacks); PowerInfo->SetMaximumTargetConsumption(FTFStats::ForTier(Tier).ActiveMW); PowerInfo->SetTargetConsumption(0); RefreshStops(); }
    UpdateDisplay();
}
void ATFDrive::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATFDrive,ShaftBuiltFirst);DOREPLIFETIME(ATFDrive,PreservedShaftHeight);
    DOREPLIFETIME(ATFDrive,GateStop);DOREPLIFETIME(ATFDrive,DoorPhase);
    DOREPLIFETIME(ATFDrive,CabinHeight); DOREPLIFETIME(ATFDrive,Target); DOREPLIFETIME(ATFDrive,Docked); DOREPLIFETIME(ATFDrive,Direction);
    DOREPLIFETIME(ATFDrive,Paused); DOREPLIFETIME(ATFDrive,Compartments); DOREPLIFETIME(ATFDrive,TankResources); DOREPLIFETIME(ATFDrive,Routes);
    DOREPLIFETIME(ATFDrive,Cargo); DOREPLIFETIME(ATFDrive,Stops); DOREPLIFETIME(ATFDrive,Status); DOREPLIFETIME(ATFDrive,Powered);
}
bool ATFDrive::CanDismantle_Implementation() const
{
    if(!Cargo.IsEmpty())return false;
    TArray<AActor*> Dependencies;GetDismantleDependencies_Implementation(Dependencies);
    return Dependencies.IsEmpty();
}
void ATFDrive::GetDismantleDependencies_Implementation(TArray<AActor*>& Out) const
{
    Super::GetDismantleDependencies_Implementation(Out);
    for(auto Stop:Stops)if(FreightPresent(Stop))Out.AddUnique(Stop);
    for(TActorIterator<ATFShaft> It(GetWorld());It;++It)if(FreightPresent(*It)&&It->Drive()==this)Out.AddUnique(*It);
}
void ATFDrive::Upgrade_Implementation(AActor* NewActor)
{
    auto* New=Cast<ATFDrive>(NewActor);if(!New||!HasAuthority()||New->IsFluid!=IsFluid||New->Tier<=Tier)return;
    New->ShaftBuiltFirst=ShaftBuiltFirst;New->PreservedShaftHeight=PreservedShaftHeight;
    New->CabinHeight=CabinHeight;New->Target=Target;New->Docked=Docked;New->Direction=Direction;New->Paused=Paused;
    New->GateStop=GateStop;New->DoorPhase=DoorPhase;New->TravelActive=false;
    New->Compartments=Compartments;New->TankResources=TankResources;New->Routes=Routes;New->Cargo=Cargo;
    New->Snapshot=Snapshot;New->FluidSnapshot=FluidSnapshot;New->Dwell=Dwell;New->DepartRequested=DepartRequested;New->SweepEnd=SweepEnd;New->HasSweep=HasSweep;
    New->CabinInventory->Resize(FTFStats::ForTier(New->Tier).Stacks);
    TArray<FInventoryStack> Stacks;CabinInventory->GetInventoryStacks(Stacks);
    for(const auto& S:Stacks) {const int32 N=New->CabinInventory->AddStack(S,true);CabinInventory->Remove(S.Item.GetItemClass(),N);}
    for(auto Stop:Stops) if(IsValid(Stop)) {Stop->Elevator=New;Stop->Tier=New->Tier;Stop->Incoming->Resize(FTFStats::ForTier(New->Tier).Stacks);Stop->Outgoing->Resize(FTFStats::ForTier(New->Tier).Stacks);Stop->ForceNetUpdate();}
    TArray<AFGBuildableWire*> Wires;Power->GetWires(Wires);
    for(auto* W:Wires) if(IsValid(W)) {auto* Other=W->GetOppositeConnection(Power);W->Disconnect();if(Other)W->Connect(New->Power,Other);}
    TArray<UFGCircuitConnectionComponent*> Hidden;Power->GetHiddenConnections(Hidden);
    for(auto* Other:Hidden) if(IsValid(Other)) {Power->RemoveHiddenConnection(Other);New->Power->AddHiddenConnection(Other);}
    Cargo.Reset();Stops.Reset();New->RefreshStops();New->ForceNetUpdate();
}
bool ATFDrive::CanAttach(const FVector& P,const ATFStop* Ignore) const
{
    if(Ignore && Ignore->GetBlueprintDesigner()!=GetBlueprintDesigner())return false;
    const FVector Local=GetActorTransform().InverseTransformPosition(P);
    if(FMath::Abs(Local.X)>2 || FMath::Abs(Local.Y)>2 || Local.Z<-.1 || Local.Z>TF::MaxTravel+.1 || FMath::Abs(Local.Z-FMath::GridSnap(Local.Z,100.f))>1) return false;
    if(Local.Z+800>ShaftTop()+.1f)return false;
    int32 N=0;
    for(TActorIterator<ATFStop> It(GetWorld());It;++It) if(*It!=Ignore && It->Elevator==this)
    { if(FMath::Abs(It->GetActorLocation().Z-P.Z)<TF::StopSpacing-1) return false; ++N; }
    return N<FTFStats::ForTier(Tier).Stops;
}
float ATFDrive::ShaftTop(const ATFShaft* Ignore) const
{
    float Top=FMath::Clamp(PreservedShaftHeight,800.f,25600.f);
    // Old saves retain their existing tower; new stops cannot lengthen it.
    if(!ShaftBuiltFirst)for(auto S:Stops)if(FreightPresent(S))Top=FMath::Max(Top,S->Height()+800);
    TArray<float> Starts;
    for(TActorIterator<ATFShaft> It(GetWorld());It;++It)
    {
        if(*It==Ignore||!FreightPresent(*It)||It->GetBlueprintDesigner()!=GetBlueprintDesigner())continue;
        const FVector P=GetActorTransform().InverseTransformPosition(It->GetActorLocation());
        if(FMath::Abs(P.X)<2&&FMath::Abs(P.Y)<2&&P.Z>=0&&P.Z<=25200)Starts.Add(P.Z);
    }
    Starts.Sort();for(float Z:Starts)if(Z<=Top+.1f)Top=FMath::Max(Top,Z+400);
    return FMath::Min(Top,25600.f);
}
void ATFDrive::RefreshStops()
{
    Stops.Reset();
    for(TActorIterator<ATFStop> It(GetWorld());It;++It) if(It->Elevator==this) Stops.Add(*It);
    Stops.Sort([](const ATFStop& A,const ATFStop& B){return A.GetActorLocation().Z<B.GetActorLocation().Z;});
    RefreshAutoShaft();
}
void ATFDrive::RefreshAutoShaft()
{
    TArray<float> Landings,Manual;
    for(auto Stop:Stops)if(IsValid(Stop)&&!Stop->IsActorBeingDestroyed()&&Stop->Elevator==this)Landings.Add(Stop->Height());
    if(HasAuthority()&&!ShaftBuiltFirst)for(float H:Landings)PreservedShaftHeight=FMath::Max(PreservedShaftHeight,H+800);
    // Manual modules may be placed or removed after the floors are linked.
    for(TActorIterator<ATFShaft> It(GetWorld());It;++It)
    {
        if(It->IsActorBeingDestroyed()||It->GetBlueprintDesigner()!=GetBlueprintDesigner())continue;
        const FVector Local=GetActorTransform().InverseTransformPosition(It->GetActorLocation());
        if(FMath::Abs(Local.X)<2&&FMath::Abs(Local.Y)<2&&Local.Z>=0&&Local.Z<=TF::MaxTravel+400)
            Manual.Add(Local.Z);
    }
    const auto Layout=FTFShaftLayout::Make(Landings,Manual,ShaftTop());
    if(Layout==AutoShaftLayout)return;
    AutoShaftLayout=Layout;
    // The authoritative server and clients share the continuous player barrier.
    ConfigureCollision(Layout.Height);
    if(GetNetMode()==NM_DedicatedServer)return;
    AutoShaftFrames->ClearInstances();AutoShaftScreens->ClearInstances();
    for(const FVector2D& Span:Layout.Frames)
        AutoShaftFrames->AddInstance(FTransform(FQuat::Identity,FVector(0,0,Span.X),FVector(1,1,Span.Y/400.f)));
    for (const FVector2D& Span : Layout.Screens)
        AutoShaftScreens->AddInstance(FTransform(FQuat::Identity, FVector(0,0,Span.X), FVector(1,1,Span.Y/200.f)));
}
float ATFDrive::CargoAmount(int32 Tank) const { float N=0; for(const auto& C:Cargo) if(Tank==INDEX_NONE || C.Compartment==Tank) N+=C.Amount; return N; }
int32 ATFDrive::CargoSlots() const
{
    TMap<TSubclassOf<UFGItemDescriptor>,int32> Counts;
    for(const auto& C:Cargo) Counts.FindOrAdd(C.Resource)+=FMath::RoundToInt(C.Amount);
    int32 Slots=0; for(const auto& P:Counts) Slots+=FMath::DivideAndRoundUp(P.Value,TF::StackSize(P.Key)); return Slots;
}
int32 ATFDrive::TankFor(TSubclassOf<UFGItemDescriptor> Resource) const
{
    for(int32 I=0;I<Compartments;++I) if(TankResources.IsValidIndex(I)&&TankResources[I]==Resource && CabinRoom(Resource,I)>TF::Epsilon) return I;
    return INDEX_NONE;
}
float ATFDrive::CabinRoom(TSubclassOf<UFGItemDescriptor> Resource,int32 Tank) const
{
    const auto S=FTFStats::ForTier(Tier);
    if(IsFluid) return Tank>=0 && Tank<Compartments ? FMath::Max(0.f,S.Fluid/Compartments-CargoAmount(Tank)):0;
    TMap<TSubclassOf<UFGItemDescriptor>,int32> Counts;
    for(const auto& C:Cargo) Counts.FindOrAdd(C.Resource)+=FMath::RoundToInt(C.Amount);
    int32 Other=0; for(const auto& P:Counts) if(P.Key!=Resource) Other+=FMath::DivideAndRoundUp(P.Value,TF::StackSize(P.Key));
    return FMath::Max(0,(S.Stacks-Other)*TF::StackSize(Resource)-Counts.FindRef(Resource));
}
int32 ATFDrive::ChooseRoute(TSubclassOf<UFGItemDescriptor> Resource,ATFStop* Source,float& Room,int32 Compartment) const
{
    int32 Best=INDEX_NONE; double Score=TNumericLimits<double>::Max(); Room=0;
    for(int32 I=0;I<Routes.Num();++I)
    {
        const auto& R=Routes[I]; auto* D=R.Destination.Get();
        if(R.Resource!=Resource || !IsValid(D)||D==Source||D->Elevator!=this||!D->Enabled||R.Weight<=0) continue;
        float Free=0;
        if(IsFluid)
        {
            for(int32 K=0;K<Compartments;++K) if((Compartment==INDEX_NONE||Compartment==K)&&TankResources.IsValidIndex(K)&&TankResources[K]==Resource) Free+=D->FluidFree(K,false);
        }
        else Free=D->ItemFree(Resource);
        if(Free<(IsFluid?.001f:1.f)) continue;
        // Weighted least-served routing balances quantities, including cargo in flight.
        const double Candidate=R.Delivered/R.Weight;
        if(Candidate<Score) { Best=I; Score=Candidate; Room=Free; }
    }
    return Best;
}
bool ATFDrive::HasPickup(ATFStop* Stop) const
{
    if(!IsValid(Stop)||!Stop->Enabled||Stop->Elevator!=this) return false;
    if(IsFluid)
    {
        for(int32 K=0;K<Compartments;++K) if(Stop->Tanks.IsValidIndex(K)&&Stop->Tanks[K].Input>.001f)
        { const auto R=TankResources.IsValidIndex(K)?TankResources[K]:nullptr; float Room; if(R && CabinRoom(R,K)>.001 && ChooseRoute(R,Stop,Room,K)!=INDEX_NONE) return true; }
    }
    else
    {
        TArray<FInventoryStack> Stacks; Stop->Incoming->GetInventoryStacks(Stacks);
        for(const auto& S:Stacks) if(S.HasItems() && !S.Item.HasState()) { float Room; if(CabinRoom(S.Item.GetItemClass(),0)>=1 && ChooseRoute(S.Item.GetItemClass(),Stop,Room)!=INDEX_NONE) return true; }
    }
    return false;
}
ATFStop* ATFDrive::NextStop()
{
    TArray<ATFStop*> Pending;
    for(auto Stop:Stops) if(IsValid(Stop))
    {
        bool Delivery=false; for(const auto& C:Cargo) if(C.Destination==Stop) { Delivery=true;break; }
        if(Delivery || HasPickup(Stop)) Pending.Add(Stop);
    }
    if(Pending.IsEmpty()) { HasSweep=false; return nullptr; }
    // Bound each sweep so new requests cannot continually push the endpoint out.
    if(!HasSweep)
    {
        ATFStop* Oldest=nullptr;
        for(auto S:Pending) if(!Oldest || (S->WaitingSince>0 && (Oldest->WaitingSince==0||S->WaitingSince<Oldest->WaitingSince))) Oldest=S;
        Direction=Oldest->Height()>=CabinHeight?1:-1; SweepEnd=CabinHeight;
        for(auto S:Pending) if(Direction>0) SweepEnd=FMath::Max(SweepEnd,S->Height()); else SweepEnd=FMath::Min(SweepEnd,S->Height());
        HasSweep=true;
    }
    ATFStop* Best=nullptr; float Distance=FLT_MAX;
    for(auto S:Pending)
    {
        const float H=S->Height(),D=(H-CabinHeight)*Direction;
        if(D<-.1 || (H-SweepEnd)*Direction>.1 || S==Docked) continue;
        if(D<Distance) { Best=S;Distance=D; }
    }
    if(Best) return Best;
    Direction=-Direction; HasSweep=false;
    // A fresh bounded sweep starts on the following tick.
    return nullptr;
}
void ATFDrive::Arrive(ATFStop* Stop)
{
    Docked=Stop;GateStop=Stop;Target=nullptr;Dwell=0;TransferCredit=0;DepartRequested=false;
    DoorPhase=0;TravelActive=false;TravelElapsed=0;ForceNetUpdate();
    Snapshot.Reset();FluidSnapshot.SetNumZeroed(4);
    if(IsFluid) for(int32 I=0;I<Compartments;++I) FluidSnapshot[I]=Stop->Tanks[I].Input;
    else Stop->Incoming->GetInventoryStacks(Snapshot);
}
bool ATFDrive::Unload(float& Budget)
{
    if(!IsValid(Docked)) return false;
    bool Transferred=false;
    for(int32 I=Cargo.Num()-1;I>=0 && Budget>TF::Epsilon;--I)
    {
        auto& C=Cargo[I]; if(C.Destination!=Docked) continue;
        float N=FMath::Min(C.Amount,Budget);
        if(IsFluid)
        {
            if(!Docked->Tanks.IsValidIndex(C.Compartment)) continue;
            auto& T=Docked->Tanks[C.Compartment]; if(T.Resource && T.Resource!=C.Resource) continue;
            T.Resource=C.Resource; const float Max=FTFStats::ForTier(Tier).Fluid/Compartments;
            N=FMath::Min(N,FMath::Max(0.f,Max-T.Output)); T.Output+=N;
        }
        else { const int32 Count=FMath::FloorToInt(FMath::Min(N,Docked->ItemFree(C.Resource,false))); N=Count>0?Docked->Outgoing->AddStack(FInventoryStack(Count,C.Resource),true):0; if(N>0) CabinInventory->Remove(C.Resource,FMath::RoundToInt(N)); }
        C.Amount-=N;Budget-=N;Transferred|=N>TF::Epsilon;
        if(C.Amount<TF::Epsilon) Cargo.RemoveAt(I);
    }
    return Transferred;
}
bool ATFDrive::Load(float& Budget)
{
    if(!IsValid(Docked)||!Docked->Enabled||DepartRequested) return false;
    bool Transferred=false;
    auto Transfer=[&](TSubclassOf<UFGItemDescriptor> Resource,float Available,int32 Tank)->float
    {
        float Room=0; const int32 R=ChooseRoute(Resource,Docked,Room,IsFluid?Tank:INDEX_NONE);
        if(R==INDEX_NONE) return 0;
        auto* Dest=Routes[R].Destination.Get();
        if(IsFluid) Room=FMath::Min(Room,Dest->FluidFree(Tank,false));
        // Small quanta keep weighted shares responsive even within one cabin load.
        float N=FMath::Min(FMath::Min(Available,Budget),FMath::Min(Room,CabinRoom(Resource,Tank)));
        N=FMath::Min(N,IsFluid?1.f:1.f); if(!IsFluid) N=FMath::FloorToFloat(N);
        if(N<=TF::Epsilon) return 0;
        if(!IsFluid) {N=CabinInventory->AddStack(FInventoryStack(FMath::RoundToInt(N),Resource),true);if(N<=0)return 0;}
        FTFCargo* Existing=Cargo.FindByPredicate([&](const FTFCargo& C){return C.Resource==Resource&&C.Destination==Dest&&C.Compartment==Tank;});
        if(Existing) Existing->Amount+=N; else { FTFCargo C;C.Resource=Resource;C.Destination=Dest;C.Amount=N;C.Compartment=Tank;Cargo.Add(C); }
        Routes[R].Delivered+=N; Budget-=N;
        return N;
    };
    // Bounded work even if a long frame accumulates a large transfer budget.
    for(int32 Pass=0;Pass<256 && Budget>TF::Epsilon;++Pass)
    {
        bool Progress=false;
        if(IsFluid)
        {
            for(int32 K=0;K<Compartments && Budget>TF::Epsilon;++K)
            {
                const auto Resource=TankResources[K];
                const float Eligible=Docked->WaitFull?Docked->Tanks[K].Input:FMath::Min(Docked->Tanks[K].Input,FluidSnapshot[K]);
                const float N=Transfer(Resource,Eligible,K);
                Docked->Tanks[K].Input-=N; FluidSnapshot[K]=FMath::Max(0.f,FluidSnapshot[K]-N);Progress|=N>TF::Epsilon;
            }
        }
        else
        {
            TArray<FInventoryStack> Current; if(Docked->WaitFull) Docked->Incoming->GetInventoryStacks(Current); else Current=Snapshot;
            for(auto& S:Current) if(S.HasItems() && !S.Item.HasState() && Budget>=1)
            {
                const auto Resource=S.Item.GetItemClass();
                const float Available=FMath::Min(S.NumItems,Docked->Incoming->GetNumItems(Resource));
                const int32 N=FMath::RoundToInt(Transfer(Resource,Available,0));
                if(N>0) { Docked->Incoming->Remove(Resource,N); if(!Docked->WaitFull) for(auto& Q:Snapshot) if(Q.Item.GetItemClass()==Resource && Q.NumItems>0) {const int32 Take=FMath::Min(Q.NumItems,N);Q.NumItems-=Take;break;} Progress=true; }
            }
        }
        Transferred|=Progress; if(!Progress) break;
    }
    return Transferred;
}
bool ATFDrive::HasLoadingWork() const
{
    if(!IsValid(Docked)||!Docked->Enabled||DepartRequested)return false;
    if(IsFluid)
    {
        for(int32 K=0;K<Compartments;++K)
        {
            const auto Resource=TankResources[K];float Room=0;
            const float Available=Docked->WaitFull?Docked->Tanks[K].Input:FMath::Min(Docked->Tanks[K].Input,FluidSnapshot.IsValidIndex(K)?FluidSnapshot[K]:0.f);
            if(Available>.001f&&CabinRoom(Resource,K)>.001f&&ChooseRoute(Resource,Docked,Room,K)!=INDEX_NONE)return true;
        }
    }
    else
    {
        TArray<FInventoryStack> Stacks;if(Docked->WaitFull)Docked->Incoming->GetInventoryStacks(Stacks);else Stacks=Snapshot;
        for(const auto& Q:Stacks)if(Q.HasItems()&&!Q.Item.HasState())
        {
            const auto Resource=Q.Item.GetItemClass();float Room=0;
            if(Docked->Incoming->GetNumItems(Resource)>0&&CabinRoom(Resource,0)>=1&&ChooseRoute(Resource,Docked,Room)!=INDEX_NONE)return true;
        }
    }
    return false;
}
bool ATFDrive::AtCapacity() const
{
    if(!IsFluid)
    {
        if(CargoSlots()<FTFStats::ForTier(Tier).Stacks) return false;
        TMap<TSubclassOf<UFGItemDescriptor>,int32> Counts;for(const auto& C:Cargo) Counts.FindOrAdd(C.Resource)+=FMath::RoundToInt(C.Amount);
        for(const auto& P:Counts) if(P.Value%TF::StackSize(P.Key)!=0) return false;
        return true;
    }
    bool Any=false;
    for(int32 K=0;K<Compartments;++K)
    {
        if(!TankResources[K]) continue;
        bool Participates=FluidSnapshot.IsValidIndex(K)&&FluidSnapshot[K]>TF::Epsilon;
        for(const auto& P:Docked->Ports) if(P.Input&&P.Compartment==K) Participates=true;
        if(!Participates) continue;
        Any=true;if(CabinRoom(TankResources[K],K)>.001f) return false;
    }
    return Any;
}
bool ATFDrive::SafeCompartments(int32 Count)
{
    if((Count!=1&&Count!=2&&Count!=4)||!Cargo.IsEmpty()) return false;
    for(auto S:Stops) if(IsValid(S)&&S->HasContents()) return false;
    Compartments=Count;TankResources.SetNum(4);
    for(auto S:Stops) if(IsValid(S)) {for(auto& P:S->Ports) P.Compartment=FMath::Min(P.Compartment,Count-1); S->ApplyConfig();}
    return true;
}
bool ATFDrive::SetTankResource(int32 Index,TSubclassOf<UFGItemDescriptor> Resource)
{
    if(!IsFluid||Index<0||Index>=Compartments||(Resource&&!TF::Fluid(Resource))) return false;
    if(TankResources[Index]==Resource) return true;
    if(CargoAmount(Index)>TF::Epsilon) return false;
    for(auto S:Stops) if(IsValid(S))
    {
        if(S->Tanks[Index].Input+S->Tanks[Index].Output>TF::Epsilon) return false;
        for(int32 P=0;P<S->Pipes.Num();++P) if(S->Ports[P].Compartment==Index && (S->Pipes[P]->IsConnected()||S->Pipes[P]->Content()>TF::Epsilon)) return false;
    }
    TankResources[Index]=Resource;for(auto S:Stops) if(IsValid(S)) S->Tanks[Index].Resource=Resource;
    return true;
}
void ATFDrive::UpdateDisplay()
{
    if(GetNetMode()==NM_DedicatedServer)return;
    if(Display){Display->SetText(FText::FromString(FString::Printf(TEXT("IRONWORKS / MK%d\n%s\n%.0fm"),Tier,*Status,CabinHeight/100.f)));
    Display->SetTextRenderColor(Powered?FColor(255,196,104):FColor(255,90,50));}
    const float Top=AutoShaftLayout.Height;
    if(HoistHead)HoistHead->SetRelativeLocation(FVector(0,0,Top-190));
    if(HoistDrum)HoistDrum->SetRelativeLocation(FVector(0,0,Top-95));
    if(FreightCrates)FreightCrates->SetVisibility(!IsFluid&&CargoAmount()>0);
    for(int32 I=0;I<Screens.Num();++I)if(Screens[I])Screens[I]->SetVisibility(!ScreenAtStop(this,I*200+100));
    for(int32 I=0;I<PressureTanks.Num();++I)if(auto* C=PressureTanks[I].Get())
    {
        C->SetVisibility(IsFluid&&I<Compartments);
        const float Scale=Compartments==1?2.35f:Compartments==2?1.25f:1.f;
        C->SetRelativeScale3D(FVector(Scale,Scale,1));
        C->SetRelativeLocation(Compartments==1?FVector::ZeroVector:Compartments==2?FVector(I?120:-120,0,0):FVector((I&1)?120:-120,(I&2)?120:-120,0));
    }
}
void ATFDrive::Tick(float Delta)
{
    Super::Tick(Delta);
    // Clients derive the same geometry from replicated floors and existing shafts.
    if(!HasAuthority()){AutoShaftTimer+=Delta;if(AutoShaftTimer>=1){AutoShaftTimer=0;RefreshAutoShaft();}}
    if(GetNetMode()!=NM_DedicatedServer&&Cabin)
    {
        SetActorTickInterval(0);
        const float VisualHeight=FMath::FInterpTo(Cabin->GetRelativeLocation().Z,CabinHeight,Delta,18.f);
        Cabin->SetRelativeLocation(FVector(0,0,VisualHeight));
        GateOpen=FMath::FInterpConstantTo(GateOpen,DoorPhase,Delta,1.f/TF::DoorSeconds);AnimateGates(Gates,TF::Ease(GateOpen),0,1);
        for(int32 I=0;I<TankLevels.Num();++I)if(auto* Level=TankLevels[I].Get())
        {
            const float Fill=I<Compartments?FMath::Clamp(CargoAmount(I)/(FTFStats::ForTier(Tier).Fluid/Compartments),0.f,1.f):0.f;
            VisualTankFill[I]=FMath::FInterpTo(VisualTankFill[I],Fill,Delta,5.f);
            Level->SetVisibility(IsFluid&&I<Compartments&&VisualTankFill[I]>.001f);
            Level->SetRelativeScale3D(FVector(1,1,FMath::Max(.001f,VisualTankFill[I])));
            Level->SetRelativeLocation(FVector(0,0,310.f));
        }
        const float Top=AutoShaftLayout.Height;
        if(LiftCables){const float Start=VisualHeight+600;LiftCables->SetRelativeLocation(FVector(0,0,Start));LiftCables->SetRelativeScale3D(FVector(1,1,FMath::Max(0.f,Top-95-Start)/100));}
        if(HoistDrum)HoistDrum->SetRelativeRotation(FRotator(0,0,VisualHeight/70*180/PI));
    }
    VisualTimer+=Delta;if(VisualTimer>.15f) {VisualTimer=0;UpdateDisplay();}
}
void ATFDrive::PreFactoryTick(AFGBuildableSubsystem*,float)
{
    if(!HasAuthority()) return;
    const auto S=FTFStats::ForTier(Tier);
    const bool Active=IsValid(Target)||IsValid(Docked);
    const float Demand=Active&&!Paused?S.ActiveMW:S.IdleMW;
    const auto* Circuit=Power->GetPowerCircuit();
    // Release demand during a blackout so fuel generators can finish restarting.
    // A battery-backed circuit remains usable even with no generation online.
    const bool CircuitReady=Circuit && !PowerInfo->IsFuseTriggered() && (Circuit->GetPowerProductionCapacity()>.001f || Circuit->GetBatterySumPowerStore()>.001f);
    PowerInfo->SetTargetConsumption(CircuitReady?Demand:0);
}
void ATFDrive::PostFactoryTick(AFGBuildableSubsystem*,float Delta)
{
    if(!HasAuthority())return;
    DiscoveryTimer+=Delta;if(DiscoveryTimer>1){DiscoveryTimer=0;RefreshStops();}
    const auto S=FTFStats::ForTier(Tier);
    const float Demand=(IsValid(Target)||IsValid(Docked))&&!Paused?S.ActiveMW:S.IdleMW;
    const auto* GameState=GetWorld()->GetGameState<AFGGameState>();
    Powered=QABypassPower || (GameState&&GameState->GetCheatNoPower()) || (PowerInfo->HasPower()&&!PowerInfo->IsFuseTriggered()&&PowerInfo->GetActualConsumption()>=Demand-.01f);
    if(!Powered) {Status=TEXT("NO POWER");return;}
    if(Paused) {Status=TEXT("PAUSED");return;}
    // The authoritative door interlock runs on dedicated servers too.
    // A full tick at the endpoint lets visual interpolation settle before transfer or travel.
    if(IsValid(Target) || !IsValid(Docked))
    {
        if(DoorPhase>0){DoorPhase=FMath::Max(0.f,DoorPhase-Delta/TF::DoorSeconds);if(DoorPhase<KINDA_SMALL_NUMBER)DoorPhase=0;Status=TEXT("CLOSING GATES");return;}
    }
    if(IsValid(Target))
    {
        const float H=Target->Height();
        if(!TravelActive){TravelStart=CabinHeight;TravelElapsed=0;TravelActive=true;}
        TravelElapsed+=Delta;
        CabinHeight=TravelStart+FMath::Sign(H-TravelStart)*TF::TravelPosition(TravelElapsed,FMath::Abs(H-TravelStart),S.Speed);
        Status=TEXT("IN TRANSIT");
        if(FMath::Abs(CabinHeight-H)<.01f){CabinHeight=H;Arrive(Target);}
        return;
    }
    TravelActive=false;
    if(IsValid(Docked))
    {
        GateStop=Docked;
        if(DoorPhase<1){DoorPhase=FMath::Min(1.f,DoorPhase+Delta/TF::DoorSeconds);if(DoorPhase>1.f-KINDA_SMALL_NUMBER)DoorPhase=1;Status=TEXT("OPENING GATES");return;}
        TransferCredit=FMath::Min(TransferCredit+Delta*(IsFluid?S.FluidRate:S.ItemRate),IsFluid?S.FluidRate:S.ItemRate);
        float Budget=TransferCredit;const bool Unloaded=Unload(Budget);
        bool StillUnloading=false;for(const auto& C:Cargo) if(C.Destination==Docked) StillUnloading=true;
        if(!StillUnloading) Load(Budget);
        TransferCredit=Budget;
        if(StillUnloading) {Status=Unloaded?TEXT("UNLOADING"):TEXT("OUTPUT BLOCKED");return;}
        Dwell+=Delta;
        if(Docked->WaitFull)
        {
            if(!DepartRequested&&!AtCapacity()&&Dwell<Docked->MaxWait){Status=FString::Printf(TEXT("FILLING / %.0fs"),Docked->MaxWait-Dwell);return;}
        }
        else if(HasLoadingWork()){Status=TEXT("LOADING SNAPSHOT");return;}
        // Snapshot is consumed, full, blocked, or timed out. Continue serving other floors.
        auto* Previous=Docked.Get(); Target=NextStop(); Docked=nullptr;
        if(Target==Previous) {Target=nullptr;Status=TEXT("WAITING FOR ROUTABLE CARGO");}
        DepartRequested=false;
    }
    else Target=NextStop();
    Status=Target?TEXT("DEPARTING"):TEXT("READY");
}
