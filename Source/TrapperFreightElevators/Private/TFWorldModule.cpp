#include "TFWorldModule.h"
void TFStartQA(UWorld* World);
void UTFWorldModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
    Super::DispatchLifecycleEvent(Phase);
    if(Phase==ELifecyclePhase::POST_INITIALIZATION) TFStartQA(GetWorld());
}
