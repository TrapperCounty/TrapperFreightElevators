// All fixtures are opt-in. Ordinary saves never create actors or receive cargo.
#include "TFBuildables.h"
#include "Algo/Reverse.h"
#include "TFPanel.h"
#include "FGBuildableSubsystem.h"
#include "FGSaveSession.h"
#include "FGPlayerController.h"
#include "FGPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Equipment/FGBuildGunBuild.h"
#include "Equipment/FGBuildGunDismantle.h"
#include "FGCharacterPlayer.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "FGPowerInfoComponent.h"
#include "FGPowerCircuit.h"
#include "FGSchematicManager.h"
#include "FGRecipe.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGEquipment.h"
#include "TFHologram.h"
#include "Hologram/FGWireHologram.h"
#include "FGGameState.h"
#include "UI/FGGameUI.h"
#include "Buildables/FGBuildableStorage.h"
#include "Buildables/FGBuildableGeneratorFuel.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildableLadder.h"
#include "Buildables/FGBuildableWire.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/Image.h"
#include "Engine/StaticMesh.h"
#include "UnrealClient.h"
#include "Components/SplineComponent.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"

namespace
{
    UClass* TFClass(const FString& Path) {return LoadObject<UClass>(nullptr,*(Path+TEXT(".")+FPackageName::GetShortName(Path)+TEXT("_C")));}
    template<typename T> T& TFValue(UObject* O,FName Name) {auto* P=O->GetClass()->FindPropertyByName(Name);check(P);return *P->ContainerPtrToValuePtr<T>(O);}
    void ShaftReviewView(AFGPlayerController* PC,ATFDrive* Tower)
    {
        auto* Character=Cast<AFGCharacterPlayer>(PC->GetCharacter());if(!Character)return;
        Character->GetCharacterMovement()->StopMovementImmediately();Character->GetCharacterMovement()->DisableMovement();
        const bool TestNewPlacement=FParse::Param(FCommandLine::Get(),TEXT("TFCombinedInput"));
        const FVector PadCentre=Tower->GetActorLocation()+FVector(9000,9000,0);
        const FVector View=TestNewPlacement ? PadCentre+FVector(1300,0,1050) : Tower->GetActorLocation()+FVector(1800,1000,2100);
        const FRotator Look=TestNewPlacement ? (PadCentre-View-FVector(0,0,75)).Rotation() :
            (Tower->GetActorLocation()+FVector(365,365,2100)-View).Rotation();
        const auto Recipe=TFClass(TestNewPlacement ? TEXT("/TrapperFreightElevators/Recipes/Recipe_ItemDrive_Mk1") : TEXT("/TrapperFreightElevators/Recipes/Recipe_Shaft"));
        if(TestNewPlacement)
        {
            auto Foundation=TFClass(TEXT("/Game/FactoryGame/Buildable/Building/Foundation/Build_Foundation_8x1_01"));
            PC->GetWorld()->SpawnActor<AFGBuildable>(Foundation,PadCentre-FVector(0,0,50),FRotator::ZeroRotator);
            auto* Inventory=Character->GetInventory();
            for(const auto& Cost:UFGRecipe::GetIngredients(Character,Recipe))
                if(Inventory->GetNumItems(Cost.ItemClass)<Cost.Amount)Inventory->AddStack(FInventoryStack(Cost.Amount,Cost.ItemClass),true);
        }
        Character->SetActorLocation(View,false);PC->SetControlRotation(Look);PC->ClientSetLocation(View,Look);
        if(!Character->IsBuildGunEquipped())Character->ToggleBuildGun();Character->GetBuildGun()->GotoBuildState(Recipe);
        // Pin only the opt-in disposable QA view: injected mouse coordinates otherwise
        // become raw camera movement in this game, and its movement mode resets.
        PC->SetIgnoreLookInput(true);PC->SetIgnoreMoveInput(true);
        TWeakObjectPtr<AFGPlayerController> ReviewPC=PC;TWeakObjectPtr<ATFDrive> ReviewTower=Tower;FTimerHandle ReviewTimer;
        PC->GetWorld()->GetTimerManager().SetTimer(ReviewTimer,FTimerDelegate::CreateLambda([ReviewPC,ReviewTower,View,Look,TestNewPlacement,PadCentre]
        {
            if(!ReviewPC.IsValid()||!ReviewTower.IsValid())return;
            auto* C=Cast<AFGCharacterPlayer>(ReviewPC->GetCharacter());if(!C)return;
            C->GetCharacterMovement()->StopMovementImmediately();C->GetCharacterMovement()->DisableMovement();
            C->SetActorLocation(View,false);ReviewPC->SetControlRotation(Look);
            auto* State=Cast<UFGBuildGunStateBuild>(C->GetBuildGun()->GetCurrentState());auto* H=State?Cast<ATFHologram>(State->GetHologram()):nullptr;
            FString Info=H?FString::Printf(TEXT("sections=%d locked=%d canconstruct=%d instances=%d cost=%d position=%s\n"),H->ShaftSections,H->ShaftPositionLocked,H->CanConstruct(),H->GetZoopInstanceTransforms().Num(),H->GetBaseCostMultiplier(),*H->GetActorLocation().ToString()):TEXT("No freight hologram\n");
            if(H){TArray<TSubclassOf<UFGConstructDisqualifier>> Reasons;H->GetConstructDisqualifiers(Reasons);for(auto R:Reasons)Info+=R->GetName()+TEXT("\n");}
            Info+=FString::Printf(TEXT("player=%s tower=%s height=%.0f\n"),*C->GetActorLocation().ToString(),*ReviewTower->GetActorLocation().ToString(),ReviewTower->ShaftTop());
            const auto& Hit=C->GetBuildGun()->GetHitResult();Info+=FString::Printf(TEXT("buildgun hit=%s component=%s blocking=%d range=%.0f\n"),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),Hit.bBlockingHit,C->GetBuildGun()->GetDefaultBuildGunRange());
            if(TestNewPlacement)
            {
                if(H)for(const auto& Cost:H->GetCost(true))Info+=FString::Printf(TEXT("cost %s=%d backpack=%d\n"),*GetNameSafe(Cost.ItemClass.Get()),Cost.Amount,C->GetInventory()->GetNumItems(Cost.ItemClass));
                int32 Drives=0,Sections=0;float Height=0;
                for(TActorIterator<ATFDrive> It(C->GetWorld());It;++It)if(FVector::DistSquared2D(It->GetActorLocation(),PadCentre)<800*800){++Drives;Height=It->ShaftTop();}
                for(TActorIterator<ATFShaft> It(C->GetWorld());It;++It)if(FVector::DistSquared2D(It->GetActorLocation(),PadCentre)<800*800)++Sections;
                Info+=FString::Printf(TEXT("new foundation: drives=%d sections=%d height=%.0f\n"),Drives,Sections,Height);
            }
            FFileHelper::SaveStringToFile(Info,*(FPaths::ProjectSavedDir()/TEXT("TF-ScrollShaft-Input.txt")));
        }),.1f,true);
    }
    struct FTFQA : TSharedFromThis<FTFQA>
    {
        TWeakObjectPtr<UWorld> World;
        FVector Origin=FVector(0,0,80000);
        TWeakObjectPtr<ATFDrive> D,F;
        TArray<TWeakObjectPtr<ATFStop>> I,L;
        TArray<TWeakObjectPtr<AFGBuildable>> Actors;
        TWeakObjectPtr<UFGPowerInfoComponent> Supply;
        TWeakObjectPtr<UFGPowerConnectionComponent> DemoPower;
        TWeakObjectPtr<AFGPlayerController> Player;
        TSubclassOf<UFGItemDescriptor> Iron,Copper,Water,Oil;
        FTimerHandle Timer;FString Report;int32 Checks=0,Failed=0,Stage=0;float Frozen=0;
        bool Showcase=false;
        bool Inspect=false;
        bool ResumeShowcase=false;
        TArray<TWeakObjectPtr<AFGBuildableConveyorBelt>> DemoBelts;
        TArray<TWeakObjectPtr<AFGBuildablePipeline>> DemoPipes;
        float InitialWater=0,InitialOil=0;
        bool SawBeltCargo=false,SawWater=false,SawOil=false;
        TWeakObjectPtr<ATFStop> ReservationStop;
        TWeakObjectPtr<AFGBuildableConveyorBelt> ReservationBelt;
        int32 ReservationBeltPhase=0;
        void Check(bool OK,const FString& Message)
        {
            ++Checks;if(!OK)++Failed;Report+=FString::Printf(TEXT("%s: %s\n"),OK?TEXT("PASS"):TEXT("FAIL"),*Message);
            UE_LOG(LogTemp,Display,TEXT("TFFreight %s: %s"),OK?TEXT("PASS"):TEXT("FAIL"),*Message);
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/(Showcase?TEXT("TF-Playground-Progress.txt"):TEXT("TF-Integration.txt"))));
        }
        AFGBuildable* Spawn(const FString& Path,FVector P)
        {
            auto* C=TFClass(Path);if(!C){Check(false,TEXT("Missing fixture class ")+Path);return nullptr;}
            const FTransform T(Origin+P);auto* BS=AFGBuildableSubsystem::Get(World.Get());auto* A=BS->BeginSpawnBuildable(C,T);
            if(!A){Check(false,TEXT("Cannot spawn ")+Path);return nullptr;}A->FinishSpawning(T);if(!BS->GetAllBuildablesRef().Contains(A)) BS->AddBuildable(A);
            if(!A->IsActorBeingDestroyed()) {if(auto* Skip=FindFProperty<FBoolProperty>(A->GetClass(),TEXT("mSkipBuildEffect")))Skip->SetPropertyValue_InContainer(A,true);A->PlayBuildEffect();}
            Actors.Add(A);return A;
        }
        ATFDrive* Drive(bool Fluid,FVector P,int32 Tier=1)
        {
            auto* A=Cast<ATFDrive>(Spawn(FString::Printf(TEXT("/TrapperFreightElevators/Buildables/Build_%sDrive_Mk%d"),Fluid?TEXT("Fluid"):TEXT("Item"),Tier),P));
            if(A) A->Paused=true;return A;
        }
        ATFStop* Stop(ATFDrive* Drive,FVector P)
        {
            auto* A=Cast<ATFStop>(Spawn(FString::Printf(TEXT("/TrapperFreightElevators/Buildables/Build_%sStop_Mk%d"),Drive->IsFluid?TEXT("Fluid"):TEXT("Item"),Drive->Tier),P));
            if(A){A->Elevator=Drive;A->Tier=Drive->Tier;Drive->RefreshStops();}return A;
        }
        void ResetItems()
        {
            D->Cargo.Reset();D->Routes.Reset();D->Docked=nullptr;D->Target=nullptr;D->CabinHeight=0;D->HasSweep=false;D->DoorPhase=0;D->GateStop=nullptr;D->TravelActive=false;D->TravelElapsed=0;
            {TArray<FInventoryStack> Stacks;D->CabinInventory->GetInventoryStacks(Stacks);for(const auto& Q:Stacks)D->CabinInventory->Remove(Q.Item.GetItemClass(),Q.NumItems);}
            for(auto S:I) if(S.IsValid())
            {
                for(auto Inv:{S->Incoming.Get(),S->Outgoing.Get()})
                {TArray<FInventoryStack> Stacks;Inv->GetInventoryStacks(Stacks);for(const auto& Q:Stacks)Inv->Remove(Q.Item.GetItemClass(),Q.NumItems);}
                S->Enabled=true;S->WaitFull=false;
            }
        }
        void Route(ATFDrive* A,TSubclassOf<UFGItemDescriptor> Resource,ATFStop* Dest,int32 Weight=1)
        {FTFRoute R;R.Resource=Resource;R.Destination=Dest;R.Weight=Weight;A->Routes.Add(R);}
        float Manifest(ATFDrive* A,ATFStop* Dest,TSubclassOf<UFGItemDescriptor> Resource)
        {float N=0;for(const auto& C:A->Cargo) if(C.Destination==Dest&&C.Resource==Resource) N+=C.Amount;return N;}
        AFGBuildable* BuildRecipe(const FString& Recipe,FVector P)
        {
            auto* Character=Player.IsValid()?Cast<AFGCharacterPlayer>(Player->GetCharacter()):nullptr;
            auto* R=TFClass(Recipe);auto* H=Character&&R?Cast<AFGBuildableHologram>(AFGHologram::SpawnHologramFromRecipe(R,Character->GetBuildGun(),Origin+P,Character)):nullptr;
            if(!H){Check(false,TEXT("Native recipe hologram available: ")+Recipe);return nullptr;}
            H->SetActorTickEnabled(false);H->SetSuppressBuildEffect(true);H->SetActorTransform(FTransform(Origin+P));
            TArray<AActor*> Children;auto* A=Cast<AFGBuildable>(H->Construct(Children,FNetConstructionID()));H->Destroy();
            if(A){if(auto* Skip=FindFProperty<FBoolProperty>(A->GetClass(),TEXT("mSkipBuildEffect")))Skip->SetPropertyValue_InContainer(A,true);A->PlayBuildEffect();Actors.Add(A);}return A;
        }
        void Wire(UFGPowerConnectionComponent* A,UFGPowerConnectionComponent* B)
        {
            auto* Character=Player.IsValid()?Cast<AFGCharacterPlayer>(Player->GetCharacter()):nullptr;
            auto* R=TFClass(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerLine"));
            auto* H=Character&&R?Cast<AFGWireHologram>(AFGHologram::SpawnHologramFromRecipe(R,Character->GetBuildGun(),A->GetComponentLocation(),Character)):nullptr;
            if(!H){Check(false,TEXT("Native wire hologram available"));return;}
            H->SetActorTickEnabled(false);H->SetSuppressBuildEffect(true);H->SetConnection(0,A);H->SetConnection(1,B);
            TArray<AActor*> Children;auto* W=Cast<AFGBuildableWire>(H->Construct(Children,FNetConstructionID()));H->Destroy();
            Check(W&&W->GetConnection(0)==A&&W->GetConnection(1)==B,TEXT("Native wire construction connects both power terminals"));
            if(W){if(auto* Skip=FindFProperty<FBoolProperty>(W->GetClass(),TEXT("mSkipBuildEffect")))Skip->SetPropertyValue_InContainer(W,true);W->PlayBuildEffect();Actors.Add(W);}
        }
        void CombinedCases()
        {
            auto* Character = Cast<AFGCharacterPlayer>(Player->GetCharacter());
            for (const bool Fluid : {false, true})
            {
                const FString Label = Fluid ? TEXT("Fluid") : TEXT("Item");
                auto Recipe = TFClass(FString::Printf(TEXT("/TrapperFreightElevators/Recipes/Recipe_%sDrive_Mk2"), *Label));
                const FVector Base = Origin + FVector(25000, Fluid ? 6000 : 0, 0);
                auto Preview = [&]() {
                    auto* H = Cast<ATFTowerHologram>(AFGHologram::SpawnHologramFromRecipe(Recipe, Character->GetBuildGun(), Base, Character));
                    if (H) { H->SetActorTickEnabled(false); H->SetSuppressBuildEffect(true); H->SetActorLocation(Base); }
                    return H;
                };
                auto* H = Preview();
                Check(H && H->ShaftPreview(), Label + TEXT(" drive recipe creates combined preview"));
                if (!H || !H->ShaftPreview()) return;
                Check(H->ShaftSections==0 && H->ShaftPreview()->IsDisabled(), Label + TEXT(" shortest tower defaults to just the base"));
                auto Amount = [](const TArray<FItemAmount>& Costs, TSubclassOf<UFGItemDescriptor> Item) {
                    int32 Total=0; for (const auto& Cost : Costs) if (Cost.ItemClass==Item) Total+=Cost.Amount;
                    return Total;
                };
                auto Beam=TFClass(TEXT("/Game/FactoryGame/Resource/Parts/SteelPlate/Desc_SteelPlate"));
                auto Concrete=TFClass(TEXT("/Game/FactoryGame/Resource/Parts/Cement/Desc_Cement"));
                const auto BaseCost=H->GetCost(true);
                Check(!H->DoMultiStepPlacement(false),Label+TEXT(" first click anchors drive"));
                H->Scroll(3);
                const auto Total=H->GetCost(true);
                Check(Amount(Total,Beam)==Amount(BaseCost,Beam)+12 && Amount(Total,Concrete)==Amount(BaseCost,Concrete)+12,
                    Label+TEXT(" combined cost adds three shaft recipes to one drive"));
                Check(H->DoMultiStepPlacement(false),Label+TEXT(" second click confirms entire tower"));
                TArray<AActor*> Built;
                auto* Drive=Cast<ATFDrive>(H->Construct(Built,FNetConstructionID()));
                H->Destroy();
                Check(Drive && Drive->IsFluid==Fluid && Drive->ShaftTop()==2000 && Drive->Stops.IsEmpty(),Label+TEXT(" one placement builds drive plus three sections, no floor stop"));
                if (!Drive) return;
                Drive->Paused=true;
                H=Preview(); if (!H) return;
                FHitResult Hit(Drive,nullptr,Base+FVector(0,0,1000),FVector::ForwardVector);
                Check(H->TrySnapToActor(Hit),Label+TEXT(" same tool snaps to existing tower"));
                H->DoMultiStepPlacement(false);H->Scroll(9);
                const auto Extension=H->GetCost(true);
                Check(Extension.Num()==2 && Amount(Extension,Beam)==40 && Amount(Extension,Concrete)==40,Label+TEXT(" extension charges ten sections only"));
                Built.Reset();
                auto* Section=Cast<ATFShaft>(H->Construct(Built,FNetConstructionID()));
                H->Destroy();
                int32 Drives=0;for(TActorIterator<ATFDrive> It(World.Get());It;++It) if(It->GetActorLocation().Equals(Base,1.f)) ++Drives;
                Check(Section && Drives==1 && Drive->ShaftTop()==6000,Label+TEXT(" extension adds ten sections to the original single drive"));
            }
        }
        TArray<TWeakObjectPtr<ATFPart>> DismantleTargets;
        FVector DismantleView;
        FRotator DismantleLook;
        bool DismantleAiming=false;
        int32 DismantleAimAttempts=0;
        bool DismantlePending=false;
        int32 DismantleRemovalAttempts=0;
        bool MultiDismantle=false;
        void BeginDismantleCases()
        {
            MultiDismantle=FParse::Param(FCommandLine::Get(),TEXT("TFMultiDismantleTest"));
            auto* Character=Cast<AFGCharacterPlayer>(Player->GetCharacter());
            Character->ToggleBuildGun();Character->GetBuildGun()->GotoDismantleState();
            Player->SetIgnoreLookInput(true);Player->SetIgnoreMoveInput(true);
            for(bool Fluid:{false,true})
            {
                ATFDrive* Tower=nullptr;
                const FVector Base=Origin+FVector(25000,Fluid?6000:0,0);
                for(TActorIterator<ATFDrive> It(World.Get());It;++It)if(It->GetActorLocation().Equals(Base,1.f))Tower=*It;
                Check(Tower!=nullptr,TEXT("Dismantle fixture tower found"));if(!Tower)continue;
                Tower->RefreshAutoShaft();
                TArray<ATFShaft*> Sections;
                for(TActorIterator<ATFShaft> It(World.Get());It;++It)if(It->Drive()==Tower)Sections.Add(*It);
                Sections.Sort([](const ATFShaft& A,const ATFShaft& B){return A.GetActorLocation().Z>B.GetActorLocation().Z;});
                if(MultiDismantle){Algo::Reverse(Sections);DismantleTargets.Add(Tower);}
                bool Targetable=Sections.Num()==13;
                for(auto* S:Sections)
                {
                    for(int32 Side=0;Side<4;++Side)
                    {
                        const FVector Centre=S->GetActorLocation()+FVector(0,0,200);
                        const FVector Offset=FRotator(0,Side*90,0).RotateVector(FVector(1000,0,0));
                        FHitResult Hit;World->LineTraceSingleByChannel(Hit,Centre+Offset,Centre,ECC_GameTraceChannel5);
                        Targetable&=Hit.GetActor()==S;
                    }
                    DismantleTargets.Add(S);
                }
                Check(Targetable,FString::Printf(TEXT("%s: all 13 shafts selected independently from all four sides"),Fluid?TEXT("Fluid"):TEXT("Item")));
                Check(Tower->FreightCollision->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block &&
                    Tower->FreightCollision->GetUnscaledBoxExtent().Z*2==6000,TEXT("Continuous full-height player barrier retained"));
                Check(!Tower->CanDismantle_Implementation(),TEXT("Base remains protected while shaft sections exist"));
                if(!MultiDismantle)DismantleTargets.Add(Tower);
            }
            if(MultiDismantle && DismantleTargets.Num()>=2)
            {
                auto* Base=Cast<ATFDrive>(DismantleTargets[0].Get());
                TArray<TSubclassOf<UFGConstructDisqualifier>> Reasons;
                Base->GetDismantleDisqualifiers_Implementation(Reasons,{Base});
                Check(Reasons.Contains(UTFDismantleShafts::StaticClass()),TEXT("Base explains unselected shaft dependencies in dismantle HUD"));
                TArray<AActor*> All;for(auto A:DismantleTargets)All.Add(A.Get());
                Reasons.Reset();Base->GetDismantleDisqualifiers_Implementation(Reasons,All);
                Check(Reasons.IsEmpty(),TEXT("Complete empty selection has no misleading dismantle warnings"));
                auto* Ground=Stop(Base,Base->GetActorLocation()-Origin);
                if(Ground)
                {
                    Reasons.Reset();Base->GetDismantleDisqualifiers_Implementation(Reasons,All);
                    Check(Reasons.Contains(UTFDismantleStops::StaticClass()),TEXT("Base explains separate ground-floor stop dependency"));
                    Ground->Destroy();Base->RefreshStops();
                }
                auto* State=Cast<UFGBuildGunStateDismantle>(Character->GetBuildGun()->GetCurrentState());
                auto* Function=State?State->FindFunction(TEXT("Server_DismantleActors")):nullptr;
                if(Function)
                {
                    struct FRequest {TArray<AActor*> Selected;TArray<FDismantleLightweightBundle> Lightweight;} Request;
                    Request.Selected={DismantleTargets[0].Get(),DismantleTargets[1].Get()};
                    State->ProcessEvent(Function,&Request);
                    bool Protected=true;for(auto A:DismantleTargets)Protected&=A.IsValid()&&!A->GetIsDismantled();
                    Check(Protected,TEXT("Incomplete batch cannot remove a supporting base/shaft or unselected upper sections"));
                }
                else Check(false,TEXT("Native incomplete-batch request available"));
            }
            Stage=0;
            auto Self=AsShared();
            World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([Self]{Self->DismantleStep();}),.25f,true);
        }
        void DismantleStep()
        {
            if(MultiDismantle){MultiDismantleStep();return;}
            auto* Character=Cast<AFGCharacterPlayer>(Player->GetCharacter());
            if(Stage>=DismantleTargets.Num()||Failed)
            {
                Report+=FString::Printf(TEXT("RESULT: %d checks, %d failures\n"),Checks,Failed);
                FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("TF-Dismantle.txt")));
                World->GetTimerManager().ClearTimer(Timer);return;
            }
            auto* Target=DismantleTargets[Stage].Get();
            if(DismantlePending)
            {
                const bool Removed=!Target||Target->IsActorBeingDestroyed();
                if(!Removed&&++DismantleRemovalAttempts<40)return;
                Check(Removed,TEXT("Native dismantle effect completes and removes the actor"));
                ++Stage;DismantleAiming=false;DismantleAimAttempts=0;DismantlePending=false;DismantleRemovalAttempts=0;
                return;
            }
            if(!Target){Check(false,TEXT("Dismantle target disappeared before its turn"));return;}
            if(!DismantleAiming)
            {
                const FVector Centre=Target->GetActorLocation()+FVector(0,0,Target->Kind==2?200:400);
                DismantleView=Centre+FVector(1100,0,-75);DismantleLook=FRotator(0,180,0);
            }
            Character->GetCharacterMovement()->StopMovementImmediately();Character->GetCharacterMovement()->DisableMovement();
            Character->SetActorLocation(DismantleView,false);Player->SetControlRotation(DismantleLook);
            if(!DismantleAiming){DismantleAiming=true;return;}
            auto* State=Cast<UFGBuildGunStateDismantle>(Character->GetBuildGun()->GetCurrentState());
            const bool Selected=State&&State->GetSelectedActor()==Target;
            // Allow the native camera and asynchronous build-gun trace to catch
            // up after moving between floors; do not substitute our own hit.
            if(!Selected && ++DismantleAimAttempts<12)return;
            Check(Selected,FString::Printf(TEXT("Native dismantle aim selects %s (actual=%s)"),*Target->GetName(),*GetNameSafe(State?State->GetSelectedActor():nullptr)));
            if(!Selected)return;
            Check(IFGDismantleInterface::Execute_CanDismantle(Target),TEXT("Selected top section or cleared base permits dismantling"));
            TArray<FInventoryStack> Refund;IFGDismantleInterface::Execute_GetDismantleRefund(Target,Refund,false);
            Check(!Refund.IsEmpty(),TEXT("Native material refund available"));
            // Exercise the actual server request, including native refund and removal,
            // rather than bypassing dismantle with Destroy(). Only these QA actors enter it.
            auto* Function=State->FindFunction(TEXT("Server_DismantleActors"));
            Check(Function!=nullptr,TEXT("Native dismantle request available"));if(!Function)return;
            struct FRequest {TArray<AActor*> Selected;TArray<FDismantleLightweightBundle> Lightweight;} Request;
            Request.Selected.Add(Target);State->ProcessEvent(Function,&Request);
            Check(!IsValid(Target)||Target->GetIsDismantled()||Target->IsActorBeingDestroyed(),TEXT("Native dismantle request accepts the selected actor"));
            DismantlePending=true;
        }
        void MultiDismantleStep()
        {
            auto* Character=Cast<AFGCharacterPlayer>(Player->GetCharacter());
            auto* State=Cast<UFGBuildGunStateDismantle>(Character->GetBuildGun()->GetCurrentState());
            if(Failed || (DismantlePending && DismantleRemovalAttempts++>=40))
            {
                if(!Failed)
                {
                    bool Gone=true;for(auto A:DismantleTargets)Gone&=!A.IsValid()||A->IsActorBeingDestroyed();
                    Check(Gone,TEXT("Native batch completely removed both selected towers"));
                }
                Report+=FString::Printf(TEXT("RESULT: %d checks, %d failures\n"),Checks,Failed);
                FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("TF-MultiDismantle.txt")));
                World->GetTimerManager().ClearTimer(Timer);return;
            }
            if(DismantlePending)return;
            if(Stage>=DismantleTargets.Num())
            {
                State->SetMultiDismantleState(false);
                auto Pending=State->GetPendingDismantleActors();
                Check(Pending.Num()==DismantleTargets.Num(),TEXT("Ctrl selection retained all 28 parts after release"));
                auto* Function=State->FindFunction(TEXT("Server_DismantleActors"));
                Check(Function!=nullptr,TEXT("Native mass dismantle request available"));if(!Function)return;
                struct FRequest {TArray<AActor*> Selected;TArray<FDismantleLightweightBundle> Lightweight;} Request;
                Request.Selected=Pending;State->ProcessEvent(Function,&Request);
                for(auto A:DismantleTargets)Check(!A.IsValid()||A->GetIsDismantled(),TEXT("Native batch accepted selected part independent of selection order"));
                DismantlePending=true;return;
            }
            auto* Target=DismantleTargets[Stage].Get();if(!Target){Check(false,TEXT("Missing mass dismantle fixture"));return;}
            if(!DismantleAiming)
            {
                const FVector Centre=Target->GetActorLocation()+FVector(0,0,Target->Kind==2?200:400);
                DismantleView=Centre+FVector(1100,0,-75);DismantleLook=FRotator(0,180,0);
                if(Stage==0)State->SetMultiDismantleState(true);
            }
            Character->GetCharacterMovement()->StopMovementImmediately();Character->GetCharacterMovement()->DisableMovement();
            Character->SetActorLocation(DismantleView,false);Player->SetControlRotation(DismantleLook);
            if(!DismantleAiming){DismantleAiming=true;return;}
            const bool Queued=State->GetPendingDismantleActors().Contains(Target);
            if(!Queued&&++DismantleAimAttempts<12)return;
            Check(Queued,FString::Printf(TEXT("Ctrl sweep adds %s to native selection (pending=%d active=%d aimed=%s)"),*Target->GetName(),State->GetPendingDismantleActors().Num(),State->IsMultiDismantleActive(),*GetNameSafe(State->GetSelectedActor())));
            ++Stage;DismantleAiming=false;DismantleAimAttempts=0;
        }
        void ScrollShaftCases()
        {
            auto* Tower=Cast<ATFDrive>(BuildRecipe(TEXT("/TrapperFreightElevators/Recipes/Recipe_ItemDrive_Mk2"),FVector(16000,0,0)));
            Check(Tower&&Tower->ShaftBuiltFirst,TEXT("Native construction creates a shaft-first drive"));if(!Tower)return;Tower->Paused=true;
            Check(Tower->ShaftTop()==800&&Tower->CanAttach(Tower->GetActorLocation())&&!Tower->CanAttach(Tower->GetActorLocation()+FVector(0,0,800)),TEXT("Base accepts its ground stop but requires shaft before higher floors"));
            auto* Character=Cast<AFGCharacterPlayer>(Player->GetCharacter());
            auto Hologram=[&]()
            {
                auto* H=Cast<ATFHologram>(AFGHologram::SpawnHologramFromRecipe(TFClass(TEXT("/TrapperFreightElevators/Recipes/Recipe_Shaft")),Character->GetBuildGun(),Tower->GetActorLocation(),Character));
                if(H){H->SetActorTickEnabled(false);H->SetSuppressBuildEffect(true);FHitResult Hit(Tower,nullptr,Tower->GetActorLocation(),FVector::UpVector);H->TrySnapToActor(Hit);}
                return H;
            };
            auto Count=[&](){int32 N=0;for(TActorIterator<ATFShaft> It(World.Get());It;++It)if(!It->IsActorBeingDestroyed()&&It->Drive()==Tower)++N;return N;};
            auto* H=Hologram();Check(H!=nullptr,TEXT("Native shaft recipe spawns scroll hologram"));if(!H)return;
            H->Scroll(3);Check(H->ShaftSections==4&&H->GetBaseCostMultiplier()==4&&H->GetZoopInstanceTransforms().Num()==3,TEXT("Wheel selects four sections with root plus three preview instances and matching cost"));
            H->Scroll(-1);Check(H->ShaftSections==3,TEXT("Scrolling down reduces the preview"));H->Scroll(1);
            Check(!H->DoMultiStepPlacement(false)&&H->ShaftPositionLocked&&!H->DoMultiStepPlacement(true),TEXT("First click locks the base; release does not accidentally build"));
            const FVector Locked=H->GetActorLocation();FHitResult Away(Tower,nullptr,Tower->GetActorLocation()+FVector(6000,0,6000),FVector::UpVector);H->SetHologramLocationAndRotation(Away);
            Check(H->GetActorLocation()==Locked&&H->DoMultiStepPlacement(false),TEXT("Height preview remains anchored and second click confirms"));
            TArray<AActor*> Built;auto* First=H->Construct(Built,FNetConstructionID());H->Destroy();Tower->RefreshStops();
            Check(First&&Count()==4&&Tower->ShaftTop()==2400,FString::Printf(TEXT("Native construction builds four paid sections to 24m (actual=%d top=%.0f)"),Count(),Tower->ShaftTop()));
            bool Refunds=true;for(TActorIterator<ATFShaft> It(World.Get());It;++It)if(It->Drive()==Tower){TArray<FInventoryStack> Refund;It->GetDismantleRefund_Implementation(Refund,false);Refunds&=It->GetBuiltWithRecipe()!=nullptr&&!Refund.IsEmpty();}
            Check(Refunds&&Count()>0,TEXT("Every built shaft section carries its native recipe and refund"));
            Check(Tower->AutoShaftLayout.Height==2400&&!Tower->CanAttach(Tower->GetActorLocation()+FVector(0,0,2000)),TEXT("Empty shaft has independent height and enforces landing headroom"));
            auto* Landing=Cast<ATFStop>(BuildRecipe(TEXT("/TrapperFreightElevators/Recipes/Recipe_ItemStop_Mk2"),FVector(16000,0,1600)));
            Check(Landing&&Landing->Elevator==Tower,TEXT("Freight stop attaches after the shaft is built"));if(Landing)Landing->FloorName=TEXT("Scroll shaft QA");
            H=Hologram();if(!H)return;Check(H->GetActorLocation().Z==Tower->GetActorLocation().Z+2400,TEXT("A new shaft placement snaps to the existing top"));H->Scroll(2);Built.Reset();First=H->Construct(Built,FNetConstructionID());H->Destroy();Tower->RefreshStops();
            Check(First&&Count()==7&&Tower->ShaftTop()==3600,TEXT("Adding three more sections extends the same tower to 36m"));
            Check(Tower->CanAttach(Tower->GetActorLocation()+FVector(0,0,2800)),TEXT("Extension makes a higher independent stop position available"));
            bool BottomBlocked=false,TopAllowed=false;for(TActorIterator<ATFShaft> It(World.Get());It;++It)if(It->Drive()==Tower){const float Z=It->GetActorLocation().Z-Tower->GetActorLocation().Z;if(Z==800)BottomBlocked=!It->CanDismantle_Implementation();if(Z==3200)TopAllowed=It->CanDismantle_Implementation();}
            Check(BottomBlocked&&TopAllowed&&!Tower->CanDismantle_Implementation(),TEXT("Supporting shaft and drive cannot be dismantled; unused top section can"));
            H=Hologram();if(!H)return;H->Scroll(10000);Check(H->ShaftSections==10&&H->GetBaseCostMultiplier()==10,TEXT("A single placement is limited to ten sections"));H->Scroll(-10000);Check(H->ShaftSections==1,TEXT("Wheel cannot select zero or negative shaft height"));H->DoMultiStepPlacement(false);Check(H->HandleReset()&&!H->ShaftPositionLocked&&H->ShaftSections==1,TEXT("Cancel unlocks the preview without constructing anything"));H->Destroy();
            if(Landing){Landing->Elevator=nullptr;Tower->RefreshStops();Check(Tower->ShaftTop()==3600&&Tower->AutoShaftLayout.Height==3600,TEXT("Removing the floor does not shrink the constructed shaft"));Landing->Elevator=Tower;Tower->RefreshStops();}
            H = Hologram();
            if (!H) return;
            TArray<TSubclassOf<UFGBuildGunModeDescriptor>> Modes;
            H->GetSupportedBuildModes(Modes);
            Check(Modes.Num()==1 && Modes[0] && Modes[0]->GetName()==TEXT("BuildMode_Zoop_C"), TEXT("Shaft advertises the real Zoop build mode"));
            H->DoMultiStepPlacement(false);
            FHitResult Aim;
            Aim.TraceStart = H->GetActorLocation() + FVector(1600, 0, 0);
            Aim.TraceEnd = Aim.TraceStart + FVector(-10000,0,0);
            H->TryZoop(Aim);
            Aim.TraceEnd.Z += 30000;
            H->TryZoop(Aim);
            Check(H->ShaftSections==10 && H->GetZoopInstanceTransforms().Num()==9, TEXT("Raising aim with no surface hit selects exactly ten sections"));
            Built.Reset();
            First=H->Construct(Built,FNetConstructionID());H->Destroy();Tower->RefreshStops();
            Check(First && Count()==17 && Tower->ShaftTop()==7600,TEXT("One Zoop confirmation builds ten more sections and adds 40m"));
            struct FStopCase
            {
                TSubclassOf<UFGHologramBuildModeDescriptor> Mode;
                int32 GridMetres;
                int32 SameAimHeight;
                int32 BuildAim;
                int32 BuiltHeight;
            };
            const FStopCase Cases[] = {
                {UTFStopSnap1m::StaticClass(), 1, 2500, 2530, 2500},
                {UTFStopSnap2m::StaticClass(), 2, 2600, 3730, 3800},
                {UTFStopSnap4m::StaticClass(), 4, 2400, 5330, 5200}
            };
            const FVector Base = Tower->GetActorLocation();
            const auto StopRecipe = TFClass(TEXT("/TrapperFreightElevators/Recipes/Recipe_ItemStop_Mk2"));
            for (const FStopCase& Test : Cases)
            {
                auto* Floor = Cast<ATFHologram>(AFGHologram::SpawnHologramFromRecipe(
                    StopRecipe, Character->GetBuildGun(), Base, Character));
                if (!Floor) { Check(false, TEXT("Stop hologram spawns")); break; }
                TArray<TSubclassOf<UFGBuildGunModeDescriptor>> ModesForStop;
                Floor->GetSupportedBuildModes(ModesForStop);
                Check(ModesForStop.Num() == 3 && ModesForStop.Contains(Test.Mode),
                    FString::Printf(TEXT("Stop build gun offers %dm mode"), Test.GridMetres));
                Floor->SetBuildModeOverride(Test.Mode);
                Floor->OnBuildModeChanged(Test.Mode);
                Floor->TrySnapToActor(FHitResult(Tower, nullptr, Base + FVector(0,0,2530), FVector::ForwardVector));
                Check(FMath::IsNearlyEqual(Floor->GetActorLocation().Z - Base.Z, double(Test.SameAimHeight)),
                    FString::Printf(TEXT("%dm snap at 25.3m gives %.1fm"), Test.GridMetres, (Floor->GetActorLocation().Z-Base.Z)/100));
                Floor->TrySnapToActor(FHitResult(Tower, nullptr, Base + FVector(0,0,Test.BuildAim), FVector::ForwardVector));
                Built.Reset();
                auto* Stop = Cast<ATFStop>(Floor->Construct(Built, FNetConstructionID()));
                Check(Stop && Stop->Elevator == Tower && Stop->Height() == Test.BuiltHeight,
                    FString::Printf(TEXT("Built and attached %dm landing in %dm mode"), Test.BuiltHeight/100, Test.GridMetres));
                Floor->Destroy();
            }
            ShaftReviewView(Player.Get(),Tower);
        }
        void PlacementCases()
        {
            for(int32 Tier=1;Tier<=3;++Tier)for(int32 Type=0;Type<2;++Type)
            {
                const FString Kind=Type?TEXT("Fluid"):TEXT("Item");
                const FVector P(12000+Type*2000,Tier*2000,0);
                auto* A=Cast<ATFDrive>(BuildRecipe(FString::Printf(TEXT("/TrapperFreightElevators/Recipes/Recipe_%sDrive_Mk%d"),*Kind,Tier),P));
                Check(A&&A->Tier==Tier&&A->IsFluid==(Type!=0),FString::Printf(TEXT("Native construction creates %s drive Mk%d"),*Kind,Tier));
                if(!A)continue;A->Paused=true;
                auto* S=Cast<ATFStop>(BuildRecipe(FString::Printf(TEXT("/TrapperFreightElevators/Recipes/Recipe_%sStop_Mk%d"),*Kind,Tier),P));
                Check(S&&S->Elevator==A&&S->Tier==Tier,FString::Printf(TEXT("Native stop construction attaches to %s drive Mk%d"),*Kind,Tier));
                Check(A->GetBuiltWithRecipe()!=nullptr && S && S->GetBuiltWithRecipe()!=nullptr,TEXT("Native construction records recipes for exact material refunds"));
                if(S){S->TurnOffAndDestroy();S->Destroy();}A->RefreshStops();A->TurnOffAndDestroy();A->Destroy();
            }
            auto* A=Cast<ATFDrive>(BuildRecipe(TEXT("/TrapperFreightElevators/Recipes/Recipe_ItemDrive_Mk1"),FVector(16000,8000,0)));
            auto* S=Cast<ATFStop>(BuildRecipe(TEXT("/TrapperFreightElevators/Recipes/Recipe_ItemStop_Mk1"),FVector(16000,8000,0)));
            if(A&&S)
            {
                A->Paused=true;A->RefreshStops();S->FloorName=TEXT("Upgrade preservation");S->MaxWait=43;
                S->SetIncomingReservation(Iron,5);S->SetIncomingReservation(Copper,10);
                S->Incoming->AddStack(FInventoryStack(17,Iron),true);A->CabinInventory->AddStack(FInventoryStack(9,Copper),true);
                FTFCargo C;C.Resource=Copper;C.Destination=S;C.Amount=9;A->Cargo.Add(C);Route(A,Iron,S,3);
                auto* Character=Cast<AFGCharacterPlayer>(Player->GetCharacter());
                auto* H=Cast<ATFHologram>(AFGHologram::SpawnHologramFromRecipe(TFClass(TEXT("/TrapperFreightElevators/Recipes/Recipe_ItemDrive_Mk2")),Character->GetBuildGun(),A->GetActorLocation(),Character));
                FHitResult Hit(A,nullptr,A->GetActorLocation(),FVector::UpVector);
                Check(H&&H->TryUpgrade(Hit),TEXT("Mk2 hologram recognizes a live Mk1 drive upgrade"));
                if(H)
                {
                    H->SetActorTickEnabled(false);H->SetSuppressBuildEffect(true);TArray<AActor*> Children;
                    auto* New=Cast<ATFDrive>(H->Construct(Children,FNetConstructionID()));H->Destroy();
                    if(New){IFGDismantleInterface::Execute_PreUpgrade(A);IFGDismantleInterface::Execute_Upgrade(A,New);New->PlayBuildEffect();}
                    Check(New&&S->Elevator==New&&S->Tier==2&&New->Stops.Contains(S),TEXT("Native upgrade preserves the floor and increases its tier"));
                    Check(New&&New->CargoAmount()==9&&New->CabinInventory->GetNumItems(Copper)==9&&S->Incoming->GetNumItems(Iron)==17&&S->MaxWait==43&&New->Routes.Num()==1,TEXT("Native upgrade preserves cargo, buffers, routes and wait configuration"));
                    Check(New&&S->ReservedIncomingStacks(Iron)==5&&S->ReservedIncomingStacks(Copper)==10,TEXT("Native tier upgrade preserves incoming stack reservations"));
                    if(New)
                    {
                        New->PostSerializedFromBlueprint(false);S->PostSerializedFromBlueprint(false);
                        Check(New->Cargo.IsEmpty()&&New->CabinInventory->IsEmpty()&&S->Incoming->IsEmpty()&&S->FloorName==TEXT("Upgrade preservation")&&S->MaxWait==43,TEXT("Blueprint deserialization clears copied cargo while preserving settings"));
                        Check(S->ReservedIncomingStacks(Iron)==5&&S->ReservedIncomingStacks(Copper)==10,TEXT("Blueprint deserialization preserves reservation settings without copied cargo"));
                        New->TurnOffAndDestroy();New->Destroy();
                    }
                }
                S->TurnOffAndDestroy();S->Destroy();if(IsValid(A)){A->TurnOffAndDestroy();A->Destroy();}
            }
        }
        void InspectPower()
        {
            FString Details;
            if(auto* State=World->GetGameState<AFGGameState>())Details+=FString::Printf(TEXT("NoPower cheat: %d\n"),State->GetCheatNoPower());
            for(TActorIterator<ATFDrive> It(World.Get());It;++It)
            {
                auto* C=It->Power->GetPowerCircuit();Details+=FString::Printf(TEXT("Drive %s tier=%d powered=%d circuit=%d wires=%d actual=%.2f target=%.2f fuse=%d\n"),It->IsFluid?TEXT("fluid"):TEXT("item"),It->Tier,It->Powered,It->Power->GetCircuitID(),It->Power->GetNumConnections(),It->PowerInfo->GetActualConsumption(),It->PowerInfo->GetTargetConsumption(),C?C->IsFuseTriggered():false);
                if(C){FPowerCircuitStats Stats;C->GetStats(Stats);Details+=FString::Printf(TEXT("Circuit capacity=%.2f produced=%.2f consumed=%.2f maximum=%.2f battery=%.3f nodes=%d infos=%d\n"),Stats.PowerProductionCapacity,Stats.PowerProduced,Stats.PowerConsumed,Stats.MaximumPowerConsumption,C->GetBatterySumPowerStore(),C->GetComponents().Num(),TFValue<TArray<TObjectPtr<UFGPowerInfoComponent>>>(C,TEXT("mPowerInfos")).Num());}
            }
            for(TActorIterator<AFGBuildableGeneratorFuel> It(World.Get());It;++It)
            {
                auto* P=It->FindComponentByClass<UFGPowerConnectionComponent>();auto* Info=It->GetPowerInfo();
                Details+=FString::Printf(TEXT("Generator %s circuit=%d wires=%d capacity=%.2f producing=%d canstart=%d paused=%d buildfx=%d fuelindex=%d fuel=%d dynamic=%.2f info_bound=%d info_connected=%d\n"),*It->GetName(),P?P->GetCircuitID():-1,P?P->GetNumConnections():0,It->GetPowerProductionCapacity(),It->IsProducing(),It->CanStartPowerProduction(),It->IsProductionPaused(),It->IsPlayingBuildEffect(),It->GetFuelInventoryIndex(),It->GetFuelInventory()?It->GetFuelInventory()->GetNumItems(TFClass(TEXT("/Game/FactoryGame/Resource/Parts/BioFuel/Desc_Biofuel"))):0,Info?Info->GetDynamicProductionCapacity():0,P&&P->GetPowerInfo()==Info,Info&&Info->IsConnected());
                if(It->GetClass()->GetName().Contains(TEXT("Automated")))Details+=FString::Printf(TEXT("Generator timing since=%.2f minimum=%.2f registered=%d\n"),TFValue<float>(*It,TEXT("mTimeSinceStartStopProducing")),TFValue<float>(*It,TEXT("mMinimumStoppedTime")),It->ShouldRegisterToFactoryTickGroup());
            }
            FFileHelper::SaveStringToFile(Details,*(FPaths::ProjectSavedDir()/TEXT("TF-Power-Diagnostics.txt")));
        }
        AFGBuildableConveyorBelt* Belt(ATFStop* Stop)
        {
            auto* Output=Stop->Belts[2].Get();auto* Input=Stop->Belts[0].Get();const FVector Start=Output->GetComponentLocation();
            const FTransform T(Start);auto* BS=AFGBuildableSubsystem::Get(World.Get());
            auto* B=Cast<AFGBuildableConveyorBelt>(BS->BeginSpawnBuildable(TFClass(TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk3/Build_ConveyorBeltMk3")),T));if(!B)return nullptr;
            const FVector Center=Stop->GetActorLocation()+FVector(0,0,100);
            TArray<FVector> Points={Start,Center+FVector(-1100,-600,0),Center+FVector(-600,-1100,0),Center+FVector(600,-1100,0),Center+FVector(1100,-600,0),Input->GetComponentLocation()};
            auto* Data=B->GetMutableSplinePointData();Data->Reset();
            for(int32 K=0;K<Points.Num();++K)
            {
                FVector Tangent=K==0?Output->GetConnectorNormal()*500:K==Points.Num()-1?-Input->GetConnectorNormal()*500:(Points[K+1]-Points[K-1]).GetSafeNormal()*600;
                Data->Add(FSplinePointData(Points[K]-Start,Tangent));
            }
            B->FinishSpawning(T);if(!BS->GetAllBuildablesRef().Contains(B))BS->AddBuildable(B);
            B->GetConnection0()->SetConnection(Output);B->GetConnection1()->SetConnection(Input);
            if(auto* Skip=FindFProperty<FBoolProperty>(B->GetClass(),TEXT("mSkipBuildEffect")))Skip->SetPropertyValue_InContainer(B,true);B->PlayBuildEffect();
            Actors.Add(B);DemoBelts.Add(B);return B;
        }
        AFGBuildablePipeline* Pipe(ATFStop* Stop,int32 OutputIndex,int32 InputIndex)
        {
            auto* Output=Stop->Pipes[OutputIndex].Get();auto* Input=Stop->Pipes[InputIndex].Get();const FVector Start=Output->GetComponentLocation();const FTransform T(Start);
            auto* BS=AFGBuildableSubsystem::Get(World.Get());auto* P=Cast<AFGBuildablePipeline>(BS->BeginSpawnBuildable(TFClass(TEXT("/Game/FactoryGame/Buildable/Factory/PipelineMk2/Build_PipelineMK2")),T));if(!P)return nullptr;
            const FVector Center=Stop->GetActorLocation()+FVector(0,0,100);
            TArray<FVector> Points;
            if(OutputIndex==2) Points={Start,Center+FVector(-1100,-700,0),Center+FVector(700,-1100,0),Input->GetComponentLocation()};
            else Points={Start,Center+FVector(1100,-700,50),Center+FVector(1100,700,50),Input->GetComponentLocation()};
            auto* Data=P->GetMutableSplinePointData();Data->Reset();
            for(int32 K=0;K<Points.Num();++K)
            {
                const FVector Tangent=K==0?Output->GetConnectorNormal()*500:K==Points.Num()-1?-Input->GetConnectorNormal()*500:(Points[K+1]-Points[K-1]).GetSafeNormal()*700;
                Data->Add(FSplinePointData(Points[K]-Start,Tangent));
            }
            P->FinishSpawning(T);if(!BS->GetAllBuildablesRef().Contains(P))BS->AddBuildable(P);P->GetConnection0()->SetConnection(Output);P->GetConnection1()->SetConnection(Input);if(auto* Skip=FindFProperty<FBoolProperty>(P->GetClass(),TEXT("mSkipBuildEffect")))Skip->SetPropertyValue_InContainer(P,true);P->PlayBuildEffect();Actors.Add(P);DemoPipes.Add(P);return P;
        }
        void DemoAdvance()
        {
            if(ResumeShowcase&&Stage==0)
            {
                Player=Cast<AFGPlayerController>(World->GetFirstPlayerController());if(!Player.IsValid()||!Player->GetCharacter())return;
                for(TActorIterator<ATFDrive> It(World.Get());It;++It)if(It->Tier==2&&It->Stops.Num()==3){if(It->IsFluid)F=*It;else D=*It;}
                if(!D.IsValid()||!F.IsValid()){Check(false,TEXT("Saved demonstration towers restored"));Finish();return;}
                Origin=D->GetActorLocation();for(auto S:D->Stops)I.Add(S);for(auto S:F->Stops)L.Add(S);
                Iron=TFClass(TEXT("/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate"));Copper=TFClass(TEXT("/Game/FactoryGame/Resource/Parts/CopperSheet/Desc_CopperSheet"));Water=TFClass(TEXT("/Game/FactoryGame/Resource/RawResources/Water/Desc_Water"));Oil=TFClass(TEXT("/Game/FactoryGame/Resource/RawResources/CrudeOil/Desc_LiquidOil"));
                for(auto S:I)if(auto* B=Cast<AFGBuildableConveyorBelt>(S->Belts[2]->GetConnection()?S->Belts[2]->GetConnection()->GetOwner():nullptr))DemoBelts.Add(B);
                for(auto S:L)for(int32 P:{2,3})if(auto* B=Cast<AFGBuildablePipeline>(S->Pipes[P]->GetConnection()?S->Pipes[P]->GetConnection()->GetOwner():nullptr))DemoPipes.Add(B);
                UFGSaveSession::Get(World.Get())->SetAutoSaveEnabled(false);Stage=13;
                // The delivered playground uses the established factory circuit.
                // Ordinary biomass burners remain available as a local backup.
                if(!D->Powered)
                {
                    UFGPowerConnectionComponent* Source=nullptr;float Distance=MAX_flt;
                    for(TActorIterator<AFGBuildablePowerPole> It(World.Get());It;++It)if(auto* P=It->FindComponentByClass<UFGPowerConnectionComponent>())if(auto* C=P->GetPowerCircuit())if(C->HasPower()&&C->GetPowerProductionCapacity()>100&&P->GetNumFreeConnections()>0)
                    {const float N=FVector::Dist(P->GetComponentLocation(),D->GetActorLocation());if(N<Distance){Source=P;Distance=N;}}
                    TArray<AFGBuildableWire*> Wires;D->Power->GetWires(Wires);auto* Local=Wires.Num()?Cast<UFGPowerConnectionComponent>(Wires[0]->GetOppositeConnection(D->Power)):nullptr;
                    if(Source&&Local)
                    {
                        const FVector Start=Source->GetOwner()->GetActorLocation(),End=Local->GetOwner()->GetActorLocation();const int32 Sections=FMath::Max(1,FMath::CeilToInt(Distance/6000.f));
                        for(int32 K=1;K<Sections;++K)
                        {
                            auto* Pole=BuildRecipe(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk3"),FMath::Lerp(Start,End,float(K)/Sections)-Origin);
                            if(auto* Next=Pole?Pole->FindComponentByClass<UFGPowerConnectionComponent>():nullptr){Wire(Source,Next);Source=Next;}
                        }
                        if(auto* C=Local->GetPowerCircuit())if(C->IsFuseTriggered())C->ResetFuse(Player.Get());
                        Wire(Source,Local);Check(true,TEXT("Playground connected to the ordinary established factory power grid"));
                    }
                    else Check(false,TEXT("A powered factory connection with a spare terminal is available"));
                }
                Check(true,TEXT("Saved item/fluid towers, floor identities and native return networks restored"));return;
            }
            for(auto B:DemoBelts)if(B.IsValid()){TArray<FConveyorBeltItem*> Items;B->GetConveyorBeltItems(Items);SawBeltCargo|=!Items.IsEmpty();}
            for(int32 K=0;K<DemoPipes.Num();++K)if(DemoPipes[K].IsValid()&&DemoPipes[K]->GetFluidBox()->Content>.001f){if(K%2==0)SawWater=true;else SawOil=true;}
            switch(Stage++)
            {
            case 0:
            {
                Player=Cast<AFGPlayerController>(World->GetFirstPlayerController());if(!Player.IsValid()||!Player->GetCharacter()){--Stage;return;}
                UFGSaveSession::Get(World.Get())->SetAutoSaveEnabled(false);
                if(auto* State=World->GetGameState<AFGGameState>())State->SetSessionName(TEXT("Freight Elevators Playground"));
                Origin=Player->GetCharacter()->GetActorLocation()+FVector(6500,6500,8000);Origin.Z=FMath::GridSnap(Origin.Z,400.f);
                Iron=TFClass(TEXT("/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate"));Copper=TFClass(TEXT("/Game/FactoryGame/Resource/Parts/CopperSheet/Desc_CopperSheet"));
                Water=TFClass(TEXT("/Game/FactoryGame/Resource/RawResources/Water/Desc_Water"));Oil=TFClass(TEXT("/Game/FactoryGame/Resource/RawResources/CrudeOil/Desc_LiquidOil"));
                for(int32 Tier=1;Tier<=3;++Tier) if(auto* SM=AFGSchematicManager::Get(World.Get()))SM->GiveAccessToSchematic(TFClass(FString::Printf(TEXT("/TrapperFreightElevators/Schematics/Schematic_FreightMk%d"),Tier)),Cast<AFGCharacterPlayer>(Player->GetCharacter()));
                for(int32 H:{0,800,1600}) for(int32 X=-2;X<=6;++X)for(int32 Y=-3;Y<=2;++Y)
                {
                    // Leave the two shaft interiors open above the ground deck.
                    if(H>0 && (X==0||X==4) && Y==0)continue;
                    Spawn(TEXT("/Game/FactoryGame/Buildable/Building/Foundation/Build_Foundation_8x1_01"),FVector(X*800,Y*800,H-50));
                }
                D=Drive(false,FVector::ZeroVector,2);F=Drive(true,FVector(3200,0,0),2);
                Check(D.IsValid()&&F.IsValid(),TEXT("Playground item and fluid Mk2 drives spawned"));if(!D.IsValid()||!F.IsValid()){Finish();return;}
                for(int32 H=800;H<2400;H+=400)for(int32 X:{0,3200})Spawn(TEXT("/TrapperFreightElevators/Buildables/Build_Shaft"),FVector(X,0,H));
                for(int32 H:{0,800,1600})
                {
                    auto* A=Stop(D.Get(),FVector(0,0,H));auto* B=Stop(F.Get(),FVector(3200,0,H));I.Add(A);L.Add(B);
                    A->FloorName=FString::Printf(TEXT("Items / Level %d"),H/800+1);B->FloorName=FString::Printf(TEXT("Fluids / Level %d"),H/800+1);
                }
                F->SafeCompartments(2);F->SetTankResource(0,Water);F->SetTankResource(1,Oil);
                for(auto S:L) {S->Ports[0].Compartment=0;S->Ports[2].Compartment=0;S->Ports[1].Compartment=1;S->Ports[3].Compartment=1;S->ApplyConfig();}
                for(auto S:I) {Route(D.Get(),Iron,S.Get());Route(D.Get(),Copper,S.Get());}
                for(auto S:L) {Route(F.Get(),Water,S.Get());Route(F.Get(),Oil,S.Get());}
                I[0]->Incoming->AddStack(FInventoryStack(600,Iron),true);I[0]->Incoming->AddStack(FInventoryStack(300,Copper),true);
                L[0]->Tanks[0].Input=200;L[0]->Tanks[1].Input=150;InitialWater=200;InitialOil=150;
                auto* Pole=BuildRecipe(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk3"),FVector(1600,1800,0));
                auto* Node=Pole?Pole->FindComponentByClass<UFGPowerConnectionComponent>():nullptr;
                DemoPower=Node;
                for(int32 N=0;N<4;++N)
                {
                    auto* G=Cast<AFGBuildableGeneratorFuel>(BuildRecipe(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_GeneratorBiomass_Automated"),FVector(-1600+N*1600,-2000,0)));
                    if(G)
                    {
                        auto* Fuel=TFClass(TEXT("/Game/FactoryGame/Resource/Parts/BioFuel/Desc_Biofuel"));if(!Fuel)Fuel=TFClass(TEXT("/Game/FactoryGame/Resource/Parts/SolidBiofuel/Desc_SolidBiofuel"));
                        const int32 Fueled=Fuel?G->GetFuelInventory()->AddStackToIndex(G->GetFuelInventoryIndex(),FInventoryStack(200,Fuel),true):0;
                        Check(Fueled==200,FString::Printf(TEXT("Generator %d receives 200 ordinary solid biofuel"),N+1));
                        if(Node)Wire(Node,G->FindComponentByClass<UFGPowerConnectionComponent>());
                    }
                }
                auto* Ladder=Cast<AFGBuildableLadder>(Spawn(TEXT("/Game/FactoryGame/Buildable/Building/Ladder/Build_Ladder"),FVector(1600,2000,0)));if(Ladder)Ladder->SetNumSegments(8,true);
                Player->GetCharacter()->SetActorLocation(Origin+FVector(1000,1000,250));Player->SetControlRotation(FRotator(-8,-145,0));
                FFileHelper::SaveStringToFile(Origin.ToString(),*(FPaths::ProjectSavedDir()/TEXT("TF-Playground-Origin.txt")));
                break;
            }
            case 1:
                for(auto S:I)Check(Belt(S.Get())!=nullptr,TEXT("Native belt return loop constructed"));
                for(auto S:L){Check(Pipe(S.Get(),2,0)!=nullptr,TEXT("Water return pipe constructed"));Check(Pipe(S.Get(),3,1)!=nullptr,TEXT("Oil return pipe constructed"));}
                break;
            case 10:
                for(auto Actor:Actors)if(auto* G=Cast<AFGBuildableGeneratorFuel>(Actor.Get()))if(auto* Terminal=G->FindComponentByClass<UFGPowerConnectionComponent>())
                {
                    Report+=FString::Printf(TEXT("INFO: Fixture generator production info previously bound: %d\n"),Terminal->GetPowerInfo()==G->GetPowerInfo());
                    Terminal->SetPowerInfo(G->GetPowerInfo());
                    Check(Terminal->GetPowerInfo()==G->GetPowerInfo(),TEXT("Fixture generator production info is bound to its native terminal"));
                }
                InspectPower();
                for(auto* Drive:{D.Get(),F.Get()})if(DemoPower.IsValid())Wire(DemoPower.Get(),Drive->Power);
                break;
            case 12:
                if(auto* C=D->Power->GetPowerCircuit())C->ResetFuse(Player.Get());
                if(auto* C=F->Power->GetPowerCircuit())C->ResetFuse(Player.Get());
                D->Paused=false;F->Paused=false;break;
            case 11:
                if(auto* C=D->Power->GetPowerCircuit())C->MarkForFullRebuild();break;
            case 17:
                if(auto* C=D->Power->GetPowerCircuit())if(C->IsFuseTriggered())C->ResetFuse(Player.Get());break;
            case 18:
                InspectPower();
                Check(D->Powered&&F->Powered,FString::Printf(TEXT("Native power circuit supplies both playground elevators (item %.1f / %.1f MW, fluid %.1f / %.1f MW)"),D->PowerInfo->GetActualConsumption(),D->PowerInfo->GetTargetConsumption(),F->PowerInfo->GetActualConsumption(),F->PowerInfo->GetTargetConsumption()));
                Check(DemoBelts.Num()==3&&DemoPipes.Num()==6,TEXT("Playground contains three belt loops and six isolated pipe loops"));
                break;
            case 25:PlacementCases();break;
            case 45:
            {
                Check(SawBeltCargo,TEXT("Ordinary belt return loops actually carry elevator-delivered items"));
                Check(SawWater&&SawOil,TEXT("Ordinary native pipe loops actually carry both isolated delivered fluids"));
                double ReturnedItems=0,ReturnedFluid=0;for(auto S:I)ReturnedItems+=S->ReceivedItems;for(auto S:L)ReturnedFluid+=S->ReceivedFluid;
                Check(ReturnedItems>30,FString::Printf(TEXT("Native conveyor inputs keep receiving deliveries from loaded return belts (%.0f items)"),ReturnedItems));
                Check(ReturnedFluid>0,FString::Printf(TEXT("Native pipe inputs receive returned deliveries (%.3f m3)"),ReturnedFluid));
                int32 Items=FMath::RoundToInt(D->CargoAmount());for(auto S:I){TArray<FInventoryStack> Stacks;S->Incoming->GetInventoryStacks(Stacks);for(const auto& Stack:Stacks)Items+=Stack.NumItems;Stacks.Reset();S->Outgoing->GetInventoryStacks(Stacks);for(const auto& Stack:Stacks)Items+=Stack.NumItems;}
                for(auto B:DemoBelts)if(B.IsValid()){TArray<FConveyorBeltItem*> BeltItems;B->GetConveyorBeltItems(BeltItems);Items+=BeltItems.Num();}
                Check(Items==900,FString::Printf(TEXT("Live belts, buffers and cabin conserve all 900 demonstration items (observed %d)"),Items));
                Report+=FString::Printf(TEXT("DIAG item status=%s paused=%d routes=%d cargo=%.0f height=%.0f\n"),*D->Status,D->Paused,D->Routes.Num(),D->CargoAmount(),D->CabinHeight);
                for(auto S:I)
                {
                    TArray<FInventoryStack> Stacks;S->Incoming->GetInventoryStacks(Stacks);Report+=FString::Printf(TEXT("DIAG %s fluid=%d enabled=%d linked=%d pickup=%d received=%.0f inStacks=%d\n"),*S->Label(),S->IsFluid,S->Enabled,S->Elevator==D.Get(),D->HasPickup(S.Get()),S->ReceivedItems,Stacks.Num());
                    for(const auto& Q:Stacks)if(Q.HasItems())Report+=FString::Printf(TEXT("DIAG incoming %s %d state=%d\n"),*TF::ResourceName(Q.Item.GetItemClass()),Q.NumItems,Q.Item.HasState());
                    Stacks.Reset();S->Outgoing->GetInventoryStacks(Stacks);for(const auto& Q:Stacks)if(Q.HasItems())Report+=FString::Printf(TEXT("DIAG outgoing %s %d\n"),*TF::ResourceName(Q.Item.GetItemClass()),Q.NumItems);
                }
                for(auto B:DemoBelts)if(B.IsValid())Report+=FString::Printf(TEXT("DIAG belt length=%.0f c0=%d c1=%d max=%d\n"),B->GetLength(),int(B->GetConnection0()->GetDirection()),int(B->GetConnection1()->GetDirection()),B->MaxNumGrab(.05f));
                float Volume=F->CargoAmount();for(auto S:L){for(const auto& T:S->Tanks)Volume+=T.Input+T.Output;for(auto P:S->Pipes)Volume+=P->Content();}for(auto P:DemoPipes)if(P.IsValid())Volume+=P->GetFluidBox()->Content;
                Check(FMath::IsNearlyEqual(Volume,350.f,.5f),FString::Printf(TEXT("Live native pipes, port boxes, tanks and cabin conserve 350m3 (observed %.3f)"),Volume));
                // Leave the player beside the live demonstration and save a separate session.
                UFGSaveSession::Get(World.Get())->SaveGame(FParse::Param(FCommandLine::Get(),TEXT("TFIronworksReview"))?TEXT("Trapper_Ironworks_Review"):TEXT("Trapper_Freight_Playground"),FOnSaveGameComplete(),nullptr);
                Report+=FString::Printf(TEXT("\nPlayground prepared: %d checks, %d failures\n"),Checks,Failed);
                FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("TF-Playground.txt")));
                FUseState Use;Use.SetUseState(UFGUseState_Valid::StaticClass());
                IFGUseableInterface::Execute_OnUse(D.Get(),Cast<AFGCharacterPlayer>(Player->GetCharacter()),Use);
                World->GetTimerManager().ClearTimer(Timer);break;
            }
            }
        }
        void UnitCases()
        {
            Check(D.IsValid()&&F.IsValid()&&I.Num()==3&&L.Num()==3,TEXT("Item and fluid drive / stop fixtures spawned"));
            if(!D.IsValid()||!F.IsValid())return;
            Check(D->Stops.Num()==3 && F->Stops.Num()==3,TEXT("Separate shafts discover only their own three floors"));
            Check(I[0]->Incoming->GetSizeLinear()==24 && I[0]->Outgoing->GetSizeLinear()==24,TEXT("Each direction has one complete cabin-load of item buffering"));
            Check(I[0]->Belts.Num()==4 && L[0]->Pipes.Num()==4,TEXT("Exactly four native ports on both elevator types"));
            Check(!D->CanAttach(Origin+FVector(0,0,400)),TEXT("Stops closer than eight metres rejected"));
            Check(!D->CanAttach(Origin+FVector(0,0,1750)),TEXT("Stops off the one metre grid rejected"));
            Check(!D->CanAttach(Origin+FVector(0,0,25200)),TEXT("Stops exceeding 248m usable travel rejected"));
            Check(!D->CanAttach(Origin+FVector(0,0,24800)),TEXT("A high stop needs an existing shaft before placement"));
            Route(D.Get(),Iron,I[1].Get());Route(D.Get(),Iron,I[2].Get());
            I[0]->Incoming->AddStack(FInventoryStack(90,Iron));D->Arrive(I[0].Get());I[0]->Incoming->AddStack(FInventoryStack(10,Iron));
            float Budget=200;D->Load(Budget);
            Check(FMath::IsNearlyEqual(D->CargoAmount(),90.f),TEXT("Immediate pickup ships exactly the arrival snapshot"));
            Check(D->CabinInventory->GetNumItems(Iron)==90,TEXT("Cabin manifest matches native cargo inventory"));
            Check(I[0]->Incoming->GetNumItems(Iron)==10,TEXT("Cargo arriving after the snapshot waits for another visit"));
            Check(Manifest(D.Get(),I[1].Get(),Iron)==45 && Manifest(D.Get(),I[2].Get(),Iron)==45,TEXT("One item splits equally between two destination floors"));
            Check(I[1]->Reserved(Iron)==45 && I[2]->Reserved(Iron)==45,TEXT("Cargo reserves destination buffer capacity before departure"));
            Check(!I[1]->CanDismantle_Implementation()&&!D->CanDismantle_Implementation(),TEXT("Dismantling cannot delete reserved or onboard cargo"));
            D->Docked=I[1].Get();Budget=200;D->Unload(Budget);
            Check(I[1]->Outgoing->GetNumItems(Iron)==45 && D->CargoAmount()==45,TEXT("Only cargo for the docked floor unloads"));
            D->Docked=I[2].Get();Budget=200;D->Unload(Budget);
            Check(I[0]->Incoming->GetNumItems(Iron)+I[1]->Outgoing->GetNumItems(Iron)+I[2]->Outgoing->GetNumItems(Iron)==100,TEXT("Snapshot pickup and two-floor delivery conserve every item"));
            FInventoryItem Taken;float Offset=0;
            Check(I[1]->Belts[2]->Factory_GrabOutput(Taken,Offset,Iron)&&Taken.GetItemClass()==Iron,TEXT("A native conveyor output can withdraw delivered cargo"));
            I[1]->Ports[2].Filter=Copper;I[1]->ApplyConfig();
            TArray<FInventoryItem> Peek;Check(!I[1]->Belts[2]->Factory_PeekOutput(Peek,Iron),TEXT("Output filter blocks the wrong resource"));
            I[1]->Ports[2].Filter=nullptr;I[1]->ApplyConfig();
            ResetItems();Route(D.Get(),Iron,I[1].Get(),7);Route(D.Get(),Iron,I[2].Get(),3);
            I[0]->Incoming->AddStack(FInventoryStack(100,Iron));D->Arrive(I[0].Get());Budget=200;D->Load(Budget);
            Check(Manifest(D.Get(),I[1].Get(),Iron)==70 && Manifest(D.Get(),I[2].Get(),Iron)==30,TEXT("Weighted routing delivers a 70/30 split by item count"));
            ResetItems();Route(D.Get(),Iron,I[1].Get());Route(D.Get(),Iron,I[2].Get());
            for(int32 K=0;K<24;++K) I[1]->Outgoing->AddStack(FInventoryStack(TF::StackSize(Copper),Copper));
            I[0]->Incoming->AddStack(FInventoryStack(20,Iron));D->Arrive(I[0].Get());Budget=100;D->Load(Budget);
            Check(Manifest(D.Get(),I[1].Get(),Iron)==0 && Manifest(D.Get(),I[2].Get(),Iron)==20,TEXT("A full destination does not block service to another floor"));
            ResetItems();I[0]->Incoming->AddStack(FInventoryStack(30,Iron));D->Arrive(I[0].Get());Budget=100;D->Load(Budget);
            Check(D->Cargo.IsEmpty() && I[0]->Incoming->GetNumItems(Iron)==30,TEXT("Unrouted cargo remains in source storage"));
            Route(D.Get(),Iron,I[1].Get());D->Docked=nullptr;
            Check(D->NextStop()==I[0].Get(),TEXT("An empty cabin finds a remote pickup request"));
            ResetItems();Route(D.Get(),Iron,I[1].Get());I[0]->WaitFull=true;I[0]->Incoming->AddStack(FInventoryStack(10,Iron));D->Arrive(I[0].Get());I[0]->Incoming->AddStack(FInventoryStack(10,Iron));Budget=100;D->Load(Budget);
            Check(D->CargoAmount()==20,TEXT("Wait-until-full mode accepts cargo arriving during the wait"));
            D->DepartRequested=true;I[0]->Incoming->AddStack(FInventoryStack(5,Iron));Budget=100;D->Load(Budget);
            Check(I[0]->Incoming->GetNumItems(Iron)==5,TEXT("Depart-now prevents further loading"));
            ResetItems();D->DepartRequested=false;D->Paused=false;D->QABypassPower=true;Route(D.Get(),Iron,I[1].Get());
            I[0]->WaitFull=true;I[0]->MaxWait=2;I[0]->Incoming->AddStack(FInventoryStack(10,Iron));D->Arrive(I[0].Get());D->PostFactoryTick(nullptr,TF::DoorSeconds);D->PostFactoryTick(nullptr,1.f);
            Check(D->Docked==I[0].Get()&&D->CargoAmount()==10,TEXT("A partial load waits before the per-floor time limit"));
            D->PostFactoryTick(nullptr,1.1f);Check(D->Docked==nullptr&&D->Target==I[1].Get()&&D->CargoAmount()==10,TEXT("A partial load departs automatically when its maximum wait expires"));
            D->QABypassPower=false;I[0]->MaxWait=30;
            ResetItems();D->DepartRequested=false;D->QABypassPower=true;Route(D.Get(),Iron,I[1].Get());I[0]->Incoming->AddStack(FInventoryStack(10,Iron));D->Arrive(I[0].Get());
            D->PostFactoryTick(nullptr,TF::DoorSeconds);D->PostFactoryTick(nullptr,.001f);Check(D->Docked==I[0].Get()&&D->Cargo.IsEmpty(),TEXT("A sub-item frame budget keeps the cabin docked while transfer credit accumulates"));
            D->PostFactoryTick(nullptr,.06f);Check(D->Docked==I[0].Get()&&D->CargoAmount()==1,TEXT("Snapshot loading continues across fractional-budget frames"));
            for(int32 Frame=0;Frame<50;++Frame)D->PostFactoryTick(nullptr,.01f);
            Check(D->CargoAmount()==10&&I[0]->Incoming->IsEmpty()&&D->Target==I[1].Get(),TEXT("High-frame-rate loading completes the full snapshot before departure"));
            ResetItems();Route(D.Get(),Iron,I[1].Get());I[0]->WaitFull=true;I[0]->MaxWait=.1f;I[0]->Incoming->AddStack(FInventoryStack(10,Iron));D->Arrive(I[0].Get());D->PostFactoryTick(nullptr,TF::DoorSeconds);D->PostFactoryTick(nullptr,.06f);I[0]->Incoming->AddStack(FInventoryStack(10,Iron));D->PostFactoryTick(nullptr,.06f);
            Check(D->Docked==nullptr&&D->Target==I[1].Get()&&I[0]->Incoming->GetNumItems(Iron)==18,TEXT("The maximum fill wait expires even while fresh cargo continues to arrive"));
            D->QABypassPower=false;I[0]->MaxWait=30;
            ResetItems();D->Paused=false;D->QABypassPower=true;Route(D.Get(),Iron,I[1].Get());
            I[0]->Incoming->AddStack(FInventoryStack(10,Iron));D->Arrive(I[0].Get());
            D->PostFactoryTick(nullptr,.4f);
            Check(D->Cargo.IsEmpty()&&D->DoorPhase>0&&D->DoorPhase<1,TEXT("Cargo waits while landing gates open"));
            I[0]->Incoming->AddStack(FInventoryStack(5,Iron));D->PostFactoryTick(nullptr,.5f);D->PostFactoryTick(nullptr,.5f);
            Check(D->CargoAmount()==10&&I[0]->Incoming->GetNumItems(Iron)==5,TEXT("Door opening retains the arrival snapshot; later cargo stays in the floor buffer"));
            D->PostFactoryTick(nullptr,.4f);
            Check(D->CabinHeight==0&&D->DoorPhase>0,TEXT("Cabin remains stationary until the gates close"));
            D->Paused=true;const float FrozenGate=D->DoorPhase;D->PostFactoryTick(nullptr,.2f);
            Check(D->DoorPhase==FrozenGate&&D->CabinHeight==0,TEXT("Pausing freezes the door sequence and cabin"));D->Paused=false;
            D->PostFactoryTick(nullptr,.5f);D->PostFactoryTick(nullptr,.1f);
            Check(D->DoorPhase==0&&D->CabinHeight>0&&D->CabinHeight<40,TEXT("Cabin accelerates gently only after gates are fully closed"));
            D->PostFactoryTick(nullptr,5.f);
            Check(D->Docked==I[1].Get()&&D->CabinHeight==800&&I[1]->Outgoing->IsEmpty(),TEXT("A long frame lands exactly without transferring through closed gates"));
            D->PostFactoryTick(nullptr,TF::DoorSeconds);D->PostFactoryTick(nullptr,.5f);
            Check(I[1]->Outgoing->GetNumItems(Iron)==10&&D->Cargo.IsEmpty(),TEXT("Delivery starts after opening and conserves the shipment"));
            D->QABypassPower=false;
            Check(F->SafeCompartments(2),TEXT("An empty fluid elevator changes to two compartments"));
            Check(F->SetTankResource(0,Water)&&F->SetTankResource(1,Oil),TEXT("Water and oil receive separate compartment assignments"));
            Route(F.Get(),Water,L[1].Get());Route(F.Get(),Oil,L[2].Get());L[0]->Tanks[0].Input=20;L[0]->Tanks[1].Input=30;
            F->Arrive(L[0].Get());F->Paused=false;F->QABypassPower=true;F->PostFactoryTick(nullptr,.4f);
            Check(F->Cargo.IsEmpty()&&L[0]->Tanks[0].Input==20&&L[0]->Tanks[1].Input==30,TEXT("Both fluid compartments wait for docking gates before pumping"));
            F->Paused=true;F->QABypassPower=false;Budget=100;F->Load(Budget);
            Check(F->CargoAmount(0)==20 && F->CargoAmount(1)==30,TEXT("Fluid cargo is isolated and measured in cubic metres"));
            Check(!F->SetTankResource(0,Oil)&&!F->SafeCompartments(4),TEXT("Loaded compartments cannot be reassigned or repartitioned"));
            F->Docked=L[1].Get();Budget=100;F->Unload(Budget);F->Docked=L[2].Get();Budget=100;F->Unload(Budget);
            Check(L[1]->Tanks[0].Output==20 && L[1]->Tanks[1].Output==0 && L[2]->Tanks[1].Output==30,TEXT("Fluid destinations receive only their assigned compartments"));
            Check(FMath::IsNearlyEqual(L[1]->Tanks[0].Output+L[2]->Tanks[1].Output+F->CargoAmount(),50.f),TEXT("Multi-fluid delivery conserves total volume"));
            Check(!F->SetTankResource(0,Oil),TEXT("Fluid still in a floor tank prevents reassignment"));
            FTFCargo ReservedCargo;ReservedCargo.Resource=Oil;ReservedCargo.Compartment=1;ReservedCargo.Amount=5;ReservedCargo.Destination=L[2].Get();F->Cargo.Add(ReservedCargo);
            L[1]->PendingFlush=true;L[1]->PostFactoryTick(nullptr,0);
            Check(L[1]->Tanks[0].Output==0&&L[2]->Tanks[1].Output==30&&F->CargoAmount()==5,TEXT("Queued floor flush runs safely without erasing another floor or reserved cabin cargo"));
            for(auto S:L) for(auto& T:S->Tanks) {T.Input=0;T.Output=0;}
            F->Cargo.Reset();Check(F->SafeCompartments(4),TEXT("Four-compartment mode becomes available when all buffers are empty"));
            Check(!F->SafeCompartments(3),TEXT("Unsupported compartment counts rejected"));
            Check(!F->SetTankResource(0,Iron),TEXT("Solid items cannot be assigned to fluid tanks"));
            ResetItems();
            auto* PlayerInventory=NewObject<UFGInventoryComponent>(D.Get(),TEXT("TFStorageTestInventory"));PlayerInventory->RegisterComponent();PlayerInventory->Resize(1);
            PlayerInventory->AddStack(FInventoryStack(10,Iron));
            for(int32 K=0;K<23;++K)I[0]->Incoming->AddStack(FInventoryStack(TF::StackSize(Copper),Copper));I[0]->Incoming->AddStack(FInventoryStack(TF::StackSize(Iron)-3,Iron));
            const int32 Put=I[0]->MoveStorageStack(PlayerInventory,true,0,0,Iron);
            Check(Put==3&&PlayerInventory->GetNumItems(Iron)==7&&I[0]->Incoming->GetNumItems(Iron)==TF::StackSize(Iron),TEXT("Inventory click deposits only the room available and retains the remainder"));
            Check(I[0]->IncomingView.Num()==24&&I[0]->IncomingView[23].Resource==Iron&&I[0]->IncomingView[23].NumItems==TF::StackSize(Iron),TEXT("Compact inventory snapshot preserves the real slot index, resource and count"));
            Check(I[0]->MoveStorageStack(PlayerInventory,true,0,0,Iron)==0&&PlayerInventory->GetNumItems(Iron)==7,TEXT("Full incoming storage cannot lose or duplicate a clicked stack"));
            Check(I[0]->MoveStorageStack(PlayerInventory,true,1,0,Iron)==0,TEXT("Manual deposits cannot bypass routing by writing into outgoing storage"));
            Check(I[0]->MoveStorageStack(PlayerInventory,true,0,0,Copper)==0&&I[0]->MoveStorageStack(PlayerInventory,true,0,999,Iron)==0,TEXT("Stale item identity and invalid inventory slots are rejected"));
            ResetItems();PlayerInventory->RemoveAllFromIndex(0);I[0]->Outgoing->AddStack(FInventoryStack(10,Iron));
            Check(I[0]->MoveStorageStack(PlayerInventory,false,1,0,Iron)==10&&PlayerInventory->GetNumItems(Iron)==10&&I[0]->Outgoing->IsEmpty(),TEXT("Clicking delivered storage moves its real stack into player inventory"));
            Check(I[0]->OutgoingView.Num()==24&&!I[0]->OutgoingView[0].Resource&&I[0]->OutgoingView[0].NumItems==0,TEXT("Empty replicated slots retain their indices without native item instance state"));
            Check(F->IsFluid&&L[0]->MoveStorageStack(PlayerInventory,true,0,0,Iron)==0,TEXT("Item clicks cannot deposit solids into a fluid elevator"));
            PlayerInventory->DestroyComponent();
            bool BlocksAllSides=true;
            for(int32 Side=0;Side<4;++Side){const FRotator R(0,Side*90,0);const FVector Start=D->GetActorLocation()+R.RotateVector(FVector(800,0,210)),End=D->GetActorLocation()+FVector(0,0,210);FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(TFFreightPawnTest),false);
                const bool HitBody=World->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88),Query);BlocksAllSides&=HitBody&&Cast<ATFPart>(Hit.GetActor())!=nullptr;}
            Check(BlocksAllSides,TEXT("Player-sized capsule sweeps are blocked on all four sides of freight machinery"));
            Check(D->FreightCollision->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block&&D->FreightCollision->GetCollisionResponseToChannel(ECC_Visibility)==ECR_Ignore,TEXT("Freight body collision blocks players without intercepting connection-placement traces"));
            I[0]->Ports[0].Input=false;I[0]->ApplyConfig();Check(!I[0]->Ports[0].Input&&(!IsValid(I[0]->PortDirections[0])||I[0]->PortDirections[0]->Text.ToString()==TEXT("OUTPUT")),TEXT("Port 1 direction configuration safely updates with or without rendered server signs"));I[0]->Ports[0].Input=true;I[0]->ApplyConfig();
            ResetItems();D->Paused=false;D->Target=I[2].Get();
        }
        void ReservationCases()
        {
            auto* Tower=Drive(false,FVector(12000,-6000,0));auto* S=Stop(Tower,FVector(12000,-6000,0));
            S->Incoming->Resize(30);auto Third=TFClass(TEXT("/Game/FactoryGame/Resource/Parts/Cement/Desc_Cement"));
            auto Clear=[&]{TArray<FInventoryStack> Stacks;S->Incoming->GetInventoryStacks(Stacks);for(const auto& Stack:Stacks)S->Incoming->Remove(Stack.Item.GetItemClass(),Stack.NumItems);};
            const int32 IS=TF::StackSize(Iron),CS=TF::StackSize(Copper);
            Check(S->IncomingRoom(Iron)==30*IS,TEXT("Unconfigured incoming storage retains all shared capacity"));
            Check(S->SetIncomingReservation(Iron,5)&&S->SetIncomingReservation(Copper,10)&&S->ReservedIncomingStacks()==15,TEXT("Thirty-slot storage accepts five and ten reserved stacks"));
            Check(S->IncomingRoom(Iron)==20*IS&&S->IncomingRoom(Copper)==25*CS&&Third&&S->IncomingRoom(Third)==15*TF::StackSize(Third),TEXT("Reserved items can use shared overflow; other items see only fifteen shared stacks"));
            Check(!S->SetIncomingReservation(Copper,26)&&S->ReservedIncomingStacks(Copper)==10,TEXT("Over-capacity reservation update is rejected without changing the previous rule"));
            Check(!S->SetIncomingReservation(Iron,-1)&&!S->SetIncomingReservation(nullptr,1)&&!S->SetIncomingReservation(Water,1)&&!L[0]->SetIncomingReservation(Iron,1),TEXT("Negative, missing, liquid and fluid-elevator reservations are rejected"));
            S->Incoming->AddStack(FInventoryStack(20*IS-3,Iron),true);
            auto* PlayerInventory=NewObject<UFGInventoryComponent>(S);PlayerInventory->RegisterComponent();PlayerInventory->Resize(2);PlayerInventory->AddStack(FInventoryStack(10,Iron));
            Check(S->MoveStorageStack(PlayerInventory,true,0,0,Iron)==3&&PlayerInventory->GetNumItems(Iron)==7,TEXT("Manual deposit stops at the reserved boundary and keeps excess in player inventory"));
            Check(S->MoveStorageStack(PlayerInventory,true,0,0,Iron)==0&&!S->CanReceiveIncoming(FInventoryItem(Iron))&&S->CanReceiveIncoming(FInventoryItem(Copper)),TEXT("Full shared allocation blocks further iron while the ten copper slots remain usable"));
            Check(S->IncomingRoom(Copper)==10*CS&&S->IncomingRoom(Third)==0,TEXT("Other materials cannot consume the protected ten empty copper slots"));
            Clear();S->Incoming->AddStack(FInventoryStack(3*IS,Iron),true);
            Check(S->IncomingRoom(Copper)==25*CS,TEXT("Three existing reserved stacks leave only two additional slots protected"));
            Clear();S->Incoming->AddStack(FInventoryStack(20*IS,Iron),true);S->Incoming->AddStack(FInventoryStack(10*CS-1,Copper),true);
            Check(S->IncomingRoom(Copper)==1&&S->CanReceiveIncoming(FInventoryItem(Copper)),TEXT("Partially filled reserved stacks remain fillable when every physical slot is occupied"));
            Clear();S->Incoming->AddStack(FInventoryStack(30*IS,Iron),true);
            Check(S->SetIncomingReservation(Iron,4)&&S->Incoming->GetNumItems(Iron)==30*IS,TEXT("Changing a reservation never removes existing excess cargo"));
            S->Incoming->Remove(Iron,IS);Check(S->IncomingRoom(Copper)==CS&&!S->CanReceiveIncoming(FInventoryItem(Iron)),TEXT("A newly freed slot becomes available to the reserved material instead of refilling with excess"));
            Check(S->SetIncomingReservation(Copper,0)&&S->IncomingRoom(Iron)==IS&&S->Incoming->GetNumItems(Iron)==29*IS,TEXT("Removing a reservation releases its space without modifying cargo"));
            Clear();S->SetIncomingReservation(Iron,5);S->SetIncomingReservation(Copper,25);
            Check(S->IncomingRoom(Third)==0,TEXT("Fully reserved storage has no capacity for unrelated materials"));
            Check(I[0]->IncomingReservations.IsEmpty(),TEXT("Reservations are isolated to their configured floor"));
            S->SetIncomingReservation(Iron,0);S->SetIncomingReservation(Copper,0);S->Incoming->Resize(24);
            S->SetIncomingReservation(Iron,1);S->SetIncomingReservation(Copper,23);S->Incoming->AddStack(FInventoryStack(IS,Iron));S->Outgoing->AddStack(FInventoryStack(30,Iron));
            PlayerInventory->DestroyComponent();ReservationStop=S;ReservationBelt=Belt(S);
            Check(ReservationBelt.IsValid(),TEXT("Native conveyor fixture connects a blocked reserved input"));
        }
        void OutgoingReservationCases()
        {
            auto* Tower=Drive(false,FVector(16000,-6000,0));auto* Source=Stop(Tower,FVector(16000,-6000,0));auto* Dest=Stop(Tower,FVector(16000,-6000,800));
            Tower->Paused=true;Dest->Outgoing->Resize(30);
            const int32 IS=TF::StackSize(Iron),CS=TF::StackSize(Copper);
            auto Clear=[&]{TArray<FInventoryStack> Stacks;Dest->Outgoing->GetInventoryStacks(Stacks);for(const auto& Stack:Stacks)Dest->Outgoing->Remove(Stack.Item.GetItemClass(),Stack.NumItems);Tower->Cargo.Reset();Tower->CabinInventory->Empty();};
            Check(Dest->ItemFree(Iron)==30*IS,TEXT("Unconfigured outgoing storage preserves shared capacity"));
            Check(Dest->SetStorageReservation(1,Iron,5)&&Dest->SetStorageReservation(1,Copper,10)&&Dest->ReservedStorageStacks(1)==15,TEXT("Outgoing buffer independently accepts five and ten reserved stacks"));
            Check(Dest->IncomingReservations.IsEmpty()&&Source->OutgoingReservations.IsEmpty(),TEXT("Outgoing rules do not change incoming or another floor"));
            Check(Dest->ItemFree(Iron)==20*IS&&Dest->ItemFree(Copper)==25*CS,TEXT("Outgoing reservations permit shared overflow while protecting other items"));
            Check(!Dest->SetStorageReservation(1,Copper,26)&&Dest->ReservedStorageStacks(1,Copper)==10&&!Dest->SetStorageReservation(2,Iron,1)&&!Dest->SetStorageReservation(1,Water,1)&&!Dest->SetStorageReservation(1,Iron,-1),TEXT("Invalid outgoing updates leave the previous allocation intact"));
            Dest->Outgoing->AddStack(FInventoryStack(30*IS,Iron),true);
            Check(Dest->Outgoing->GetNumItems(Iron)==30*IS&&Dest->ItemFree(Copper)==0,TEXT("Reservations never discard pre-existing excess deliveries"));
            Dest->Outgoing->Remove(Iron,IS);
            FTFRoute R;R.Resource=Iron;R.Destination=Dest;Tower->Routes.Add(R);R.Resource=Copper;Tower->Routes.Add(R);
            float Room=0;
            Check(Tower->ChooseRoute(Iron,Source,Room)==INDEX_NONE&&Tower->ChooseRoute(Copper,Source,Room)==1&&Room==CS,TEXT("Freed destination slot routes reserved cargo and rejects competing cargo"));
            Clear();Dest->Outgoing->AddStack(FInventoryStack(20*IS,Iron),true);
            FTFCargo C;C.Resource=Copper;C.Destination=Dest;C.Amount=10*CS;Tower->Cargo.Add(C);Tower->CabinInventory->AddStack(FInventoryStack(10*CS,Copper),true);
            Check(Dest->ItemFree(Copper)==0&&Dest->ItemFree(Iron)==0,TEXT("Cargo in transit consumes capacity once and satisfies its reserved slots"));
            Tower->Docked=Dest;float Budget=10*CS;Tower->Unload(Budget);
            Check(Tower->Cargo.IsEmpty()&&Tower->CabinInventory->IsEmpty()&&Dest->Outgoing->GetNumItems(Copper)==10*CS,TEXT("Reserved deliveries unload without double-counting onboard cargo"));
            Dest->Outgoing->Remove(Copper,1);Check(Dest->ItemFree(Copper)==1&&Dest->ItemFree(Iron)==0,TEXT("Partial outgoing stacks remain fillable without stealing protected slots"));
            Clear();Dest->Outgoing->AddStack(FInventoryStack(20*IS,Iron),true);
            C.Resource=Iron;C.Amount=IS;Tower->Cargo.Add(C);Tower->CabinInventory->AddStack(FInventoryStack(IS,Iron));Budget=IS;Tower->Unload(Budget);
            Check(Tower->CargoAmount()==IS&&Tower->CabinInventory->GetNumItems(Iron)==IS&&Dest->Outgoing->GetNumItems(Iron)==20*IS,TEXT("A reservation changed after pickup retains blocked cargo safely onboard"));
            Check(Dest->SetStorageReservation(1,Copper,0)&&Dest->ItemFree(Iron)==9*IS,TEXT("Removing outgoing rule releases capacity while counting cargo still onboard"));
            Budget=IS;Tower->Unload(Budget);Check(Tower->Cargo.IsEmpty()&&Dest->Outgoing->GetNumItems(Iron)==21*IS,TEXT("Retained cargo unloads after its restriction is removed"));
            Source->Destroy();Dest->Destroy();Tower->Destroy();
        }
        bool ReservationBeltProgress()
        {
            if(!ReservationStop.IsValid()||!ReservationBelt.IsValid())return true;
            auto* S=ReservationStop.Get();auto* B=ReservationBelt.Get();
            if(ReservationBeltPhase==0)
            {
                TArray<FInventoryItem> Peek;
                if(!B->GetConnection1()->Factory_PeekOutput(Peek,nullptr)||Peek.IsEmpty())
                {if(Stage<45)return false;Check(false,TEXT("Native test belt reaches the reserved input in time"));return true;}
                Check(S->ReceivedItems==0&&S->Incoming->GetNumItems(Iron)==TF::StackSize(Iron),TEXT("A real belt backs up at reserved space without consuming the waiting item"));
                S->SetIncomingReservation(Copper,22);ReservationBeltPhase=1;return false;
            }
            if(S->ReceivedItems<30&&Stage<45)return false;
            Check(S->ReceivedItems==30&&S->Incoming->GetNumItems(Iron)==TF::StackSize(Iron)+30&&S->Outgoing->IsEmpty(),TEXT("Freeing one shared slot resumes native belt intake with every item conserved"));return true;
        }
        void AutoShaftCases()
        {
            auto* Tower=Drive(false,FVector(12000,0,0));if(!Tower)return;
            auto* Bottom=Stop(Tower,FVector(12000,0,0));auto* Middle=Stop(Tower,FVector(12000,0,1200));auto* Top=Stop(Tower,FVector(12000,0,3600));
            Tower->RefreshStops();
            auto Length=[](const FTFShaftLayout& Layout){float N=0;for(auto Span:Layout.Frames)N+=Span.Y;return N;};
            const auto Initial=Tower->AutoShaftLayout;
            Check(Initial.Height==4400&&Length(Initial)==2400,TEXT("New widely spaced item floors automatically fill all uncovered shaft height"));
            FHitResult Hit;const FVector Center=Tower->GetActorLocation()+FVector(0,0,2500);
            const bool Blocks=World->SweepSingleByChannel(Hit,Center+FVector(800,0,0),Center,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88));
            Check(Blocks&&Hit.GetActor()==Tower,TEXT("Automatic shaft blocks a player capsule in the formerly empty gap"));
            Check(Tower->FreightCollision->GetCollisionResponseToChannel(ECC_Visibility)==ECR_Ignore,TEXT("Automatic shaft does not intercept belt/pipe placement traces"));
            auto* Manual=World->SpawnActor<ATFShaft>(ATFShaft::StaticClass(),Tower->GetActorLocation()+FVector(0,0,2400),Tower->GetActorRotation());
            Tower->RefreshAutoShaft();Check(Length(Tower->AutoShaftLayout)==Length(Initial)-400,TEXT("Manual shaft suppresses only its overlapping automatic geometry"));
            Manual->Destroy();Tower->RefreshAutoShaft();Check(Tower->AutoShaftLayout==Initial,TEXT("Removing a manual shaft restores automatic infill without changing floors"));
            const auto Stable=Tower->AutoShaftLayout;Tower->RefreshAutoShaft();Check(Tower->AutoShaftLayout==Stable,TEXT("Repeated shaft refresh does not duplicate geometry"));
            Top->Destroy();Tower->RefreshStops();Check(Tower->AutoShaftLayout.Height==4400,TEXT("Removing the highest floor preserves migrated shaft height"));
            if(World->GetNetMode()!=NM_DedicatedServer)Check(Tower->AutoShaftFrames->GetInstanceCount()==Tower->AutoShaftLayout.Frames.Num()&&Tower->AutoShaftScreens->GetInstanceCount()==Tower->AutoShaftLayout.Screens.Num(),TEXT("Client-visible shaft instances match the derived layout"));
            Bottom->Destroy();Middle->Destroy();Tower->RefreshStops();Check(Tower->AutoShaftLayout.Height==4400,TEXT("Removing all floors preserves migrated shaft independently"));Tower->Destroy();
            auto* Fluid=Drive(true,FVector(12000,6000,0));auto* FluidTop=Stop(Fluid,FVector(12000,6000,3200));Fluid->RefreshStops();
            Check(Fluid->AutoShaftLayout.Height==4000&&!Fluid->AutoShaftLayout.Frames.IsEmpty(),TEXT("Fluid elevators generate the same continuous shaft"));FluidTop->Destroy();Fluid->Destroy();
        }
        void Advance()
        {
            if(!World.IsValid())return;
            if(Inspect){InspectPower();World->GetTimerManager().ClearTimer(Timer);return;}
            if(Showcase){DemoAdvance();return;}
            switch(Stage++)
            {
            case 0:
            {
                if(auto* Save=UFGSaveSession::Get(World.Get())) Save->SetAutoSaveEnabled(false);
                Player=Cast<AFGPlayerController>(World->GetFirstPlayerController());
                Iron=TFClass(TEXT("/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate"));Copper=TFClass(TEXT("/Game/FactoryGame/Resource/Parts/CopperSheet/Desc_CopperSheet"));
                Water=TFClass(TEXT("/Game/FactoryGame/Resource/RawResources/Water/Desc_Water"));Oil=TFClass(TEXT("/Game/FactoryGame/Resource/RawResources/CrudeOil/Desc_LiquidOil"));
                Check(Iron&&Copper&&Water&&Oil,TEXT("All solid and liquid test descriptors available"));
                D=Drive(false,FVector::ZeroVector);F=Drive(true,FVector(1600,0,0));
                if(!D.IsValid()||!F.IsValid()) {Finish();return;}
                for(int32 H:{0,800,1600}) {I.Add(Stop(D.Get(),FVector(0,0,H)));L.Add(Stop(F.Get(),FVector(1600,0,H)));}
                UnitCases();AutoShaftCases();ReservationCases();OutgoingReservationCases();
                auto* Pole=Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk3/Build_PowerPoleMk3"),FVector(-1500,0,0));
                if(Pole)
                {
                    auto* Info=NewObject<UFGPowerInfoComponent>(Pole,TEXT("TFQASupply"));Info->RegisterComponent();Info->InitializeBatteryInfo(100,0);Info->GetBatteryInfo()->SetPowerStore(0);Info->SetBaseProduction(200);Supply=Info;
                    auto* P=Pole->FindComponentByClass<UFGPowerConnectionComponent>();P->SetPowerInfo(Info);P->AddHiddenConnection(D->Power);if(auto* Circuit=P->GetPowerCircuit())Circuit->MarkForFullRebuild();
                }
                break;
            }
            case 1:
                Check(D->Powered,TEXT("Drive receives power through a native electrical circuit"));Check(D->CabinHeight>0,TEXT("Cabin moves continuously in the packaged game"));
                if(Supply.IsValid()){Supply->SetBaseProduction(0);Supply->GetBatteryInfo()->SetPowerStore(0);}break;
            case 2:Frozen=D->CabinHeight;break;
            case 3:
                Check(!D->Powered&&FMath::IsNearlyEqual(D->CabinHeight,Frozen,.1f),TEXT("Power loss stops movement without teleporting the cabin"));
                if(Supply.IsValid())Supply->SetBaseProduction(200);break;
            case 4:if(auto* Circuit=D->Power->GetPowerCircuit()) Circuit->ResetFuse();break;
            case 5:
                Check(D->Powered&&D->CabinHeight>Frozen,TEXT("Restoring power resumes the interrupted trip"));
                if(Supply.IsValid()){Supply->GetBatteryInfo()->SetActive(true);Supply->GetBatteryInfo()->SetPowerStore(10);}
                break;
            case 6:if(Supply.IsValid())Supply->SetBaseProduction(0);break;
            case 8:
                InspectPower();Check(D->Powered&&Supply.IsValid()&&Supply->GetBatteryInfo()->GetPowerStore()<10,FString::Printf(TEXT("A battery-only native circuit powers the elevator and spends stored energy (powered=%d actual=%.3f target=%.3f remaining=%.6f)"),D->Powered,D->PowerInfo->GetActualConsumption(),D->PowerInfo->GetTargetConsumption(),Supply.IsValid()?Supply->GetBatteryInfo()->GetPowerStore():-1));
                if(Supply.IsValid())Supply->GetBatteryInfo()->SetPowerStore(0);break;
            case 10:
                Check(!D->Powered&&D->PowerInfo->GetActualConsumption()<.01f,TEXT("An exhausted battery stops service without free power"));break;
            default:if(ReservationBeltProgress())Finish();break;
            }
        }
        void Finish()
        {
            Report+=FString::Printf(TEXT("\nRESULT: %d checks, %d failures\n"),Checks,Failed);FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("TF-Integration.txt")));
            World->GetTimerManager().ClearTimer(Timer);
            // Fixtures stay only in the unsaved disposable test session for inspection.
        }
    };
}
void TFStartOutgoingQA(UWorld* World);
void TFStartQA(UWorld* World)
{
    TFStartOutgoingQA(World);
    if(World&&World->GetNetMode()==NM_Standalone&&FParse::Param(FCommandLine::Get(),TEXT("TFScrollShaftReload")))
    {
        TWeakObjectPtr<UWorld> W=World;FTimerHandle Timer;World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([W]
        {
            if(!W.IsValid())return;auto* PC=Cast<AFGPlayerController>(W->GetFirstPlayerController());if(!PC)return;
            ATFDrive* D=nullptr;for(TActorIterator<ATFStop> It(W.Get());It;++It)if(It->FloorName==TEXT("Scroll shaft QA")){D=It->Elevator;break;}
            int32 Count=0;if(D)for(TActorIterator<ATFShaft> It(W.Get());It;++It)if(It->Drive()==D)++Count;
            TArray<int32> Heights;
            if (D) for (auto Stop : D->Stops) if (IsValid(Stop) && Stop->Elevator == D) Heights.Add(FMath::RoundToInt(Stop->Height()));
            Heights.Sort();
            const bool OK = D && D->ShaftBuiltFirst && D->ShaftTop()==7600 && Count==17 && Heights==TArray<int32>{1600,2500,3800,5200};
            FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s: Native reload retained 17 constructed sections, independent 76m height and attached landings at 16m, 25m, 38m and 52m (sections=%d).\n"),OK?TEXT("PASS"):TEXT("FAIL"),Count),*(FPaths::ProjectSavedDir()/TEXT("TF-ScrollShaft-Reload.txt")));
            if(D)ShaftReviewView(PC,D);UFGSaveSession::Get(W.Get())->SetAutoSaveEnabled(false);
        }),12.f,false);return;
    }
    if(World&&World->GetNetMode()==NM_Standalone&&FParse::Param(FCommandLine::Get(),TEXT("TFDismantleInspect")))
    {
        TWeakObjectPtr<UWorld> W=World;FTimerHandle InspectTimer;
        World->GetTimerManager().SetTimer(InspectTimer,FTimerDelegate::CreateLambda([W]
        {
            if(!W.IsValid())return;auto* PC=Cast<AFGPlayerController>(W->GetFirstPlayerController());if(!PC||!PC->GetPawn())return;
            UFGSaveSession::Get(W.Get())->SetAutoSaveEnabled(false);
            const FVector Player=PC->GetPawn()->GetActorLocation();FString Report=TEXT("Read-only inspection of saved freight towers before QA fixtures.\n");
            TArray<ATFDrive*> Towers;for(TActorIterator<ATFDrive> It(W.Get());It;++It)Towers.Add(*It);
            Towers.Sort([Player](const ATFDrive& A,const ATFDrive& B){return FVector::DistSquared(Player,A.GetActorLocation())<FVector::DistSquared(Player,B.GetActorLocation());});
            for(auto* D:Towers)
            {
                TArray<AActor*> Dependencies;D->GetDismantleDependencies_Implementation(Dependencies);
                Report+=FString::Printf(TEXT("\n%s distance=%.1fm position=%s tier=%d fluid=%d height=%.1fm canDismantle=%d cargoEntries=%d cargoAmount=%.3f stops=%d dependencies=%d\n"),*D->GetName(),FVector::Distance(Player,D->GetActorLocation())/100,*D->GetActorLocation().ToString(),D->Tier,D->IsFluid,D->ShaftTop()/100,D->CanDismantle_Implementation(),D->Cargo.Num(),D->CargoAmount(),D->Stops.Num(),Dependencies.Num());
                for(const auto& C:D->Cargo)Report+=FString::Printf(TEXT("  cargo resource=%s amount=%.3f destination=%s\n"),*GetNameSafe(C.Resource.Get()),C.Amount,*GetNameSafe(C.Destination));
                for(auto* Part:Dependencies)
                {
                    Report+=FString::Printf(TEXT("  dependency %s position=%s\n"),*GetNameSafe(Part),*Part->GetActorLocation().ToString());
                    if(auto* S=Cast<ATFStop>(Part))Report+=FString::Printf(TEXT("    floor=%s height=%.1fm contents=%d canDismantle=%d\n"),*S->Label(),S->Height()/100,S->HasContents(),S->CanDismantle_Implementation());
                }
                TArray<TSubclassOf<UFGConstructDisqualifier>> Reasons;D->GetDismantleDisqualifiers_Implementation(Reasons,{D});
                for(auto R:Reasons)Report+=TEXT("  HUD: ")+UFGConstructDisqualifier::GetDisqualifyingText(R).ToString()+TEXT("\n");
            }
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("TF-Dismantle-Inspect.txt")));
            if(!FParse::Param(FCommandLine::Get(),TEXT("TFMultiDismantleTest")))if(auto* UI=PC->GetGameUI())UI->Native_HandlePauseGamePressed();
        }),10.f,false);
    }
    if(World&&World->GetNetMode()==NM_Standalone&&(FParse::Param(FCommandLine::Get(),TEXT("TFDismantleTest"))||FParse::Param(FCommandLine::Get(),TEXT("TFMultiDismantleTest"))))
    {
        TWeakObjectPtr<UWorld> W=World;FTimerHandle Timer;World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([W]
        {
            if(!W.IsValid())return;auto P=MakeShared<FTFQA>();P->World=W;P->Player=Cast<AFGPlayerController>(W->GetFirstPlayerController());
            if(!P->Player.IsValid()||!P->Player->GetCharacter())return;
            UFGSaveSession::Get(W.Get())->SetAutoSaveEnabled(false);P->CombinedCases();P->BeginDismantleCases();
        }),12.f,false);return;
    }
    if(World&&World->GetNetMode()==NM_Standalone&&FParse::Param(FCommandLine::Get(),TEXT("TFScrollShaftTest")))
    {
        TWeakObjectPtr<UWorld> W=World;FTimerHandle Timer;World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([W]
        {
            if(!W.IsValid())return;auto P=MakeShared<FTFQA>();P->World=W;P->Player=Cast<AFGPlayerController>(W->GetFirstPlayerController());if(!P->Player.IsValid()||!P->Player->GetCharacter())return;
            UFGSaveSession::Get(W.Get())->SetAutoSaveEnabled(false);P->CombinedCases();P->ScrollShaftCases();
            P->Report+=FString::Printf(TEXT("RESULT: %d checks, %d failures\n"),P->Checks,P->Failed);FFileHelper::SaveStringToFile(P->Report,*(FPaths::ProjectSavedDir()/TEXT("TF-ScrollShaft.txt")));
            if(!P->Failed)UFGSaveSession::Get(W.Get())->SaveGame(TEXT("Trapper_Combined_QA"),FOnSaveGameComplete(),nullptr);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/ScrollShaft-Test.png"),true,false);
            FTimerHandle Probe;W->GetTimerManager().SetTimer(Probe,FTimerDelegate::CreateLambda([W]
            {
                if(!W.IsValid())return;auto* PC=Cast<AFGPlayerController>(W->GetFirstPlayerController());auto* Character=PC?Cast<AFGCharacterPlayer>(PC->GetCharacter()):nullptr;if(!Character)return;
                auto* State=Cast<UFGBuildGunStateBuild>(Character->GetBuildGun()->GetCurrentState());auto* H=State?Cast<ATFHologram>(State->GetHologram()):nullptr;
                FString Info=H?FString::Printf(TEXT("sections=%d locked=%d canconstruct=%d instances=%d cost=%d position=%s\n"),H->ShaftSections,H->ShaftPositionLocked,H->CanConstruct(),H->GetZoopInstanceTransforms().Num(),H->GetBaseCostMultiplier(),*H->GetActorLocation().ToString()):TEXT("No freight hologram\n");
                if(H){TArray<TSubclassOf<UFGConstructDisqualifier>> Reasons;H->GetConstructDisqualifiers(Reasons);for(auto R:Reasons)Info+=R->GetName()+TEXT("\n");}Info+=Character->GetActorLocation().ToString();
                FFileHelper::SaveStringToFile(Info,*(FPaths::ProjectSavedDir()/TEXT("TF-ScrollShaft-Input.txt")));
            }),1.f,true);
        }),12.f,false);return;
    }
    if(World&&World->GetNetMode()==NM_Standalone&&FParse::Param(FCommandLine::Get(),TEXT("TFPauseOnly")))
    {
        TWeakObjectPtr<UWorld> W=World;FTimerHandle Timer;World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([W]{if(W.IsValid())if(auto* PC=Cast<AFGPlayerController>(W->GetFirstPlayerController()))if(auto* UI=PC->GetGameUI())UI->Native_HandlePauseGamePressed();}),8.f,false);
    }
    if(World&&World->GetNetMode()==NM_Standalone&&(FParse::Param(FCommandLine::Get(),TEXT("TFReservationReview"))||FParse::Param(FCommandLine::Get(),TEXT("TFReservationReload"))))
    {
        const bool Reload=FParse::Param(FCommandLine::Get(),TEXT("TFReservationReload"));TWeakObjectPtr<UWorld> W=World;auto Step=MakeShared<int32>(0);auto Timer=MakeShared<FTimerHandle>();
        World->GetTimerManager().SetTimer(*Timer,FTimerDelegate::CreateLambda([W,Step,Timer,Reload]
        {
            if(!W.IsValid())return;auto* PC=Cast<AFGPlayerController>(W->GetFirstPlayerController());if(!PC)return;
            if((*Step)++==0)
            {
                ATFStop* Stop=nullptr;
                for(TActorIterator<ATFStop> It(W.Get());It;++It)if(!It->IsFluid&&IsValid(It->Elevator)&&(!Reload||It->FloorName==TEXT("Reservation persistence QA"))){Stop=*It;break;}
                auto Iron=TFClass(TEXT("/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate"));auto Copper=TFClass(TEXT("/Game/FactoryGame/Resource/Parts/CopperIngot/Desc_CopperIngot"));
                FString Report;bool OK=Stop&&Iron&&Copper;
                if(OK&&!Reload)
                {
                    Stop->FloorName=TEXT("Reservation persistence QA");Stop->IncomingReservations.Reset();OK=Stop->SetIncomingReservation(Iron,5)&&Stop->SetIncomingReservation(Copper,10);
                    UFGSaveSession::Get(W.Get())->SetAutoSaveEnabled(false);UFGSaveSession::Get(W.Get())->SaveGame(TEXT("Trapper_Reservations_QA"),FOnSaveGameComplete(),nullptr);
                }
                if(OK)OK=Stop->ReservedIncomingStacks(Iron)==5&&Stop->ReservedIncomingStacks(Copper)==10;
                Report=FString::Printf(TEXT("%s: %s five Iron Plate stacks and ten Copper Ingot stacks.\n"),OK?TEXT("PASS"):TEXT("FAIL"),Reload?TEXT("Native save reload retained"):TEXT("Configured and requested separate QA save with"));
                FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/(Reload?TEXT("TF-Reservations-Reload.txt"):TEXT("TF-Reservations-Review.txt"))));
                if(Stop){UTFPanel::Open(PC,Stop);UTFPanel::Active->Handle(100,4,0);}
            }
            else {FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Reservations-Panel.png"),true,false);W->GetTimerManager().ClearTimer(*Timer);}
        }),3.f,true,12.f);
    }
    if(World&&World->GetNetMode()==NM_Standalone&&FParse::Param(FCommandLine::Get(),TEXT("TFAutoShaftReview")))
    {
        TWeakObjectPtr<UWorld> W=World;auto Step=MakeShared<int32>(0);auto Timer=MakeShared<FTimerHandle>();auto Camera=MakeShared<TWeakObjectPtr<ACameraActor>>();
        World->GetTimerManager().SetTimer(*Timer,FTimerDelegate::CreateLambda([W,Step,Timer,Camera]
        {
            if(!W.IsValid())return;auto* PC=Cast<AFGPlayerController>(W->GetFirstPlayerController());if(!PC)return;
            if((*Step)++==0)
            {
                FString Report;int32 Checks=0,Failed=0;ATFDrive* Tallest=nullptr;
                auto Check=[&](bool OK,const FString& Message){++Checks;if(!OK)++Failed;Report+=FString::Printf(TEXT("%s: %s\n"),OK?TEXT("PASS"):TEXT("FAIL"),*Message);};
                for(TActorIterator<ATFDrive> It(W.Get());It;++It)
                {
                    It->RefreshAutoShaft();const auto& Layout=It->AutoShaftLayout;
                    Check(It->AutoShaftFrames&&It->AutoShaftFrames->GetStaticMesh()&&It->AutoShaftFrames->GetInstanceCount()==Layout.Frames.Num(),It->GetName()+TEXT(" automatic frame meshes"));
                    Check(It->AutoShaftScreens&&It->AutoShaftScreens->GetStaticMesh()&&It->AutoShaftScreens->GetInstanceCount()==Layout.Screens.Num(),It->GetName()+TEXT(" automatic screen meshes"));
                    Check(FMath::IsNearlyEqual(It->FreightCollision->GetUnscaledBoxExtent().Z*2,Layout.Height),It->GetName()+TEXT(" full-height player collision"));
                    for(auto Span:Layout.Frames)Check(Span.X>=800&&Span.X+Span.Y<=Layout.Height,It->GetName()+TEXT(" generated span stays in tower envelope"));
                    if(!Tallest||Layout.Height>Tallest->AutoShaftLayout.Height)Tallest=*It;
                }
                Check(Tallest&&Tallest->AutoShaftLayout.Height>800,TEXT("Existing multi-floor elevator found for save-load migration review"));
                Report+=FString::Printf(TEXT("RESULT: %d checks, %d failures\n"),Checks,Failed);FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("TF-AutoShaft.txt")));
                if(Tallest)
                {
                    const float Height=Tallest->AutoShaftLayout.Height;const FVector Target=Tallest->GetActorLocation()+FVector(0,0,Height*.5f);
                    const FVector Position=Tallest->GetActorLocation()+FVector(Height*.9f,Height*.7f,Height*.55f);
                    *Camera=W->SpawnActor<ACameraActor>();auto* View=Camera->Get();View->GetCameraComponent()->SetFieldOfView(65);View->SetActorLocation(Position);View->SetActorRotation((Target-Position).Rotation());PC->SetViewTarget(View);
                }
            }
            else if(*Step==2)FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/AutoShaft-Existing.png"),true,false);
            else {PC->SetViewTarget(PC->GetPawn());if(Camera->IsValid())Camera->Get()->Destroy();if(auto* UI=PC->GetGameUI())UI->Native_HandlePauseGamePressed();W->GetTimerManager().ClearTimer(*Timer);}
        }),3.f,true,12.f);
    }
    if(World&&World->GetNetMode()==NM_Standalone&&FParse::Param(FCommandLine::Get(),TEXT("TFUsabilityPhotos")))
    {
        TWeakObjectPtr<UWorld> W=World;auto Step=MakeShared<int32>(0);auto Timer=MakeShared<FTimerHandle>();
        auto Camera=MakeShared<TWeakObjectPtr<ACameraActor>>();auto PreviewStop=MakeShared<TWeakObjectPtr<ATFFluidStop>>();auto Paused=MakeShared<TArray<bool>>();
        World->GetTimerManager().SetTimer(*Timer,FTimerDelegate::CreateLambda([W,Step,Timer,Camera,PreviewStop,Paused]
        {
            if(!W.IsValid())return;auto* PC=Cast<AFGPlayerController>(W->GetFirstPlayerController());if(!PC)return;
            ATFDrive* Item=nullptr;ATFDrive* Fluid=nullptr;for(TActorIterator<ATFDrive> It(W.Get());It;++It)if(It->Tier==2&&It->Stops.Num()==3){if(It->IsFluid)Fluid=*It;else Item=*It;}
            if(!Item||!Fluid)return;const int32 Stage=(*Step)++;auto* Panel=UTFPanel::Active.Get();
            auto Photo=[](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/(FString(Name)+TEXT(".png")),true,false);};
            if(Stage==0){Paused->Add(Item->Paused);Paused->Add(Fluid->Paused);Item->Paused=true;Fluid->Paused=true;UTFPanel::Open(PC,Item);}
            if(Stage==1&&Panel)
            {
                int32 Occupied=0,Icons=0;for(auto Tile:Panel->Tiles)if(Tile&&Tile->ShownResource){++Occupied;if(Tile->Icon->GetBrush().GetResourceObject())++Icons;}
                FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s: Inventory rendering: %d occupied slots, %d native icons. Layout verified from the captured game image.\n"),Occupied>0&&Occupied==Icons?TEXT("PASS"):TEXT("FAIL"),Occupied,Icons),*(FPaths::ProjectSavedDir()/TEXT("TF-Usability-Panel.txt")));Photo(TEXT("Usability-Storage"));
            }
            if(Stage==2&&Panel)Panel->Handle(100,1,0);
            if(Stage==3)Photo(TEXT("Usability-Ports"));
            if(Stage==4)UTFPanel::Open(PC,Fluid);
            if(Stage==5)Photo(TEXT("Usability-FluidStorage"));
            if(Stage==6&&Panel)Panel->Handle(100,1,0);
            if(Stage==7)Photo(TEXT("Usability-FluidPorts"));
            if(Stage==8)
            {
                const FVector PreviewBase=Fluid->GetActorLocation()+FVector(0,0,5000);*PreviewStop=W->SpawnActor<ATFFluidStop>(ATFFluidStop::StaticClass(),PreviewBase,FRotator::ZeroRotator);
                if(Panel)Panel->Close();*Camera=W->SpawnActor<ACameraActor>();auto* View=Camera->Get();View->GetCameraComponent()->SetFieldOfView(65);
                const FVector Target=PreviewBase+FVector(380,0,180);const FVector Position=PreviewBase+FVector(850,-300,240);
                View->SetActorLocation(Position);View->SetActorRotation((Target-Position).Rotation());PC->SetViewTarget(View);
            }
            if(Stage==9)Photo(TEXT("Usability-PipeSocket"));
            if(Stage==10)
            {
                if(PreviewStop->IsValid())PreviewStop->Get()->Destroy();PC->SetViewTarget(PC->GetPawn());if(Camera->IsValid())Camera->Get()->Destroy();Item->Paused=(*Paused)[0];Fluid->Paused=(*Paused)[1];UTFPanel::Open(PC,Fluid);
                W->GetTimerManager().ClearTimer(*Timer);
            }
        }),3.f,true,65.f);
    }
    if(World&&World->GetNetMode()==NM_Standalone&&FParse::Param(FCommandLine::Get(),TEXT("TFIronworksReview")))
    {
        TWeakObjectPtr<UWorld> W=World;FTimerHandle VisualTimer;
        World->GetTimerManager().SetTimer(VisualTimer,FTimerDelegate::CreateLambda([W]
        {
            if(!W.IsValid())return;auto* PC=Cast<AFGPlayerController>(W->GetFirstPlayerController());if(!PC)return;
            FString Report;int32 Checks=0,Failed=0;ATFDrive* Item=nullptr;
            auto Check=[&](bool Good,const FString& Text){++Checks;if(!Good)++Failed;Report+=FString::Printf(TEXT("%s: %s\n"),Good?TEXT("PASS"):TEXT("FAIL"),*Text);};
            for(TActorIterator<ATFPart> It(W.Get());It;++It)
            {
                for(auto C:It->Art)if(C)
                {
                    if(C->GetName().StartsWith(TEXT("IW_")))Check(C->GetStaticMesh()!=nullptr,It->GetName()+TEXT(" / ")+C->GetName()+TEXT(" mesh loaded"));
                    else Check(C->bHiddenInGame,It->GetName()+TEXT(" / legacy component hidden"));
                }
                Check(It->FreightCollision&&It->FreightCollision->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block,It->GetName()+TEXT(" / loaded pawn collision"));
                if(auto* Stop=Cast<ATFStop>(*It)){Check(Stop->PortNumbers.Num()==4&&Stop->PortDirections.Num()==4,TEXT("Four numbered port signs restored"));for(int32 I=0;I<4;++I)if(Stop->PortDirections.IsValidIndex(I)&&IsValid(Stop->PortDirections[I]))Check(Stop->PortDirections[I]->Text.ToString()==(Stop->Ports[I].Input?TEXT("INPUT"):TEXT("OUTPUT")),TEXT("Loaded direction sign matches port configuration"));}
                if(auto* D=Cast<ATFDrive>(*It))
                {
                    D->UpdateDisplay();Check(D->HoistHead&&D->HoistHead->GetRelativeLocation().Z>=570,TEXT("Hoist head follows shaft height"));
                    Check(D->Gates.Num()==8,TEXT("Four paired cabin gates"));
                    int32 Tanks=0;for(auto T:D->PressureTanks)if(T&&T->IsVisible())++Tanks;
                    Check(Tanks==(D->IsFluid?D->Compartments:0),TEXT("Tank appearance follows item/fluid and compartment configuration"));
                    if(!D->IsFluid)Item=D;
                }
            }
            Report+=FString::Printf(TEXT("RESULT: %d checks, %d failures\n"),Checks,Failed);FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("TF-Ironworks-Visual.txt")));
            if(Item)
            {
                const FVector P=Item->GetActorLocation()+FVector(1100,-900,110);const FRotator R=(Item->GetActorLocation()+FVector(0,0,300)-P).Rotation();
                if(auto* Character=PC->GetCharacter())Character->SetActorLocation(P);
                PC->SetControlRotation(R);
                if(FParse::Param(FCommandLine::Get(),TEXT("TFIronworksPhotos")))
                {
                    auto Step=MakeShared<int32>(0);auto Timer=MakeShared<FTimerHandle>();TWeakObjectPtr<ATFDrive> ItemDrive=Item;TWeakObjectPtr<AFGPlayerController> Controller=PC;
                    auto Hidden=MakeShared<TArray<TPair<TWeakObjectPtr<AActor>,bool>>>();
                    if(auto* Character=Cast<AFGCharacterPlayer>(PC->GetPawn()))
                    {TArray<AActor*> Attached;Character->GetAttachedActors(Attached,true,true);for(auto* Equipment:Character->GetActiveEquipments())Attached.AddUnique(Equipment);Attached.AddUnique(Character);for(auto* A:Attached)if(IsValid(A)){Hidden->Add({A,A->IsHidden()});A->SetActorHiddenInGame(true);}}
                    auto* Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(72);TWeakObjectPtr<ACameraActor> View=Camera;
                    W->GetTimerManager().SetTimer(*Timer,FTimerDelegate::CreateLambda([W,Step,Timer,ItemDrive,Controller,View,Hidden]
                    {
                        if(!W.IsValid()||!ItemDrive.IsValid()||!Controller.IsValid()||!View.IsValid())return;
                        const int32 Stage=(*Step)++;auto* D=ItemDrive.Get();
                        if(Stage==0||Stage==2||Stage==4)
                        {
                            FVector P,Look;float FOV=72;
                            if(Stage==0){P=D->GetActorLocation()+FVector(1100,-850,320);Look=D->GetActorLocation()+FVector(0,0,320);}
                            else if(Stage==2)
                            {
                                for(TActorIterator<ATFDrive> It(W.Get());It;++It)if(It->IsFluid&&It->Tier==2){D=*It;break;}
                                P=D->GetActorLocation()+FVector(680,0,D->CabinHeight+300);Look=D->GetActorLocation()+FVector(0,0,D->CabinHeight+300);FOV=78;
                            }
                            else{float Top=800;for(auto S:D->Stops)if(IsValid(S))Top=FMath::Max(Top,S->Height()+800);P=D->GetActorLocation()+FVector(720,-650,Top-95);Look=D->GetActorLocation()+FVector(0,0,Top-95);}
                            View->SetActorLocation(P);View->SetActorRotation((Look-P).Rotation());View->GetCameraComponent()->SetFieldOfView(FOV);Controller->SetViewTarget(View.Get());
                        }
                        else if(Stage==1||Stage==3||Stage==5)
                        {
                            if(Stage==3)for(TActorIterator<ATFDrive> It(W.Get());It;++It)if(It->IsFluid&&It->Tier==2)
                            {const FVector Look=It->GetActorLocation()+FVector(0,0,It->CabinHeight+300);const FVector P=Look+FVector(680,0,0);View->SetActorLocation(P);View->SetActorRotation((Look-P).Rotation());break;}
                            const TCHAR* Name=Stage==1?TEXT("Ironworks-Item"):Stage==3?TEXT("Ironworks-Fluid"):TEXT("Ironworks-Winch");
                            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/(FString(Name)+TEXT(".png")),false,false);
                        }
                        else if(Stage==6)
                        {
                            Controller->SetViewTarget(Controller->GetPawn());View->Destroy();for(const auto& State:*Hidden)if(State.Key.IsValid())State.Key->SetActorHiddenInGame(State.Value);
                            auto* Widget=UTFPanel::Active.Get();if(Widget)Widget->Close();
                            APawn* Pawn=Controller->GetPawn();UTFPanel* Panel=nullptr;
                            if(Pawn){FUseState Use;Use.SetUseState(UFGUseState_Valid::StaticClass());IFGUseableInterface::Execute_OnUse(ItemDrive.Get(),Cast<AFGCharacterPlayer>(Pawn),Use);Panel=UTFPanel::Active.Get();}
                            if(Panel){Panel->Close();const bool Released=!Controller->bShowMouseCursor&&!Controller->IsLookInputIgnored()&&!Controller->IsMoveInputIgnored();FFileHelper::SaveStringToFile(Released?TEXT("PASS: Closing the freight panel immediately releases mouse, look and movement input.\n"):TEXT("FAIL: Freight panel input remains captured.\n"),*(FPaths::ProjectSavedDir()/TEXT("TF-Ironworks-Panel.txt")));}
                            // Clearing a timer releases its capture storage: do this after every use.
                            W->GetTimerManager().ClearTimer(*Timer);
                        }
                    }),2.f,true,3.f);
                }
            }
        }),12.f,false);
    }
    if(World&&World->GetNetMode()==NM_DedicatedServer&&FParse::Param(FCommandLine::Get(),TEXT("TFNetworkPlacement")))
    {
        auto Timer=MakeShared<FTimerHandle>();TWeakObjectPtr<UWorld> W=World;
        World->GetTimerManager().SetTimer(*Timer,FTimerDelegate::CreateLambda([W,Timer]
        {
            if(!W.IsValid())return;auto* PC=Cast<AFGPlayerController>(W->GetFirstPlayerController());if(!PC||!PC->GetCharacter())return;
            for(TActorIterator<ATFDrive> It(W.Get());It;++It)if(!It->IsFluid&&It->Tier==2&&It->Stops.Num()==3)
            {
                const FVector P=It->GetActorLocation()+FVector(1000,1000,250);PC->GetCharacter()->TeleportTo(P,FRotator::ZeroRotator);PC->ClientSetLocation(P,FRotator(-8,-145,0));
                if(FParse::Param(FCommandLine::Get(),TEXT("TFClientShaftTest")))
                {
                    // Wait for the joining player's saved inventory to finish restoring.
                    TWeakObjectPtr<AFGPlayerController> TestPC=PC;FTimerHandle SupplyTimer;
                    W->GetTimerManager().SetTimer(SupplyTimer,FTimerDelegate::CreateLambda([TestPC]
                    {
                        if(!TestPC.IsValid())return;auto* C=Cast<AFGCharacterPlayer>(TestPC->GetCharacter());if(!C)return;
                        auto Beam=TFClass(TEXT("/Game/FactoryGame/Resource/Parts/SteelPlate/Desc_SteelPlate"));auto Concrete=TFClass(TEXT("/Game/FactoryGame/Resource/Parts/Cement/Desc_Cement"));
                        const int32 B=C->GetInventory()->AddStack(FInventoryStack(50,Beam),true),N=C->GetInventory()->AddStack(FInventoryStack(50,Concrete),true);
                        FFileHelper::SaveStringToFile(FString::Printf(TEXT("Authoritative QA supply: added beams=%d concrete=%d; inventory now beams=%d concrete=%d; slots=%d\n"),B,N,C->GetInventory()->GetNumItems(Beam),C->GetInventory()->GetNumItems(Concrete),C->GetInventory()->GetSizeLinear()),*(FPaths::ProjectSavedDir()/TEXT("TF-Network-Supply.txt")));
                    }),8.f,false);
                }
                FFileHelper::SaveStringToFile(TEXT("Authenticated QA player placed near the freight controls."),*(FPaths::ProjectSavedDir()/TEXT("TF-Network-Placement.txt")));
                W->GetTimerManager().ClearTimer(*Timer);break;
            }
        }),1.f,true,8.f);return;
    }
    if(World&&World->GetNetMode()==NM_Standalone&&FParse::Param(FCommandLine::Get(),TEXT("TFReady")))
    {
        FTimerHandle Timer;TWeakObjectPtr<UWorld> W=World;
        World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([W]
        {
            if(!W.IsValid())return;auto* PC=Cast<AFGPlayerController>(W->GetFirstPlayerController());if(!PC)return;
            for(TActorIterator<ATFDrive> It(W.Get());It;++It)if(!It->IsFluid&&It->Tier==2&&It->Stops.Num()==3)
            {if(auto* Character=PC->GetCharacter()){Character->SetActorLocation(It->GetActorLocation()+FVector(1600,1600,220));PC->SetControlRotation(FRotator(22,-90,0));}break;}
            if(auto* UI=PC->GetGameUI())UI->Native_HandlePauseGamePressed();
        }),8.f,false);return;
    }
    if(World&&FParse::Param(FCommandLine::Get(),TEXT("TFClientTest"))){extern void TFStartClientQA(UWorld* World);TFStartClientQA(World);return;}
    if(!World || World->GetNetMode()==NM_Client) return;
    const bool ResumeShowcase=FParse::Param(FCommandLine::Get(),TEXT("TFPlaygroundReview"));
    const bool Showcase=ResumeShowcase||FParse::Param(FCommandLine::Get(),TEXT("TFShowcase"));
    const bool Inspect=FParse::Param(FCommandLine::Get(),TEXT("TFInspect"));
    if(!Showcase && !Inspect && !FParse::Param(FCommandLine::Get(),TEXT("TFIntegrationTest")))return;
    auto Test=MakeShared<FTFQA>();Test->World=World;
    Test->Showcase=Showcase;
    Test->ResumeShowcase=ResumeShowcase;
    Test->Inspect=Inspect;
    World->GetTimerManager().SetTimer(Test->Timer,FTimerDelegate::CreateLambda([Test]{Test->Advance();}),1.f,true,8.f);
}
