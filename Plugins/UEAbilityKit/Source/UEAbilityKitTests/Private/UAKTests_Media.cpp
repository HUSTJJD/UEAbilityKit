#include "Misc/AutomationTest.h"

#include "UAKTestCommon.h"
#include "UAKLatentCommands.h"
#include "UAKMediaCapture.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityInstance.h"
#include "UAKAbilityTrack.h"
#include "UAKTrace.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "UAKTests.Media"

/**
 * 「表现层」用例：真跑一次技能，同时逐帧抓图。
 *
 * 与 UAK.Instance.* 的区别：
 *   - Instance 层断言"时序对不对"（零渲染、毫秒级、CI 可跑）
 *   - Media 层产出"给人类看的证据"（截图 / mp4 / 血缘树），只在本地开采集时跑
 * 两者共用同一套资产构造代码，所以素材不会因为"另写一份演示"而失真。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKMediaAbilityPlaybackTest, "UAK.Media.AbilityPlayback.RecordsFrames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKMediaAbilityPlaybackTest::RunTest(const FString& Parameters)
{
	// 没开采集（headless / -NullRHI）：这一层没有意义，直接算通过。
	// run_tests.ps1 默认带 -UAKCaptureMedia，所以本地一键跑时一定会进下面的分支。
	if (!UAKTest::FUAKMediaRecorder::IsMediaCaptureEnabled())
	{
		UE_LOG(LogTemp, Display, TEXT("[UAK] 未开启媒体采集（-UAKCaptureMedia），跳过 UAK.Media.*"));
		return true;
	}

	const float Length = 1.f;
	const int32 FrameCount = 60;          // 60 帧 / ffmpeg 30fps → 2s 视频
	const float DeltaTime = Length / FrameCount;

	UAKAbilityAsset* Asset = UAKTest::NewAsset(Length);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);

	UAKAbilityTask_TestProbe* First = UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.1f, 0.f);
	First->TaskId = FName(TEXT("First"));

	UAKAbilityTask_TestProbe* Second = UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.5f, 0.f);
	Second->TaskId = FName(TEXT("Second"));

	UAKAbilityInstance* Instance = UAKTest::NewInstance(Asset);
	TestTrue(TEXT("实例应创建成功"), Instance != nullptr);
	if (!Instance)
	{
		return false;
	}

	// 每帧推进一次实例：帧号 → 播放时间，因此视频里看到的顺序就是真实执行顺序
	ADD_LATENT_AUTOMATION_COMMAND(FUAKMediaRecordCommand(
		TEXT("UAK.Media.AbilityPlayback"), FrameCount,
		[Instance, DeltaTime](int32 FrameIndex)
		{
			Instance->Tick(DeltaTime);
		},
		this));

	return true;
}

/**
 * 血缘树文本：headless 也一定产出。
 *
 * 它是 Showcase 的兜底素材——没有 GPU 时至少还能展示"一次施放的完整因果链"，
 * 而不是空荡荡的一句"本次运行没有开启媒体采集"。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKMediaTraceTextTest, "UAK.Media.Trace.DumpsText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKMediaTraceTextTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(1.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);

	UAKAbilityTask_TestProbe* Probe = UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 0.f);
	Probe->TaskId = FName(TEXT("Traced"));

	UAKTrace* Trace = NewObject<UAKTrace>(GetTransientPackage(), UAKTrace::StaticClass(), NAME_None, RF_Standalone);
	Trace->AddNode(INDEX_NONE, EAKTraceNodeKind::Ability, TEXT("MediaTraceAbility"), 0.f);

	UAKAbilityInstance* Instance = UAKTest::NewInstance(Asset);
	TestTrue(TEXT("实例应创建成功"), Instance != nullptr);
	if (!Instance)
	{
		return false;
	}

	Instance->SetTrace(Trace);
	Instance->Tick(0.1f);

	TestTrue(TEXT("血缘树应记录到任务节点"), Trace->GetNodes().Num() > 1);
	TestTrue(TEXT("血缘树文本应写盘成功"),
		UAKTest::SaveTraceText(TEXT("UAK.Media.Trace"), TEXT("trace"), Trace));

	return true;
}

#undef LOCTEXT_NAMESPACE
