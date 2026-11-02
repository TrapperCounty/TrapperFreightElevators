#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FGRemoteCallObject.h"
#include "Module/GameInstanceModule.h"
#include "TFTypes.h"
#include "Components/ComboBoxString.h"
#include "TFPanel.generated.h"
class ATFPart;
class ATFDrive;
class ATFStop;
class AFGPlayerController;
class UVerticalBox;
class UTextBlock;
class UComboBoxString;
class USpinBox;
class UEditableTextBox;
class UTFPanel;
class UButton;
class UImage;
class UBorder;
class UHorizontalBox;
class UProgressBar;

UCLASS()
class TRAPPERFREIGHTELEVATORS_API UTFResourceCombo : public UComboBoxString
{
    GENERATED_BODY()
public:
    void OpenResults(bool Open, bool Focus = false);
};

UCLASS()
class TRAPPERFREIGHTELEVATORS_API UTFRemoteCall : public UFGRemoteCallObject
{
    GENERATED_BODY()
public:
    UPROPERTY(Replicated) bool Ready = true;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UFUNCTION(Server,Reliable) void Command(ATFPart* Anchor,int32 Action,int32 A,int32 B,float Number,TSubclassOf<UFGItemDescriptor> Resource,const FString& Text);
    UFUNCTION(Client,Reliable) void Feedback(const FString& Text);
    UFUNCTION(Server,Reliable) void MoveStack(ATFPart* Anchor,ATFStop* Stop,bool Deposit,int32 Buffer,int32 Slot,TSubclassOf<UFGItemDescriptor> Expected);
};

UCLASS()
class TRAPPERFREIGHTELEVATORS_API UTFInstanceModule : public UGameInstanceModule
{
    GENERATED_BODY()
public: UTFInstanceModule();
};

UCLASS()
class TRAPPERFREIGHTELEVATORS_API UTFButtonAction : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UTFPanel> Panel;
    UPROPERTY() TSubclassOf<UFGItemDescriptor> Resource;
    UPROPERTY() TObjectPtr<UButton> Button;
    bool Selected = false;
    int32 StyleState = INDEX_NONE;
    void RefreshStyle();
    int32 Action=0,A=0,B=0;
    UFUNCTION() void Click();
};

UCLASS()
class TRAPPERFREIGHTELEVATORS_API UTFInventoryTile : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UImage> Icon;
    UPROPERTY() TObjectPtr<UTextBlock> Count;
    UPROPERTY() TObjectPtr<UBorder> Badge;
    UPROPERTY() TObjectPtr<UBorder> Frame;
    UPROPERTY() TObjectPtr<UTextBlock> ReservationMark;
    UPROPERTY() TObjectPtr<UButton> Button;
    UPROPERTY() TObjectPtr<UTFButtonAction> Action;
    UPROPERTY() TSubclassOf<UFGItemDescriptor> ShownResource;
    int32 ShownCount=-1,Bucket=0,Index=0;
};

UCLASS()
class TRAPPERFREIGHTELEVATORS_API UTFPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    static TWeakObjectPtr<UTFPanel> Active;
    static void Open(AFGPlayerController* Player,ATFPart* Part);
    UPROPERTY() TObjectPtr<ATFPart> Anchor;
    UPROPERTY() TObjectPtr<UVerticalBox> Body;
    UPROPERTY() TObjectPtr<UVerticalBox> Header;
    UPROPERTY() TObjectPtr<UComboBoxString> FloorChoice;
    UPROPERTY() TArray<TObjectPtr<UTFInventoryTile>> Tiles;
    UPROPERTY() TArray<TObjectPtr<UProgressBar>> FluidBars;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> FluidAmounts;
    UPROPERTY() TObjectPtr<UTextBlock> Live;
    UPROPERTY() TObjectPtr<UTextBlock> Message;
    UPROPERTY() TObjectPtr<UTFResourceCombo> ResourceChoice;
    UPROPERTY() TObjectPtr<UEditableTextBox> ResourceSearch;
    UPROPERTY() TObjectPtr<UTextBlock> ResourceSummary;
    UPROPERTY() TArray<TSubclassOf<UFGItemDescriptor>> AllResources;
    UPROPERTY() TSubclassOf<UFGItemDescriptor> SelectedResource;
    FString SearchQuery;
    bool UpdatingResourcePicker = false;
    bool FloorRebuild = false;
    UPROPERTY() TObjectPtr<UComboBoxString> DestinationChoice;
    UPROPERTY() TObjectPtr<USpinBox> Weight;
    UPROPERTY() TObjectPtr<USpinBox> ReservationCount;
    UPROPERTY() TObjectPtr<USpinBox> Timeout;
    UPROPERTY() TObjectPtr<UEditableTextBox> Name;
    UPROPERTY() TArray<TObjectPtr<UTFButtonAction>> Actions;
    UPROPERTY() TArray<TSubclassOf<UFGItemDescriptor>> Resources;
    UPROPERTY() TArray<TObjectPtr<ATFStop>> DestinationStops;
    int32 SelectedFloor=0;
    int32 Page=0,StorageView=0,SelectedPort=0,ReservationBuffer=0;
    float RefreshTimer=0;
    float RebuildDelay=-1;
    FString PendingFeedback;
    bool FlushArmed=false;
    bool CapturedInput=false;
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
    virtual void NativeDestruct() override;
    void Build();
    void Handle(int32 Action,int32 A,int32 B,TSubclassOf<UFGItemDescriptor> ResourceOverride=nullptr);
    void Close();
    void ReleaseInput();
    UTextBlock* Text(UVerticalBox* Parent,const FString& Value,int32 Size=16,FLinearColor Color=FLinearColor(.82,.87,.91));
    UButton* Button(UVerticalBox* Parent,const FString& Label,int32 Action,int32 A=0,int32 B=0);
    UVerticalBox* Column(UHorizontalBox* Row,float Weight=1.f);
    void ResourcePicker(UVerticalBox* Parent,TSubclassOf<UFGItemDescriptor> Current=nullptr);
    void FilterResources();
    UFUNCTION() void ResourceSearchChanged(const FText& Value);
    UFUNCTION() void ResourceChanged(FString Value,ESelectInfo::Type SelectionType);
    UFUNCTION() UWidget* ResourceRow(FString Value);
    void InventoryGrid(UVerticalBox* Parent,int32 Bucket,int32 Slots,int32 Columns);
    void RefreshStorage();
    void MoveInventory(int32 Bucket,int32 Slot,TSubclassOf<UFGItemDescriptor> Expected);
    UFUNCTION() void FloorChanged(FString Value,ESelectInfo::Type SelectionType);
    TSubclassOf<UFGItemDescriptor> ChosenResource() const;
    ATFStop* Selected() const;
};
