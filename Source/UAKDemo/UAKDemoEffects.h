#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "UAKDemoEffects.generated.h"

class UGameplayEffect;

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Demo_Cooldown);

/**
 * 示例用的 GameplayEffect 全部写成 C++ 类。
 *
 * 为什么不用 .uasset：
 * - 仓库里不塞二进制，clone 下来就能跑（UAK 自己主张"配置可 diff"）；
 * - CI / 命令行里没有内容也能编译与验证；
 * - 冷却 Tag 用原生 Tag 注册，不依赖任何 .ini，避免"配了但没生效"。
 *
 * 真实项目里这些 GE 通常由策划在编辑器里配（那就是数据资产，可导出 Json 进 Git）；
 * 示例刻意走 C++ 路径，是为了让"最小可运行"不依赖任何手工点击。
 */

/** 近战伤害：Health -30 */
UCLASS()
class UAKDEMO_API UAKDemoGE_MeleeDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAKDemoGE_MeleeDamage();
};

/** 火球伤害：Health -25 */
UCLASS()
class UAKDEMO_API UAKDemoGE_FireballDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAKDemoGE_FireballDamage();
};

/** 冷却：5 秒，授予 Cooldown.Demo.Sample */
UCLASS()
class UAKDEMO_API UAKDemoGE_Cooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAKDemoGE_Cooldown();
};

/** 消耗：Mana -20 */
UCLASS()
class UAKDEMO_API UAKDemoGE_Cost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAKDemoGE_Cost();
};

/** 初始属性：把血蓝拉满（可选，填进角色的 DefaultAttributesEffect） */
UCLASS()
class UAKDEMO_API UAKDemoGE_DefaultAttributes : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAKDemoGE_DefaultAttributes();
};
