// Explicit local repair/reload verification only. No normal-load mutations.
#include "TFBuildables.h"
#include "TFPanel.h"
#include "FGSaveSession.h"
#include "FGPlayerController.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void TFStartOutgoingQA(UWorld* World)
{
    if(!World||World->GetNetMode()!=NM_Standalone)return;
    const bool Repair=FParse::Param(FCommandLine::Get(),TEXT("TFOutgoingRepair"));
    const bool Reload=FParse::Param(FCommandLine::Get(),TEXT("TFOutgoingReload"));
    if(!Repair&&!Reload)return;
    FString TargetName,ReturnName;
    if(!FParse::Value(FCommandLine::Get(),TEXT("TFOutgoingTarget="),TargetName)||!FParse::Value(FCommandLine::Get(),TEXT("TFOutgoingReturn="),ReturnName))return;
    TWeakObjectPtr<UWorld> W=World;auto Timer=MakeShared<FTimerHandle>();auto Tick=MakeShared<int32>(0);auto Log=MakeShared<FString>();auto Baseline=MakeShared<double>(0);
    World->GetTimerManager().SetTimer(*Timer,FTimerDelegate::CreateLambda([W,Timer,Tick,Log,Baseline,TargetName,ReturnName,Repair,Reload]
    {
        if(!W.IsValid())return;
        auto Report=[&](const FString& Message){*Log+=Message+TEXT("\n");FFileHelper::SaveStringToFile(*Log,*(FPaths::ProjectSavedDir()/(Reload?TEXT("TF-Outgoing-Reload.txt"):TEXT("TF-Outgoing-Repair.txt"))));};
        auto Fail=[&](const FString& Message){Report(TEXT("FAIL: ")+Message);W->GetTimerManager().ClearTimer(*Timer);};
        ATFStop* Dest=nullptr;ATFStop* Source=nullptr;
        for(TActorIterator<ATFStop> It(W.Get());It;++It){if(It->GetName()==TargetName)Dest=*It;if(It->GetName()==ReturnName)Source=*It;}
        auto Rod=LoadClass<UFGItemDescriptor>(nullptr,TEXT("/Game/FactoryGame/Resource/Parts/IronRod/Desc_IronRod.Desc_IronRod_C"));
        auto Plate=LoadClass<UFGItemDescriptor>(nullptr,TEXT("/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate.Desc_IronPlate_C"));
        if(!Dest||!Source||!Rod||!Plate||Dest->IsFluid||Source->IsFluid||!IsValid(Dest->Elevator)||Source->Elevator!=Dest->Elevator||!FMath::IsNearlyEqual(Dest->Height(),4800.f,.1f)){Fail(TEXT("Requested 48m item stop and return floor were not found on the same tower."));return;}
        auto* D=Dest->Elevator.Get();auto* PC=Cast<AFGPlayerController>(W->GetFirstPlayerController());
        if(!PC)return;
        const auto* Route=D->Routes.FindByPredicate([&](const FTFRoute& R){return R.Resource==Rod&&R.Destination==Dest&&R.Weight>0;});
        if(!Route){Fail(TEXT("Iron Rod route missing; nothing changed."));return;}
        if((*Tick)++==0)
        {
            UFGSaveSession::Get(W.Get())->SetAutoSaveEnabled(false);
            *Baseline=Route->Delivered;
            if(Repair)
            {
                const int32 Needed=FMath::Max(0,5-FMath::CeilToInt((Dest->Outgoing->GetNumItems(Rod)+Dest->ItemFree(Rod,false))/TF::StackSize(Rod)));
                const int32 ReturnCount=Needed*TF::StackSize(Plate);
                if(Dest->Outgoing->GetNumItems(Plate)<ReturnCount||Source->IncomingRoom(Plate)<ReturnCount){Fail(TEXT("Insufficient safe return capacity; no items or rules changed."));return;}
                if(!Dest->SetStorageReservation(1,Rod,5)){Fail(TEXT("Reservation rejected; nothing moved."));return;}
                const int32 Before=Dest->Outgoing->GetNumItems(Plate)+Source->Incoming->GetNumItems(Plate);
                const int32 Moved=ReturnCount?Source->Incoming->AddStack(FInventoryStack(ReturnCount,Plate),true):0;
                Dest->Outgoing->Remove(Plate,Moved);
                const int32 After=Dest->Outgoing->GetNumItems(Plate)+Source->Incoming->GetNumItems(Plate);
                if(Before!=After||Moved!=ReturnCount){Fail(TEXT("Return transfer verification failed; inspect report before saving."));return;}
                Report(FString::Printf(TEXT("PASS: Reserved 5 outgoing Iron Rod slots at 48m; returned %d Iron Plates to %.0fm incoming storage. Plate total before=%d after=%d."),Moved,Source->Height()/100,Before,After));
            }
            else
            {
                if(Dest->ReservedStorageStacks(1,Rod)!=5){Fail(TEXT("Outgoing reservation did not persist."));return;}
                Report(FString::Printf(TEXT("PASS: Native save reload retained five outgoing Iron Rod slots; stored rods=%d, routed rods=%.0f."),Dest->Outgoing->GetNumItems(Rod),Route->Delivered));
                UTFPanel::Open(PC,Dest);if(UTFPanel::Active.IsValid()){UTFPanel::Active->ReservationBuffer=1;UTFPanel::Active->Handle(100,4,0);}
                W->GetTimerManager().ClearTimer(*Timer);return;
            }
        }
        const int32 Rods=Dest->Outgoing->GetNumItems(Rod);const double Delivered=Route->Delivered-*Baseline;
        if(*Tick%5==0)Report(FString::Printf(TEXT("Progress: rods stored=%d, newly routed=%.0f, plate free=%.0f, cabin=%.1fm, status=%s"),Rods,Delivered,Dest->ItemFree(Plate),D->CabinHeight/100,*D->Status));
        if(Rods>0&&Delivered>=5*TF::StackSize(Rod))
        {
            Report(FString::Printf(TEXT("PASS: Live elevator delivered rods to 48m. Stored=%d newly routed=%.0f; reservation=%d."),Rods,Delivered,Dest->ReservedStorageStacks(1,Rod)));
            UTFPanel::Open(PC,Dest);if(UTFPanel::Active.IsValid()){UTFPanel::Active->ReservationBuffer=1;UTFPanel::Active->Handle(100,4,0);}
            UFGSaveSession::Get(W.Get())->SaveGame(TEXT("Trapper_Outgoing_Reservations"),FOnSaveGameComplete::CreateLambda([Log](bool OK,const FText& Error,void*){*Log+=OK?TEXT("PASS: Saved repaired factory as Trapper_Outgoing_Reservations.\n"):FString(TEXT("FAIL: Save failed: "))+Error.ToString();FFileHelper::SaveStringToFile(*Log,*(FPaths::ProjectSavedDir()/TEXT("TF-Outgoing-Repair.txt")));}),nullptr);
            // Clearing an executing timer can release its delegate captures.
            // Do this last, after handing the log to the save callback.
            W->GetTimerManager().ClearTimer(*Timer);return;
        }
        else if(*Tick>=90){Fail(TEXT("Timed out waiting for rods; factory has not been saved by this diagnostic."));}
    }),2.f,true,10.f);
}
