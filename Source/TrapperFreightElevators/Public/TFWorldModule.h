#pragma once
#include "Module/GameWorldModule.h"
#include "TFWorldModule.generated.h"
UCLASS() class TRAPPERFREIGHTELEVATORS_API UTFWorldModule : public UGameWorldModule
{
    GENERATED_BODY()
public: virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
};
