#include "Misc/AutomationTest.h"
#include "TFStorageLayout.h"
#include "TFBuildingSnap.h"
#include "TFPanel.h"
#include "Resources/FGBuildingDescriptor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Misc/ScopeExit.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStorageLayoutTest,"TrapperFreightElevators.StorageDisplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTFStorageLayoutTest::RunTest(const FString&)
{
    const TSubclassOf<UFGItemDescriptor> Iron=UFGItemDescriptor::StaticClass(),Copper=UFGBuildingDescriptor::StaticClass();
    const auto Size=[](TSubclassOf<UFGItemDescriptor>){return 100;};
    // Inventory constructors and SetItemClass are editor SDK stubs. Populate the
    // serialized fields directly; the packaged-runtime fixture uses real constructors.
    auto* ItemClass=FindFProperty<FClassProperty>(FInventoryItem::StaticStruct(),TEXT("ItemClass"));
    if(!TestNotNull(TEXT("Inventory item class field available"),ItemClass))return false;
    auto Stack=[ItemClass](int32 Count,TSubclassOf<UFGItemDescriptor> Resource)
    {
        FInventoryStack Result;Result.NumItems=Count;
        ItemClass->SetObjectPropertyValue_InContainer(&Result.Item,Resource.Get());
        return Result;
    };
    FTFStackReservation A;A.Resource=Iron;A.Stacks=4;
    FTFStackReservation B;B.Resource=Copper;B.Stacks=1;
    TArray<FInventoryStack> Stacks;for(int32 I=0;I<8;++I)Stacks.Add(Stack(0,nullptr));Stacks[6]=Stack(35,Iron);Stacks[2]=Stack(100,Copper);
    auto CheckPermutation=[&](const TArray<FTFStorageTile>& Tiles,int32 Count)
    {
        TSet<int32> Seen;for(const auto& Tile:Tiles){TestTrue(TEXT("Mapped source is in range"),Tile.SourceIndex>=0&&Tile.SourceIndex<Count);Seen.Add(Tile.SourceIndex);}
        TestEqual(TEXT("Exactly one tile per real slot"),Tiles.Num(),Count);TestEqual(TEXT("No source slot duplicated or hidden"),Seen.Num(),Count);
    };
    auto Tiles=TFStorageLayout(Stacks,8,{A,B},{},Size);CheckPermutation(Tiles,8);
    for(int32 I=0;I<4;++I)TestEqual(TEXT("Four reserved iron tiles first"),Tiles[I].ReservedFor,Iron);
    TestEqual(TEXT("Occupied iron tile maps to its actual slot"),Tiles[0].SourceIndex,6);
    TestEqual(TEXT("Second rule claims copper before empty assignments"),Tiles[4].SourceIndex,2);
    TestEqual(TEXT("Layout did not move inventory"),Stacks[6].NumItems,35);
    TestFalse(TEXT("Empty reservation is immediately available"),Tiles[1].WaitingForSpace);
    Tiles=TFStorageLayout(Stacks,8,{A,B},{{Iron,65}},Size);
    TestFalse(TEXT("Transit that fits a partial stack consumes no extra tile"),Tiles[1].InTransit);
    Tiles=TFStorageLayout(Stacks,8,{A,B},{{Iron,166}},Size);
    TestTrue(TEXT("Transit claims first empty reserved tile"),Tiles[1].InTransit);
    TestTrue(TEXT("Transit claims second empty reserved tile"),Tiles[2].InTransit);
    TestFalse(TEXT("Other reserved tile stays empty"),Tiles[3].InTransit);
    for(auto& Entry:Stacks)Entry=Stack(100,Copper);
    Tiles=TFStorageLayout(Stacks,8,{A,B},{},Size);CheckPermutation(Tiles,8);
    for(int32 I=0;I<4;++I)TestTrue(TEXT("Occupied conflict is shown as waiting"),Tiles[I].WaitingForSpace);
    Tiles=TFStorageLayout(Stacks,8,{},{});CheckPermutation(Tiles,8);
    for(int32 I=0;I<8;++I){TestEqual(TEXT("No rules retains physical order"),Tiles[I].SourceIndex,I);TestFalse(TEXT("Removed reservations leave shared tiles"),bool(Tiles[I].ReservedFor));}
    A.Stacks=50;Tiles=TFStorageLayout(Stacks,8,{A,B},{},Size);CheckPermutation(Tiles,8);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFBuildingSnapTest,"TrapperFreightElevators.BuildingSnap",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTFBuildingSnapTest::RunTest(const FString&)
{
    for(float Yaw:{0.f,37.f,90.f})
    {
        const FTransform Tower(FRotator(0,Yaw,0),FVector(1234,-5678,900));
        for(FVector Face:{FVector(1,0,0),FVector(-1,0,0),FVector(0,1,0),FVector(0,-1,0)})
        {
            for(float Height:{100.f,200.f,400.f})
            {
                FTransform Snap;
                TestTrue(TEXT("All foundation thicknesses snap on every rotated face"),FTFBuildingSnap::Solve(Tower,4000,{1600},Tower.TransformPosition(Face*395+FVector(0,0,1590)),Tower.TransformVectorNoScale(Face),false,FVector(800,800,Height),90,Snap));
                TestTrue(TEXT("Foundation edge is flush and top aligns to landing"),Tower.InverseTransformPosition(Snap.GetLocation()).Equals(Face*800+FVector(0,0,1600-Height*.5f),.01));
            }
            FTransform Snap;
            TestTrue(TEXT("Wall snaps between stops"),FTFBuildingSnap::Solve(Tower,4000,{1600},Tower.TransformPosition(Face*395+FVector(0,0,2401)),Tower.TransformVectorNoScale(Face),true,FVector(800,1,400),0,Snap));
            TestTrue(TEXT("Wall base is on shaft face and height grid"),Tower.InverseTransformPosition(Snap.GetLocation()).Equals(Face*400+FVector(0,0,2400),.01));
            TestTrue(TEXT("Wall width runs along face"),FMath::Abs(FVector::DotProduct(Snap.GetRotation().GetAxisX(),Tower.TransformVectorNoScale(Face)))<.001);
        }
    }
    FTransform Out;
    auto Solve=[&](float Z,bool Wall=true,float Top=4000){return FTFBuildingSnap::Solve(FTransform::Identity,Top,{1600},FVector(395,0,Z),FVector(1,0,0),Wall,FVector(800,800,400),0,Out);};
    TestFalse(TEXT("Wall cannot cover a landing opening"),Solve(1600));
    TestFalse(TEXT("Wall cannot overlap the bottom of an opening"),Solve(1400));
    TestFalse(TEXT("Foundation cannot cross a landing's ports"),Solve(1800,false));
    TestTrue(TEXT("Foundation at landing deck is allowed"),Solve(1600,false));
    TestFalse(TEXT("Wall leaves drive control panel clear"),Solve(400));
    TestFalse(TEXT("Wall cannot extend above the shaft"),Solve(3900));
    TestFalse(TEXT("No snapping beyond built shaft"),Solve(4100,false));
    TestTrue(TEXT("Extending the shaft adds snap levels"),Solve(4100,false,4400));
    TestTrue(TEXT("Unrotated rectangular foundation snaps"),FTFBuildingSnap::Solve(FTransform::Identity,4000,{},FVector(395,0,1200),FVector(1,0,0),false,FVector(800,400,100),0,Out));
    TestTrue(TEXT("Rectangular unrotated extent"),Out.GetLocation().Equals(FVector(800,0,1150)));
    TestTrue(TEXT("Rotated rectangular foundation snaps"),FTFBuildingSnap::Solve(FTransform::Identity,4000,{},FVector(395,0,1200),FVector(1,0,0),false,FVector(800,400,100),90,Out));
    TestTrue(TEXT("Rectangular rotated extent"),Out.GetLocation().Equals(FVector(600,0,1150),.01));
    TestTrue(TEXT("Inner frame surface can snap"),FTFBuildingSnap::Solve(FTransform::Identity,4000,{},FVector(380,0,1200),FVector(-1,0,0),false,FVector(800,800,100),0,Out));
    TestTrue(TEXT("Inner frame normal does not jump to opposite face"),Out.GetLocation().Equals(FVector(800,0,1150),.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFResourceSearchTest,"TrapperFreightElevators.ResourceSearch",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTFResourceSearchTest::RunTest(const FString&)
{
    auto* Panel=NewObject<UTFPanel>();Panel->WidgetTree=NewObject<UWidgetTree>(Panel);
    Panel->ResourceChoice=Panel->WidgetTree->ConstructWidget<UTFResourceCombo>();
    Panel->ResourceSummary=Panel->WidgetTree->ConstructWidget<UTextBlock>();
    const TSubclassOf<UFGItemDescriptor> Resource=UFGItemDescriptor::StaticClass();
    Panel->AllResources={Resource};Panel->SelectedResource=Resource;Panel->FilterResources();
    TestEqual(TEXT("Selected resource maps to first result"),Panel->ResourceChoice->GetSelectedIndex(),1);
    Panel->SearchQuery=TEXT("__no_such_item_384723__");Panel->FilterResources();
    TestEqual(TEXT("Empty results keep only the explicit clear option"),Panel->ResourceChoice->GetOptionCount(),1);
    TestEqual(TEXT("Filtering out selection preserves its identity"),Panel->ChosenResource(),Resource);
    TestEqual(TEXT("Hidden selection does not select Any item"),Panel->ResourceChoice->GetSelectedIndex(),INDEX_NONE);
    Panel->SearchQuery.Empty();Panel->FilterResources();
    TestEqual(TEXT("Clearing search restores selected option"),Panel->ResourceChoice->GetSelectedIndex(),1);
    Panel->ResourceChoice->SetSelectedIndex(0);Panel->ResourceChanged(TEXT("Any item"),ESelectInfo::Direct);
    TestFalse(TEXT("Explicit Any item clears selected resource"),bool(Panel->ChosenResource()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFResourceSearchTypingTest,"TrapperFreightElevators.ResourceSearchTyping",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
// Preserve Slate's tunnel/bubble routing without requiring a live game world.
class STFSearchTestHost : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STFSearchTestHost) {} SLATE_DEFAULT_SLOT(FArguments,Content) SLATE_END_ARGS()
    UTFPanel* Panel=nullptr;
    void Construct(const FArguments& Args,UTFPanel* InPanel){Panel=InPanel;ChildSlot[Args._Content.Widget];}
    virtual FReply OnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override{return Panel->NativeOnPreviewKeyDown(Geometry,Event);}
    virtual FReply OnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override{return Panel->NativeOnKeyDown(Geometry,Event);}
};
bool FTFResourceSearchTypingTest::RunTest(const FString&)
{
    auto* DisplayName=FindFProperty<FTextProperty>(UFGItemDescriptor::StaticClass(),TEXT("mDisplayName"));
    auto* UseDisplayName=FindFProperty<FBoolProperty>(UFGItemDescriptor::StaticClass(),TEXT("mUseDisplayNameAndDescription"));
    if(!TestNotNull(TEXT("Descriptor display name"),DisplayName)||!TestNotNull(TEXT("Descriptor naming flag"),UseDisplayName))return false;
    auto* WireDefault=GetMutableDefault<UFGItemDescriptor>();auto* CableDefault=GetMutableDefault<UFGBuildingDescriptor>();
    const FText OldWire=DisplayName->GetPropertyValue_InContainer(WireDefault),OldCable=DisplayName->GetPropertyValue_InContainer(CableDefault);
    const bool OldWireFlag=UseDisplayName->GetPropertyValue_InContainer(WireDefault),OldCableFlag=UseDisplayName->GetPropertyValue_InContainer(CableDefault);
    ON_SCOPE_EXIT{DisplayName->SetPropertyValue_InContainer(WireDefault,OldWire);DisplayName->SetPropertyValue_InContainer(CableDefault,OldCable);UseDisplayName->SetPropertyValue_InContainer(WireDefault,OldWireFlag);UseDisplayName->SetPropertyValue_InContainer(CableDefault,OldCableFlag);};
    DisplayName->SetPropertyValue_InContainer(WireDefault,FText::FromString(TEXT("Wire")));UseDisplayName->SetPropertyValue_InContainer(WireDefault,true);
    DisplayName->SetPropertyValue_InContainer(CableDefault,FText::FromString(TEXT("Cable")));UseDisplayName->SetPropertyValue_InContainer(CableDefault,true);
    // Route real character/key events through Slate. Calling the filter directly
    // cannot catch a popup interrupting the next character (issue #3).
    auto* Panel=NewObject<UTFPanel>();Panel->AddToRoot();Panel->WidgetTree=NewObject<UWidgetTree>(Panel);
    Panel->ResourceSearch=Panel->WidgetTree->ConstructWidget<UEditableTextBox>();
    Panel->ResourceSearch->OnTextChanged.AddDynamic(Panel,&UTFPanel::ResourceSearchChanged);
    Panel->ResourceChoice=Panel->WidgetTree->ConstructWidget<UTFResourceCombo>();
    Panel->ResourceChoice->OnSelectionChanged.AddDynamic(Panel,&UTFPanel::ResourceChanged);
    Panel->ResourceSummary=Panel->WidgetTree->ConstructWidget<UTextBlock>();
    const TSubclassOf<UFGItemDescriptor> Resource=UFGItemDescriptor::StaticClass(),Cable=UFGBuildingDescriptor::StaticClass();
    Panel->AllResources={Resource,Cable};Panel->SelectedResource=Resource;
    Panel->CreateResourceResults(Panel->WidgetTree->ConstructWidget<UVerticalBox>());
    auto Window=SNew(SWindow).ClientSize(FVector2D(500,400)).CreateTitleBar(false).SupportsMaximize(false).SupportsMinimize(false);
    Window->SetContent(SNew(STFSearchTestHost,Panel)[SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[Panel->ResourceSearch->TakeWidget()]
        +SVerticalBox::Slot().AutoHeight()[Panel->ResourceChoice->TakeWidget()]
        +SVerticalBox::Slot().AutoHeight()[Panel->ResourceResults->TakeWidget()]]);
    auto& Slate=FSlateApplication::Get();Slate.AddWindow(Window,false);Window->SlatePrepass();
    Panel->FilterResources();Panel->ResourceSearch->SetKeyboardFocus();
    TestTrue(TEXT("Search receives keyboard focus"),Panel->ResourceSearch->HasKeyboardFocus());
    FString Typed;
    for(TCHAR Character:FString(TEXT("WiRe")))
    {
        Slate.ProcessKeyCharEvent(FCharacterEvent(Character,FModifierKeysState(),0,false));Typed.AppendChar(Character);
        TestEqual(TEXT("Every character reaches the search field"),Panel->ResourceSearch->GetText().ToString(),Typed);
        TestEqual(TEXT("Every character updates the query"),Panel->SearchQuery,Typed);
        TestTrue(TEXT("Typing retains search focus"),Panel->ResourceSearch->HasKeyboardFocus());
        TestFalse(TEXT("Typing does not open a modal results popup"),Panel->ResourceChoice->IsOpen());
        TestEqual(TEXT("Matching list appears automatically"),Panel->ResourceResults->GetVisibility(),ESlateVisibility::Visible);
        TestEqual(TEXT("Each query shows its matching item"),Panel->ResourceResultActions.Num(),1);
        if(!Panel->ResourceResultActions.IsEmpty())TestEqual(TEXT("Mixed-case query shows Wire"),Panel->ResourceResultActions[0]->Resource,Resource);
        TestEqual(TEXT("Typing does not change the chosen resource"),Panel->ChosenResource(),Resource);
    }
    Slate.ProcessKeyDownEvent(FKeyEvent(EKeys::A,FModifierKeysState(false,false,true,false,false,false,false,false,false),0,false,0,0));
    for(TCHAR Character:FString(TEXT("cable")))Slate.ProcessKeyCharEvent(FCharacterEvent(Character,FModifierKeysState(),0,false));
    TestEqual(TEXT("Select all and replace accepts a complete new query"),Panel->SearchQuery,FString(TEXT("cable")));
    Slate.ProcessKeyCharEvent(FCharacterEvent(TCHAR('\b'),FModifierKeysState(),0,false));
    TestEqual(TEXT("Backspace edits the full query"),Panel->SearchQuery,FString(TEXT("cabl")));
    Panel->Handle(105,0,0);
    TestTrue(TEXT("Clear empties the field"),Panel->ResourceSearch->GetText().IsEmpty());
    TestTrue(TEXT("Clear empties the query"),Panel->SearchQuery.IsEmpty());
    TestTrue(TEXT("Clear returns focus to search"),Panel->ResourceSearch->HasKeyboardFocus());
    TestEqual(TEXT("Clear restores all resources"),Panel->ResourceChoice->GetOptionCount(),3);
    TestEqual(TEXT("Clear hides automatic results"),Panel->ResourceResults->GetVisibility(),ESlateVisibility::Collapsed);
    TestTrue(TEXT("Old result handlers are released"),Panel->Actions.IsEmpty());
    TestEqual(TEXT("Clear restores the selection"),Panel->ResourceChoice->GetSelectedIndex(),1);
    Slate.ProcessKeyCharEvent(FCharacterEvent(TCHAR('x'),FModifierKeysState(),0,false));
    TestTrue(TEXT("Unmatched query has no selectable results"),Panel->ResourceResultActions.IsEmpty());
    TestEqual(TEXT("Unmatched query displays a message"),Panel->ResourceResultRows->GetChildrenCount(),1);
    Slate.ProcessKeyDownEvent(FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0));
    TestTrue(TEXT("Escape clears the field and query"),Panel->ResourceSearch->GetText().IsEmpty()&&Panel->SearchQuery.IsEmpty());
    TestEqual(TEXT("Escape restores all results"),Panel->ResourceChoice->GetOptionCount(),3);
    Slate.ProcessKeyDownEvent(FKeyEvent(EKeys::Down,FModifierKeysState(),0,false,0,0));
    TestTrue(TEXT("Explicit activation still opens results"),Panel->ResourceChoice->IsOpen());
    Panel->ResourceChoice->OpenResults(false);
    Panel->ResourceSearch->SetKeyboardFocus();
    Slate.ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
    TestTrue(TEXT("Enter opens results before the text box consumes it"),Panel->ResourceChoice->IsOpen());
    Panel->ResourceChoice->OpenResults(false);
    Panel->ResourceSearch->SetKeyboardFocus();
    for(TCHAR Character:FString(TEXT("cab")))Slate.ProcessKeyCharEvent(FCharacterEvent(Character,FModifierKeysState(),0,false));
    Window->SlatePrepass();
    Slate.ProcessKeyDownEvent(FKeyEvent(EKeys::Down,FModifierKeysState(),0,false,0,0));
    if(TestEqual(TEXT("Cable result is selectable"),Panel->ResourceResultActions.Num(),1))
    {
        TestTrue(TEXT("Down focuses the inline result"),Panel->ResourceResultActions[0]->Button->HasKeyboardFocus());
        Slate.ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
        Slate.ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
        TestEqual(TEXT("Enter selects the matching resource"),Panel->ChosenResource(),Cable);
        TestTrue(TEXT("Selection finishes the search"),Panel->SearchQuery.IsEmpty());
        TestTrue(TEXT("Selection returns typing focus"),Panel->ResourceSearch->HasKeyboardFocus());
    }
    Slate.RequestDestroyWindow(Window);Panel->RemoveFromRoot();
    return true;
}
#endif
