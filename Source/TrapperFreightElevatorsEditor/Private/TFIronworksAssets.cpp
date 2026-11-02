#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

// Editor-only, reproducible hard-surface assets. Runtime uses ordinary cooked static meshes.
namespace Ironworks
{
    enum EMat { Paint, Steel, Dark, Mesh, Hazard, Amber, Cable, Count };
    TArray<UMaterialInterface*> Materials;
    bool Save(UObject* A)
    {
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
        return UPackage::SavePackage(A->GetOutermost(),A,*FPackageName::LongPackageNameToFilename(A->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
    }
    bool LabelMaterial()
    {
        const TCHAR* Path=TEXT("/TrapperFreightElevators/Art/Ironworks/M_IW_Label");
        auto* M=LoadObject<UMaterial>(nullptr,TEXT("/TrapperFreightElevators/Art/Ironworks/M_IW_Label.M_IW_Label"),nullptr,LOAD_NoWarn);
        if(!M){auto* Original=LoadObject<UMaterial>(nullptr,TEXT("/Engine/EngineMaterials/DefaultTextMaterialOpaque.DefaultTextMaterialOpaque"));if(!Original)return false;M=DuplicateObject<UMaterial>(Original,CreatePackage(Path),TEXT("M_IW_Label"));M->SetFlags(RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(M);}
        M->SetShadingModel(MSM_Unlit);M->GetEditorOnlyData()->EmissiveColor=M->GetEditorOnlyData()->BaseColor;M->PostEditChange();return Save(M);
    }
    UTexture2D* Surface(const TCHAR* Name,int32 Mode)
    {
        const FString Path=FString(TEXT("/TrapperFreightElevators/Art/Ironworks/"))+Name;
        auto* T=LoadObject<UTexture2D>(nullptr,*(Path+TEXT(".")+Name),nullptr,LOAD_NoWarn);
        if(!T){T=NewObject<UTexture2D>(CreatePackage(*Path),Name,RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(T);}
        constexpr int32 N=256;TArray<FColor> Pixels;Pixels.SetNum(N*N);FRandomStream Random(174);
        for(int32 Y=0;Y<N;++Y)for(int32 X=0;X<N;++X)
        {
            const float Noise=Random.FRand();const float Brush=.004f*FMath::Sin(Y*.9f)+.003f*FMath::Sin(Y*2.7f);
            uint8 V=FMath::Clamp(FMath::RoundToInt(238+Brush*255+Noise*5),0,255);
            FColor C(V,V,V,255);
            if(Mode==1){const bool Wire=((X+Y)%64<2)||((X-Y+512)%64<2);C=FColor(V,V,V,Wire?255:0);}
            if(Mode==2){const bool Yellow=((X+Y)%128)<64;C=Yellow?FColor(233,162,29):FColor(23,26,27);if(Noise>.985f)C=FColor(133,139,139);}
            Pixels[Y*N+X]=C;
        }
        T->Source.Init(N,N,1,1,TSF_BGRA8,reinterpret_cast<const uint8*>(Pixels.GetData()));
        T->SRGB=true;T->CompressionSettings=TC_Default;T->LODGroup=TEXTUREGROUP_World;T->PostEditChange();Save(T);return T;
    }
    UMaterial* Material(const TCHAR* Name,FLinearColor Color,float Metal,float Rough,UTexture2D* T,bool Mask=false,bool Glow=false)
    {
        const FString Path=FString(TEXT("/TrapperFreightElevators/Art/Ironworks/"))+Name;
        auto* M=LoadObject<UMaterial>(nullptr,*(Path+TEXT(".")+Name),nullptr,LOAD_NoWarn);
        if(!M){M=NewObject<UMaterial>(CreatePackage(*Path),Name,RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(M);}
        M->GetExpressionCollection().Empty();
        auto* C=NewObject<UMaterialExpressionConstant3Vector>(M);C->Constant=Color;M->GetExpressionCollection().AddExpression(C);
        auto* Met=NewObject<UMaterialExpressionConstant>(M);Met->R=Metal;M->GetExpressionCollection().AddExpression(Met);
        auto* R=NewObject<UMaterialExpressionConstant>(M);R->R=Rough;M->GetExpressionCollection().AddExpression(R);
        M->GetEditorOnlyData()->Metallic.Expression=Met;M->GetEditorOnlyData()->Roughness.Expression=R;
        M->GetEditorOnlyData()->BaseColor.Expression=C;
        if(T)
        {
            auto* Tex=NewObject<UMaterialExpressionTextureSample>(M);Tex->Texture=T;Tex->SamplerType=SAMPLERTYPE_Color;M->GetExpressionCollection().AddExpression(Tex);
            auto* Mul=NewObject<UMaterialExpressionMultiply>(M);Mul->A.Expression=C;Mul->B.Expression=Tex;M->GetExpressionCollection().AddExpression(Mul);M->GetEditorOnlyData()->BaseColor.Expression=Mul;
            if(Mask){M->BlendMode=BLEND_Masked;M->TwoSided=true;M->GetEditorOnlyData()->OpacityMask.Expression=Tex;M->GetEditorOnlyData()->OpacityMask.OutputIndex=4;M->OpacityMaskClipValue=.4f;}
        }
        if(Glow)M->GetEditorOnlyData()->EmissiveColor.Expression=C;
        bool NeedsRecompile=false;M->SetMaterialUsage(NeedsRecompile,MATUSAGE_InstancedStaticMeshes);
        M->PostEditChange();Save(M);return M;
    }
    struct FMesh
    {
        FMeshDescription D;TUniquePtr<FStaticMeshAttributes> A;TArray<FPolygonGroupID> Groups;FTransform Transform=FTransform::Identity;
        FMesh(){A=MakeUnique<FStaticMeshAttributes>(D);A->Register();A->GetVertexInstanceUVs().SetNumChannels(1);for(int32 I=0;I<Count;++I)Groups.Add(D.CreatePolygonGroup());}
        void Face(TArray<FVector> P,int32 Mat,const TArray<FVector>& Normals={})
        {
            for(auto& V:P)V=Transform.TransformPosition(V);
            FVector N=FVector::CrossProduct(P[1]-P[0],P[2]-P[0]).GetSafeNormal();if(N.IsNearlyZero())return;
            FVector U,V;if(FMath::Abs(N.Z)>.7f){U=FVector::ForwardVector;V=FVector::RightVector;}else{U=FVector::CrossProduct(FVector::UpVector,N).GetSafeNormal();V=FVector::UpVector;}
            TArray<FVertexInstanceID> Corners;
            for(int32 Corner=0;Corner<P.Num();++Corner){const auto Pt=P[Corner];const FVector VN=Normals.IsValidIndex(Corner)?Transform.TransformVectorNoScale(Normals[Corner]).GetSafeNormal():N;const FVector VT=(U-VN*FVector::DotProduct(U,VN)).GetSafeNormal();auto Id=D.CreateVertex();A->GetVertexPositions()[Id]=FVector3f(Pt);auto VI=D.CreateVertexInstance(Id);A->GetVertexInstanceNormals()[VI]=FVector3f(VN);A->GetVertexInstanceTangents()[VI]=FVector3f(VT);A->GetVertexInstanceBinormalSigns()[VI]=1;A->GetVertexInstanceUVs().Set(VI,0,FVector2f(FVector::DotProduct(Pt,U)/100,FVector::DotProduct(Pt,V)/100));A->GetVertexInstanceColors()[VI]=FVector4f(1,1,1,1);Corners.Add(VI);}
            D.CreatePolygon(Groups[Mat],Corners);
        }
        void OutFace(TArray<FVector> P,FVector Center,int32 Mat)
        {
            if(FVector::DotProduct(FVector::CrossProduct(P[1]-P[0],P[2]-P[0]),P[0]-Center)<0)Algo::Reverse(P);Face(P,Mat);
        }
        void Box(FVector P,FVector S,int32 Mat,float B=2)
        {
            FVector H=S*.5;B=FMath::Min(B,FMath::Min3(H.X,H.Y,H.Z)*.7);const FVector E=H-FVector(B);
            // Six inset faces, twelve chamfer strips and eight triangular corners.
            for(int32 Axis=0;Axis<3;++Axis)for(int32 Sign:{-1,1})
            {int32 J=(Axis+1)%3,K=(Axis+2)%3;TArray<FVector> Q;for(auto Pair:TArray<FIntPoint>{{-1,-1},{1,-1},{1,1},{-1,1}}){FVector V=P;V[Axis]+=Sign*H[Axis];V[J]+=Pair.X*E[J];V[K]+=Pair.Y*E[K];Q.Add(V);}OutFace(Q,P,Mat);}
            for(int32 Axis=0;Axis<3;++Axis)for(int32 SJ:{-1,1})for(int32 SK:{-1,1})
            {int32 J=(Axis+1)%3,K=(Axis+2)%3;FVector V0=P,V1=P,V2=P,V3=P;V0[Axis]-=E[Axis];V1[Axis]+=E[Axis];V2[Axis]+=E[Axis];V3[Axis]-=E[Axis];V0[J]+=SJ*H[J];V1[J]+=SJ*H[J];V2[J]+=SJ*E[J];V3[J]+=SJ*E[J];V0[K]+=SK*E[K];V1[K]+=SK*E[K];V2[K]+=SK*H[K];V3[K]+=SK*H[K];OutFace({V0,V1,V2,V3},P,Mat);}
            for(int32 X:{-1,1})for(int32 Y:{-1,1})for(int32 Z:{-1,1})OutFace({P+FVector(X*H.X,Y*E.Y,Z*E.Z),P+FVector(X*E.X,Y*H.Y,Z*E.Z),P+FVector(X*E.X,Y*E.Y,Z*H.Z)},P,Mat);
        }
        void Cylinder(FVector P,float Radius,float Length,int32 Mat,FVector Axis=FVector::UpVector,int32 Segments=24,float RadiusTop=-1)
        {
            if(RadiusTop<0)RadiusTop=Radius;const FQuat Q=FQuat::FindBetweenNormals(FVector::UpVector,Axis.GetSafeNormal());
            for(int32 I=0;I<Segments;++I)
            {const float A0=I*2*PI/Segments,A1=(I+1)*2*PI/Segments;auto Pt=[&](float A,float R,float Z){return P+Q.RotateVector(FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,Z));};
            FVector B0=Pt(A0,Radius,-Length*.5),B1=Pt(A1,Radius,-Length*.5),T0=Pt(A0,RadiusTop,Length*.5),T1=Pt(A1,RadiusTop,Length*.5);
            auto Normal=[&](float A){return Q.RotateVector(FVector(FMath::Cos(A),FMath::Sin(A),(Radius-RadiusTop)/Length).GetSafeNormal());};
            if(Segments>=16)Face({B0,B1,T1,T0},Mat,{Normal(A0),Normal(A1),Normal(A1),Normal(A0)});else Face({B0,B1,T1,T0},Mat);
            Face({P-Q.RotateVector(FVector(0,0,Length*.5)),B1,B0},Mat);Face({P+Q.RotateVector(FVector(0,0,Length*.5)),T0,T1},Mat);}
        }
        void Profile(const TArray<FVector2D>& Outline,float Back,float Front,int32 Mat)
        {
            FVector Center((Back+Front)*.5,0,0);for(auto P:Outline){Center.Y+=P.X/Outline.Num();Center.Z+=P.Y/Outline.Num();}
            TArray<FVector> A0,A1;for(auto P:Outline){A0.Add(FVector(Back,P.X,P.Y));A1.Add(FVector(Front,P.X,P.Y));}
            OutFace(A0,Center,Mat);OutFace(A1,Center,Mat);
            for(int32 I=0;I<Outline.Num();++I){int32 J=(I+1)%Outline.Num();OutFace({A0[I],A0[J],A1[J],A1[I]},Center,Mat);}
        }
        void Beam(FVector P,FVector Q,float R,int32 Mat){Cylinder((P+Q)*.5,R,(Q-P).Size(),Mat,Q-P,8);}
        void Screen(float X,float Y,float Z,float Width,float Height)
        {Face({FVector(X,Y-Width/2,Z-Height/2),FVector(X,Y+Width/2,Z-Height/2),FVector(X,Y+Width/2,Z+Height/2),FVector(X,Y-Width/2,Z+Height/2)},Mesh);}
        void Bolts(FVector P,FVector Extent)
        {for(int32 Y:{-1,1})for(int32 Z:{-1,1})Cylinder(P+FVector(0,Y*Extent.Y,Z*Extent.Z),3,3,Steel,FVector::ForwardVector,6);}
        bool Finish(const TCHAR* Name)
        {
            const FString Path=FString(TEXT("/TrapperFreightElevators/Art/Ironworks/SM_IW_"))+Name;
            auto* S=LoadObject<UStaticMesh>(nullptr,*(Path+TEXT(".")+FPackageName::GetShortName(Path)),nullptr,LOAD_NoWarn);
            if(!S){S=NewObject<UStaticMesh>(CreatePackage(*Path),*FPackageName::GetShortName(Path),RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(S);}
            S->GetStaticMaterials().Reset();for(int32 I=0;I<Count;++I){const FName Slot(*FString::Printf(TEXT("IW_%d"),I));S->GetStaticMaterials().Add(FStaticMaterial(Materials[I],Slot,Slot));A->GetPolygonGroupMaterialSlotNames()[Groups[I]]=Slot;}
            if(S->GetNumSourceModels()==0)S->AddSourceModel();auto& Settings=S->GetSourceModel(0).BuildSettings;Settings.bRecomputeNormals=false;Settings.bRecomputeTangents=false;Settings.bGenerateLightmapUVs=false;Settings.bUseFullPrecisionUVs=true;
            S->CreateMeshDescription(0,MoveTemp(D));S->CommitMeshDescription(0);S->Build(false);S->CreateBodySetup();S->GetBodySetup()->CollisionTraceFlag=CTF_UseComplexAsSimple;S->SetLightingGuid();S->PostEditChange();
            UE_LOG(LogTemp,Display,TEXT("Ironworks mesh %s: %d triangles"),Name,S->GetNumTriangles(0));return Save(S);
        }
    };
    void Pillars(FMesh& M,float Height,bool Feet=false)
    {
        for(int32 X:{-1,1})for(int32 Y:{-1,1})
        {
            FVector P(X*350,Y*350,Height/2);M.Box(P,FVector(86,86,Height),Dark,12);
            M.Box(P+FVector(X*40,0,0),FVector(8,52,Height-120),Paint,3);
            M.Box(P+FVector(0,Y*40,0),FVector(52,8,Height-120),Paint,3);
            M.Box(P+FVector(X*44,0,0),FVector(4,18,Height-160),Steel,1);M.Box(P+FVector(0,Y*44,0),FVector(18,4,Height-160),Steel,1);
            for(float Z:{20.f,Height-20}){M.Box(FVector(X*350,Y*350,Z),FVector(92,92,38),Paint,6);for(int32 S:{-1,1}){M.Cylinder(FVector(X*397,Y*(350+S*25),Z),4,3,Steel,FVector::ForwardVector,6);M.Cylinder(FVector(X*(350+S*25),Y*397,Z),4,3,Steel,FVector::RightVector,6);}}
            if(Feet){M.Box(FVector(X*350,Y*350,17),FVector(98,98,34),Steel,5);M.Box(FVector(X*350,Y*350,65),FVector(90,90,95),Dark,10);}
        }
        for(int32 Side=0;Side<4;++Side)
        {M.Transform=FTransform(FRotator(0,Side*90,0));M.Box(FVector(350,0,Height-12),FVector(60,620,24),Steel,3);M.Box(FVector(315,-280,Height/2),FVector(16,20,Height),Steel,1);M.Box(FVector(315,280,Height/2),FVector(16,20,Height),Steel,1);M.Transform=FTransform::Identity;}
    }
}

bool TFGenerateIronworksAssets()
{
    using namespace Ironworks;bool OK=true;auto* Texture=Surface(TEXT("T_IW_Brushed"),0);auto* Grate=Surface(TEXT("T_IW_ExpandedMesh"),1);auto* Stripes=Surface(TEXT("T_IW_Hazard"),2);
    Materials={Material(TEXT("M_IW_Paint"),FLinearColor(.58,.165,.023),.32,.38,Texture),Material(TEXT("M_IW_Steel"),FLinearColor(.25,.29,.31),.8,.3,Texture),Material(TEXT("M_IW_Dark"),FLinearColor(.055,.068,.078),.55,.43,Texture),Material(TEXT("M_IW_Mesh"),FLinearColor(.1,.12,.13),.8,.45,Grate,true),Material(TEXT("M_IW_Hazard"),FLinearColor::White,.25,.55,Stripes),Material(TEXT("M_IW_Amber"),FLinearColor(1.8,.95,.32),.1,.25,nullptr,false,true),Material(TEXT("M_IW_Cable"),FLinearColor(.09,.1,.11),.85,.4,Texture)};
    {FMesh M;Pillars(M,400);OK&=M.Finish(TEXT("Shaft"));}
    {FMesh M;for(int32 S=0;S<4;++S){M.Transform=FTransform(FRotator(0,S*90,0));M.Screen(352,0,100,590,180);M.Box(FVector(352,0,8),FVector(12,608,16),Dark);M.Box(FVector(352,0,192),FVector(12,608,16),Dark);for(int32 Y:{-1,1})M.Box(FVector(352,Y*299,100),FVector(12,12,180),Dark);}OK&=M.Finish(TEXT("Screen"));}
    {FMesh M;Pillars(M,800,true);for(int32 S=0;S<4;++S){M.Transform=FTransform(FRotator(0,S*90,0));M.Box(FVector(347,0,35),FVector(90,604,60),Steel,7);M.Box(FVector(380,0,80),FVector(20,560,60),Hazard,3);M.Box(FVector(382,-215,275),FVector(18,130,150),Dark,8);M.Box(FVector(393,-215,290),FVector(5,100,85),Steel,3);M.Box(FVector(397,-215,350),FVector(5,90,10),Amber,1);M.Transform=FTransform::Identity;}OK&=M.Finish(TEXT("Base"));}
    {FMesh M;for(int32 S=0;S<4;++S){M.Transform=FTransform(FRotator(0,S*90,0));M.Box(FVector(355,0,22),FVector(86,620,44),Paint,6);M.Box(FVector(385,0,50),FVector(24,560,30),Steel,3);M.Box(FVector(399,0,62),FVector(2,210,25),Hazard,1);for(int32 Y:{-1,1}){M.Box(FVector(355,Y*210,300),FVector(62,55,560),Dark,9);M.Box(FVector(388,Y*210,300),FVector(6,24,410),Paint);M.Box(FVector(393,Y*210,420),FVector(5,13,65),Amber,1);M.Bolts(FVector(391,Y*210,300),FVector(0,12,200));}M.Box(FVector(355,0,580),FVector(70,390,40),Paint,9);
        for(int32 Sign:{-1,1})M.Profile({{Sign*237.f,475},{Sign*237.f,560},{Sign*195.f,600},{Sign*155.f,600},{Sign*190.f,545},{Sign*190.f,475}},326,389,Paint);M.Box(FVector(390,0,575),FVector(8,150,12),Amber,1);for(int32 Y:{-1,1}){M.Box(FVector(352,Y*283,310),FVector(36,70,520),Dark,5);M.Screen(372,Y*283,310,55,480);}M.Transform=FTransform::Identity;}OK&=M.Finish(TEXT("Stop"));}
    {FMesh M;M.Box(FVector(0,0,25),FVector(560,560,50),Steel,8);M.Box(FVector(0,0,578),FVector(560,560,44),Paint,9);M.Box(FVector(0,0,552),FVector(500,500,10),Dark,3);for(int32 X:{-1,1})for(int32 Y:{-1,1}){M.Box(FVector(X*259,Y*259,300),FVector(42,42,520),Paint,7);M.Box(FVector(X*282,Y*259,300),FVector(5,27,450),Steel,2);M.Box(FVector(X*259,Y*282,300),FVector(27,5,450),Steel,2);}for(int32 S=0;S<4;++S){M.Transform=FTransform(FRotator(0,S*90,0));for(int32 Y:{-1,1}){M.Box(FVector(275,Y*188,300),FVector(15,100,480),Dark,2);M.Screen(284,Y*188,300,92,450);M.Box(FVector(287,Y*235,470),FVector(6,12,70),Amber,1);}M.Box(FVector(279,0,58),FVector(10,470,30),Hazard,2);M.Box(FVector(277,0,535),FVector(18,480,18),Steel,2);M.Transform=FTransform::Identity;}OK&=M.Finish(TEXT("Cabin"));}
    // Stationary storage has a shaped armored shell and a real recessed belt throat.
    {FMesh M;for(int32 S=0;S<4;++S){M.Transform=FTransform(FRotator(0,S*90,0));
        M.Profile({{-155,40},{155,40},{155,205},{125,245},{-125,245},{-155,205}},302,385,Dark);
        for(int32 Sign:{-1,1}){
            TArray<FVector2D> Side={{Sign*92.f,48},{Sign*150.f,48},{Sign*150.f,200},{Sign*124.f,235},{Sign*92.f,215}};
            M.Profile(Side,380,397,Paint);
            M.Bolts(FVector(397,Sign*120,135),FVector(0,13,62));
        }
        M.Profile({{-90,153},{90,153},{122,235},{-122,235}},384,399,Steel);
        M.Box(FVector(399,0,211),FVector(2,180,13),Hazard,1);
        M.Box(FVector(399,0,173),FVector(2,135,6),Amber,1);
        M.Box(FVector(397,0,49),FVector(4,180,14),Steel,2);
        M.Box(FVector(388,0,100),FVector(2,166,94),Dark,1);
        for(int32 Y=-3;Y<=3;++Y)M.Box(FVector(344,Y*31,247),FVector(70,9,4),Steel,1);
    }OK&=M.Finish(TEXT("Buffer"));}
    // Fill gauge origin is the bottom of its scale; runtime changes its height.
    {FMesh M;M.Box(FVector(93,0,75),FVector(4,11,150),Amber,1);OK&=M.Finish(TEXT("Level"));}
    // A shared half gate. Its mate is mirrored in Y; doors retreat into the cage side pockets.
    {FMesh M;M.Screen(282,-55,300,106,450);M.Box(FVector(283,-108,300),FVector(12,8,470),Steel,1);M.Box(FVector(283,-2,300),FVector(12,8,470),Paint,1);for(float Z:{68.f,300.f,532.f})M.Box(FVector(283,-55,Z),FVector(12,110,10),Steel,1);M.Box(FVector(291,-14,300),FVector(8,8,45),Steel,2);OK&=M.Finish(TEXT("Gate"));}
    {FMesh M;for(int32 Y:{-1,1})for(int32 Z:{0,1}){FVector P(0,Y*112,140+Z*230);M.Box(P,FVector(352,202,202),Dark,12);for(int32 X:{-1,1})M.Box(P+FVector(X*172,0,0),FVector(12,192,186),Steel,3);for(int32 S:{-1,1}){M.Box(P+FVector(0,S*100,0),FVector(330,8,165),Steel,3);M.Box(P+FVector(0,S*105,0),FVector(260,4,110),Dark,2);M.Box(P+FVector(0,S*109,55),FVector(160,3,8),Paint,1);}for(int32 X:{-1,1})M.Box(P+FVector(X*80,0,104),FVector(14,202,8),Paint,1);}OK&=M.Finish(TEXT("Cargo"));}
    {FMesh M;M.Cylinder(FVector(0,0,270),85,330,Steel,FVector::UpVector,32);M.Cylinder(FVector(0,0,92),51,28,Steel,FVector::UpVector,32,85);M.Cylinder(FVector(0,0,449),85,28,Steel,FVector::UpVector,32,51);for(float Z:{120.f,420.f}){M.Cylinder(FVector(0,0,Z),89,12,Dark,FVector::UpVector,32);M.Cylinder(FVector(0,0,Z+8),90,4,Steel,FVector::UpVector,32);}M.Cylinder(FVector(0,0,477),28,25,Steel);M.Cylinder(FVector(0,0,493),35,8,Paint);M.Box(FVector(86,0,385),FVector(7,27,175),Dark,3);M.Box(FVector(91,0,385),FVector(4,11,150),Dark,1);for(int32 X:{-1,1})M.Box(FVector(X*56,0,75),FVector(16,110,55),Dark,3);M.Beam(FVector(0,0,500),FVector(0,65,500),9,Steel);M.Beam(FVector(0,65,500),FVector(0,65,430),9,Steel);OK&=M.Finish(TEXT("Tank"));}
    {FMesh M;for(int32 S=0;S<4;++S){M.Transform=FTransform(FRotator(0,S*90,0));
        M.Profile({{-155,40},{155,40},{155,205},{125,245},{-125,245},{-155,205}},302,387,Dark);
        for(int32 Y:{-1,1}){M.Box(FVector(393,Y*125,140),FVector(10,45,165),Paint,7);M.Bolts(FVector(399,Y*125,140),FVector(0,13,60));}
        M.Box(FVector(394,0,215),FVector(8,185,18),Steel,3);
        M.Box(FVector(399,0,215),FVector(2,160,8),Amber,1);
    }OK&=M.Finish(TEXT("FluidBuffer"));}
    // Open annular pipe socket: front rim, internal bore and a recessed dark back.
    {FMesh M;for(int32 S=0;S<4;++S){M.Transform=FTransform(FRotator(0,S*90,0));
        constexpr int32 N=48;const float Front=412,Back=388,Outer=52,Inner=33;
        for(int32 I=0;I<N;++I){float A0=I*2*PI/N,A1=(I+1)*2*PI/N;
            auto P=[](float X,float R,float A){return FVector(X,R*FMath::Cos(A),100+R*FMath::Sin(A));};
            FVector N0(0,FMath::Cos(A0),FMath::Sin(A0)),N1(0,FMath::Cos(A1),FMath::Sin(A1));
            M.Face({P(Front,Outer,A0),P(Front,Outer,A1),P(Front,Inner,A1),P(Front,Inner,A0)},Steel);
            M.Face({P(Back,Outer,A0),P(Back,Outer,A1),P(Front,Outer,A1),P(Front,Outer,A0)},Paint,{N0,N1,N1,N0});
            M.Face({P(Back,Inner,A0),P(Front,Inner,A0),P(Front,Inner,A1),P(Back,Inner,A1)},Dark,{-N0,-N0,-N1,-N1});
            M.Face({P(Back,Outer,A1),P(Back,Outer,A0),P(Back,Inner,A0),P(Back,Inner,A1)},Steel);
        }
        M.Cylinder(FVector(388,0,100),32,1,Dark,FVector::ForwardVector,48);
        for(int32 I=0;I<8;++I){float A=I*PI/4;M.Cylinder(FVector(414,FMath::Cos(A)*43,100+FMath::Sin(A)*43),3,5,Dark,FVector::ForwardVector,6);}
    }OK&=M.Finish(TEXT("Flanges"));}
    {FMesh M;for(int32 S=0;S<4;++S){M.Transform=FTransform(FRotator(0,S*90,0));
        M.Box(FVector(397,0,282),FVector(10,210,66),Dark,4);
        M.Box(FVector(403,-66,282),FVector(2,55,56),Paint,1);
        M.Box(FVector(403,32,282),FVector(2,130,56),Steel,1);
    }OK&=M.Finish(TEXT("PortSigns"));}
    {FMesh M;
        // A perimeter service deck leaves the cable path and center winch open.
        for(int32 S=0;S<4;++S){M.Transform=FTransform(FRotator(0,S*90,0));M.Box(FVector(335,0,10),FVector(120,790,20),Steel,4);}M.Transform=FTransform::Identity;
        for(int32 X:{-1,1}){M.Transform=FTransform(FRotator(0,X<0?180:0,0));
            M.Profile({{-240,30},{240,30},{240,130},{185,167},{-185,167},{-240,130}},204,366,Paint);
            M.Box(FVector(370,0,97),FVector(8,350,80),Dark,8);M.Bolts(FVector(375,0,97),FVector(0,145,25));M.Transform=FTransform::Identity;M.Box(FVector(X*285,0,165),FVector(140,420,12),Dark,3);for(int32 J=-4;J<=4;++J)M.Box(FVector(X*285,J*36,178),FVector(125,14,8),Steel,1);M.Cylinder(FVector(X*185,0,95),60,40,Steel,FVector::ForwardVector,32);}
        for(int32 Y:{-1,1}){M.Cylinder(FVector(0,Y*165,95),45,80,Steel,FVector::ForwardVector,32);M.Cylinder(FVector(0,Y*165,95),43,84,Cable,FVector::ForwardVector,32);}
        for(int32 S=0;S<4;++S){M.Transform=FTransform(FRotator(0,S*90,0));for(int32 Y:{-1,0,1})M.Beam(FVector(370,Y*300,25),FVector(370,Y*300,175),4,Paint);M.Beam(FVector(370,-300,175),FVector(370,300,175),4,Paint);M.Box(FVector(381,0,25),FVector(5,180,18),Hazard,1);}OK&=M.Finish(TEXT("Head"));}
    {FMesh M;M.Cylinder(FVector(0,0,0),50,320,Dark,FVector::ForwardVector,32);for(int32 X:{-1,1})M.Cylinder(FVector(X*166,0,0),68,14,Steel,FVector::ForwardVector,32);for(int32 J=-15;J<=15;++J)M.Cylinder(FVector(J*10,0,0),55,5,Cable,FVector::ForwardVector,32);OK&=M.Finish(TEXT("Drum"));}
    {FMesh M;for(int32 Y:{-1,1})M.Cylinder(FVector(0,Y*210,50),2.5,100,Cable,FVector::UpVector,8);OK&=M.Finish(TEXT("Cables"));}
    OK&=LabelMaterial();Materials.Reset();return OK;
}
