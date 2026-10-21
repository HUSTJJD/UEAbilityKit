#include "UAKMediaCapture.h"

#include "UAKTrace.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"   // FScreenshotRequest

#define LOCTEXT_NAMESPACE "UAKMediaCapture"

namespace UAKTest
{
	/** 单帧截图在引擎里落盘的位置：Saved/Screenshots/<Name>.png */
	static FString ScreenShotPath(const FString& ShotName)
	{
		return FPaths::ScreenShotDir() / (ShotName + TEXT(".png"));
	}

	static FString MediaRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("UAKAutomation"));
	}

	bool FUAKMediaRecorder::IsMediaCaptureEnabled()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("UAKCaptureMedia"));
	}

	bool FUAKMediaRecorder::Begin(const FString& InTestName)
	{
		if (bRecording)
		{
			return true;
		}

		if (!IsMediaCaptureEnabled())
		{
			return false;
		}

		TestName = InTestName;
		FramesDir = MediaRoot() / TestName / TEXT("frames");

		// 每轮运行清空旧帧，避免"上次跑的图"混进这次的 mp4
		IFileManager::Get().DeleteDirectory(*FramesDir, false, true);
		if (!IFileManager::Get().MakeDirectory(*FramesDir, true))
		{
			UE_LOG(LogTemp, Warning, TEXT("[UAK] 无法创建帧目录：%s"), *FramesDir);
			return false;
		}

		FrameCount = 0;
		FramesRequested = 0;
		WaitTicks = 0;
		bShotPending = false;
		bRecording = true;
		return true;
	}

	bool FUAKMediaRecorder::CaptureFrame()
	{
		if (!bRecording)
		{
			return true;
		}

		if (!bShotPending)
		{
			const FString ShotName = FString::Printf(TEXT("UAKFrame_%s_%05d"), *TestName, FramesRequested);
			// bShowUI=false：不要 UI 层；bAddFilenameSuffix=false：文件名必须可预测，
			// 否则等不到这个路径上的文件。
			FScreenshotRequest::RequestScreenshot(ShotName, false, false);
			bShotPending = true;
			WaitTicks = 0;
			return false;
		}

		const FString ShotName = FString::Printf(TEXT("UAKFrame_%s_%05d"), *TestName, FramesRequested);
		const FString Source = ScreenShotPath(ShotName);
		const FString Target = FramesDir / FString::Printf(TEXT("frame_%05d.png"), FrameCount);

		if (IFileManager::Get().FileExists(*Source))
		{
			// Move 而非 Copy：帧数多时省一次全量写入
			if (IFileManager::Get().Move(*Target, *Source) != 0)
			{
				++FrameCount;
			}
			++FramesRequested;
			bShotPending = false;
			return true;
		}

		// 引擎没给像素（NullRHI / 窗口最小化 / 渲染线程卡住）时不要死等
		if (++WaitTicks > 120)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UAK] 第 %d 帧超时未落盘，跳过（RHI 可能没有像素）"), FramesRequested);
			++FramesRequested;
			bShotPending = false;
			return true;
		}

		return false;
	}

	int32 FUAKMediaRecorder::End()
	{
		bRecording = false;
		bShotPending = false;
		UE_LOG(LogTemp, Display, TEXT("[UAK] 采集结束：%s，%d 帧 → %s"), *TestName, FrameCount, *FramesDir);
		return FrameCount;
	}

	bool SaveTraceText(const FString& TestName, const FString& FileName, const UAKTrace* Trace)
	{
		if (!Trace)
		{
			return false;
		}

		const FString Dir = MediaRoot() / TestName;
		if (!IFileManager::Get().MakeDirectory(*Dir, true))
		{
			return false;
		}

		const FString Path = Dir / (FileName + TEXT(".txt"));
		return FFileHelper::SaveStringToFile(Trace->ToDebugString(), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}

#undef LOCTEXT_NAMESPACE
