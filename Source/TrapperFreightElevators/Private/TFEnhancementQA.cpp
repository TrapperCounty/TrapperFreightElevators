#include "TFBuildables.h"
#include "TFBuildingSnap.h"
#include "TFStorageLayout.h"
#include "Hologram/FGFactoryBuildingHologram.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Components/BoxComponent.h"
#include "FGConstructDisqualifier.h"
#include "FGRecipe.h"
#include "FGSaveSession.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "Modules/ModuleManager.h"

// Opt-in, disposable dedicated-server fixtures only. Never runs in a player's local game.
void TFStartEnhancementQA(UWorld* World)
{
    if(!World||World->GetNetMode()!=NM_DedicatedServer||!FParse::Param(FCommandLine::Get(),TEXT("TFEnhancementTest")))return;
    TWeakObjectPtr<UWorld> W=World;FTimerHandle Timer;
    World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([W]
    {
        if(!W.IsValid())return;
        UFGSaveSession::Get(W.Get())->SetAutoSaveEnabled(false);
        FString Report;int32 Checks=0,Failed=0;
        auto Check=[&](bool OK,const FString& Message)
        {
            ++Checks;if(!OK)++Failed;Report+=(OK?TEXT("PASS: "):TEXT("FAIL: "))+Message+TEXT("\n");
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("TF-Enhancements.txt")));
        };
        const FTransform Tower(FRotator(0,37,0),FVector(0,0,110000));
        auto* DriveClass=LoadClass<ATFDrive>(nullptr,TEXT("/TrapperFreightElevators/Buildables/Build_ItemDrive_Mk1.Build_ItemDrive_Mk1_C"));
        auto* BS=AFGBuildableSubsystem::Get(W.Get());
        auto* Drive=DriveClass?Cast<ATFDrive>(BS->BeginSpawnBuildable(DriveClass,Tower)):nullptr;
        Check(Drive!=nullptr,TEXT("Disposable rotated tower fixture"));
        if(Drive)
        {
            Drive->FinishSpawning(Tower);Drive->Paused=true;Drive->PreservedShaftHeight=4000;Drive->RefreshAutoShaft();
            auto ShaftClass=LoadClass<ATFShaft>(nullptr,TEXT("/TrapperFreightElevators/Buildables/Build_Shaft.Build_Shaft_C"));
            const FTransform ShaftTransform(Tower.GetRotation(),Tower.TransformPosition(FVector(0,0,2400)));
            auto* Shaft=ShaftClass?Cast<ATFShaft>(BS->BeginSpawnBuildable(ShaftClass,ShaftTransform)):nullptr;
            if(Shaft)Shaft->FinishSpawning(ShaftTransform);
            Check(Shaft&&Shaft->Drive()==Drive,TEXT("Manual shaft fixture links to rotated tower"));
            TArray<ATFPart*> Surfaces{Drive};if(Shaft)Surfaces.Add(Shaft);
            const TCHAR* Recipes[]={
                TEXT("/Game/FactoryGame/Recipes/Buildings/Foundations/Recipe_Foundation_8x1_01.Recipe_Foundation_8x1_01_C"),
                TEXT("/Game/FactoryGame/Recipes/Buildings/Foundations/Recipe_Foundation_8x2_01.Recipe_Foundation_8x2_01_C"),
                TEXT("/Game/FactoryGame/Recipes/Buildings/Foundations/Recipe_Foundation_8x4_01.Recipe_Foundation_8x4_01_C"),
                TEXT("/Game/FactoryGame/Recipes/Buildings/Walls/Recipe_Wall_8x4_01.Recipe_Wall_8x4_01_C"),
                TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_1m.Recipe_PoweredFoundation_1m_C"),
                TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_2m.Recipe_PoweredFoundation_2m_C"),
                TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_4m.Recipe_PoweredFoundation_4m_C")};
            for(const TCHAR* Path:Recipes)
            {
                if(FString(Path).StartsWith(TEXT("/TrapperPoweredFoundations/"))&&!FModuleManager::Get().IsModuleLoaded(TEXT("TrapperPoweredFoundations")))continue;
                auto Recipe=LoadClass<UFGRecipe>(nullptr,Path);Check(Recipe!=nullptr,FString(TEXT("Native recipe: "))+Path);if(!Recipe)continue;
                auto* H=Cast<AFGFactoryBuildingHologram>(AFGHologram::SpawnHologramFromRecipe(Recipe,Drive,Tower.GetLocation()));
                Check(H!=nullptr,TEXT("Native hologram spawned"));if(!H)continue;
                H->SetActorTickEnabled(false);
                const auto* Foundation=Cast<AFGBuildableFoundation>(H->GetBuildClass()->GetDefaultObject());
                for(auto* Surface:Surfaces)for(FVector Face:{FVector(1,0,0),FVector(-1,0,0),FVector(0,1,0),FVector(0,-1,0)})
                {
                    const FVector Point=Tower.TransformPosition(Face*395+FVector(0,0,2400)),Normal=Tower.TransformVectorNoScale(Face);
                    FHitResult Hit(Surface,Surface->FreightSelection,Point,Normal);Hit.bBlockingHit=true;
                    Check(H->IsValidHitResult(Hit),TEXT("Native hologram accepts shaft hit"));
                    Check(H->TrySnapToActor(Hit),TEXT("Native TrySnapToActor hook accepts shaft face"));
                    const FVector Expected=Face*(Foundation?800:400)+FVector(0,0,2400-(Foundation?Foundation->mHeight*.5f:0));
                    Check(Tower.InverseTransformPosition(H->GetActorLocation()).Equals(Expected,.1),TEXT("Runtime placement matches face and deck height"));
                    Check(H->GetSnappedBuilding()==Surface,TEXT("Native snapped-building state points to targeted shaft part"));
                    // Match the native validation pass's reset, then test placement only.
                    // Cost/initialization checks require a player and are outside this server fixture.
                    H->ResetConstructDisqualifiers();H->CheckValidPlacement();TArray<TSubclassOf<UFGConstructDisqualifier>> Reasons;H->GetConstructDisqualifiers(Reasons);
                    FString ReasonText;for(auto Reason:Reasons)ReasonText+=Reason->GetName()+TEXT(" ");
                    const bool Blocked=Reasons.ContainsByPredicate([](auto Reason){return !UFGConstructDisqualifier::GetIsSoftDisqualifier(Reason);});
                    Check(!Blocked&&H->CanConstruct(),TEXT("Native placement geometry permits construction (native warnings retained): ")+ReasonText);
                }
                FHitResult Miss;H->TrySnapToActor(Miss);
                Check(!Cast<ATFPart>(H->GetSnappedBuilding()),TEXT("Moving off shaft clears freight snap state"));
                H->Destroy();
            }
            const TSubclassOf<UFGItemDescriptor> Iron=LoadClass<UFGItemDescriptor>(nullptr,TEXT("/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate.Desc_IronPlate_C"));
            Check(Iron!=nullptr,TEXT("Real item descriptor loads"));
            if(Iron)
            {
                const int32 StackSize=TF::StackSize(Iron);Check(StackSize>1,TEXT("Runtime descriptor supplies actual stack size"));
                FTFStackReservation Rule;Rule.Resource=Iron;Rule.Stacks=4;
                TArray<FInventoryStack> Stacks;Stacks.SetNum(8);Stacks[6]=FInventoryStack(StackSize-1,Iron);
                auto Tiles=TFStorageLayout(Stacks,8,{Rule},{{Iron,1}});
                Check(Tiles.Num()==8&&Tiles[0].SourceIndex==6&&!Tiles[1].InTransit,TEXT("Real partial stack absorbs one in-flight item without another tile"));
                Tiles=TFStorageLayout(Stacks,8,{Rule},{{Iron,StackSize+2}});
                Check(Tiles[1].InTransit&&Tiles[2].InTransit&&!Tiles[3].InTransit,TEXT("Real stack size accounts for in-flight reservation tiles"));
            }
            if(Shaft)Shaft->Destroy();Drive->Destroy();
        }
        Report+=FString::Printf(TEXT("RESULT: %d checks, %d failures\n"),Checks,Failed);
        FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("TF-Enhancements.txt")));
    }),12.f,false);
}
