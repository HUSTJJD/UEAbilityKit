#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"
#include "UAKSettings.generated.h"

/**
 * UAK 的编辑器设置（Config/DefaultUEAbilityKit.ini）。
 *
 * 这里只放"每个工程只需决定一次、且工程之间必然不同"的东西，典型例子就是预览角色：
 * 插件既不该猜宿主用哪只角色，也不该拿自己随包分发的示例角色顶替 —— 那只会让人
 * 预览到一个根本不会出场的角色，骨骼与 Montage 还对不上。所以交给宿主显式配一次。
 */
UCLASS(config=UEAbilityKit, defaultconfig, meta = (DisplayName = "UE Ability Kit"))
class UEABILITYKITEDITOR_API UUAKSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * 技能编辑器预览用的角色类。
	 *
	 * 留空时回退到项目设置的默认 GameMode 的 DefaultPawnClass —— 那常常是 C++ 基类
	 * （一般没配网格，看得见胶囊看不见人）。
	 * 想预览真正会出场的那只角色，把它的蓝图填在这里。
	 */
	UPROPERTY(config, EditAnywhere, Category = "Preview", meta = (MetaClass = "/Script/Engine.Actor"))
	FSoftClassPath DefaultPreviewCharacterClass;

	/**
	 * 技能编辑器左栏（轨道列表）的宽度（像素）。
	 *
	 * 抄 Sequencer：左栏宽度是"编辑器状态"而不是资产内容，拖完要记住，
	 * 下次打开还是你习惯的那个宽度（`SSequencer.cpp:782-792` 就是这么存的）。
	 */
	UPROPERTY(config, EditAnywhere, Category = "Editor", meta = (ClampMin = "120.0", UIMin = "120.0", ClampMax = "520.0", UIMax = "520.0"))
	float TimelineOutlinerWidth = 220.f;

	/**
	 * 时间轴吸附总开关。
	 *
	 * 抄 Sequencer：吸附是"编辑器状态"而不是资产属性，关掉后可以自由拖动
	 * （Sequencer 工具栏上有 Snap 总开关 + 下拉，我们只做最有用的两档）。
	 */
	UPROPERTY(config, EditAnywhere, Category = "Editor")
	bool bTimelineSnapEnabled = true;

	/** 除按帧对齐外，是否再吸附到命名标记（Marker 是语义锚点，通常想吸） */
	UPROPERTY(config, EditAnywhere, Category = "Editor")
	bool bTimelineSnapToMarkers = true;
};
