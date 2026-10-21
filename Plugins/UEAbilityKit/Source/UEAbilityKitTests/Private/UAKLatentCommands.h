#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "UAKMediaCapture.h"

/**
 * 逐帧录制命令：每帧先推进一段逻辑（OnFrame），再抓一帧。
 *
 * 为什么用 latent 而不是同步 for 循环：截图是异步的（渲染线程落盘），
 * 同步循环里拿不到像素；latent 的 Update() 每 tick 被调用一次，正好当"帧"用。
 *
 * 注意：这里"一帧 = 一次 Update"，不是真实帧率。视频的时长由 Python 侧
 * ffmpeg 的 -framerate 决定（默认 30），因此 FrameCount 直接换算成时长，
 * 与机器快慢无关——这正是我们要的确定性。
 */
class FUAKMediaRecordCommand : public IAutomationLatentCommand
{
public:
	/**
	 * @param OnFrame  (帧序号)：推进被测逻辑；不传就是纯录屏（例如录编辑器 UI）
	 * @param OwnerTest 传入用例自身时，收尾会在用例里断言"确实采到帧了"
	 */
	FUAKMediaRecordCommand(FString InTestName, int32 InFrameCount,
		TFunction<void(int32)> InOnFrame = nullptr, FAutomationTestBase* InOwnerTest = nullptr)
		: TestName(MoveTemp(InTestName))
		, FrameCount(InFrameCount)
		, OnFrame(MoveTemp(InOnFrame))
		, OwnerTest(InOwnerTest)
	{
	}

	virtual bool Update() override
	{
		if (FrameIndex == 0)
		{
			if (!Recorder.Begin(TestName))
			{
				// 没开采集（headless）——什么都不做，直接结束，不算失败
				return true;
			}
		}

		if (!Recorder.IsRecording())
		{
			return true;
		}

		if (OnFrame)
		{
			OnFrame(FrameIndex);
		}

		if (Recorder.CaptureFrame())
		{
			++FrameIndex;
		}

		if (FrameIndex >= FrameCount)
		{
			RecordedFrames = Recorder.End();
			if (OwnerTest && UAKTest::FUAKMediaRecorder::IsMediaCaptureEnabled())
			{
				OwnerTest->TestTrue(TEXT("开启采集时应至少拿到一帧（没有像素说明跑在 NullRHI 下）"), RecordedFrames > 0);
			}
			return true;
		}

		return false;
	}

	int32 GetRecordedFrames() const { return RecordedFrames; }

private:
	FString TestName;
	int32 FrameCount = 0;
	int32 FrameIndex = 0;
	int32 RecordedFrames = 0;
	TFunction<void(int32)> OnFrame;
	FAutomationTestBase* OwnerTest = nullptr;
	UAKTest::FUAKMediaRecorder Recorder;
};

/** 等指定的墙钟秒数：给"等客户端连上 / 等属性复制"这类场景用 */
class FUAKWaitSecondsCommand : public IAutomationLatentCommand
{
public:
	explicit FUAKWaitSecondsCommand(float InSeconds)
		: Seconds(InSeconds)
		, StartSeconds(FPlatformTime::Seconds())
	{
	}

	virtual bool Update() override
	{
		return (FPlatformTime::Seconds() - StartSeconds) >= Seconds;
	}

private:
	float Seconds = 0.f;
	double StartSeconds = 0.0;
};
