#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * 教程脚本（<Project>/Tutorials/*.json）的数据模型。
 *
 * 一条教程 = 一串步骤。每一步同时是三样东西：
 *   1. 给读者看的教学内容：title / say（讲解）/ do（手上怎么点）/ expect（应该看到什么）
 *   2. 给引擎执行的动作：action / args
 *   3. 要采集的素材：capture（截图 / 录屏）
 *
 * 三者同源，是这条流水线的关键——**录制过程即文档**，
 * 不是"先写文档再补截图"，所以文档永远不会和真实编辑器对不上。
 *
 * 由 UEAbilityKitEditor 模块加载与执行（控制台命令 UAK.Tutorial.*），
 * 产物交给 Scripts/tutorial/generate_tutorials.py 渲染成 Markdown 教程页。
 */

/** 步骤的原子动作（脚本里 action 字段的取值） */
enum class EUAKTutorialAction : uint8
{
	None = 0,
	/** 只讲解不操作：用于"这一步只需要看图"的教学步骤 */
	Note,
	/** 新建（或重置）教程用的技能资产 */
	CreateAsset,
	/** 打开技能资产编辑器 */
	OpenAsset,
	/** 关闭当前技能资产编辑器 */
	CloseAsset,
	/** 激活某个 Tab：Timeline / Bloodline / Details / Preview */
	FocusTab,
	AddTrack,
	AddTask,
	SelectTask,
	SelectTrack,
	SetTrackMode,
	SetTaskProp,
	SetAssetProp,
	SetCursor,
	Play,
	Pause,
	Stop,
	StepFrames,
	ToggleFrames,
	/** 跑一次校验，把错误 / 警告数写进产物 */
	Validate,
	ExportJson,
	ImportJson,
	/** 空转若干秒（录屏留白，让动效被录进去） */
	Wait,
	/** 执行一条控制台命令（例如 UAK.Trace.Dump、UAK.Debug.Enable） */
	Exec,
};

/** 截图 / 录屏采哪个控件 */
enum class EUAKTutorialRegion : uint8
{
	/** 时间轴 Tab（含 GAS 影响条与校验条）——教程的默认视角 */
	Auto = 0,
	/** 整个资产编辑器窗口 */
	Editor,
	Timeline,
	Bloodline,
	/** Inspector（Details Tab） */
	Details,
	Validation,
};

EUAKTutorialAction UAKTutorialActionFromString(const FString& Name);
const TCHAR* UAKTutorialActionToString(EUAKTutorialAction Action);
EUAKTutorialRegion UAKTutorialRegionFromString(const FString& Name);

/**
 * 引导高亮锚点用的 Slate Tag。
 *
 * 官方 IntroTutorials 的 NamedWidget 锚点匹配的是 `SWidget::GetTag()`
 * （见 STutorialContent::HandlePaintNamedWidget），这是 Slate 的通用机制，
 * 不需要引擎那套 wrapper——所以 UAK 自己的面板只要打个 Tag 就能被高亮框住。
 */
FName UAKTutorialTagForRegion(EUAKTutorialRegion Region);

/** 这一步要采集什么 */
struct FUAKTutorialCapture
{
	bool bShot = false;
	bool bRecord = false;
	float Seconds = 0.f;
	float Fps = 30.f;
	EUAKTutorialRegion Region = EUAKTutorialRegion::Auto;
};

/**
 * 「这一步算做完了吗」——交互引导（UAK.Tutorial.Guided）用的完成条件。
 *
 * 为什么单独有这一层：录制模式（UAK.Tutorial.Run）是引擎**替你点**，
 * 步骤是串行的，走完就走完了；引导模式是**你自己点**，引擎必须能判断
 * "你点到位了没有"，否则就只能靠"下一步"按钮硬推（等于没有引导）。
 *
 * 条件是**声明式**的：只描述目标状态的阈值，不描述操作序列。
 * 所以读者可以用和我们脚本不同的顺序、不同的手法达成它，引导照样放行——
 * 这是引导与录制的本质区别，也是它们能共用同一份脚本的原因。
 *
 * 全部字段留空（默认）时 = 该步要求读者手动点「下一步」。
 */
struct FUAKTutorialAdvance
{
	/** 轨道数 >= 该值 */
	int32 MinTracks = INDEX_NONE;
	/** 全部轨道的任务总数 >= 该值 */
	int32 MinTasks = INDEX_NONE;
	/** 光标时间 >= 该值（秒） */
	float MinCursorTime = -1.f;
	/** 需要先选中一个任务 */
	bool bRequireSelectedTask = false;
	/** 需要先选中一条轨道 */
	bool bRequireSelectedTrack = false;
	/** 校验错误数 == 该值（< 0 表示不检查） */
	int32 ExpectErrors = INDEX_NONE;
	/**
	 * 超时自动放行（秒）。
	 * 为什么必须给：条件万一因版本差异永远无法满足，读者会被永久卡住。
	 * 宁可放行（读者可能没做对），也不能让人走不下去。
	 */
	float Timeout = 0.f;

	bool IsManual() const
	{
		return MinTracks == INDEX_NONE && MinTasks == INDEX_NONE
			&& MinCursorTime < 0.f && !bRequireSelectedTask && !bRequireSelectedTrack
			&& ExpectErrors == INDEX_NONE;
	}
};

/** 教程资产：教程开始时会被重置，保证每次录制结果一致 */
struct FUAKTutorialAsset
{
	/** 包路径，例如 /Game/UAK/Tutorials/TA_FirstAbility */
	FString Path;
	float Length = 1.f;
	int32 FrameRate = 60;
};

struct FUAKTutorialStep
{
	FString Id;
	FString Title;

	/** 讲解：这一步"为什么这么做" */
	FString Say;
	/** 手上怎么点：人类可读的操作描述 */
	FString Do;
	/** 预期：做完应该看到什么 */
	FString Expect;

	EUAKTutorialAction Action = EUAKTutorialAction::Note;
	FString ActionName;
	TSharedPtr<FJsonObject> Args;

	FUAKTutorialCapture Capture;

	/** 引导模式的完成条件（录制模式忽略） */
	FUAKTutorialAdvance Advance;

	/** Validate 步骤的断言：期望的错误数（默认 0 错误） */
	int32 ExpectErrors = 0;
	/** 期望的警告数；< 0 表示不检查 */
	int32 ExpectWarnings = -1;
};

// 跨模块导出：UEAbilityKitTests 直接读脚本做静态校验（LNK2019 就是少了这个）
struct UEABILITYKITEDITOR_API FUAKTutorialScript
{
	FString Id;
	FString Title;
	FString Summary;

	/** 难度：beginner / intermediate / advanced（只用于展示） */
	FString Level;
	/** 预计时长，例如 "8 min"（只用于展示） */
	FString DurationText;

	FUAKTutorialAsset Asset;

	TArray<FString> Prerequisites;
	/** 学完自查清单 */
	TArray<FString> Checks;

	float Fps = 30.f;
	TArray<FUAKTutorialStep> Steps;

	/** 脚本文件路径（回写到产物里，便于追溯） */
	FString SourcePath;

	static bool LoadFromFile(const FString& Path, FUAKTutorialScript& OutScript, FString& OutError);
};
