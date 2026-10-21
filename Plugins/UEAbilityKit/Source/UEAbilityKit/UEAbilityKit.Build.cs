using UnrealBuildTool;

public class UEAbilityKit : ModuleRules
{
	public UEAbilityKit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"PhysicsCore",    // FCollisionShape（碰撞查询任务）
			"Json",           // Json 双向序列化
			"JsonUtilities",
			"UnrealClient"    // FScreenshotRequest：UAK.Auto.Record / Screenshot（进程外 e2e 的录屏）
		});
	}
}
