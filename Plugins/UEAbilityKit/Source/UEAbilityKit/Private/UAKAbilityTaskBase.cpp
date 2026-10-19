#include "UAKAbilityTaskBase.h"

#include "UAKAbilityAsset.h"

#define LOCTEXT_NAMESPACE "UAKAbilityTaskBase"

void UAKAbilityTaskBase::Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const
{
	// 通用规则：时间区间必须落在时间轴内
	if (Asset && GetEndTime() > Asset->Length + KINDA_SMALL_NUMBER)
	{
		OutResult.Add(EAKValidationSeverity::Warning,
			FText::Format(LOCTEXT("TaskOverflow", "任务结束时间 {0}s 超出了技能长度 {1}s。"),
				FText::AsNumber(GetEndTime()), FText::AsNumber(Asset->Length)),
			const_cast<UAKAbilityTaskBase*>(this));
	}

	// 通用规则：声明了 Tick 却是单帧任务，属于配置矛盾
	if (bNeedsTick && IsSingleFrame())
	{
		OutResult.Add(EAKValidationSeverity::Warning,
			LOCTEXT("TickOnSingleFrame", "该任务勾选了 NeedsTick，但持续时间为 0（单帧任务不会收到 Tick）。"),
			const_cast<UAKAbilityTaskBase*>(this));
	}
}

#if WITH_EDITOR
namespace
{
	struct FAKTaskMeta
	{
		FText Category;
		FText DisplayName;
		FText Description;
		FLinearColor Color = FLinearColor::White;
	};

	/**
	 * 内置任务的编辑器元数据（集中管理）。
	 *
	 * 为什么不放在各个子类里：这样一张表就能看到所有任务在编辑器里的呈现，
	 * 改文案/配色不用逐个文件翻。项目自己的任务既可以在这张表以外直接覆写虚函数，
	 * 也可以沿用默认的"去前缀类名 + General 分类"。
	 */
	const TMap<FName, FAKTaskMeta>& GetTaskMetaTable()
	{
		static const TMap<FName, FAKTaskMeta> Table =
		{
			{
				FName(TEXT("UAKAbilityTask_ApplyGameplayEffect")),
				{ LOCTEXT("Cat_Effect", "Effect"), LOCTEXT("Name_ApplyGE", "施加 GE"),
				  LOCTEXT("Desc_ApplyGE", "对目标施加 GameplayEffect（伤害 / 治疗 / Buff / 护盾）"),
				  FLinearColor(0.95f, 0.35f, 0.35f) }
			},
			{
				FName(TEXT("UAKAbilityTask_PlayMontage")),
				{ LOCTEXT("Cat_Presentation", "Presentation"), LOCTEXT("Name_Montage", "播放蒙太奇"),
				  LOCTEXT("Desc_Montage", "通过 ASC 播放 Montage，蒙太奇进入 GAS 的管理范围"),
				  FLinearColor(0.40f, 0.80f, 0.95f) }
			},
			{
				FName(TEXT("UAKAbilityTask_SendGameplayCue")),
				{ LOCTEXT("Cat_Presentation", "Presentation"), LOCTEXT("Name_Cue", "发送 Cue"),
				  LOCTEXT("Desc_Cue", "触发一次性或持续型表现，逻辑与表现严格分离"),
				  FLinearColor(0.40f, 0.80f, 0.95f) }
			},
			{
				FName(TEXT("UAKAbilityTask_CollisionQuery")),
				{ LOCTEXT("Cat_Query", "Query"), LOCTEXT("Name_Query", "形状查询"),
				  LOCTEXT("Desc_Query", "查询球 / 盒 / 胶囊 / 锥形范围内的目标并写入上下文"),
				  FLinearColor(0.55f, 0.85f, 0.45f) }
			},
			{
				FName(TEXT("UAKAbilityTask_AreaEffect")),
				{ LOCTEXT("Cat_Area", "Area"), LOCTEXT("Name_Area", "区域效果"),
				  LOCTEXT("Desc_Area", "在持续时间内周期性对区域内目标施加 GE"),
				  FLinearColor(0.30f, 0.80f, 0.70f) }
			},
			{
				FName(TEXT("UAKAbilityTask_SpawnProjectile")),
				{ LOCTEXT("Cat_Projectile", "Projectile"), LOCTEXT("Name_Projectile", "生成投射物"),
				  LOCTEXT("Desc_Projectile", "服务端权威生成投射物，命中时施加 GE 与 Cue"),
				  FLinearColor(1.00f, 0.40f, 0.70f) }
			},
			{
				FName(TEXT("UAKAbilityTask_Dash")),
				{ LOCTEXT("Cat_Motion", "Motion"), LOCTEXT("Name_Dash", "冲刺位移"),
				  LOCTEXT("Desc_Dash", "用 LaunchCharacter 施加一次性速度，移动交给引擎"),
				  FLinearColor(0.95f, 0.75f, 0.30f) }
			},
			{
				FName(TEXT("UAKAbilityTask_Branch")),
				{ LOCTEXT("Cat_Rule", "Rule"), LOCTEXT("Name_Branch", "条件分支"),
				  LOCTEXT("Desc_Branch", "按 GAS 的 Tag / 属性条件开关另一条轨道"),
				  FLinearColor(0.75f, 0.50f, 0.95f) }
			},
			{
				FName(TEXT("UAKAbilityTask_WaitUntil")),
				{ LOCTEXT("Cat_Rule", "Rule"), LOCTEXT("Name_Wait", "等待条件"),
				  LOCTEXT("Desc_Wait", "条件满足即继续，可冻结时间轴推进"),
				  FLinearColor(0.75f, 0.50f, 0.95f) }
			}
		};
		return Table;
	}

	const FAKTaskMeta* FindTaskMeta(const UClass* Class)
	{
		// 沿继承链向上找，这样蓝图派生任务也能继承内置元数据
		for (const UClass* Current = Class; Current; Current = Current->GetSuperClass())
		{
			if (const FAKTaskMeta* Found = GetTaskMetaTable().Find(Current->GetFName()))
			{
				return Found;
			}
		}
		return nullptr;
	}
}

FText UAKAbilityTaskBase::GetTaskCategory() const
{
	if (const FAKTaskMeta* Meta = FindTaskMeta(GetClass()))
	{
		return Meta->Category;
	}
	return LOCTEXT("Category_General", "General");
}

FText UAKAbilityTaskBase::GetTaskDisplayName() const
{
	if (const FAKTaskMeta* Meta = FindTaskMeta(GetClass()))
	{
		return Meta->DisplayName;
	}
	return FText::FromString(GetClass()->GetName().Replace(TEXT("UAKAbilityTask_"), TEXT("")));
}

FText UAKAbilityTaskBase::GetTaskDescription() const
{
	if (const FAKTaskMeta* Meta = FindTaskMeta(GetClass()))
	{
		return Meta->Description;
	}
	return FText::GetEmpty();
}

FLinearColor UAKAbilityTaskBase::GetTaskColor() const
{
	if (const FAKTaskMeta* Meta = FindTaskMeta(GetClass()))
	{
		return Meta->Color;
	}
	return FLinearColor(0.60f, 0.60f, 0.65f);
}
#endif

void UAKAbilityTaskBase::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	OutJson->SetStringField(TEXT("$type"), GetClass()->GetName());

	if (!TaskId.IsNone())
	{
		OutJson->SetStringField(TEXT("taskId"), TaskId.ToString());
	}
	OutJson->SetNumberField(TEXT("startTime"), StartTime);
	OutJson->SetNumberField(TEXT("duration"), Duration);
	OutJson->SetBoolField(TEXT("needsTick"), bNeedsTick);
	OutJson->SetStringField(TEXT("netMode"), StaticEnum<EAKTaskNetMode>()->GetNameStringByValue(static_cast<int64>(NetMode)));

	if (!Note.IsEmpty())
	{
		OutJson->SetStringField(TEXT("note"), Note.ToString());
	}
}

void UAKAbilityTaskBase::ImportJson(const FJsonObject& InJson)
{
	FString ImportedTaskId;
	if (InJson.TryGetStringField(TEXT("taskId"), ImportedTaskId) && !ImportedTaskId.IsEmpty())
	{
		TaskId = FName(*ImportedTaskId);
	}

	double OutStart = StartTime;
	if (InJson.TryGetNumberField(TEXT("startTime"), OutStart))
	{
		StartTime = static_cast<float>(OutStart);
	}

	double OutDuration = Duration;
	if (InJson.TryGetNumberField(TEXT("duration"), OutDuration))
	{
		Duration = static_cast<float>(OutDuration);
	}

	bool bOutTick = bNeedsTick;
	if (InJson.TryGetBoolField(TEXT("needsTick"), bOutTick))
	{
		bNeedsTick = bOutTick;
	}

	FString NetModeName;
	if (InJson.TryGetStringField(TEXT("netMode"), NetModeName))
	{
		if (const UEnum* Enum = StaticEnum<EAKTaskNetMode>())
		{
			NetMode = static_cast<EAKTaskNetMode>(Enum->GetValueByNameString(NetModeName));
		}
	}

	FString NoteText;
	if (InJson.TryGetStringField(TEXT("note"), NoteText))
	{
		Note = FText::FromString(NoteText);
	}
}

#undef LOCTEXT_NAMESPACE
