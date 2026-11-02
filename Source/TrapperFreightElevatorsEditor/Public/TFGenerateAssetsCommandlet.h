#pragma once
#include "Commandlets/Commandlet.h"
#include "TFGenerateAssetsCommandlet.generated.h"
UCLASS() class UTFGenerateAssetsCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UTFGenerateAssetsCommandlet();
    virtual int32 Main(const FString& Params) override;
};
