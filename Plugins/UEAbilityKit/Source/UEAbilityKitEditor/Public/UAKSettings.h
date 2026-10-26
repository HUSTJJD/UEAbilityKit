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
	 * 留空时回退到项目设置的默认 GameMode 的 DefaultPawnClass —— Lyra 宿主下那是
	 * ALyraCharacter（C++ 基类，一般没配网格，看得见胶囊看不见人）。
	 * 想预览真正会出场的那只角色，把它的蓝图填在这里：Lyra 下就是 PawnData
	 * （例如 HeroData_ShooterGame）里的 PawnClass。
	 */
	UPROPERTY(config, EditAnywhere, Category = "Preview", meta = (MetaClass = "/Script/Engine.Actor"))
	FSoftClassPath DefaultPreviewCharacterClass;
};
