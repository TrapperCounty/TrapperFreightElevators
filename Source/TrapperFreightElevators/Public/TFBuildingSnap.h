#pragma once
#include "CoreMinimal.h"

// Local shaft coordinates, centimetres. Straight walls and flat foundations only.
struct FTFBuildingSnap
{
    static bool Solve(const FTransform& Tower, float Top, const TArray<float>& Landings,
        FVector Hit, FVector Normal, bool Wall, FVector Size, float Rotation, FTransform& Out)
    {
        if(Hit.ContainsNaN()||Normal.ContainsNaN()||Size.ContainsNaN()||!FMath::IsFinite(Top)||Top<800||
            Size.X<=0||Size.Y<=0||Size.Z<=0||!FMath::IsFinite(Rotation))return false;
        const FVector P=Tower.InverseTransformPosition(Hit);
        const FVector N=Tower.InverseTransformVectorNoScale(Normal);
        if(FMath::Abs(P.X)>470||FMath::Abs(P.Y)>470||P.Z<0||P.Z>Top)return false;
        // Prefer the struck face, with the hit point breaking ties on top/frame edges.
        const FVector Side=N.SizeSquared2D()>.25?N:P;
        // A rail's inner surface may point inward; its location still identifies the outer face.
        const FVector Face=FMath::Abs(Side.X)>=FMath::Abs(Side.Y)?FVector(P.X>=0?1:-1,0,0):FVector(0,P.Y>=0?1:-1,0);
        const float Z=FMath::GridSnap(static_cast<float>(P.Z),100.f);
        if(Z<0||Z>Top||(Wall&&Z+Size.Z>Top))return false;
        if(Wall&&Z<800)return false; // Keep the drive's machinery and control panel accessible.
        for(float Landing:Landings)
        {
            if(Wall ? (Z<Landing+600-.1f&&Z+Size.Z>Landing+.1f) : (Z>Landing+.1f&&Z<Landing+600-.1f))return false;
        }
        const float Yaw=Wall?FMath::RadiansToDegrees(FMath::Atan2(Face.Y,Face.X))-90.f+FMath::GridSnap(Rotation,180.f):FMath::GridSnap(Rotation,90.f);
        const FQuat LocalRotation=FRotator(0,Yaw,0).Quaternion();
        float Radius=0;
        if(!Wall)
        {
            const FVector X=LocalRotation.GetAxisX(),Y=LocalRotation.GetAxisY();
            Radius=FMath::Abs(FVector::DotProduct(Face,X))*Size.X*.5f+FMath::Abs(FVector::DotProduct(Face,Y))*Size.Y*.5f;
        }
        const FVector Location=Face*(400+Radius)+FVector(0,0,Z-(Wall?0:Size.Z*.5f));
        Out=FTransform(Tower.GetRotation()*LocalRotation,Tower.TransformPosition(Location));
        return true;
    }
};

class AFGBuildableHologram;
struct FHitResult;
bool TFTryBuildingSnap(AFGBuildableHologram* Hologram,const FHitResult& Hit);
void TFInstallBuildingSnaps();
void TFRemoveBuildingSnaps();
