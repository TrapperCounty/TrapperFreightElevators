#include "TFGenerateAssetsCommandlet.h"
#include "TFBuildables.h"
#include "TFPanel.h"
#include "TFWorldModule.h"
#include "FGRecipe.h"
#include "FGSchematic.h"
#include "FGCategory.h"
#include "FGBuildCategory.h"
#include "FGBuildSubCategory.h"
#include "Engine/Texture2D.h"
#include "Resources/FGBuildingDescriptor.h"
#include "Unlocks/FGUnlockRecipe.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
extern bool TFGenerateIronworksAssets();

namespace
{
    UClass* Class(const FString& Path)
    {
        auto* C=LoadObject<UClass>(nullptr,*(Path+TEXT(".")+FPackageName::GetShortName(Path)+TEXT("_C")));
        checkf(C,TEXT("Missing class: %s"),*Path);return C;
    }
    template<typename T> T& V(UObject* O,FName Name)
    {
        auto* P=O->GetClass()->FindPropertyByName(Name);checkf(P,TEXT("Missing %s"),*Name.ToString());check(P->GetElementSize()==sizeof(T));return *P->ContainerPtrToValuePtr<T>(O);
    }
    void Bool(UObject* O,FName Name,bool Value) {auto* P=FindFProperty<FBoolProperty>(O->GetClass(),Name);check(P);P->SetPropertyValue_InContainer(O,Value);}
    UBlueprint* BP(const FString& Path,UClass* Parent)
    {
        if(auto* Existing=LoadObject<UBlueprint>(nullptr,*(Path+TEXT(".")+FPackageName::GetShortName(Path)),nullptr,LOAD_NoWarn))
        {
            if(Existing->ParentClass!=Parent) {Existing->ParentClass=Parent;FBlueprintEditorUtils::RefreshAllNodes(Existing);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Existing);FKismetEditorUtilities::CompileBlueprint(Existing);}
            return Existing;
        }
        auto* Result=FKismetEditorUtilities::CreateBlueprint(Parent,CreatePackage(*Path),*FPackageName::GetShortName(Path),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass(),TEXT("TrapperFreightElevators"));check(Result);FAssetRegistryModule::AssetCreated(Result);return Result;
    }
    bool Save(UObject* Asset)
    {
        if(auto* B=Cast<UBlueprint>(Asset)) {FKismetEditorUtilities::CompileBlueprint(B);if(B->Status==BS_Error) return false;}
        Asset->GetOutermost()->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
        const FString File=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        const bool OK=UPackage::SavePackage(Asset->GetOutermost(),Asset,*File,Args);UE_LOG(LogTemp,Display,TEXT("Freight asset %s: %s"),OK?TEXT("OK"):TEXT("FAIL"),*File);return OK;
    }
    UMaterial* Material(const TCHAR* Name,FLinearColor Color,float Metal,float Rough,bool Emissive=false)
    {
        const FString Path=FString(TEXT("/TrapperFreightElevators/Art/"))+Name;
        if(auto* Existing=LoadObject<UMaterial>(nullptr,*(Path+TEXT(".")+Name),nullptr,LOAD_NoWarn)) return Existing;
        auto* M=NewObject<UMaterial>(CreatePackage(*Path),Name,RF_Public|RF_Standalone);
        auto* C=NewObject<UMaterialExpressionConstant3Vector>(M);C->Constant=Color;M->GetExpressionCollection().AddExpression(C);
        auto* Metallic=NewObject<UMaterialExpressionConstant>(M);Metallic->R=Metal;M->GetExpressionCollection().AddExpression(Metallic);
        auto* R=NewObject<UMaterialExpressionConstant>(M);R->R=Rough;M->GetExpressionCollection().AddExpression(R);
        M->GetEditorOnlyData()->BaseColor.Expression=C;M->GetEditorOnlyData()->Metallic.Expression=Metallic;M->GetEditorOnlyData()->Roughness.Expression=R;
        if(Emissive) M->GetEditorOnlyData()->EmissiveColor.Expression=C;
        M->PostEditChange();FAssetRegistryModule::AssetCreated(M);return M;
    }
    FItemAmount Item(const TCHAR* Folder,int32 Amount)
    {
        FItemAmount I;I.ItemClass=Class(FString(TEXT("/Game/FactoryGame/Resource/Parts/"))+Folder+TEXT("/Desc_")+Folder);I.Amount=Amount;return I;
    }
    UTexture2D* Icon(const FString& Key,int32 Kind,int32 Tier,bool Fluid)
    {
        const FString Path=TEXT("/TrapperFreightElevators/Art/Icon_")+Key;
        auto* T=LoadObject<UTexture2D>(nullptr,*(Path+TEXT(".Icon_")+Key),nullptr,LOAD_NoWarn);
        if(!T){T=NewObject<UTexture2D>(CreatePackage(*Path),*FString(TEXT("Icon_")+Key),RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(T);}
        constexpr int32 Size=128;TArray<FColor> Pixels;Pixels.Init(FColor(12,23,31,255),Size*Size);
        auto Rect=[&](int32 X,int32 Y,int32 W,int32 H,FColor C){for(int32 J=FMath::Max(0,Y);J<FMath::Min(Size,Y+H);++J)for(int32 I=FMath::Max(0,X);I<FMath::Min(Size,X+W);++I)Pixels[J*Size+I]=C;};
        const FColor Orange(244,128,45),Steel(140,169,180),Dark(34,53,63),Glow(61,230,210),Cargo(190,207,210);
        Rect(6,6,116,116,Dark);Rect(8,8,112,112,FColor(18,32,42));
        Rect(27,22,9,79,Steel);Rect(92,22,9,79,Steel);Rect(21,16,86,11,Orange);Rect(21,98,86,10,Orange);
        if(Kind==2){Rect(36,52,56,6,Orange);for(int32 I=0;I<47;++I)Rect(39+I,29+I,4,4,Steel);}
        else if(Kind==1){Rect(15,73,98,13,Orange);Rect(12,57,19,18,Cargo);Rect(97,57,19,18,Cargo);Rect(38,59,52,17,Dark);Rect(40,60,48,4,Glow);}
        else
        {
            Rect(41,30,46,7,Orange);Rect(43,37,4,46,Steel);Rect(81,37,4,46,Steel);Rect(39,82,50,7,Orange);
            Rect(49,48,30,31,Fluid?Glow:Cargo);Rect(53,52,22,23,Fluid?FColor(28,121,148):Dark);
            if(Fluid){Rect(53,50,22,5,Steel);Rect(53,73,22,5,Steel);}else{Rect(62,52,4,23,Cargo);Rect(53,62,22,4,Cargo);}
            Rect(53,10,22,6,Dark);
        }
        for(int32 I=0;I<Tier;++I)Rect(47+I*13,111,9,5,Fluid?Glow:Orange);
        T->Source.Init(Size,Size,1,1,TSF_BGRA8,reinterpret_cast<const uint8*>(Pixels.GetData()));T->CompressionSettings=TC_EditorIcon;T->MipGenSettings=TMGS_NoMipmaps;T->LODGroup=TEXTUREGROUP_UI;T->SRGB=true;T->PostEditChange();Save(T);return T;
    }
}
UTFGenerateAssetsCommandlet::UTFGenerateAssetsCommandlet() {IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UTFGenerateAssetsCommandlet::Main(const FString& Params)
{
    bool OK=TFGenerateIronworksAssets();
    auto* Steel=Material(TEXT("M_Steel"),FLinearColor(.21,.26,.29),.8,.35);
    auto* Orange=Material(TEXT("M_Orange"),FLinearColor(.95,.31,.035),.35,.38);
    auto* Dark=Material(TEXT("M_Dark"),FLinearColor(.025,.045,.055),.65,.45);
    auto* Light=Material(TEXT("M_Light"),FLinearColor(.04,.85,.75),.1,.3,true);
    OK&=Save(Steel)&&Save(Orange)&&Save(Dark)&&Save(Light);
    auto* Category=BP(TEXT("/TrapperFreightElevators/Categories/Cat_Freight"),UFGBuildCategory::StaticClass());
    auto* CategoryCDO=Category->GeneratedClass->GetDefaultObject();V<FText>(CategoryCDO,TEXT("mDisplayName"))=FText::FromString(TEXT("Freight Elevators"));V<float>(CategoryCDO,TEXT("mMenuPriority"))=45;
    FSlateBrush CategoryIcon;CategoryIcon.SetResourceObject(Icon(TEXT("Category"),0,3,false));CategoryIcon.ImageSize=FVector2D(128);V<FSlateBrush>(CategoryCDO,TEXT("mCategoryIcon"))=CategoryIcon;OK&=Save(Category);
    // The native browser filters category classes by UFGBuildCategory and
    // UFGBuildSubCategory. Generic categories remain searchable but have no tab.
    TArray<UClass*> SubCategories;
    const TCHAR* GroupKeys[]={TEXT("Items"),TEXT("Fluids"),TEXT("Shafts")};
    const TCHAR* GroupNames[]={TEXT("Item Freight"),TEXT("Fluid Freight"),TEXT("Shafts")};
    for(int32 Group=0;Group<3;++Group)
    {
        auto* Sub=BP(FString(TEXT("/TrapperFreightElevators/Categories/SC_Freight"))+GroupKeys[Group],UFGBuildSubCategory::StaticClass());
        auto* SubCDO=Sub->GeneratedClass->GetDefaultObject();
        V<FText>(SubCDO,TEXT("mDisplayName"))=FText::FromString(GroupNames[Group]);
        V<float>(SubCDO,TEXT("mMenuPriority"))=Group;
        V<FSlateBrush>(SubCDO,TEXT("mCategoryIcon"))=CategoryIcon;
        OK&=Save(Sub);SubCategories.Add(Sub->GeneratedClass.Get());
    }
    TArray<TSubclassOf<UFGSchematic>> Schematics;
    for(int32 Tier=1;Tier<=3;++Tier)
    {
        TArray<TSubclassOf<UFGRecipe>> Recipes;
        for(int32 Type=0;Type<2;++Type)
        for(int32 Kind=0;Kind<2;++Kind)
        {
            const FString Key=FString::Printf(TEXT("%s%s_Mk%d"),Type?TEXT("Fluid"):TEXT("Item"),Kind?TEXT("Stop"):TEXT("Drive"),Tier);
            const FString Name=FString::Printf(TEXT("%s Freight %s Mk.%d"),Type?TEXT("Fluid"):TEXT("Item"),Kind?TEXT("Stop"):TEXT("Elevator"),Tier);
            UClass* Native=Kind?(Type?ATFFluidStop::StaticClass():ATFItemStop::StaticClass()):ATFDrive::StaticClass();
            auto* Build=BP(TEXT("/TrapperFreightElevators/Buildables/Build_")+Key,Native);
            auto* CDO=Build->GeneratedClass->GetDefaultObject<ATFPart>();CDO->Tier=Tier;CDO->IsFluid=Type!=0;
            V<FText>(CDO,TEXT("mDisplayName"))=FText::FromString(Name);
            V<FText>(CDO,TEXT("mDescription"))=FText::FromString(Kind?TEXT("Four configurable ports, separate incoming and outgoing holding buffers. Place on a matching freight shaft, at the floor height. Use Build Mode to choose 1m, 2m or 4m snapping. Leave 8m between stops and 8m of headroom. Press E to configure."):TEXT("Programmable freight transport with reserved destination storage, per-floor pickup settings and power-aware cabin movement. Click to anchor the drive, aim up or scroll to add up to ten shaft sections, then click to build. Aim this tool at a matching tower to extend it. Add stops where needed."));
            for(auto C:CDO->Art)
            {
                if(C->GetName().StartsWith(TEXT("IW_")))continue;
                const FString N=C->GetName();UMaterialInterface* M=Steel;
                if(N.Contains(TEXT("Frame"))||N.Contains(TEXT("Roof"))||N.Contains(TEXT("Motor"))) M=Orange;
                if(N.Contains(TEXT("Buffer"))||N.Contains(TEXT("Console"))||N.Contains(TEXT("CargoBox"))||N.Contains(TEXT("Control"))) M=Dark;
                if(N.Contains(TEXT("Light"))) M=Light;C->SetMaterial(0,M);
            }
            CDO->HideLegacyArt();
            auto* Desc=BP(TEXT("/TrapperFreightElevators/Descriptors/Desc_")+Key,Class(TEXT("/Game/FactoryGame/Buildable/Factory/StorageContainerMk1/Desc_StorageContainerMk1")));
            auto* DescCDO=Desc->GeneratedClass->GetDefaultObject();V<TSubclassOf<AFGBuildable>>(DescCDO,TEXT("mBuildableClass"))=Build->GeneratedClass;
            V<FText>(DescCDO,TEXT("mDisplayName"))=FText::FromString(Name);
            V<TSubclassOf<UFGCategory>>(DescCDO,TEXT("mCategory"))=Category->GeneratedClass;V<TArray<TSubclassOf<UFGCategory>>>(DescCDO,TEXT("mSubCategories"))={SubCategories[Type]};
            auto* Texture=Icon(Key,Kind,Tier,Type!=0);V<TObjectPtr<UTexture2D>>(DescCDO,TEXT("mSmallIcon"))=Texture;V<TObjectPtr<UTexture2D>>(DescCDO,TEXT("mPersistentBigIcon"))=Texture;
            auto* Recipe=BP(TEXT("/TrapperFreightElevators/Recipes/Recipe_")+Key,Class(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_StorageContainerMk1")));
            auto* R=Recipe->GeneratedClass->GetDefaultObject();TArray<FItemAmount> Cost;
            if(Kind) Cost={Item(TEXT("SteelPlate"),10),Item(TEXT("SteelPipe"),10),Item(TEXT("IronPlateReinforced"),4),Item(TEXT("Rotor"),4)};
            else if(Tier==1) Cost={Item(TEXT("ModularFrame"),10),Item(TEXT("Motor"),10),Item(TEXT("SteelPlate"),40),Item(TEXT("SteelPipe"),40)};
            else if(Tier==2) Cost={Item(TEXT("ModularFrameHeavy"),10),Item(TEXT("Computer"),10),Item(TEXT("Motor"),20),Item(TEXT("SteelPlateReinforced"),40)};
            else Cost={Item(TEXT("ModularFrameFused"),10),Item(TEXT("MotorLightweight"),10),Item(TEXT("ModularFrameLightweight"),20),Item(TEXT("AluminumPlate"),80)};
            if(Type) Cost.Add(Item(TEXT("CopperSheet"),Kind?8:20*(1<<(Tier-1))));
            V<TArray<FItemAmount>>(R,TEXT("mIngredients"))=Cost;
            FItemAmount Product;Product.ItemClass=Desc->GeneratedClass;Product.Amount=1;V<TArray<FItemAmount>>(R,TEXT("mProduct"))={Product};
            Bool(R,TEXT("mDisplayNameOverride"),true);V<FText>(R,TEXT("mDisplayName"))=FText::FromString(Name);
            Recipes.Add(Recipe->GeneratedClass.Get());OK&=Save(Build)&&Save(Desc)&&Save(Recipe);
        }
        if(Tier==1)
        {
            auto* Build=BP(TEXT("/TrapperFreightElevators/Buildables/Build_Shaft"),ATFShaft::StaticClass());auto* CDO=Build->GeneratedClass->GetDefaultObject<ATFPart>();
            V<FText>(CDO,TEXT("mDisplayName"))=FText::FromString(TEXT("Freight Shaft / Vertical Zoop"));V<FText>(CDO,TEXT("mDescription"))=FText::FromString(TEXT("Snap to a freight drive or the top of an existing shaft. Click to anchor, aim up to Zoop 1 to 10 sections, then click to build. Scroll to fine-tune the count. Add stops afterward with 8m headroom. Extend the top later. Maximum tower height: 256m."));
            for(auto C:CDO->Art)if(!C->GetName().StartsWith(TEXT("IW_")))C->SetMaterial(0,C->GetName().Contains(TEXT("Brace"))?Orange:Steel);CDO->HideLegacyArt();
            auto* Desc=BP(TEXT("/TrapperFreightElevators/Descriptors/Desc_Shaft"),Class(TEXT("/Game/FactoryGame/Buildable/Factory/StorageContainerMk1/Desc_StorageContainerMk1")));auto* ShaftDesc=Desc->GeneratedClass->GetDefaultObject();V<TSubclassOf<AFGBuildable>>(ShaftDesc,TEXT("mBuildableClass"))=Build->GeneratedClass;
            V<TSubclassOf<UFGCategory>>(ShaftDesc,TEXT("mCategory"))=Category->GeneratedClass;V<TArray<TSubclassOf<UFGCategory>>>(ShaftDesc,TEXT("mSubCategories"))={SubCategories[2]};auto* ShaftIcon=Icon(TEXT("Shaft"),2,1,false);V<TObjectPtr<UTexture2D>>(ShaftDesc,TEXT("mSmallIcon"))=ShaftIcon;V<TObjectPtr<UTexture2D>>(ShaftDesc,TEXT("mPersistentBigIcon"))=ShaftIcon;
            auto* Recipe=BP(TEXT("/TrapperFreightElevators/Recipes/Recipe_Shaft"),Class(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_StorageContainerMk1")));auto* R=Recipe->GeneratedClass->GetDefaultObject();
            V<TArray<FItemAmount>>(R,TEXT("mIngredients"))={Item(TEXT("SteelPlate"),4),Item(TEXT("Cement"),4)};FItemAmount P;P.ItemClass=Desc->GeneratedClass;P.Amount=1;V<TArray<FItemAmount>>(R,TEXT("mProduct"))={P};
            Bool(R,TEXT("mDisplayNameOverride"),true);V<FText>(R,TEXT("mDisplayName"))=FText::FromString(TEXT("Freight Shaft / Vertical Zoop"));Recipes.Add(Recipe->GeneratedClass.Get());OK&=Save(Build)&&Save(Desc)&&Save(Recipe);
        }
        const FString Key=FString::Printf(TEXT("Schematic_FreightMk%d"),Tier);
        auto* Schematic=BP(TEXT("/TrapperFreightElevators/Schematics/")+Key,UFGSchematic::StaticClass());
        auto* S=Schematic->GeneratedClass->GetDefaultObject();V<FText>(S,TEXT("mDisplayName"))=FText::FromString(FString::Printf(TEXT("Freight Elevators Mk.%d"),Tier));
        V<FText>(S,TEXT("mDescription"))=FText::FromString(TEXT("Programmable item and fluid freight elevators with configurable floor ports and holding storage."));
        V<ESchematicType>(S,TEXT("mType"))=ESchematicType::EST_Milestone;
        V<int32>(S,TEXT("mTechTier"))=Tier*2+2;
        V<TArray<TObjectPtr<UFGAvailabilityDependency>>>(S,TEXT("mSchematicDependencies")).Reset();
        V<TArray<FItemAmount>>(S,TEXT("mCost"))={Item(TEXT("SteelPlate"),Tier*20),Item(TEXT("Motor"),Tier*5)};
        V<float>(S,TEXT("mTimeToComplete"))=0;
        auto* Unlock=NewObject<UFGUnlockRecipe>(S,Class(TEXT("/Game/FactoryGame/Unlocks/BP_UnlockRecipe")),NAME_None,RF_Transactional);Unlock->Init(Recipes);V<TArray<TObjectPtr<UFGUnlock>>>(S,TEXT("mUnlocks"))={Unlock};
        Schematics.Add(Schematic->GeneratedClass.Get());OK&=Save(Schematic);
    }
    auto* World=BP(TEXT("/TrapperFreightElevators/RootGameWorld_TrapperFreightElevators"),UTFWorldModule::StaticClass());auto* W=World->GeneratedClass->GetDefaultObject<UTFWorldModule>();W->bRootModule=true;W->mSchematics=Schematics;OK&=Save(World);
    auto* Instance=BP(TEXT("/TrapperFreightElevators/RootInstance_TrapperFreightElevators"),UTFInstanceModule::StaticClass());Instance->GeneratedClass->GetDefaultObject<UTFInstanceModule>()->bRootModule=true;OK&=Save(Instance);
    return OK?0:1;
}
