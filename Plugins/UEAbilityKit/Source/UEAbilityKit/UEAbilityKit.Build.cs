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
			"JsonUtilities"
			// 注意：UnrealClient.h（FScreenshotRequest）属于 Engine 模块，5.x 里没有
			// UnrealClient 这个模块了（UE4 遗留下来的依赖），写进来会报
			// "Could not find definition for module 'UnrealClient'"
		});
	}
}
