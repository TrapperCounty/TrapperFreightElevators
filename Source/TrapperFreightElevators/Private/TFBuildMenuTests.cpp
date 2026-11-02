#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FGBuildCategory.h"
#include "FGBuildSubCategory.h"
#include "FGRecipe.h"
#include "Resources/FGBuildingDescriptor.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFBuildMenuTest,"TrapperFreightElevators.BuildMenu",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTFBuildMenuTest::RunTest(const FString&)
{
    // The SDK's static descriptor getters are editor stubs. Inspect serialized
    // defaults and their category types instead; verify the browser in-game.
    auto* Category=LoadClass<UFGBuildCategory>(nullptr,TEXT("/TrapperFreightElevators/Categories/Cat_Freight.Cat_Freight_C"));
    TestNotNull(TEXT("Freight tab is a native build-menu category"),Category);
    TArray<FString> Keys{TEXT("Shaft")};
    for(int32 Tier=1;Tier<=3;++Tier)
        for(const TCHAR* Type:{TEXT("Item"),TEXT("Fluid")})
            for(const TCHAR* Kind:{TEXT("Drive"),TEXT("Stop")})
                Keys.Add(FString::Printf(TEXT("%s%s_Mk%d"),Type,Kind,Tier));
    for(const FString& Key:Keys)
    {
        const FString DescriptorPath=TEXT("/TrapperFreightElevators/Descriptors/Desc_")+Key+TEXT(".Desc_")+Key+TEXT("_C");
        auto* Descriptor=LoadClass<UFGBuildingDescriptor>(nullptr,*DescriptorPath);
        if(!TestNotNull(Key+TEXT(" descriptor loads"),Descriptor)) continue;
        auto* Defaults=Descriptor->GetDefaultObject<UFGBuildingDescriptor>();
        const auto BuildCategory=Defaults->GetCategoryFromInstance();
        TestTrue(Key+TEXT(" appears in a typed build category"),BuildCategory&&BuildCategory->IsChildOf(UFGBuildCategory::StaticClass()));
        TestTrue(Key+TEXT(" belongs to the Freight Elevators tab"),Category&&BuildCategory.Get()==Category);
        TArray<TSubclassOf<UFGCategory>> SubCategories;
        Defaults->GetSubCategoriesFromInstance(SubCategories);
        TestEqual(Key+TEXT(" has one visible build subcategory"),SubCategories.Num(),1);
        const FString Group=Key==TEXT("Shaft")?TEXT("Shafts"):(Key.StartsWith(TEXT("Fluid"))?TEXT("Fluids"):TEXT("Items"));
        if(SubCategories.Num()==1&&TestNotNull(Key+TEXT(" subcategory exists"),SubCategories[0].Get()))
        {
            TestTrue(Key+TEXT(" subcategory has the native build-menu type"),SubCategories[0]->IsChildOf(UFGBuildSubCategory::StaticClass()));
            TestEqual(Key+TEXT(" uses the matching item/fluid/shaft group"),SubCategories[0]->GetName(),TEXT("SC_Freight")+Group+TEXT("_C"));
        }
        const FString RecipePath=TEXT("/TrapperFreightElevators/Recipes/Recipe_")+Key+TEXT(".Recipe_")+Key+TEXT("_C");
        auto* Recipe=LoadClass<UFGRecipe>(nullptr,*RecipePath);
        if(TestNotNull(Key+TEXT(" recipe loads"),Recipe))
        {
            const auto& Products=Recipe->GetDefaultObject<UFGRecipe>()->GetProducts();
            TestEqual(Key+TEXT(" recipe has one product"),Products.Num(),1);
            if(Products.Num()==1)TestEqual(Key+TEXT(" recipe still produces the same descriptor"),Products[0].ItemClass.Get(),Descriptor);
        }
        auto* BuildProperty=FindFProperty<FClassProperty>(Descriptor,TEXT("mBuildableClass"));
        if(TestNotNull(Key+TEXT(" buildable property exists"),BuildProperty))
            TestNotNull(Key+TEXT(" retains its buildable class"),BuildProperty->GetObjectPropertyValue_InContainer(Defaults));
    }
    TestEqual(TEXT("All drives, stops and the shared shaft are covered"),Keys.Num(),13);
    return true;
}
#endif
