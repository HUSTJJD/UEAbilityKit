#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UAKAbilityJsonSerializer.generated.h"

class UAKAbilityAsset;
class UAKAbilityTaskBase;
class UAKAbilityTrack;

/**
 * 资产 ↔ Json 双向序列化（AbilityKit"可读配置可回导"思想的落地）。
 *
 * 要点：
 * - 只导出逻辑相关字段；Preview* 等 WITH_EDITORONLY_DATA 字段不导出。
 * - 每个任务用自己的 ExportJson / ImportJson 写强类型字段（而不是把属性表 dump 出来），
 *   因此字段名稳定、适合 review 与 diff。
 * - 资产带 version，导入时按版本迁移；任务用 $type 定位类。
 */
UCLASS()
class UEABILITYKIT_API UAKAbilityJsonSerializer : public UObject
{
	GENERATED_BODY()

public:
	static bool ExportToJson(const UAKAbilityAsset* Asset, FString& OutJsonString);
	static bool ImportFromJson(UAKAbilityAsset* Asset, const FString& JsonString);

	static bool ExportToFile(const UAKAbilityAsset* Asset, const FString& FilePath);
	static bool ImportFromFile(UAKAbilityAsset* Asset, const FString& FilePath);

	/** 由 $type 名字创建任务实例（先查本模块，再尝试蓝图类） */
	static UAKAbilityTaskBase* CreateTaskByTypeName(UObject* Outer, const FString& TypeName);
};
