#include "UAKAbilityJsonSerializer.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"
#include "UAKChanneling.h"
#include "UAKTypes.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

namespace
{
	constexpr int32 CurrentSchemaVersion = 1;

	TSharedRef<FJsonObject> ExportTrack(const UAKAbilityTrack* Track)
	{
		TSharedRef<FJsonObject> TrackJson = MakeShared<FJsonObject>();
		TrackJson->SetStringField(TEXT("name"), Track->TrackName.ToString());
		TrackJson->SetStringField(TEXT("type"),
			StaticEnum<EAKTrackType>()->GetNameStringByValue(static_cast<int64>(Track->TrackType)));
		TrackJson->SetBoolField(TEXT("enabled"), Track->bEnabled);

		// 复合编排参数（Sequence / Parallel / Repeat）
		TrackJson->SetStringField(TEXT("compositeMode"),
			StaticEnum<EAKTrackCompositeMode>()->GetNameStringByValue(static_cast<int64>(Track->CompositeMode)));
		TrackJson->SetNumberField(TEXT("repeatCount"), Track->RepeatCount);
		TrackJson->SetNumberField(TEXT("repeatInterval"), Track->RepeatInterval);
		TrackJson->SetNumberField(TEXT("startDelay"), Track->StartDelay);

		TArray<TSharedPtr<FJsonValue>> TasksArray;
		for (const UAKAbilityTaskBase* Task : Track->Tasks)
		{
			if (!Task)
			{
				continue;
			}
			TSharedRef<FJsonObject> TaskJson = MakeShared<FJsonObject>();
			Task->ExportJson(TaskJson);
			TasksArray.Add(MakeShared<FJsonValueObject>(TaskJson));
		}
		TrackJson->SetArrayField(TEXT("tasks"), TasksArray);

		return TrackJson;
	}

	void ImportTrack(UAKAbilityTrack* Track, const FJsonObject& TrackJson)
	{
		FString NameStr;
		if (TrackJson.TryGetStringField(TEXT("name"), NameStr))
		{
			Track->TrackName = FName(*NameStr);
		}

		FString TypeStr;
		if (TrackJson.TryGetStringField(TEXT("type"), TypeStr))
		{
			if (const UEnum* Enum = StaticEnum<EAKTrackType>())
			{
				Track->TrackType = static_cast<EAKTrackType>(Enum->GetValueByNameString(TypeStr));
			}
		}

		TrackJson.TryGetBoolField(TEXT("enabled"), Track->bEnabled);

		// 复合编排参数
		FString CompositeName;
		if (TrackJson.TryGetStringField(TEXT("compositeMode"), CompositeName))
		{
			if (const UEnum* Enum = StaticEnum<EAKTrackCompositeMode>())
			{
				Track->CompositeMode = static_cast<EAKTrackCompositeMode>(Enum->GetValueByNameString(CompositeName));
			}
		}
		TrackJson.TryGetNumberField(TEXT("repeatCount"), Track->RepeatCount);
		TrackJson.TryGetNumberField(TEXT("repeatInterval"), Track->RepeatInterval);
		TrackJson.TryGetNumberField(TEXT("startDelay"), Track->StartDelay);

		const TArray<TSharedPtr<FJsonValue>>* TasksArray = nullptr;
		if (!TrackJson.TryGetArrayField(TEXT("tasks"), TasksArray) || !TasksArray)
		{
			return;
		}

		Track->Tasks.Reset();
		for (const TSharedPtr<FJsonValue>& Value : *TasksArray)
		{
			const TSharedPtr<FJsonObject>* TaskObject = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(TaskObject) || !TaskObject || !TaskObject->IsValid())
			{
				continue;
			}

			FString TypeName;
			(*TaskObject)->TryGetStringField(TEXT("$type"), TypeName);

			UAKAbilityTaskBase* Task = UAKAbilityJsonSerializer::CreateTaskByTypeName(Track, TypeName);
			if (!Task)
			{
				continue;
			}

			Task->ImportJson(**TaskObject);
			Track->Tasks.Add(Task);
		}
	}
}

bool UAKAbilityJsonSerializer::ExportToJson(const UAKAbilityAsset* Asset, FString& OutJsonString)
{
	if (!Asset)
	{
		return false;
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), CurrentSchemaVersion);
	Root->SetStringField(TEXT("asset"), Asset->GetName());

	// Meta
	Root->SetStringField(TEXT("displayName"), Asset->DisplayName.ToString());
	Root->SetStringField(TEXT("description"), Asset->Description.ToString());
	Root->SetNumberField(TEXT("assetVersion"), Asset->Version);

	// Timing
	Root->SetNumberField(TEXT("length"), Asset->Length);
	Root->SetNumberField(TEXT("playRate"), Asset->PlayRate);
	Root->SetNumberField(TEXT("frameRate"), Asset->FrameRate);
	Root->SetBoolField(TEXT("loop"), Asset->bLoop);
	Root->SetNumberField(TEXT("loopStartTime"), Asset->LoopStartTime);
	Root->SetNumberField(TEXT("maxLoops"), Asset->MaxLoops);

	// Tags
	Root->SetStringField(TEXT("abilityTags"), Asset->AbilityTags.ToString());
	Root->SetStringField(TEXT("cancelAbilitiesWithTag"), Asset->CancelAbilitiesWithTag.ToString());
	Root->SetStringField(TEXT("blockAbilitiesWithTag"), Asset->BlockAbilitiesWithTag.ToString());
	Root->SetStringField(TEXT("requiredTags"), Asset->RequiredTags.ToString());

	// Markers
	TArray<TSharedPtr<FJsonValue>> MarkersArray;
	for (const FAKTimeMarker& Marker : Asset->Markers)
	{
		TSharedRef<FJsonObject> MarkerJson = MakeShared<FJsonObject>();
		MarkerJson->SetStringField(TEXT("name"), Marker.Name.ToString());
		MarkerJson->SetNumberField(TEXT("time"), Marker.Time);
		MarkersArray.Add(MakeShared<FJsonValueObject>(MarkerJson));
	}
	Root->SetArrayField(TEXT("markers"), MarkersArray);

	// Channeling（引导 / 吟唱）
	Root->SetBoolField(TEXT("channeled"), Asset->bChanneled);
	Root->SetNumberField(TEXT("channelingStartTime"), Asset->ChannelingStartTime);
	Root->SetNumberField(TEXT("channelingEndTime"), Asset->ChannelingEndTime);
	Root->SetStringField(TEXT("channelFailureAction"),
		StaticEnum<EAKChannelFailureAction>()->GetNameStringByValue(static_cast<int64>(Asset->ChannelFailureAction)));
	Root->SetBoolField(TEXT("evaluateChannelingOnAuthorityOnly"), Asset->bEvaluateChannelingOnAuthorityOnly);

	TArray<TSharedPtr<FJsonValue>> ConditionsArray;
	for (const UAKChannelingCondition* Condition : Asset->ChannelingConditions)
	{
		if (!Condition)
		{
			continue;
		}
		TSharedRef<FJsonObject> ConditionJson = MakeShared<FJsonObject>();
		Condition->ExportJson(ConditionJson);
		ConditionsArray.Add(MakeShared<FJsonValueObject>(ConditionJson));
	}
	Root->SetArrayField(TEXT("channelingConditions"), ConditionsArray);

	// Tracks（注意：Preview* 是 WITH_EDITORONLY_DATA，不导出）
	TArray<TSharedPtr<FJsonValue>> TracksArray;
	for (const UAKAbilityTrack* Track : Asset->Tracks)
	{
		if (!Track)
		{
			continue;
		}
		TracksArray.Add(MakeShared<FJsonValueObject>(ExportTrack(Track)));
	}
	Root->SetArrayField(TEXT("tracks"), TracksArray);

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonString);
	return FJsonSerializer::Serialize(Root, Writer);
}

bool UAKAbilityJsonSerializer::ImportFromJson(UAKAbilityAsset* Asset, const FString& JsonString)
{
	if (!Asset)
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	int32 SchemaVersion = 0;
	if (Root->TryGetNumberField(TEXT("version"), SchemaVersion) && SchemaVersion > CurrentSchemaVersion)
	{
		// 未来版本：拒绝静默失败，交由调用方处理
		return false;
	}

	Asset->Modify();

	// Meta
	FString DisplayName, Description;
	if (Root->TryGetStringField(TEXT("displayName"), DisplayName))
	{
		Asset->DisplayName = FText::FromString(DisplayName);
	}
	if (Root->TryGetStringField(TEXT("description"), Description))
	{
		Asset->Description = FText::FromString(Description);
	}
	Root->TryGetNumberField(TEXT("assetVersion"), Asset->Version);

	// Timing
	Root->TryGetNumberField(TEXT("length"), Asset->Length);
	Root->TryGetNumberField(TEXT("playRate"), Asset->PlayRate);
	Root->TryGetNumberField(TEXT("frameRate"), Asset->FrameRate);
	Root->TryGetBoolField(TEXT("loop"), Asset->bLoop);
	Root->TryGetNumberField(TEXT("loopStartTime"), Asset->LoopStartTime);
	Root->TryGetNumberField(TEXT("maxLoops"), Asset->MaxLoops);

	// Tags
	auto ImportTags = [](const FJsonObject& RootObj, const FString& Field, FGameplayTagContainer& OutTags)
	{
		FString TagsString;
		if (RootObj.TryGetStringField(Field, TagsString) && !TagsString.IsEmpty())
		{
			OutTags.Reset();
			OutTags.FromExportString(TagsString);
		}
	};
	ImportTags(*Root, TEXT("abilityTags"), Asset->AbilityTags);
	ImportTags(*Root, TEXT("cancelAbilitiesWithTag"), Asset->CancelAbilitiesWithTag);
	ImportTags(*Root, TEXT("blockAbilitiesWithTag"), Asset->BlockAbilitiesWithTag);
	ImportTags(*Root, TEXT("requiredTags"), Asset->RequiredTags);

	// Markers
	const TArray<TSharedPtr<FJsonValue>>* MarkersArray = nullptr;
	if (Root->TryGetArrayField(TEXT("markers"), MarkersArray) && MarkersArray)
	{
		Asset->Markers.Reset();
		for (const TSharedPtr<FJsonValue>& Value : *MarkersArray)
		{
			const TSharedPtr<FJsonObject>* MarkerObject = nullptr;
			if (Value.IsValid() && Value->TryGetObject(MarkerObject) && MarkerObject && MarkerObject->IsValid())
			{
				FAKTimeMarker Marker;
				FString NameStr;
				if ((*MarkerObject)->TryGetStringField(TEXT("name"), NameStr))
				{
					Marker.Name = FName(*NameStr);
				}
				(*MarkerObject)->TryGetNumberField(TEXT("time"), Marker.Time);
				Asset->Markers.Add(Marker);
			}
		}
	}

	// Channeling（引导 / 吟唱）
	Root->TryGetBoolField(TEXT("channeled"), Asset->bChanneled);
	Root->TryGetNumberField(TEXT("channelingStartTime"), Asset->ChannelingStartTime);
	Root->TryGetNumberField(TEXT("channelingEndTime"), Asset->ChannelingEndTime);
	Root->TryGetBoolField(TEXT("evaluateChannelingOnAuthorityOnly"), Asset->bEvaluateChannelingOnAuthorityOnly);

	FString FailureActionName;
	if (Root->TryGetStringField(TEXT("channelFailureAction"), FailureActionName))
	{
		if (const UEnum* Enum = StaticEnum<EAKChannelFailureAction>())
		{
			Asset->ChannelFailureAction = static_cast<EAKChannelFailureAction>(Enum->GetValueByNameString(FailureActionName));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* ConditionsArray = nullptr;
	if (Root->TryGetArrayField(TEXT("channelingConditions"), ConditionsArray) && ConditionsArray)
	{
		Asset->ChannelingConditions.Reset();
		for (const TSharedPtr<FJsonValue>& Value : *ConditionsArray)
		{
			const TSharedPtr<FJsonObject>* ConditionObject = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(ConditionObject) || !ConditionObject || !ConditionObject->IsValid())
			{
				continue;
			}

			FString TypeName;
			(*ConditionObject)->TryGetStringField(TEXT("$type"), TypeName);

			UClass* ConditionClass = LoadClass<UAKChannelingCondition>(nullptr,
				*FString::Printf(TEXT("/Script/UEAbilityKit.%s"), *TypeName));
			if (!ConditionClass)
			{
				ConditionClass = LoadClass<UAKChannelingCondition>(nullptr, *TypeName); // 蓝图派生
			}
			if (!ConditionClass || ConditionClass->HasAnyClassFlags(CLASS_Abstract))
			{
				continue;
			}

			UAKChannelingCondition* Condition = NewObject<UAKChannelingCondition>(Asset, ConditionClass);
			if (!Condition)
			{
				continue;
			}

			Condition->ImportJson(**ConditionObject);
			Asset->ChannelingConditions.Add(Condition);
		}
	}

	// Tracks
	const TArray<TSharedPtr<FJsonValue>>* TracksArray = nullptr;
	if (Root->TryGetArrayField(TEXT("tracks"), TracksArray) && TracksArray)
	{
		Asset->Tracks.Reset();
		for (const TSharedPtr<FJsonValue>& Value : *TracksArray)
		{
			const TSharedPtr<FJsonObject>* TrackObject = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(TrackObject) || !TrackObject || !TrackObject->IsValid())
			{
				continue;
			}

			UAKAbilityTrack* Track = NewObject<UAKAbilityTrack>(Asset);
			ImportTrack(Track, **TrackObject);
			Asset->Tracks.Add(Track);
		}
	}

	Asset->MarkPackageDirty();
	return true;
}

bool UAKAbilityJsonSerializer::ExportToFile(const UAKAbilityAsset* Asset, const FString& FilePath)
{
	FString JsonString;
	if (!ExportToJson(Asset, JsonString))
	{
		return false;
	}
	return FFileHelper::SaveStringToFile(JsonString, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool UAKAbilityJsonSerializer::ImportFromFile(UAKAbilityAsset* Asset, const FString& FilePath)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		return false;
	}
	return ImportFromJson(Asset, JsonString);
}

UAKAbilityTaskBase* UAKAbilityJsonSerializer::CreateTaskByTypeName(UObject* Outer, const FString& TypeName)
{
	if (TypeName.IsEmpty())
	{
		return nullptr;
	}

	// 1) 本模块的原生类：/Script/UEAbilityKit.<Name>
	UClass* NativeClass = LoadClass<UAKAbilityTaskBase>(nullptr,
		*FString::Printf(TEXT("/Script/UEAbilityKit.%s"), *TypeName));

	if (!NativeClass)
	{
		// 2) 蓝图派生类：按路径加载
		NativeClass = LoadClass<UAKAbilityTaskBase>(nullptr, *TypeName);
	}

	if (!NativeClass || NativeClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}

	return NewObject<UAKAbilityTaskBase>(Outer, NativeClass);
}
