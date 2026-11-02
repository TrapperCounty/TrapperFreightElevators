using UnrealBuildTool;
public class TrapperFreightElevatorsEditor : ModuleRules
{
    public TrapperFreightElevatorsEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnrealEd", "Kismet", "KismetCompiler", "AssetRegistry", "SlateCore", "FactoryGame", "SML", "AbstractInstance", "TrapperFreightElevators", "MeshDescription", "StaticMeshDescription" });
    }
}
