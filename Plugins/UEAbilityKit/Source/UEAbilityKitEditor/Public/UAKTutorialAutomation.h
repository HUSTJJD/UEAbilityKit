#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UAKTutorialTypes.h"

/**
 * 教程自动化：把"编辑器的使用过程"录下来，并直接变成文档素材。
 *
 * 三个控制台命令（编辑器进程内可用，含 -ExecCmds）：
 *   UAK.Tutorial.Run <脚本路径|教程 id>   跑一条教程：执行步骤 + 采截图 + 采帧序列
 *   UAK.Tutorial.RunAll [目录]            跑目录下全部教程（默认 <Project>/Tutorials）
 *   UAK.Tutorial.Stop                     中断当前运行
 *   UAK.Tutorial.Capture 0|1              关 / 开素材采集（关掉就是"只验证步骤"模式）
 *
 * 产物落在 <Project>/Saved/UAKAutomation/Tutorials/<tutorial>/：
 *   step_01_<id>.png              步骤截图
 *   step_01_<id>/frames/*.png     步骤录屏的帧序列（由 generate_tutorials.py 合成 mp4）
 *   run.json                      每步的执行结果（状态 / 耗时 / 校验数 / 素材路径）
 *
 * 每行结论还会打一条机器可读标记，供日志聚合：
 *   UAK_TUTORIAL <Tutorial> <Step> <OK|FAIL|SKIP> [k=v ...]
 */

/** 单步的执行结果（写进 run.json） */
struct FUAKTutorialStepResult
{
	FString Id;
	FString Title;
	FString Say;
	FString Do;
	FString Expect;
	FString Action;

	FString Status = TEXT("SKIP");   // OK / FAIL / SKIP
	float Duration = 0.f;

	/** 素材（相对教程输出目录） */
	FString Shot;
	FString FramesDir;
	int32 Frames = 0;

	/** 执行完这一步时的资产状态，直接写进文档，读者可以对照 */
	int32 Tracks = 0;
	int32 Tasks = 0;
	float Cursor = 0.f;
	int32 Errors = 0;
	int32 Warnings = 0;

	TArray<FString> Notes;
};

/** 教程执行器（进程内单例，靠 core ticker 推帧） */
class FUAKTutorialRunner
{
public:
	static FUAKTutorialRunner& Get();

	/** 开始跑一条教程；bInCapture=false 时只验证步骤不产素材 */
	bool Start(const FUAKTutorialScript& Script, bool bInCapture);

	bool IsRunning() const { return bRunning; }
	const FString& CurrentTutorial() const { return CurrentId; }
	const FString& LastRunJson() const { return RunJsonPath; }

	/** 中断：已经跑完的步骤照样写进 run.json（半程产物也有用） */
	void Abort();

	/** 排队跑多条教程（RunAll）；bInExitWhenDone 为 true 时全部跑完请求退出进程 */
	bool Enqueue(const TArray<FUAKTutorialScript>& Scripts, bool bInCapture, bool bInExitWhenDone);

private:
	FUAKTutorialRunner() = default;

	enum class EPhase : uint8
	{
		/** 等 UI 稳定几帧再动手 */
		Settle,
		/** 执行动作 */
		Act,
		/** 动作做完再等几帧，等 Slate 重绘完再截图 */
		PostSettle,
		/** 截一张 */
		Shot,
		/** 按 fps 逐帧采，采满 Seconds 为止 */
		Record,
	};

	bool Tick(float DeltaTime);

	void BeginStep();
	void ExecuteCurrentStep();
	void CaptureShot();
	void TickRecord(float DeltaTime);
	void FinishStep(const TCHAR* Status);
	void Advance();
	void FinishRun(const TCHAR* Status);

	/** 采某个区域到一张 PNG；返回相对教程输出目录的路径 */
	bool CaptureRegion(EUAKTutorialRegion Region, const FString& RelativePath, FIntPoint& OutSize);

	bool WriteRunJson();

private:
	bool bRunning = false;
	bool bCapture = true;
	/** 被 Stop 打断：最后一步的结论要标成 ABORT */
	bool bAborted = false;

	FUAKTutorialScript Script;
	FString CurrentId;
	int32 StepIndex = INDEX_NONE;
	EPhase Phase = EPhase::Settle;

	float SettleTimer = 0.f;
	float RecordTimer = 0.f;
	int32 RecordFrame = 0;
	int32 RecordTotal = 0;
	float StepElapsed = 0.f;
	/** Wait 步骤剩余的空转时间 */
	float WaitRemaining = 0.f;

	/** 当前教程在操作的资产（步骤之间共享） */
	TWeakObjectPtr<class UAKAbilityAsset> CurrentAsset;

	/** RunAll 的待跑队列；一条跑完自动接着跑下一条 */
	TArray<FUAKTutorialScript> Queued;
	bool bRequestExitWhenDone = false;

	FString OutputRoot;
	FString TutorialDir;
	FString RunJsonPath;

	FTSTicker::FDelegateHandle TickerHandle;
	TArray<FUAKTutorialStepResult> Results;
};

/** 命令注册 / 反注册，以及路径约定 */
class UEABILITYKITEDITOR_API FUAKTutorialAutomation
{
public:
	static void Register();
	static void Unregister();

	/** 教程脚本目录：<Project>/Tutorials */
	static FString GetScriptDir();

	/** 素材根目录：<Project>/Saved/UAKAutomation/Tutorials */
	static FString GetMediaRoot();
};
