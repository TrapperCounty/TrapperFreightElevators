#include "Modules/ModuleManager.h"
#include "TFBuildingSnap.h"
class FTrapperFreightElevatorsModule : public IModuleInterface
{
public:
    virtual void StartupModule() override { TFInstallBuildingSnaps(); }
    virtual void ShutdownModule() override { TFRemoveBuildingSnaps(); }
};
IMPLEMENT_MODULE(FTrapperFreightElevatorsModule, TrapperFreightElevators)
