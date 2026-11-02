#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "TFTypes.h"
#include "TFBuildables.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFSpecTest,"TrapperFreightElevators.Specifications",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTFSpecTest::RunTest(const FString&)
{
    const auto A=FTFStats::ForTier(1),B=FTFStats::ForTier(2),C=FTFStats::ForTier(3);
    TestEqual(TEXT("Mk1 cabin slots"),A.Stacks,24);TestEqual(TEXT("Mk2 cabin slots"),B.Stacks,48);TestEqual(TEXT("Mk3 cabin slots"),C.Stacks,96);
    TestEqual(TEXT("Mk1 fluid"),A.Fluid,300.f);TestEqual(TEXT("Mk2 fluid"),B.Fluid,800.f);TestEqual(TEXT("Mk3 fluid"),C.Fluid,1500.f);
    TestEqual(TEXT("Mk1 stops"),A.Stops,6);TestEqual(TEXT("Mk2 stops"),B.Stops,12);TestEqual(TEXT("Mk3 stops"),C.Stops,20);
    TestEqual(TEXT("Maximum travel + roof = 256m"),TF::MaxTravel+800,25600.f);
    TestEqual(TEXT("Item port count"),GetDefault<ATFItemStop>()->Belts.Num(),4);
    TestEqual(TEXT("Fluid port count"),GetDefault<ATFFluidStop>()->Pipes.Num(),4);
    TestEqual(TEXT("Four item port number labels"),GetDefault<ATFItemStop>()->PortNumbers.Num(),4);
    TestEqual(TEXT("Four fluid port direction labels"),GetDefault<ATFFluidStop>()->PortDirections.Num(),4);
    TestTrue(TEXT("Conveyor sink advertises receiving capacity"),GetDefault<ATFItemStop>()->EstimatedMaxNumGrab_Threadsafe(.05f)>0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFIronworksTest,"TrapperFreightElevators.IronworksAssets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTFIronworksTest::RunTest(const FString&)
{
    for(const TCHAR* Name:{TEXT("Shaft"),TEXT("Screen"),TEXT("Base"),TEXT("Stop"),TEXT("Cabin"),TEXT("Gate"),TEXT("Cargo"),TEXT("Tank"),TEXT("Flanges"),TEXT("Head"),TEXT("Drum"),TEXT("Cables"),TEXT("Buffer"),TEXT("Level"),TEXT("FluidBuffer"),TEXT("PortSigns")})
    {
        const FString Path=FString(TEXT("/TrapperFreightElevators/Art/Ironworks/SM_IW_"))+Name+TEXT(".SM_IW_")+Name;
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path);TestNotNull(FString(Name)+TEXT(" cooked mesh source"),Mesh);
        if(!Mesh)continue;TestTrue(FString(Name)+TEXT(" has rendered triangles"),Mesh->GetNumTriangles(0)>0);
        for(const auto& Slot:Mesh->GetStaticMaterials())TestNotNull(FString(Name)+TEXT(" material binding"),Slot.MaterialInterface.Get());
        if(FString(Name)==TEXT("Shaft")||FString(Name)==TEXT("Base")||FString(Name)==TEXT("Buffer")){const auto B=Mesh->GetBoundingBox();TestTrue(FString(Name)+TEXT(" remains within the 8m footprint"),B.Min.X>=-400&&B.Min.Y>=-400&&B.Max.X<=400&&B.Max.Y<=400);}
        if(FString(Name)==TEXT("Shaft")||FString(Name)==TEXT("Base"))TestTrue(FString(Name)+TEXT(" column caps stay within section height"),Mesh->GetBoundingBox().Max.Z<=(FString(Name)==TEXT("Shaft")?400.1f:800.1f));
        if(FString(Name)==TEXT("Cabin"))TestTrue(TEXT("Cabin stays within 6m travel envelope"),Mesh->GetBoundingBox().Max.Z<=600.1f);
    }
    for(auto C:GetDefault<ATFDrive>()->Art)if(C&&C->GetName().StartsWith(TEXT("IW_")))TestNotNull(C->GetName()+TEXT(" component mesh"),C->GetStaticMesh().Get());
    const auto* Drive=GetDefault<ATFDrive>();
    for(auto C:{Drive->HoistHead.Get(),Drive->HoistDrum.Get()})if(C&&C->GetStaticMesh())
    {const FBox B=C->GetStaticMesh()->GetBoundingBox().TransformBy(C->GetRelativeTransform());TestTrue(TEXT("Head machinery clears the highest 6m cabin and remains inside the 8m cap"),B.Min.Z>600&&B.Max.Z<=800);}
    const auto* Stop=GetDefault<ATFItemStop>();for(int32 I=0;I<4;++I){const FVector Expected=FRotator(0,I*90,0).RotateVector(FVector(400,0,100));TestTrue(TEXT("Existing belt endpoint remains aligned"),Stop->Belts[I]->GetRelativeLocation().Equals(Expected,.01));}
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFMotionTest,"TrapperFreightElevators.Motion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTFMotionTest::RunTest(const FString&)
{
    for(int32 Tier=1;Tier<=3;++Tier)for(float Distance:{1.f,800.f,1600.f,24800.f})
    {
        const float Speed=FTFStats::ForTier(Tier).Speed,Duration=Distance/Speed+FMath::Min(.8f,Distance/Speed);
        float Previous=0;
        for(int32 I=1;I<=200;++I)
        {
            const float T=Duration*I/200,Position=TF::TravelPosition(T,Distance,Speed);
            if(Position<Previous-.01f || Position>Distance+.01f || Position-Previous>Speed*Duration/200+.02f)
            {AddError(TEXT("Travel must be monotonic, capped at tier speed, and never overshoot"));break;}
            Previous=Position;
        }
        TestEqual(TEXT("Exact landing without overshoot"),TF::TravelPosition(Duration+1,Distance,Speed),Distance);
        const float Step=FMath::Min(.001f,Duration/1000);
        TestTrue(TEXT("Starts and stops gently"),TF::TravelPosition(Step,Distance,Speed)/Step<Speed*.02f && (Distance-TF::TravelPosition(Duration-Step,Distance,Speed))/Step<Speed*.02f+.1f);
    }
    TestEqual(TEXT("Closed gate endpoint"),TF::Ease(0),0.f);TestEqual(TEXT("Open gate endpoint"),TF::Ease(1),1.f);
    TestTrue(TEXT("Door eases into motion"),TF::Ease(.01f)<.001f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAutoShaftTest,"TrapperFreightElevators.AutomaticShaft",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTFAutoShaftTest::RunTest(const FString&)
{
    const auto Empty=FTFShaftLayout::Make({},{});
    TestTrue(TEXT("A drive alone creates no extra shaft"),Empty.Height==800&&Empty.Frames.IsEmpty()&&Empty.Screens.IsEmpty());
    for(const TArray<float>& Stops:{TArray<float>{0,800,1600},TArray<float>{0,1200,3600},TArray<float>{400,24800},TArray<float>{100,1700,3400}})
    for(const TArray<float>& Manual:{TArray<float>{},TArray<float>{800,2400,2800}})
    {
        const auto Layout=FTFShaftLayout::Make(Stops,Manual);
        TestTrue(TEXT("Tower stays inside its height envelope"),Layout.Height<=25600);
        for(float Z=800;Z<Layout.Height;Z+=100)
        {
            bool Existing=false;for(float H:Stops)Existing|=Z>=H&&Z<H+600;for(float H:Manual)Existing|=Z>=H&&Z<H+400;
            int32 Count=0;for(auto Span:Layout.Frames){TestTrue(TEXT("Infill meshes fit the 1m stop grid"),Span.Y>=100&&Span.Y<=400&&FMath::Fmod(Span.Y,100.f)==0);if(Z>=Span.X&&Z<Span.X+Span.Y)++Count;}
            if(Count!=(Existing?0:1)){AddError(TEXT("Infill must cover each gap exactly once without overlapping floor stops or manual shafts"));return false;}
            TestEqual(TEXT("Screens leave landings, manual shafts and the head cap clear"),Layout.Screens.ContainsByPredicate([Z](const FVector2D& Span){return Z>=Span.X && Z<Span.X+Span.Y;}),!Existing&&Z<Layout.Height-200);
        }
        TestTrue(TEXT("Rebuilding after load is deterministic"),Layout==FTFShaftLayout::Make(Stops,Manual));
    }
    const auto* D=GetDefault<ATFDrive>();
    TestNotNull(TEXT("Automatic frame asset"),D->AutoShaftFrames->GetStaticMesh().Get());
    TestNotNull(TEXT("Automatic screen asset"),D->AutoShaftScreens->GetStaticMesh().Get());
    return true;
}
#endif
