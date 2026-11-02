using UnrealBuildTool;
public class TrapperFreightElevators : ModuleRules
{
    public TrapperFreightElevators(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "FactoryGame", "SML", "DummyHeaders", "InputCore", "PhysicsCore", "NetCore", "GameplayTags", "GameplayTasks", "AIModule", "NavigationSystem", "UMG", "SlateCore", "Slate", "AbstractInstance", "DeveloperSettings", "AssetRegistry", "Json", "JsonUtilities" });
    }
}
