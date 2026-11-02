#include "TFPanel.h"
#include "TFBuildables.h"
#include "TFStorageLayout.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SComboBox.h"
#include "FGPlayerController.h"
#include "FGCharacterPlayer.h"
#include "FGRecipeManager.h"
#include "FGPowerConnectionComponent.h"
#include "FGPowerCircuit.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/ComboBoxString.h"
#include "Components/SpinBox.h"
#include "Components/EditableTextBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Net/UnrealNetwork.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

namespace
{
    void ReadableChoices(UComboBoxString* Choice)
    {
        // Popup rows use a dark background; the closed button keeps its native foreground.
        auto Style=Choice->GetItemStyle();Style.SetTextColor(FSlateColor(FLinearColor::White));Style.SetSelectedTextColor(FSlateColor(FLinearColor::White));Choice->SetItemStyle(Style);
    }
}

void UTFResourceCombo::OpenResults(bool Open,bool Focus)
{
    // UComboBoxString::SetIsOpen is not exported by the game. Use the supported Slate control.
    if(MyComboBox.IsValid())MyComboBox->SetIsOpen(Open,Focus);
}

UTFInstanceModule::UTFInstanceModule() { RemoteCallObjects.Add(UTFRemoteCall::StaticClass()); }
void UTFRemoteCall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UTFRemoteCall,Ready); }
void UTFRemoteCall::Feedback_Implementation(const FString& Value) { if(UTFPanel::Active.IsValid()){auto* P=UTFPanel::Active.Get();P->PendingFeedback=Value;P->RebuildDelay=P->Page==0?-1.f:.25f;if(P->Message)P->Message->SetText(FText::FromString(Value));} }
void UTFRemoteCall::MoveStack_Implementation(ATFPart* Anchor,ATFStop* Stop,bool Deposit,int32 Buffer,int32 Slot,TSubclassOf<UFGItemDescriptor> Expected)
{
    auto* Player=GetOwnerPlayerCharacter();
    if(!Player||!IsValid(Anchor)||!IsValid(Stop)||FVector::DistSquared(Player->GetActorLocation(),Anchor->GetActorLocation())>FMath::Square(3000.f))return;
    auto* D=Anchor->Drive();if(!IsValid(D)||Stop->Elevator!=D||!D->Stops.Contains(Stop)||D->IsFluid)return;
    const int32 N=Stop->MoveStorageStack(Player->GetInventory(),Deposit,Buffer,Slot,Expected);
    Feedback(N>0?FString::Printf(TEXT("Moved %d items."),N):TEXT("Nothing moved. Space may be full or reserved, the slot changed, or this item cannot be shipped."));
}
void UTFRemoteCall::Command_Implementation(ATFPart* Anchor,int32 Action,int32 A,int32 B,float Number,TSubclassOf<UFGItemDescriptor> Resource,const FString& Value)
{
    auto* Player=GetOwnerPlayerCharacter();
    if(!IsValid(Anchor)||!Player||FVector::DistSquared(Player->GetActorLocation(),Anchor->GetActorLocation())>FMath::Square(3000.f)) {Feedback(TEXT("Move closer to the control panel."));return;}
    auto* D=Anchor->Drive();if(!IsValid(D)) {Feedback(TEXT("This stop is not connected to a drive."));return;}
    if(Resource)
    {
        const auto Form=UFGItemDescriptor::GetForm(Resource);
        if((D->IsFluid && Form!=EResourceForm::RF_LIQUID)||(!D->IsFluid && Form!=EResourceForm::RF_SOLID)) {Feedback(TEXT("Select a compatible resource."));return;}
    }
    ATFStop* S=D->Stops.IsValidIndex(A)?D->Stops[A].Get():nullptr;
    bool OK=true;FString Reply=TEXT("Settings saved.");
    switch(Action)
    {
    case 1:D->Paused=!D->Paused;break;
    case 2:D->DepartRequested=true;break;
    case 4:if(S) S->FloorName=Value.Left(32);else OK=false;break;
    case 5:if(S) S->Enabled=!S->Enabled;else OK=false;break;
    case 6:if(S) S->WaitFull=!S->WaitFull;else OK=false;break;
    case 7:if(S && FMath::IsFinite(Number)) S->MaxWait=FMath::Clamp(Number,1.f,600.f);else OK=false;break;
    case 9:
        if(S && S->Ports.IsValidIndex(B))
        {
            const bool Connected=D->IsFluid?(S->Pipes.IsValidIndex(B)&&S->Pipes[B]->IsConnected()):(S->Belts.IsValidIndex(B)&&S->Belts[B]->IsConnected());
            if(Connected) {OK=false;Reply=TEXT("Disconnect the belt or pipe before changing its direction.");break;}
            if(D->IsFluid && S->Pipes[B]->Content()>TF::Epsilon) {OK=false;Reply=TEXT("Drain this pipe port before changing direction.");break;}
            S->Ports[B].Input=!S->Ports[B].Input;S->ApplyConfig();
        } else OK=false;
        break;
    case 10:if(S && S->Ports.IsValidIndex(B)&&!D->IsFluid) S->Ports[B].Filter=Resource;else OK=false;break;
    case 11:
        if(S && S->Ports.IsValidIndex(B)&&D->IsFluid)
        {
            if(S->Pipes[B]->IsConnected() || S->Pipes[B]->Content()>TF::Epsilon) {OK=false;Reply=TEXT("Disconnect and drain the pipe before reassigning its compartment.");break;}
            if(FMath::IsFinite(Number)&&Number>=0&&Number<D->Compartments&&Number==FMath::FloorToFloat(Number))S->Ports[B].Compartment=static_cast<int32>(Number);else OK=false;
        } else OK=false;
        break;
    case 12:
        if(!S||!Resource||!FMath::IsFinite(Number)) {OK=false;break;}
        {
            auto* Existing=D->Routes.FindByPredicate([&](const FTFRoute& R){return R.Resource==Resource && R.Destination==S;});
            const int32 W=FMath::Clamp(FMath::RoundToInt(Number),1,100);
            if(Existing) Existing->Weight=W;
            else if(D->Routes.Num()<256) {FTFRoute R;R.Resource=Resource;R.Destination=S;R.Weight=W;D->Routes.Add(R);}
            else {OK=false;Reply=TEXT("Route limit reached (256).");}
        }
        break;
    case 13:if(D->Routes.IsValidIndex(A)) D->Routes.RemoveAt(A);else OK=false;break;
    case 14:OK=D->SafeCompartments(B);if(!OK) Reply=TEXT("Empty the cabin, all floor tanks and pipe ports before changing the layout.");break;
    case 15:OK=D->SetTankResource(B,Resource);if(!OK) Reply=TEXT("Empty this compartment across the elevator, and disconnect its pipes before assigning a new fluid.");break;
    case 16:
        if(S)
        {
            auto* Inventory=B==0?S->Incoming.Get():S->Outgoing.Get();
            int32 Count=0;for(int32 Slot=0;Slot<Inventory->GetSizeLinear();++Slot){FInventoryStack Stack;if(Inventory->GetStackFromIndex(Slot,Stack)&&Stack.HasItems())Count+=S->MoveStorageStack(Player->GetInventory(),false,B,Slot,Stack.Item.GetItemClass());}
            Reply=FString::Printf(TEXT("Collected %d items. Anything that did not fit remains in the buffer."),Count);
        } else OK=false;
        break;
    case 17:
        if(S && D->IsFluid && B==1)
        {
            // Explicit per-floor flush never erases the cabin manifest or its reservations.
            S->PendingFlush=true;Reply=TEXT("Selected floor tanks and port contents will flush on the next factory update.");
        }else OK=false;
        break;
    case 19:if(S && !D->Target && !D->Docked) D->Target=S;else {OK=false;Reply=TEXT("Let the current trip finish or use Depart now first.");}break;
    case 21:if(auto* Circuit=D->Power->GetPowerCircuit()) Circuit->ResetFuse(GetOwnerPlayerController());else {OK=false;Reply=TEXT("Connect this elevator to a power circuit first.");}break;
    case 22:
        OK=S&&!D->IsFluid&&(B==0||B==1)&&FMath::IsFinite(Number)&&Number>=0&&Number<=FTFStats::ForTier(D->Tier).Stacks&&Number==FMath::FloorToFloat(Number)&&S->SetStorageReservation(B,Resource,static_cast<int32>(Number));
        Reply=OK?TEXT("Reservation saved. Existing items stay; protected space becomes available as they leave."):TEXT("Choose an item and whole stack count. Total reservations cannot exceed this floor's storage capacity.");break;
    case 23:
        OK=S&&!D->IsFluid&&S->SetStorageReservation(B,Resource,0);Reply=OK?TEXT("Reservation removed. Stored items are unchanged."):TEXT("That reservation is unavailable.");break;
    default:OK=false;break;
    }
    if(S) S->ForceNetUpdate();D->ForceNetUpdate();
    Feedback(OK?Reply:(Reply==TEXT("Settings saved.")?TEXT("That change is unavailable."):Reply));
}

TWeakObjectPtr<UTFPanel> UTFPanel::Active;
void UTFPanel::Open(AFGPlayerController* Player,ATFPart* Part)
{
    if(Active.IsValid())Active->Close();
    auto* W=CreateWidget<UTFPanel>(Player,StaticClass());if(!W)return;
    W->Anchor=Part;W->SetIsFocusable(true);W->AddToViewport(100);Active=W;
    if(auto* S=Cast<ATFStop>(Part))if(S->Elevator)W->SelectedFloor=FMath::Max(0,S->Elevator->Stops.IndexOfByKey(S));
    W->Build();Player->bShowMouseCursor=true;Player->SetIgnoreLookInput(true);Player->SetIgnoreMoveInput(true);W->CapturedInput=true;
    FInputModeUIOnly Mode;Mode.SetWidgetToFocus(W->TakeWidget());Player->SetInputMode(Mode);W->SetKeyboardFocus();
}
UTextBlock* UTFPanel::Text(UVerticalBox* Parent,const FString& Value,int32 Size,FLinearColor Color)
{
    auto* T=WidgetTree->ConstructWidget<UTextBlock>();T->SetText(FText::FromString(Value));auto Font=T->GetFont();Font.Size=Size;T->SetFont(Font);T->SetColorAndOpacity(FSlateColor(Color));T->SetAutoWrapText(true);
    Parent->AddChildToVerticalBox(T)->SetPadding(FMargin(0,5));return T;
}
UVerticalBox* UTFPanel::Column(UHorizontalBox* Row,float WeightValue)
{
    auto* C=WidgetTree->ConstructWidget<UVerticalBox>();auto* S=Row->AddChildToHorizontalBox(C);FSlateChildSize Size(ESlateSizeRule::Fill);Size.Value=WeightValue;S->SetSize(Size);S->SetPadding(FMargin(4));return C;
}
UButton* UTFPanel::Button(UVerticalBox* Parent,const FString& Label,int32 Action,int32 A,int32 B)
{
    auto* Button=WidgetTree->ConstructWidget<UButton>();Button->SetBackgroundColor(FLinearColor::White);
    auto* T=WidgetTree->ConstructWidget<UTextBlock>();T->SetText(FText::FromString(Label));auto Font=T->GetFont();Font.Size=17;T->SetFont(Font);T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    auto* ButtonPadding=WidgetTree->ConstructWidget<UBorder>();ButtonPadding->SetBrushColor(FLinearColor::Transparent);ButtonPadding->SetPadding(FMargin(12,7));ButtonPadding->AddChild(T);Button->AddChild(ButtonPadding);
    Parent->AddChildToVerticalBox(Button)->SetPadding(FMargin(0,3));
    auto* Handler=NewObject<UTFButtonAction>(this);Handler->Panel=this;Handler->Button=Button;Handler->Action=Action;Handler->A=A;Handler->B=B;
    Handler->Selected=(Action==100&&Page==A)||(Action==102&&StorageView==A)||(Action==103&&SelectedPort==A)||(Action==104&&ReservationBuffer==A);
    if(Action==14)if(auto* D=IsValid(Anchor)?Anchor->Drive():nullptr)Handler->Selected=D->Compartments==B;
    Actions.Add(Handler);Handler->RefreshStyle();Button->OnClicked.AddDynamic(Handler,&UTFButtonAction::Click);return Button;
}
void UTFButtonAction::RefreshStyle()
{
    if(!Button)return;
    const bool Focused=Button->HasKeyboardFocus()||(Panel->GetOwningPlayer()&&Button->HasUserFocus(Panel->GetOwningPlayer()));
    const int32 State=(Selected?1:0)|(Focused?2:0);
    if(StyleState==State)return;StyleState=State;
    const FLinearColor Edge=Focused?FLinearColor(.95,.9,.72):Selected?FLinearColor(.8,.34,.06):FLinearColor(.24,.27,.3);
    FButtonStyle Style=Button->GetStyle();
    Style.SetNormal(FSlateRoundedBoxBrush(Selected?FLinearColor(.24,.075,.018):FLinearColor(.12,.14,.16),4.f,Edge,Focused?3.f:2.f));
    Style.SetHovered(FSlateRoundedBoxBrush(FLinearColor(.035,.045,.055),4.f,FLinearColor(.95,.73,.35),3.f));
    Style.SetPressed(FSlateRoundedBoxBrush(FLinearColor(.018,.025,.03),4.f,FLinearColor(1,.8,.42),3.f));
    Button->SetStyle(Style);
}
void UTFButtonAction::Click(){if(!Panel)return;if(Action==200)Panel->MoveInventory(A,B,Resource);else Panel->Handle(Action,A,B,Resource);}
TSharedRef<SWidget> UTFPanel::RebuildWidget()
{
    if(!WidgetTree)WidgetTree=NewObject<UWidgetTree>(this);
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Canvas;
    auto* Border=WidgetTree->ConstructWidget<UBorder>();Border->SetBrushColor(FLinearColor(.035,.04,.045,.99));Border->SetPadding(FMargin(24,16));
    auto* CanvasSlot=Canvas->AddChildToCanvas(Border);CanvasSlot->SetAnchors(FAnchors(.5f,.5f));CanvasSlot->SetAlignment(FVector2D(.5,.5));CanvasSlot->SetSize(FVector2D(1160,810));
    auto* Layout=WidgetTree->ConstructWidget<UVerticalBox>();Border->AddChild(Layout);
    Header=WidgetTree->ConstructWidget<UVerticalBox>();Layout->AddChildToVerticalBox(Header);
    auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Layout->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Body=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Body);Build();return Super::RebuildWidget();
}
ATFStop* UTFPanel::Selected() const{auto* D=IsValid(Anchor)?Anchor->Drive():nullptr;return D&&D->Stops.IsValidIndex(SelectedFloor)?D->Stops[SelectedFloor].Get():nullptr;}
TSubclassOf<UFGItemDescriptor> UTFPanel::ChosenResource() const{return SelectedResource;}
void UTFPanel::ResourcePicker(UVerticalBox* Parent,TSubclassOf<UFGItemDescriptor> Current)
{
    auto* D=Anchor->Drive();SelectedResource=Current;
    auto* SearchRow=WidgetTree->ConstructWidget<UHorizontalBox>();Parent->AddChildToVerticalBox(SearchRow);
    ResourceSearch=WidgetTree->ConstructWidget<UEditableTextBox>();ResourceSearch->SetForegroundColor(FLinearColor::Black);ResourceSearch->SetHintText(FText::FromString(D->IsFluid?TEXT("Search fluids..."):TEXT("Search items...")));
    ResourceSearch->SetText(FText::FromString(SearchQuery));ResourceSearch->OnTextChanged.AddDynamic(this,&UTFPanel::ResourceSearchChanged);
    Column(SearchRow,5)->AddChildToVerticalBox(ResourceSearch);Button(Column(SearchRow,1),TEXT("Clear"),105);
    ResourceChoice=WidgetTree->ConstructWidget<UTFResourceCombo>();ReadableChoices(ResourceChoice);
    auto ComboStyle=ResourceChoice->GetWidgetStyle();
    ComboStyle.ComboButtonStyle.ButtonStyle.SetNormal(FSlateRoundedBoxBrush(FLinearColor(.10,.12,.14),3.f));
    ComboStyle.ComboButtonStyle.ButtonStyle.SetHovered(FSlateRoundedBoxBrush(FLinearColor(.035,.045,.055),3.f,FLinearColor(.95,.73,.35),2.f));
    ComboStyle.ComboButtonStyle.ButtonStyle.SetPressed(FSlateRoundedBoxBrush(FLinearColor(.018,.025,.03),3.f));
    ResourceChoice->SetWidgetStyle(ComboStyle);
    ResourceChoice->OnGenerateWidgetEvent.BindDynamic(this,&UTFPanel::ResourceRow);
    ResourceChoice->OnSelectionChanged.AddDynamic(this,&UTFPanel::ResourceChanged);
    if(auto* RM=AFGRecipeManager::Get(GetWorld()))for(auto R:RM->GetAllItemDescriptors())if(R&&UFGItemDescriptor::GetForm(R)==(D->IsFluid?EResourceForm::RF_LIQUID:EResourceForm::RF_SOLID))AllResources.AddUnique(R);
    AllResources.Sort([](const TSubclassOf<UFGItemDescriptor>& A,const TSubclassOf<UFGItemDescriptor>& B){return TF::ResourceName(A)<TF::ResourceName(B);});
    Parent->AddChildToVerticalBox(ResourceChoice)->SetPadding(FMargin(0,6));ResourceSummary=Text(Parent,TEXT(""),14);FilterResources();
}
void UTFPanel::FilterResources()
{
    if(!ResourceChoice)return;
    UpdatingResourcePicker=true;ResourceChoice->ClearOptions();Resources.Reset();
    const bool Fluid=IsValid(Anchor)&&Anchor->Drive()&&Anchor->Drive()->IsFluid;
    ResourceChoice->AddOption(Page==1?TEXT("Any item"):Fluid?TEXT("Choose fluid"):TEXT("Choose item"));
    for(auto R:AllResources)if(SearchQuery.IsEmpty()||TF::ResourceName(R).Contains(SearchQuery,ESearchCase::IgnoreCase))Resources.Add(R);
    for(auto R:Resources)
    {
        FString Label=TF::ResourceName(R);
        if(AllResources.ContainsByPredicate([&](auto Other){return Other!=R&&TF::ResourceName(Other)==Label;}))Label+=TEXT(" [")+R->GetPathName()+TEXT("]");
        ResourceChoice->AddOption(Label);
    }
    const int32 Index=Resources.IndexOfByKey(SelectedResource);
    if(SelectedResource&&Index==INDEX_NONE)ResourceChoice->ClearSelection();else ResourceChoice->SetSelectedIndex(Index+1);
    ResourceSummary->SetText(FText::FromString((Resources.IsEmpty()?FString(TEXT("No matching items. ")):FString::Printf(TEXT("%d matches. "),Resources.Num()))+(SelectedResource?TEXT("Selected: ")+TF::ResourceName(SelectedResource):TEXT(""))));
    UpdatingResourcePicker=false;
}
void UTFPanel::ResourceSearchChanged(const FText& Value){SearchQuery=Value.ToString().TrimStartAndEnd();FilterResources();if(ResourceChoice)ResourceChoice->OpenResults(!SearchQuery.IsEmpty());}
void UTFPanel::ResourceChanged(FString,ESelectInfo::Type)
{
    if(UpdatingResourcePicker||!ResourceChoice)return;
    const int32 I=ResourceChoice->GetSelectedIndex()-1;SelectedResource=Resources.IsValidIndex(I)?Resources[I]:nullptr;
    if(ResourceSummary)ResourceSummary->SetText(FText::FromString(SelectedResource?TEXT("Selected: ")+TF::ResourceName(SelectedResource):TEXT("No item selected")));
}
UWidget* UTFPanel::ResourceRow(FString Value)
{
    auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();
    const int32 I=ResourceChoice?ResourceChoice->FindOptionIndex(Value)-1:INDEX_NONE;
    if(Resources.IsValidIndex(I))
    {
        auto* Icon=WidgetTree->ConstructWidget<UImage>();Icon->SetBrushFromTexture(UFGItemDescriptor::GetSmallIcon(Resources[I]));
        auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(28);Size->SetHeightOverride(28);Size->AddChild(Icon);Row->AddChildToHorizontalBox(Size);
    }
    auto* Label=WidgetTree->ConstructWidget<UTextBlock>();Label->SetText(FText::FromString(Value));Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));Row->AddChildToHorizontalBox(Label)->SetPadding(FMargin(6,3));return Row;
}
void UTFPanel::FloorChanged(FString,ESelectInfo::Type){if(FloorChoice&&FloorChoice->GetSelectedIndex()>=0&&SelectedFloor!=FloorChoice->GetSelectedIndex()){SelectedFloor=FloorChoice->GetSelectedIndex();FlushArmed=false;FloorRebuild=true;PendingFeedback.Empty();RebuildDelay=.01f;}}
void UTFPanel::InventoryGrid(UVerticalBox* Parent,int32 Bucket,int32 Slots,int32 Columns)
{
    auto* Grid=WidgetTree->ConstructWidget<UUniformGridPanel>();Grid->SetSlotPadding(FMargin(2));Parent->AddChildToVerticalBox(Grid);
    for(int32 I=0;I<Slots;++I)
    {
        auto* Tile=NewObject<UTFInventoryTile>(this);Tile->Bucket=Bucket;Tile->Index=I;Tiles.Add(Tile);
        auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(56);Size->SetHeightOverride(56);Grid->AddChildToUniformGrid(Size,I/Columns,I%Columns);
        Tile->Frame=WidgetTree->ConstructWidget<UBorder>();Tile->Frame->SetPadding(FMargin(2));Size->AddChild(Tile->Frame);
        Tile->Button=WidgetTree->ConstructWidget<UButton>();Tile->Frame->AddChild(Tile->Button);
        FButtonStyle Style=Tile->Button->GetStyle();Style.SetNormalPadding(FMargin(0));Style.SetPressedPadding(FMargin(0));FSlateBrush Brush;Brush.DrawAs=ESlateBrushDrawType::Box;Brush.TintColor=FSlateColor(FLinearColor(.35,.35,.35));Style.SetNormal(Brush);Brush.TintColor=FSlateColor(FLinearColor(.65,.65,.65));Style.SetHovered(Brush);Brush.TintColor=FSlateColor(FLinearColor(.9,.45,.12));Style.SetPressed(Brush);Tile->Button->SetStyle(Style);
        auto* Overlay=WidgetTree->ConstructWidget<UOverlay>();auto* ContentSlot=Cast<UButtonSlot>(Tile->Button->AddChild(Overlay));ContentSlot->SetPadding(FMargin(0));ContentSlot->SetHorizontalAlignment(HAlign_Fill);ContentSlot->SetVerticalAlignment(VAlign_Fill);
        Tile->Icon=WidgetTree->ConstructWidget<UImage>();FSlateBrush IconBrush;IconBrush.SetImageSize(FVector2D(56,56));Tile->Icon->SetBrush(IconBrush);Tile->Icon->SetVisibility(ESlateVisibility::HitTestInvisible);auto* IconSlot=Overlay->AddChildToOverlay(Tile->Icon);IconSlot->SetHorizontalAlignment(HAlign_Fill);IconSlot->SetVerticalAlignment(VAlign_Fill);
        Tile->Badge=WidgetTree->ConstructWidget<UBorder>();Tile->Badge->SetBrushColor(FLinearColor(.94,.43,.11));Tile->Badge->SetPadding(FMargin(3,0));Tile->Badge->SetVisibility(ESlateVisibility::HitTestInvisible);auto* CountSlot=Overlay->AddChildToOverlay(Tile->Badge);CountSlot->SetHorizontalAlignment(HAlign_Fill);CountSlot->SetVerticalAlignment(VAlign_Bottom);
        Tile->Count=WidgetTree->ConstructWidget<UTextBlock>();auto Font=Tile->Count->GetFont();Font.Size=20;Font.TypefaceFontName=TEXT("Bold");Tile->Count->SetFont(Font);Tile->Count->SetJustification(ETextJustify::Right);Tile->Count->SetColorAndOpacity(FSlateColor(FLinearColor::White));Tile->Badge->AddChild(Tile->Count);
        Tile->ReservationMark=WidgetTree->ConstructWidget<UTextBlock>();auto MarkFont=Tile->ReservationMark->GetFont();MarkFont.Size=12;MarkFont.TypefaceFontName=TEXT("Bold");Tile->ReservationMark->SetFont(MarkFont);Tile->ReservationMark->SetColorAndOpacity(FSlateColor(FLinearColor::White));Tile->ReservationMark->SetVisibility(ESlateVisibility::HitTestInvisible);Overlay->AddChildToOverlay(Tile->ReservationMark)->SetVerticalAlignment(VAlign_Top);
        Tile->Action=NewObject<UTFButtonAction>(this);Tile->Action->Panel=this;Tile->Action->Action=200;Tile->Action->A=Bucket;Tile->Action->B=I;Actions.Add(Tile->Action);Tile->Button->OnClicked.AddDynamic(Tile->Action,&UTFButtonAction::Click);
    }
}
void UTFPanel::RefreshStorage()
{
    auto* D=IsValid(Anchor)?Anchor->Drive():nullptr;auto* S=Selected();if(!D||!S||Page!=0)return;
    if(D->IsFluid)
    {
        for(int32 I=0;I<FluidBars.Num()&&I<D->Compartments;++I){const float Amount=StorageView==2?D->CargoAmount(I):StorageView==1?S->Tanks[I].Output:S->Tanks[I].Input;const float Capacity=FTFStats::ForTier(D->Tier).Fluid/D->Compartments;FluidBars[I]->SetPercent(Amount/Capacity);FluidAmounts[I]->SetText(FText::FromString(FString::Printf(TEXT("Tank %d   %s   %.1f / %.0f m3"),I+1,*TF::ResourceName(D->TankResources[I]),Amount,Capacity)));}return;
    }
    TArray<FInventoryStack> In,Out,Player,Cabin;
    if(S->HasAuthority()){S->Incoming->GetInventoryStacks(In,true);S->Outgoing->GetInventoryStacks(Out,true);}else{for(const auto& V:S->IncomingView)In.Add(FInventoryStack(V.NumItems,V.Resource));for(const auto& V:S->OutgoingView)Out.Add(FInventoryStack(V.NumItems,V.Resource));}
    if(auto* P=Cast<AFGCharacterPlayer>(GetOwningPlayerPawn()))P->GetInventory()->GetInventoryStacks(Player,true);
    TMap<TSubclassOf<UFGItemDescriptor>,int32> Cargo;for(const auto& C:D->Cargo)Cargo.FindOrAdd(C.Resource)+=FMath::RoundToInt(C.Amount);
    TArray<TSubclassOf<UFGItemDescriptor>> Keys;Cargo.GetKeys(Keys);Keys.Sort([](const TSubclassOf<UFGItemDescriptor>& A,const TSubclassOf<UFGItemDescriptor>& B){return TF::ResourceName(A)<TF::ResourceName(B);});
    for(auto R:Keys)for(int32 Left=Cargo[R];Left>0;){const int32 N=FMath::Min(Left,TF::StackSize(R));Cabin.Add(FInventoryStack(N,R));Left-=N;}
    TMap<TSubclassOf<UFGItemDescriptor>,int32> Transit;
    for(const auto& C:D->Cargo)if(C.Destination==S)Transit.FindOrAdd(C.Resource)+=FMath::RoundToInt(C.Amount);
    const int32 Capacity=FTFStats::ForTier(D->Tier).Stacks;
    const auto IncomingLayout=TFStorageLayout(In,Capacity,S->IncomingReservations,{});
    const auto OutgoingLayout=TFStorageLayout(Out,Capacity,S->OutgoingReservations,Transit);
    for(auto T:Tiles)
    {
        const auto& Stacks=T->Bucket==0?In:T->Bucket==1?Out:T->Bucket==2?Cabin:Player;
        FTFStorageTile View;View.SourceIndex=T->Index;
        if(T->Bucket==0&&IncomingLayout.IsValidIndex(T->Index))View=IncomingLayout[T->Index];
        if(T->Bucket==1&&OutgoingLayout.IsValidIndex(T->Index))View=OutgoingLayout[T->Index];
        const bool Has=Stacks.IsValidIndex(View.SourceIndex)&&Stacks[View.SourceIndex].HasItems();auto R=Has?Stacks[View.SourceIndex].Item.GetItemClass():nullptr;const int32 Count=Has?Stacks[View.SourceIndex].NumItems:0;
        auto IconResource=Has?R:View.ReservedFor;
        if(T->ShownResource!=IconResource){T->ShownResource=IconResource;if(IconResource)T->Icon->SetBrushFromTexture(UFGItemDescriptor::GetSmallIcon(IconResource));}
        if(T->ShownCount!=Count){T->ShownCount=Count;T->Count->SetText(FText::AsNumber(Count));}
        T->Action->Resource=R;T->Action->B=View.SourceIndex;
        T->Icon->SetVisibility(IconResource?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);T->Icon->SetRenderOpacity(Has?1.f:.32f);
        T->Badge->SetVisibility(Has?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
        // Empty reserved tiles stay focusable so their reservation tooltip remains accessible.
        T->Button->SetIsEnabled(T->Bucket!=2);
        T->Frame->SetBrushColor(View.WaitingForSpace?FLinearColor(.85,.4,.04):View.ReservedFor?FLinearColor(.03,.38,.85):FLinearColor(.13,.15,.17));
        T->Button->SetBackgroundColor(View.ReservedFor?FLinearColor(.25,.5,.9):FLinearColor::White);
        T->ReservationMark->SetText(FText::FromString(View.WaitingForSpace?TEXT("WAIT"):View.InTransit?TEXT("IN"):View.ReservedFor?TEXT("R"):TEXT("")));
        FString Tip=Has?TF::ResourceName(R)+FString::Printf(TEXT(" x %d"),Count)+(T->Bucket==3?TEXT("\nClick to put into Incoming storage"):T->Bucket==2?TEXT("\nOnboard cargo is read-only"):TEXT("\nClick to take this stack")):TEXT("Empty slot");
        if(View.ReservedFor)Tip+=TEXT("\nReserved for ")+TF::ResourceName(View.ReservedFor);
        if(View.WaitingForSpace)Tip+=TEXT("\nWaiting for space: existing cargo is unchanged.");
        if(View.InTransit)Tip+=TEXT("\nIncoming: delivery already on the way.");
        if(T->Bucket==1&&R&&Transit.FindRef(R)>0)Tip+=FString::Printf(TEXT("\n%d additional items in transit to this floor."),Transit.FindRef(R));
        T->Frame->SetToolTipText(FText::FromString(Tip));T->Button->SetToolTipText(FText::FromString(Tip));
    }
}
void UTFPanel::MoveInventory(int32 Bucket,int32 InventorySlot,TSubclassOf<UFGItemDescriptor> Expected)
{
    auto* S=Selected();if(!S||!Expected||Bucket==2)return;
    auto* PC=Cast<AFGPlayerController>(GetOwningPlayer());auto* R=PC?PC->GetRemoteCallObjectOfClass<UTFRemoteCall>():nullptr;
    if(R)R->MoveStack(Anchor,S,Bucket==3,Bucket==3?0:Bucket,InventorySlot,Expected);
}
void UTFPanel::Build()
{
    int32 FocusAction=INDEX_NONE,FocusA=0,FocusB=0;
    for(auto Handler:Actions)if(Handler->Button&&Handler->Button->HasKeyboardFocus()){FocusAction=Handler->Action;FocusA=Handler->A;FocusB=Handler->B;break;}
    const bool SearchFocused=ResourceSearch&&ResourceSearch->HasKeyboardFocus();
    const bool FloorFocused=FloorChoice&&FloorChoice->HasKeyboardFocus();
    ON_SCOPE_EXIT
    {
        if(SearchFocused&&ResourceSearch)ResourceSearch->SetKeyboardFocus();
        else if(FloorFocused&&FloorChoice)FloorChoice->SetKeyboardFocus();
        else for(auto Handler:Actions)if(Handler->Button&&Handler->Action==FocusAction&&Handler->A==FocusA&&Handler->B==FocusB){Handler->Button->SetKeyboardFocus();break;}
    };
    if(!Body||!Header)return;Body->ClearChildren();Header->ClearChildren();Actions.Reset();Tiles.Reset();FluidBars.Reset();FluidAmounts.Reset();Resources.Reset();AllResources.Reset();DestinationStops.Reset();FloorChoice=nullptr;ResourceChoice=nullptr;ResourceSearch=nullptr;ResourceSummary=nullptr;DestinationChoice=nullptr;Timeout=nullptr;Name=nullptr;Weight=nullptr;ReservationCount=nullptr;
    auto* D=IsValid(Anchor)?Anchor->Drive():nullptr;
    auto* TitleRow=WidgetTree->ConstructWidget<UHorizontalBox>();Header->AddChildToVerticalBox(TitleRow);
    Text(Column(TitleRow,8),D?FString::Printf(TEXT("%s elevator  /  Mk%d"),D->IsFluid?TEXT("Fluid"):TEXT("Freight"),D->Tier):TEXT("Freight elevator"),27,FLinearColor::White);Button(Column(TitleRow,1),TEXT("Close"),0);
    Live=Text(Header,TEXT("Connecting..."),16,FLinearColor(.95,.65,.3));Message=Text(Header,TEXT(""),14,FLinearColor(.8,.8,.8));
    if(!D){Text(Body,TEXT("No drive connected. Place this floor stop on a matching elevator shaft."));return;}
    SelectedFloor=FMath::Clamp(SelectedFloor,0,FMath::Max(0,D->Stops.Num()-1));
    auto* Tabs=WidgetTree->ConstructWidget<UHorizontalBox>();Header->AddChildToVerticalBox(Tabs);
    const TCHAR* Names[]={TEXT("Storage"),TEXT("Ports"),TEXT("Routes"),TEXT("Settings"),D->IsFluid?TEXT("Fluid tanks"):TEXT("Reservations")};
    for(int32 I=0;I<5;++I)Button(Column(Tabs),Names[I],100,I);
    if(Page==0||Page==1||Page==3||(Page==4&&!D->IsFluid))
    {
        Text(Body,TEXT("Floor"),16);FloorChoice=WidgetTree->ConstructWidget<UComboBoxString>();ReadableChoices(FloorChoice);
        for(int32 I=0;I<D->Stops.Num();++I)if(auto* S=D->Stops[I].Get())FloorChoice->AddOption(FString::Printf(TEXT("%d  -  %s  (%.0f m)"),I+1,*S->Label(),S->Height()/100));
        FloorChoice->SetSelectedIndex(SelectedFloor);FloorChoice->OnSelectionChanged.AddDynamic(this,&UTFPanel::FloorChanged);Body->AddChildToVerticalBox(FloorChoice)->SetPadding(FMargin(0,4));
    }
    auto* S=Selected();
    if(Page==0)
    {
        if(!S){Text(Body,TEXT("Add a floor stop to use storage."));return;}
        auto* Views=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Views);const TCHAR* Labels[]={TEXT("Incoming"),TEXT("Outgoing"),TEXT("Onboard")};
        for(int32 I=0;I<3;++I)Button(Column(Views),Labels[I],102,I);
        if(D->Routes.IsEmpty())Text(Body,TEXT("Add a destination in Routes to start shipping."),15,FLinearColor(1,.65,.25));
        if(D->IsFluid)
        {
            Text(Body,StorageView==0?TEXT("Waiting to be picked up"):StorageView==1?TEXT("Delivered to this floor"):TEXT("Cargo in the cabin"),20);
            for(int32 I=0;I<D->Compartments;++I){FluidAmounts.Add(Text(Body,TEXT(""),21));auto* Bar=WidgetTree->ConstructWidget<UProgressBar>();Bar->SetFillColorAndOpacity(FLinearColor(.95,.45,.12));auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetHeightOverride(32);Size->AddChild(Bar);Body->AddChildToVerticalBox(Size)->SetPadding(FMargin(0,0,0,20));FluidBars.Add(Bar);}
        }
        else
        {
            Text(Body,TEXT("Click a storage stack to take it. Click your inventory to put a stack into Incoming."),14);
            if(StorageView!=2)Text(Body,TEXT("Blue / R: reserved   |   IN: delivery on the way   |   Amber / WAIT: waiting for space"),14,FLinearColor(.55,.75,1));
            auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Row);auto* Left=Column(Row,8);auto* Right=Column(Row,9);
            Text(Left,StorageView==0?TEXT("Incoming storage"):StorageView==1?TEXT("Outgoing storage"):TEXT("Onboard cargo (read-only)"),21);
            if(StorageView!=2)Button(Left,TEXT("Take all"),16,SelectedFloor,StorageView);else Text(Left,TEXT("Unloads at its assigned floor."),15);
            InventoryGrid(Left,StorageView,FTFStats::ForTier(D->Tier).Stacks,8);
            Text(Right,TEXT("Your inventory"),21);Text(Right,TEXT("Click a stack to send it to Incoming."),15);
            auto* Player=Cast<AFGCharacterPlayer>(GetOwningPlayerPawn());InventoryGrid(Right,3,Player?Player->GetInventory()->GetSizeLinear():0,9);
        }
        RefreshStorage();return;
    }
    if(Page==1)
    {
        if(!S)return;Text(Body,TEXT("Choose the number shown above the port."),16);
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Row);
        for(int32 I=0;I<4;++I)Button(Column(Row),FString::Printf(TEXT("%d   %s"),I+1,S->Ports[I].Input?TEXT("INPUT"):TEXT("OUTPUT")),103,I);
        const auto& P=S->Ports[SelectedPort];Text(Body,FString::Printf(TEXT("Port %d"),SelectedPort+1),24,FLinearColor::White);
        auto* Controls=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Controls);auto* Direction=Column(Controls);auto* Filter=Column(Controls);
        Text(Direction,TEXT("Direction"),18);Button(Direction,P.Input?TEXT("INPUT  -  Change to output"):TEXT("OUTPUT  -  Change to input"),9,SelectedFloor,SelectedPort);
        Text(Direction,TEXT("Input fills this floor's storage. Output sends out delivered cargo."),15);
        if(D->IsFluid)
        {
            Text(Filter,TEXT("Compartment"),18);DestinationChoice=WidgetTree->ConstructWidget<UComboBoxString>();for(int32 I=0;I<D->Compartments;++I)DestinationChoice->AddOption(FString::Printf(TEXT("Tank %d - %s"),I+1,*TF::ResourceName(D->TankResources[I])));DestinationChoice->SetSelectedIndex(P.Compartment);Filter->AddChildToVerticalBox(DestinationChoice);Button(Filter,TEXT("Apply compartment"),11,SelectedFloor,SelectedPort);
        }
        else{Text(Filter,TEXT("Item filter"),18);ResourcePicker(Filter,P.Filter);Button(Filter,TEXT("Apply filter"),10,SelectedFloor,SelectedPort);}
        Text(Body,D->IsFluid?TEXT("Disconnect and drain the pipe before changing direction or compartment."):TEXT("Disconnect the belt before changing its direction."),14);return;
    }
    if(Page==2)
    {
        Text(Body,D->IsFluid?TEXT("Send fluid to a floor"):TEXT("Send items to a floor"),23,FLinearColor::White);auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Row);auto* Item=Column(Row,4);auto* Floor=Column(Row,4);auto* Share=Column(Row,2);
        Text(Item,D->IsFluid?TEXT("Fluid"):TEXT("Item"));ResourcePicker(Item);Text(Floor,TEXT("Destination"));DestinationChoice=WidgetTree->ConstructWidget<UComboBoxString>();ReadableChoices(DestinationChoice);for(auto Stop:D->Stops)if(IsValid(Stop)){DestinationStops.Add(Stop);DestinationChoice->AddOption(FString::Printf(TEXT("%d - %s"),DestinationStops.Num(),*Stop->Label()));}if(DestinationStops.Num())DestinationChoice->SetSelectedIndex(0);Floor->AddChildToVerticalBox(DestinationChoice)->SetPadding(FMargin(0,6));
        Text(Share,TEXT("Share"));Weight=WidgetTree->ConstructWidget<USpinBox>();Weight->SetForegroundColor(FSlateColor(FLinearColor::Black));Weight->SetMaxFractionalDigits(0);Weight->SetMinFractionalDigits(0);Weight->SetMinValue(1);Weight->SetMaxValue(100);Weight->SetValue(1);Share->AddChildToVerticalBox(Weight)->SetPadding(FMargin(0,6));
        Button(Body,TEXT("Add destination"),12);Text(Body,TEXT("Use share 1 for equal deliveries. Add the same item again to serve another floor."),14);
        for(int32 I=0;I<D->Routes.Num();++I){const auto& R=D->Routes[I];auto* Line=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Line);Text(Column(Line,8),FString::Printf(TEXT("%s  >  %s    /    Share %d"),*TF::ResourceName(R.Resource),R.Destination?*R.Destination->Label():TEXT("Missing floor"),R.Weight),17);Button(Column(Line,2),TEXT("Remove"),13,I);}return;
    }
    if(Page==3)
    {
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Row);auto* Floor=Column(Row);auto* Service=Column(Row);
        Text(Floor,TEXT("Floor settings"),23,FLinearColor::White);
        if(S){Text(Floor,TEXT("Name"));Name=WidgetTree->ConstructWidget<UEditableTextBox>();Name->SetForegroundColor(FLinearColor::Black);Name->SetText(FText::FromString(S->Label()));Floor->AddChildToVerticalBox(Name);Button(Floor,TEXT("Save name"),4,SelectedFloor);Button(Floor,S->Enabled?TEXT("Floor enabled - Disable"):TEXT("Floor disabled - Enable"),5,SelectedFloor);
            Text(Floor,TEXT("Pickup"),18);Button(Floor,S->WaitFull?TEXT("Wait until full - Change to immediate"):TEXT("Immediate - Change to wait until full"),6,SelectedFloor);
            if(S->WaitFull){Text(Floor,TEXT("Maximum wait (seconds)"));Timeout=WidgetTree->ConstructWidget<USpinBox>();Timeout->SetForegroundColor(FSlateColor(FLinearColor::Black));Timeout->SetMaxFractionalDigits(0);Timeout->SetMinFractionalDigits(0);Timeout->SetMinValue(1);Timeout->SetMaxValue(600);Timeout->SetDelta(1);Timeout->SetValue(S->MaxWait);Floor->AddChildToVerticalBox(Timeout);Button(Floor,TEXT("Save wait time"),7,SelectedFloor);}
            if(D->IsFluid)Button(Floor,FlushArmed?TEXT("Confirm: discard floor fluids"):TEXT("Flush floor fluids..."),17,SelectedFloor,1);
        }
        Text(Service,TEXT("Elevator controls"),23,FLinearColor::White);Button(Service,D->Paused?TEXT("Resume service"):TEXT("Pause service"),1);Button(Service,TEXT("Depart now"),2);Button(Service,TEXT("Reset power fuse"),21);
        const auto Stats=FTFStats::ForTier(D->Tier);Text(Service,FString::Printf(TEXT("%d floors  /  %.0f m/s  /  %.0f MW"),D->Stops.Num(),Stats.Speed/100,Stats.ActiveMW),16);Text(Service,TEXT("Immediate pickup ships what was waiting when the cabin arrived. Later items wait for the next visit."),15);return;
    }
    if(Page==4&&!D->IsFluid)
    {
        if(!S){Text(Body,TEXT("Add a floor stop to reserve storage."));return;}
        auto* Buffers=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Buffers);
        for(int32 N=0;N<2;++N)Button(Column(Buffers),N==0?TEXT("Incoming"):TEXT("Outgoing"),104,N);
        const auto& Rules=ReservationBuffer==0?S->IncomingReservations:S->OutgoingReservations;
        const int32 Capacity=FTFStats::ForTier(D->Tier).Stacks,Reserved=S->ReservedStorageStacks(ReservationBuffer);
        Text(Body,ReservationBuffer==0?TEXT("Incoming storage reservations"):TEXT("Outgoing storage reservations"),23,FLinearColor::White);
        Text(Body,FString::Printf(TEXT("%d stacks total  |  %d reserved  |  %d shared"),Capacity,Reserved,FMath::Max(0,Capacity-Reserved)),19,FLinearColor(1,.65,.25));
        Text(Body,TEXT("Guarantee space for an item. It can also use shared space, but cannot take another item's unused reservation."),16);
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Row);auto* Item=Column(Row,7);auto* Count=Column(Row,3);
        Text(Item,TEXT("Item"));ResourcePicker(Item);Text(Count,TEXT("Reserved stacks"));
        ReservationCount=WidgetTree->ConstructWidget<USpinBox>();ReservationCount->SetForegroundColor(FSlateColor(FLinearColor::Black));ReservationCount->SetMaxFractionalDigits(0);ReservationCount->SetMinFractionalDigits(0);ReservationCount->SetMinValue(0);ReservationCount->SetMaxValue(Capacity);ReservationCount->SetDelta(1);ReservationCount->SetValue(1);Count->AddChildToVerticalBox(ReservationCount)->SetPadding(FMargin(0,6));
        Button(Body,TEXT("Save reservation"),22,SelectedFloor,ReservationBuffer);
        Text(Body,TEXT("Stored stacks count toward the reservation. Existing cargo is never removed; space becomes protected as it empties."),14);
        if(Rules.IsEmpty())Text(Body,TEXT("No reservations. All slots in this buffer are shared."),17);
        for(const auto& R:Rules)
        {
            auto* Line=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Line);
            Text(Column(Line,8),FString::Printf(TEXT("%s  /  %d reserved stacks"),*TF::ResourceName(R.Resource),R.Stacks),17);
            Button(Column(Line,2),TEXT("Remove"),23,SelectedFloor,ReservationBuffer);Actions.Last()->Resource=R.Resource;
        }
        Text(Body,ReservationBuffer==0?TEXT("Incoming: items waiting here for pickup."):TEXT("Outgoing: deliveries arriving here. Items already on the way count toward their reservation."),14);return;
    }
    if(Page==4&&D->IsFluid)
    {
        Text(Body,TEXT("Tank layout"),23,FLinearColor::White);auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Row);for(int32 N:{1,2,4})Button(Column(Row),FString::Printf(TEXT("%d tank%s"),N,N==1?TEXT(""):TEXT("s")),14,0,N);
        Text(Body,TEXT("Choose a fluid, then assign its tank."),17);ResourcePicker(Body);
        for(int32 I=0;I<D->Compartments;++I)Button(Body,FString::Printf(TEXT("Tank %d: %s - Assign selected fluid"),I+1,*TF::ResourceName(D->TankResources[I])),15,0,I);
        Text(Body,TEXT("Empty the system before changing its layout. Disconnect and empty a tank's pipes before changing its fluid."),14);
    }
}
void UTFPanel::Handle(int32 Action,int32 A,int32 B,TSubclassOf<UFGItemDescriptor> ResourceOverride)
{
    if(Action==0){Close();return;}if(Action==100){Page=A;SearchQuery.Empty();SelectedResource=nullptr;FlushArmed=false;Build();return;}if(Action==102){StorageView=A;Build();return;}if(Action==103){SelectedPort=A;Build();return;}
    if(Action==105){if(ResourceSearch){ResourceSearch->SetText(FText::GetEmpty());ResourceSearch->SetKeyboardFocus();}return;}
    if(Action==104){ReservationBuffer=FMath::Clamp(A,0,1);Build();return;}
    if(Action==101){SelectedFloor=A;FlushArmed=false;Build();return;}if(Action==17&&!FlushArmed){FlushArmed=true;Build();return;}
    auto* PC=Cast<AFGPlayerController>(GetOwningPlayer());auto* R=PC?PC->GetRemoteCallObjectOfClass<UTFRemoteCall>():nullptr;
    if(!R){Message->SetText(FText::FromString(TEXT("Connecting to the server. Try again in a moment.")));return;}
    float Number=0;FString Value;if(Action==4&&Name)Value=Name->GetText().ToString();if(Action==7&&Timeout)Number=Timeout->GetValue();if(Action==11&&DestinationChoice)Number=DestinationChoice->GetSelectedIndex();
    if(Action==12){auto* D=Anchor->Drive();int32 I=DestinationChoice?DestinationChoice->GetSelectedIndex():INDEX_NONE;if(!D||!DestinationStops.IsValidIndex(I)||!ChosenResource()){Message->SetText(FText::FromString(TEXT("Choose an item and destination.")));return;}A=D->Stops.IndexOfByKey(DestinationStops[I]);Number=Weight?Weight->GetValue():1;}
    if(Action==22)Number=ReservationCount?ReservationCount->GetValue():0;
    Message->SetText(FText::FromString(TEXT("Saving...")));R->Command(Anchor,Action,A,B,Number,ResourceOverride?ResourceOverride:ChosenResource(),Value);if(Action==17)FlushArmed=false;
}
void UTFPanel::NativeTick(const FGeometry& Geometry,float Delta)
{
    Super::NativeTick(Geometry,Delta);
    for(auto Handler:Actions)Handler->RefreshStyle();
    if(RebuildDelay>=0){RebuildDelay-=Delta;if(RebuildDelay<0){const auto Chosen=ChosenResource();const int32 Dest=DestinationChoice?DestinationChoice->GetSelectedIndex():0;const float Share=Weight?Weight->GetValue():1;const float Reserved=ReservationCount?ReservationCount->GetValue():1;Build();if(ResourceChoice&&!FloorRebuild){SelectedResource=Chosen;FilterResources();}FloorRebuild=false;if(DestinationChoice&&Page==2&&Dest>=0&&Dest<DestinationChoice->GetOptionCount())DestinationChoice->SetSelectedIndex(Dest);if(Weight)Weight->SetValue(Share);if(ReservationCount)ReservationCount->SetValue(Reserved);if(Message)Message->SetText(FText::FromString(PendingFeedback));}}
    RefreshTimer+=Delta;if(RefreshTimer<.25f)return;RefreshTimer=0;if(!IsValid(Anchor)){Close();return;}auto* D=Anchor->Drive();if(!D||!Live)return;
    FString Status=D->Status.Left(1).ToUpper()+D->Status.RightChop(1).ToLower();Live->SetText(FText::FromString(FString::Printf(TEXT("%s   |   %.1f m   |   %s"),*Status,D->CabinHeight/100,D->Powered?TEXT("Power on"):TEXT("No power"))));RefreshStorage();
}
FReply UTFPanel::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if(Event.GetKey()==EKeys::Escape)
    {
        if(ResourceChoice&&ResourceChoice->IsOpen()){ResourceChoice->OpenResults(false);return FReply::Handled();}
        if(ResourceSearch&&ResourceSearch->HasKeyboardFocus()&&!SearchQuery.IsEmpty()){ResourceSearch->SetText(FText::GetEmpty());return FReply::Handled();}
        Close();return FReply::Handled();
    }
    if(Event.GetKey()==EKeys::Down&&ResourceSearch&&ResourceSearch->HasKeyboardFocus()&&ResourceChoice){ResourceChoice->OpenResults(true,true);return FReply::Handled();}
    return Super::NativeOnKeyDown(Geometry,Event);
}
void UTFPanel::ReleaseInput()
{
    if(!CapturedInput)return;CapturedInput=false;
    if(auto* PC=GetOwningPlayer()){FInputModeGameOnly Mode;Mode.SetConsumeCaptureMouseDown(false);PC->SetInputMode(Mode);PC->bShowMouseCursor=false;PC->SetIgnoreLookInput(false);PC->SetIgnoreMoveInput(false);}if(Active.Get()==this)Active.Reset();
}
void UTFPanel::Close(){ReleaseInput();RemoveFromParent();}
void UTFPanel::NativeDestruct(){ReleaseInput();Super::NativeDestruct();}
