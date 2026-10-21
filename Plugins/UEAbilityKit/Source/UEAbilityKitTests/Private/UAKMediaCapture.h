#pragma once

#include "CoreMinimal.h"

class UAKTrace;

namespace UAKTest
{
	/**
	 * 测试素材采集器：把「逐帧截图」与「血缘树文本」写到
	 *
	 *   <Project>/Saved/UAKAutomation/<TestName>/frames/frame_00000.png
	 *   <Project>/Saved/UAKAutomation/<TestName>/<FileName>.txt
	 *
	 * 目录结构与 Scripts/report/generate_report.py 的 collect_media() 一一对应：
	 * 帧序列由 Python 用 ffmpeg 合成 mp4，txt 作为血缘树素材直接进 Showcase。
	 *
	 * 采集只在命令行带 -UAKCaptureMedia 时发生（Scripts/run_tests.ps1 默认带上）。
	 * headless / -NullRHI 下没有像素，Begin() 返回 false，用例据此"跳过并算通过"，
	 * 这样同一套用例在无 GPU 环境里不会假失败。
	 */
	class FUAKMediaRecorder
	{
	public:
		/** 是否开启了媒体采集：命令行 -UAKCaptureMedia（由 run_tests.ps1 -CaptureMedia 传入） */
		static bool IsMediaCaptureEnabled();

		/** 开始一次采集。未开启采集 / 建目录失败时返回 false */
		bool Begin(const FString& TestName);

		/**
		 * 请求一帧。每调用一次最多推进一帧：
		 *   - 第一次：发截图请求，返回 false（等引擎在渲染线程落地）
		 *   - 之后：发现文件已落盘 → 移动到 frames/ 并计数，返回 true
		 * 返回 true 表示"这一帧已经拿到"，调用方可以推进下一帧逻辑。
		 */
		bool CaptureFrame();

		/** 结束采集，返回实际落盘帧数 */
		int32 End();

		bool IsRecording() const { return bRecording; }
		int32 GetFrameCount() const { return FrameCount; }
		const FString& GetFramesDir() const { return FramesDir; }

	private:
		FString TestName;
		FString FramesDir;
		int32 FrameCount = 0;
		int32 FramesRequested = 0;
		bool bRecording = false;
		bool bShotPending = false;
		int32 WaitTicks = 0;
	};

	/**
	 * 把一棵血缘树写成文本（headless 也产出，是 Showcase 的兜底素材）。
	 * 写失败返回 false。
	 */
	bool SaveTraceText(const FString& TestName, const FString& FileName, const UAKTrace* Trace);
}
