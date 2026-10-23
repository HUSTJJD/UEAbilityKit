#include "UAKTutorialTypes.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString GetString(const FJsonObject& Obj, const TCHAR* Key, const FString& Default = FString())
	{
		FString Value;
		return Obj.TryGetStringField(Key, Value) ? Value : Default;
	}

	float GetFloat(const FJsonObject& Obj, const TCHAR* Key, float Default)
	{
		double Value = 0.0;
		return Obj.TryGetNumberField(Key, Value) ? static_cast<float>(Value) : Default;
	}

	int32 GetInt(const FJsonObject& Obj, const TCHAR* Key, int32 Default)
	{
		int32 Value = 0;
		return Obj.TryGetNumberField(Key, Value) ? Value : Default;
	}

	bool GetBool(const FJsonObject& Obj, const TCHAR* Key, bool Default)
	{
		bool Value = false;
		return Obj.TryGetBoolField(Key, Value) ? Value : Default;
	}

	void GetStringArray(const FJsonObject& Obj, const TCHAR* Key, TArray<FString>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Obj.TryGetArrayField(Key, Array) && Array)
		{
			for (const TSharedPtr<FJsonValue>& Item : *Array)
			{
				FString Text;
				if (Item.IsValid() && Item->TryGetString(Text))
				{
					Out.Add(Text);
				}
			}
		}
	}
}

EUAKTutorialAction UAKTutorialActionFromString(const FString& Name)
{
	if (Name.Equals(TEXT("Note"), ESearchCase::IgnoreCase))                 { return EUAKTutorialAction::Note; }
	if (Name.Equals(TEXT("CreateAsset"), ESearchCase::IgnoreCase))          { return EUAKTutorialAction::CreateAsset; }
	if (Name.Equals(TEXT("OpenAsset"), ESearchCase::IgnoreCase))            { return EUAKTutorialAction::OpenAsset; }
	if (Name.Equals(TEXT("CloseAsset"), ESearchCase::IgnoreCase))           { return EUAKTutorialAction::CloseAsset; }
	if (Name.Equals(TEXT("FocusTab"), ESearchCase::IgnoreCase))             { return EUAKTutorialAction::FocusTab; }
	if (Name.Equals(TEXT("AddTrack"), ESearchCase::IgnoreCase))             { return EUAKTutorialAction::AddTrack; }
	if (Name.Equals(TEXT("AddTask"), ESearchCase::IgnoreCase))              { return EUAKTutorialAction::AddTask; }
	if (Name.Equals(TEXT("SelectTask"), ESearchCase::IgnoreCase))           { return EUAKTutorialAction::SelectTask; }
	if (Name.Equals(TEXT("SelectTrack"), ESearchCase::IgnoreCase))          { return EUAKTutorialAction::SelectTrack; }
	if (Name.Equals(TEXT("SetTrackMode"), ESearchCase::IgnoreCase))         { return EUAKTutorialAction::SetTrackMode; }
	if (Name.Equals(TEXT("SetTaskProp"), ESearchCase::IgnoreCase))          { return EUAKTutorialAction::SetTaskProp; }
	if (Name.Equals(TEXT("SetAssetProp"), ESearchCase::IgnoreCase))         { return EUAKTutorialAction::SetAssetProp; }
	if (Name.Equals(TEXT("SetCursor"), ESearchCase::IgnoreCase))            { return EUAKTutorialAction::SetCursor; }
	if (Name.Equals(TEXT("Play"), ESearchCase::IgnoreCase))                 { return EUAKTutorialAction::Play; }
	if (Name.Equals(TEXT("Pause"), ESearchCase::IgnoreCase))                { return EUAKTutorialAction::Pause; }
	if (Name.Equals(TEXT("Stop"), ESearchCase::IgnoreCase))                 { return EUAKTutorialAction::Stop; }
	if (Name.Equals(TEXT("StepFrames"), ESearchCase::IgnoreCase))           { return EUAKTutorialAction::StepFrames; }
	if (Name.Equals(TEXT("ToggleFrames"), ESearchCase::IgnoreCase))         { return EUAKTutorialAction::ToggleFrames; }
	if (Name.Equals(TEXT("Validate"), ESearchCase::IgnoreCase))             { return EUAKTutorialAction::Validate; }
	if (Name.Equals(TEXT("ExportJson"), ESearchCase::IgnoreCase))           { return EUAKTutorialAction::ExportJson; }
	if (Name.Equals(TEXT("ImportJson"), ESearchCase::IgnoreCase))           { return EUAKTutorialAction::ImportJson; }
	if (Name.Equals(TEXT("Wait"), ESearchCase::IgnoreCase))                 { return EUAKTutorialAction::Wait; }
	if (Name.Equals(TEXT("Exec"), ESearchCase::IgnoreCase))                 { return EUAKTutorialAction::Exec; }
	return EUAKTutorialAction::None;
}

const TCHAR* UAKTutorialActionToString(EUAKTutorialAction Action)
{
	switch (Action)
	{
	case EUAKTutorialAction::Note:         return TEXT("Note");
	case EUAKTutorialAction::CreateAsset:  return TEXT("CreateAsset");
	case EUAKTutorialAction::OpenAsset:    return TEXT("OpenAsset");
	case EUAKTutorialAction::CloseAsset:   return TEXT("CloseAsset");
	case EUAKTutorialAction::FocusTab:     return TEXT("FocusTab");
	case EUAKTutorialAction::AddTrack:     return TEXT("AddTrack");
	case EUAKTutorialAction::AddTask:      return TEXT("AddTask");
	case EUAKTutorialAction::SelectTask:   return TEXT("SelectTask");
	case EUAKTutorialAction::SelectTrack:  return TEXT("SelectTrack");
	case EUAKTutorialAction::SetTrackMode: return TEXT("SetTrackMode");
	case EUAKTutorialAction::SetTaskProp:  return TEXT("SetTaskProp");
	case EUAKTutorialAction::SetAssetProp: return TEXT("SetAssetProp");
	case EUAKTutorialAction::SetCursor:    return TEXT("SetCursor");
	case EUAKTutorialAction::Play:         return TEXT("Play");
	case EUAKTutorialAction::Pause:        return TEXT("Pause");
	case EUAKTutorialAction::Stop:         return TEXT("Stop");
	case EUAKTutorialAction::StepFrames:   return TEXT("StepFrames");
	case EUAKTutorialAction::ToggleFrames: return TEXT("ToggleFrames");
	case EUAKTutorialAction::Validate:     return TEXT("Validate");
	case EUAKTutorialAction::ExportJson:   return TEXT("ExportJson");
	case EUAKTutorialAction::ImportJson:   return TEXT("ImportJson");
	case EUAKTutorialAction::Wait:         return TEXT("Wait");
	case EUAKTutorialAction::Exec:         return TEXT("Exec");
	default:                               return TEXT("None");
	}
}

FName UAKTutorialTagForRegion(EUAKTutorialRegion Region)
{
	switch (Region)
	{
	case EUAKTutorialRegion::Editor:      return FName(TEXT("UAK.Editor"));
	case EUAKTutorialRegion::Timeline:    return FName(TEXT("UAK.Timeline"));
	case EUAKTutorialRegion::Bloodline:   return FName(TEXT("UAK.Bloodline"));
	case EUAKTutorialRegion::Details:     return FName(TEXT("UAK.Details"));
	case EUAKTutorialRegion::Validation:  return FName(TEXT("UAK.Validation"));
	default:                              return FName(TEXT("UAK.Timeline"));
	}
}

EUAKTutorialRegion UAKTutorialRegionFromString(const FString& Name)
{
	if (Name.Equals(TEXT("Editor"), ESearchCase::IgnoreCase))      { return EUAKTutorialRegion::Editor; }
	if (Name.Equals(TEXT("Timeline"), ESearchCase::IgnoreCase))    { return EUAKTutorialRegion::Timeline; }
	if (Name.Equals(TEXT("Bloodline"), ESearchCase::IgnoreCase))   { return EUAKTutorialRegion::Bloodline; }
	if (Name.Equals(TEXT("Details"), ESearchCase::IgnoreCase))     { return EUAKTutorialRegion::Details; }
	if (Name.Equals(TEXT("Inspector"), ESearchCase::IgnoreCase))   { return EUAKTutorialRegion::Details; }
	if (Name.Equals(TEXT("Validation"), ESearchCase::IgnoreCase))  { return EUAKTutorialRegion::Validation; }
	return EUAKTutorialRegion::Auto;
}

bool FUAKTutorialScript::LoadFromFile(const FString& Path, FUAKTutorialScript& OutScript, FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		OutError = FString::Printf(TEXT("读不到教程脚本：%s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = FString::Printf(TEXT("教程脚本不是合法 Json：%s"), *Path);
		return false;
	}

	OutScript = FUAKTutorialScript();
	OutScript.SourcePath = FPaths::ConvertRelativePathToFull(Path);
	OutScript.Id = GetString(*Root, TEXT("id"), FPaths::GetBaseFilename(Path));
	OutScript.Title = GetString(*Root, TEXT("title"), OutScript.Id);
	OutScript.Summary = GetString(*Root, TEXT("summary"));
	OutScript.Level = GetString(*Root, TEXT("level"), TEXT("beginner"));
	OutScript.DurationText = GetString(*Root, TEXT("duration"), TEXT("-"));
	OutScript.Fps = GetFloat(*Root, TEXT("fps"), 30.f);
	GetStringArray(*Root, TEXT("prerequisites"), OutScript.Prerequisites);
	GetStringArray(*Root, TEXT("checks"), OutScript.Checks);

	const TSharedPtr<FJsonObject>* AssetObj = nullptr;
	if (Root->TryGetObjectField(TEXT("asset"), AssetObj) && AssetObj && AssetObj->IsValid())
	{
		OutScript.Asset.Path = GetString(**AssetObj, TEXT("path"));
		OutScript.Asset.Length = GetFloat(**AssetObj, TEXT("length"), 1.f);
		OutScript.Asset.FrameRate = GetInt(**AssetObj, TEXT("frameRate"), 60);
	}
	if (OutScript.Asset.Path.IsEmpty())
	{
		OutScript.Asset.Path = FString::Printf(TEXT("/Game/UAK/Tutorials/TA_%s"), *OutScript.Id);
	}

	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (!Root->TryGetArrayField(TEXT("steps"), Steps) || !Steps || Steps->Num() == 0)
	{
		OutError = FString::Printf(TEXT("教程脚本没有 steps：%s"), *Path);
		return false;
	}

	int32 Index = 0;
	for (const TSharedPtr<FJsonValue>& Item : *Steps)
	{
		const TSharedPtr<FJsonObject>* StepObj = nullptr;
		if (!Item.IsValid() || !Item->TryGetObject(StepObj) || !StepObj || !StepObj->IsValid())
		{
			continue;
		}

		const FJsonObject& Obj = **StepObj;
		FUAKTutorialStep Step;
		++Index;
		Step.Id = GetString(Obj, TEXT("id"), FString::Printf(TEXT("step%02d"), Index));
		Step.Title = GetString(Obj, TEXT("title"));
		Step.Say = GetString(Obj, TEXT("say"));
		Step.Do = GetString(Obj, TEXT("do"));
		Step.Expect = GetString(Obj, TEXT("expect"));
		Step.ActionName = GetString(Obj, TEXT("action"), TEXT("Note"));
		Step.Action = UAKTutorialActionFromString(Step.ActionName);
		if (Step.Action == EUAKTutorialAction::None && !Step.ActionName.IsEmpty())
		{
			OutError = FString::Printf(TEXT("教程 %s 步骤 %s 的 action 未知：%s"),
				*OutScript.Id, *Step.Id, *Step.ActionName);
			return false;
		}

		const TSharedPtr<FJsonObject>* ArgsObj = nullptr;
		if (Obj.TryGetObjectField(TEXT("args"), ArgsObj) && ArgsObj && ArgsObj->IsValid())
		{
			Step.Args = *ArgsObj;
		}
		else
		{
			Step.Args = MakeShared<FJsonObject>();
		}

		const TSharedPtr<FJsonObject>* CapObj = nullptr;
		if (Obj.TryGetObjectField(TEXT("capture"), CapObj) && CapObj && CapObj->IsValid())
		{
			Step.Capture.bShot = GetBool(**CapObj, TEXT("shot"), false);
			Step.Capture.bRecord = GetBool(**CapObj, TEXT("record"), false);
			Step.Capture.Seconds = GetFloat(**CapObj, TEXT("seconds"), 1.f);
			Step.Capture.Fps = GetFloat(**CapObj, TEXT("fps"), OutScript.Fps);
			Step.Capture.Region = UAKTutorialRegionFromString(GetString(**CapObj, TEXT("region"), TEXT("Auto")));
		}
		else if (Obj.TryGetBoolField(TEXT("shot"), Step.Capture.bShot))
		{
			// 简写："shot": true
		}
		if (Step.Capture.Fps <= 0.f)
		{
			Step.Capture.Fps = OutScript.Fps;
		}

		Step.ExpectErrors = GetInt(Obj, TEXT("expectErrors"), 0);
		Step.ExpectWarnings = GetInt(Obj, TEXT("expectWarnings"), -1);

		const TSharedPtr<FJsonObject>* AdvObj = nullptr;
		if (Obj.TryGetObjectField(TEXT("advance"), AdvObj) && AdvObj && AdvObj->IsValid())
		{
			Step.Advance.MinTracks = GetInt(**AdvObj, TEXT("minTracks"), INDEX_NONE);
			Step.Advance.MinTasks = GetInt(**AdvObj, TEXT("minTasks"), INDEX_NONE);
			Step.Advance.MinCursorTime = GetFloat(**AdvObj, TEXT("minCursorTime"), -1.f);
			Step.Advance.bRequireSelectedTask = GetBool(**AdvObj, TEXT("requireSelectedTask"), false);
			Step.Advance.bRequireSelectedTrack = GetBool(**AdvObj, TEXT("requireSelectedTrack"), false);
			Step.Advance.ExpectErrors = GetInt(**AdvObj, TEXT("expectErrors"), INDEX_NONE);
			Step.Advance.Timeout = GetFloat(**AdvObj, TEXT("timeout"), 0.f);
		}

		OutScript.Steps.Add(MoveTemp(Step));
	}

	return true;
}
