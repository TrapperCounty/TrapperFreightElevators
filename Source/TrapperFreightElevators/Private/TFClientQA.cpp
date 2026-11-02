// Opt-in loopback client test. Uses the server's normal authentication and encryption.
#include "TFBuildables.h"
#include "TFPanel.h"
#include "TFHologram.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunBuild.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "FGGameInstance.h"
#include "FGPlayerController.h"
#include "FGCharacterPlayer.h"
#include "FGCentralStorageSubsystem.h"
#include "FGFactoryConnectionComponent.h"
#include "Server/FGDSGameplayTypes.h"
#include "JsonObjectConverter.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
    void Join(UWorld* W)
    {
        auto* G=W?Cast<UFGGameInstance>(W->GetGameInstance()):nullptr;auto* P=W?W->GetFirstPlayerController():nullptr;
        FString JSON;FFGServerJoinDataResponse Data;
        if(!G||!P||!FFileHelper::LoadFileToString(JSON,TEXT("S:/SatisfactoryModding/QA/ServerPrivate/client-join.json"))||!FJsonObjectConverter::JsonObjectStringToUStruct(JSON,&Data,0,0))return;
        if(Data.EncryptionToken.IsEmpty()||Data.EncryptionKey.IsEmpty()||Data.ServerEntryToken.PrivilegeLevel!=EPrivilegeLevel::Client)return;
        FFGPendingConnectionEncryptionData Pending;Pending.EncryptionToken=Data.EncryptionToken;Pending.EncryptionData.Identifier=Data.EncryptionToken;Pending.EncryptionData.Key.SetNum(Data.EncryptionKey.Len()/2);
        if(HexToBytes(Data.EncryptionKey,Pending.EncryptionData.Key.GetData())!=Pending.EncryptionData.Key.Num())return;
        G->SetPendingConnectionEncryptionData(Pending);
        FFileHelper::SaveStringToFile(TEXT("Authenticated encrypted loopback join requested."),*(FPaths::ProjectSavedDir()/TEXT("TF-Client.txt")));
        P->ClientTravel(TEXT("127.0.0.1:17777?EncryptionToken=")+Data.EncryptionToken+TEXT("?EntryTicket=")+Data.ServerEntryToken.ToString(),TRAVEL_Absolute);
    }
    struct FProbe : TSharedFromThis<FProbe>
    {
        TWeakObjectPtr<UWorld> W;TWeakObjectPtr<ATFDrive> D,F;TWeakObjectPtr<AFGPlayerController> PC;TWeakObjectPtr<UTFRemoteCall> Remote;
        FTimerHandle Timer;FString Report;int32 Stage=0,Checks=0,Failed=0,Attempts=0;bool OriginalPaused=false,OriginalPort=false;float OriginalWait=30;
        TSubclassOf<UFGItemDescriptor> ReservationResource;int32 OriginalReservation=0;
        float ShaftStartHeight=0;int32 ShaftStartCount=0,BeamBefore=0,ConcreteBefore=0;TSubclassOf<UFGItemDescriptor> Beam,Concrete;TWeakObjectPtr<ATFHologram> ShaftPreview;TWeakObjectPtr<UFGBuildGunStateBuild> ShaftState;
        void Check(bool OK,const FString& Label){++Checks;if(!OK)++Failed;Report+=FString::Printf(TEXT("%s: %s\n"),OK?TEXT("PASS"):TEXT("FAIL"),*Label);FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("TF-Client.txt")));}
        void Command(int32 Action,int32 A=0,int32 B=0,float Number=0,TSubclassOf<UFGItemDescriptor> Resource=nullptr){if(Remote.IsValid()&&D.IsValid())Remote->Command(D.Get(),Action,A,B,Number,Resource,TEXT(""));}
        int32 Available(TSubclassOf<UFGItemDescriptor> Resource) const
        {
            auto* C=PC.IsValid()?Cast<AFGCharacterPlayer>(PC->GetCharacter()):nullptr;auto* Depot=W.IsValid()?AFGCentralStorageSubsystem::Get(W.Get()):nullptr;
            return (C?C->GetInventory()->GetNumItems(Resource):0)+(Depot?Depot->GetNumItemsFromCentralStorage(Resource):0);
        }
        void Tick()
        {
            if(!W.IsValid())return;
            if(Stage==0)
            {
                PC=Cast<AFGPlayerController>(W->GetFirstPlayerController());if(!PC.IsValid()||!PC->GetPawn()){if(++Attempts>60)Finish();return;}
                for(TActorIterator<ATFDrive> It(W.Get());It;++It)if(It->Tier==2&&It->Stops.Num()==3){if(It->IsFluid)F=*It;else D=*It;}
                if(!D.IsValid()||!F.IsValid()){if(++Attempts>60){Check(false,TEXT("Replicated playground elevators arrive"));Finish();}return;}
                bool StopsReady=true;for(auto S:D->Stops)StopsReady&=IsValid(S);for(auto S:F->Stops)StopsReady&=IsValid(S);
                if(!StopsReady){if(++Attempts>60){Check(false,TEXT("Replicated floor actor references resolve"));Finish();}return;}
                bool StateReady=true;for(auto S:D->Stops)StateReady&=S->Elevator==D.Get()&&S->IncomingView.Num()==FTFStats::ForTier(D->Tier).Stacks&&S->OutgoingView.Num()==FTFStats::ForTier(D->Tier).Stacks;for(auto S:F->Stops)StateReady&=S->Elevator==F.Get();
                if(!StateReady){if(++Attempts>60){Check(false,TEXT("Floor links and compact inventory snapshots arrive on the client"));Finish();}return;}
                Remote=PC->GetRemoteCallObjectOfClass<UTFRemoteCall>();if(!Remote.IsValid()){if(++Attempts>60){Check(false,TEXT("Server control channel available"));Finish();}return;}
                Check(W->GetNetMode()==NM_Client,TEXT("Tests run on a real client connected to a dedicated server"));
                Check(D->Tier==2&&F->Tier==2,TEXT("Saved drive tiers replicate to the client"));
                Check(D->Stops.Num()==3&&F->Stops.Num()==3,TEXT("All six floor-stop identities replicate"));
                Check(D->Stops[0]->Belts.Num()==4&&F->Stops[0]->Pipes.Num()==4,TEXT("Native belt and pipe connection components exist on the client"));
                Check(F->Compartments==2&&F->TankResources.Num()==4&&F->TankResources[0]&&F->TankResources[1]&&F->TankResources[0]!=F->TankResources[1],TEXT("Isolated water/oil assignments survive save, server load and replication"));
                Check(FVector::Dist(PC->GetPawn()->GetActorLocation(),D->GetActorLocation())<3000,TEXT("Authenticated player is near the restored control panel"));
                Check(StateReady,TEXT("All floor-to-drive links and compact inventory snapshots replicate"));
                Check(D->Stops[1]->Height()>0&&D->Stops[2]->Height()>D->Stops[1]->Height(),TEXT("Client floor heights distinguish the three landings"));
                OriginalPaused=D->Paused;OriginalPort=D->Stops[0]->Ports[1].Input;OriginalWait=D->Stops[0]->MaxWait;
                Command(1);++Stage;return;
            }
            switch(Stage++)
            {
            case 1:Check(D->Paused!=OriginalPaused,TEXT("Client pause request changes authoritative state and replicates back"));Command(1);break;
            case 2:Check(D->Paused==OriginalPaused,TEXT("Client resume restores the original service state"));Command(7,0,0,17);break;
            case 3:Check(FMath::IsNearlyEqual(D->Stops[0]->MaxWait,17.f),TEXT("Client sets a per-floor maximum wait through the server"));Command(7,0,0,OriginalWait);break;
            case 4:Check(FMath::IsNearlyEqual(D->Stops[0]->MaxWait,OriginalWait),TEXT("Original maximum wait is restored"));Command(9,0,1);break;
            case 5:Check(D->Stops[0]->Ports[1].Input!=OriginalPort,TEXT("Client changes an unconnected port's authoritative direction"));Check(D->Stops[0]->Belts[1]->GetDirection()==(OriginalPort?EFactoryConnectionDirection::FCD_OUTPUT:EFactoryConnectionDirection::FCD_INPUT),TEXT("Replicated port configuration updates the native client connection"));Command(9,0,1);break;
            case 6:Check(D->Stops[0]->Ports[1].Input==OriginalPort,TEXT("Original port direction is restored"));Check(D->Powered&&F->Powered,TEXT("Both restored elevators report live native power on the client"));break;
            case 7:
            {
                FUseState Use;Use.SetUseState(UFGUseState_Valid::StaticClass());IFGUseableInterface::Execute_OnUse(D.Get(),Cast<AFGCharacterPlayer>(PC->GetPawn()),Use);
                Check(UTFPanel::Active.IsValid(),TEXT("Native Use handler opens the multiplayer configuration panel"));
                ReservationResource=LoadObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate.Desc_IronPlate_C"));
                OriginalReservation=D->Stops[0]->ReservedIncomingStacks(ReservationResource);Command(22,0,0,5,ReservationResource);break;
            }
            case 8:Check(D->Stops[0]->ReservedIncomingStacks(ReservationResource)==5,TEXT("Client reservation changes authoritative state and replicates five stacks back"));Command(22,0,0,FTFStats::ForTier(D->Tier).Stacks+1,ReservationResource);break;
            case 9:Check(D->Stops[0]->ReservedIncomingStacks(ReservationResource)==5,TEXT("Server rejects a client's over-capacity reservation without changing the valid rule"));Command(22,0,0,OriginalReservation,ReservationResource);break;
            case 10:
                Check(D->Stops[0]->ReservedIncomingStacks(ReservationResource)==OriginalReservation,TEXT("Original per-floor reservation is restored through the client"));if(UTFPanel::Active.IsValid())UTFPanel::Active->Handle(100,4,0);
                if(!FParse::Param(FCommandLine::Get(),TEXT("TFClientShaftTest")))Finish();break;
            case 11:
            {
                if(UTFPanel::Active.IsValid())UTFPanel::Active->Handle(0,0,0);
                auto* C=Cast<AFGCharacterPlayer>(PC->GetCharacter());PC->SetIgnoreLookInput(true);Attempts=0;
                FVector Eye;FRotator Rotation;PC->GetPlayerViewPoint(Eye,Rotation);PC->SetControlRotation((D->GetActorLocation()+FVector(0,0,250)-Eye).Rotation());
                ShaftStartHeight=D->ShaftTop();for(TActorIterator<ATFShaft> It(W.Get());It;++It)if(It->Drive()==D.Get())++ShaftStartCount;
                if(!C->IsBuildGunEquipped())C->ToggleBuildGun();C->GetBuildGun()->GotoBuildState(LoadObject<UClass>(nullptr,TEXT("/TrapperFreightElevators/Recipes/Recipe_ItemDrive_Mk2.Recipe_ItemDrive_Mk2_C")));break;
            }
            case 12:
            {
                auto* C=Cast<AFGCharacterPlayer>(PC->GetCharacter());ShaftState=Cast<UFGBuildGunStateBuild>(C->GetBuildGun()->GetCurrentState());ShaftPreview=ShaftState.IsValid()?Cast<ATFHologram>(ShaftState->GetHologram()):nullptr;
                const bool Ready=ShaftPreview.IsValid()&&ShaftPreview->ShaftDrive()==D.Get()&&ShaftPreview->CanConstruct();
                if(!Ready&&Attempts++<4){FVector Eye;FRotator Rotation;PC->GetPlayerViewPoint(Eye,Rotation);PC->SetControlRotation((D->GetActorLocation()+FVector(0,0,250)-Eye).Rotation());--Stage;return;}
                Check(Ready,TEXT("Combined elevator tool targets the existing tower and is affordable with shaft materials alone"));
                if(!Ready){const auto& Hit=C->GetBuildGun()->GetHitResult();Report+=FString::Printf(TEXT("Diagnostic: hit=%s preview=%s position=%s tower=%s top=%.0f\n"),*GetNameSafe(Hit.GetActor()),*GetNameSafe(ShaftPreview.Get()),ShaftPreview.IsValid()?*ShaftPreview->GetActorLocation().ToString():TEXT("none"),*D->GetActorLocation().ToString(),D->ShaftTop());if(ShaftPreview.IsValid()){TArray<TSubclassOf<UFGConstructDisqualifier>> Reasons;ShaftPreview->GetConstructDisqualifiers(Reasons);for(auto R:Reasons)Report+=R->GetName()+TEXT("\n");}Finish();return;}
                Attempts=0;Beam=LoadObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Resource/Parts/SteelPlate/Desc_SteelPlate.Desc_SteelPlate_C"));Concrete=LoadObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Resource/Parts/Cement/Desc_Cement.Desc_Cement_C"));
                BeamBefore=Available(Beam);ConcreteBefore=Available(Concrete);
                ShaftState->PrimaryFire();ShaftState->PrimaryFireRelease();break;
            }
            case 13:
            {
                FVector Eye; FRotator Rotation;
                PC->GetPlayerViewPoint(Eye, Rotation);
                PC->SetControlRotation((D->GetActorLocation() + FVector(0,0,5000) - Eye).Rotation());
                break;
            }
            case 14:
                Check(ShaftPreview.IsValid()&&ShaftPreview->ShaftPositionLocked&&ShaftPreview->ShaftSections==10&&Cast<ATFTowerHologram>(ShaftPreview.Get()) && ShaftPreview->GetCost(true).Num()==2,TEXT("Client anchors and aims up to select ten sections through native build-gun actions"));
                Check(ShaftPreview.IsValid()&&ShaftPreview->CanConstruct(),TEXT("Scrolled client preview remains valid before network construction"));
                if(!ShaftPreview.IsValid()||!ShaftPreview->CanConstruct()){Finish();return;}ShaftState->PrimaryFire();ShaftState->PrimaryFireRelease();break;
            case 15:
            {
                int32 Count=0;for(TActorIterator<ATFShaft> It(W.Get());It;++It)if(It->Drive()==D.Get())++Count;
                if((Count!=ShaftStartCount+10||!FMath::IsNearlyEqual(D->ShaftTop(),ShaftStartHeight+4000))&&Attempts++<5){--Stage;return;}
                Check(Count==ShaftStartCount+10,TEXT("Native construction RPC creates exactly ten authoritative sections and replicates them"));
                Check(FMath::IsNearlyEqual(D->ShaftTop(),ShaftStartHeight+4000),TEXT("Server-approved extension increases the client's independent tower height by 40m"));
                Attempts=0;break;
            }
            case 16:
            {
                const int32 B=Available(Beam),N=Available(Concrete);const bool Charged=B==BeamBefore-40&&N==ConcreteBefore-40;
                if(!Charged&&Attempts++<5){--Stage;return;}
                Check(Charged,FString::Printf(TEXT("Authoritative construction charges forty beams and concrete across inventory/depot after replication (beams %d -> %d, concrete %d -> %d)"),BeamBefore,B,ConcreteBefore,N));Finish();break;
            }
            }
        }
        void Finish(){if(PC.IsValid())PC->ResetIgnoreLookInput();Report+=FString::Printf(TEXT("\nRESULT: %d checks, %d failures\n"),Checks,Failed);FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("TF-Client.txt")));if(W.IsValid())W->GetTimerManager().ClearTimer(Timer);}
    };
}
void TFStartClientQA(UWorld* World)
{
    if(World->GetNetMode()==NM_Client)
    {
        auto P=MakeShared<FProbe>();P->W=World;World->GetTimerManager().SetTimer(P->Timer,FTimerDelegate::CreateLambda([P]{P->Tick();}),1.f,true,8.f);
    }
    else
    {
        FTimerHandle Timer;TWeakObjectPtr<UWorld> W=World;World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([W]{if(W.IsValid())Join(W.Get());}),10.f,false);
    }
}
