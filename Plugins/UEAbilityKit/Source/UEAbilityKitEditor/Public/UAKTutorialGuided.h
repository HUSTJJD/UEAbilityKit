#pragma once

#include "CoreMinimal.h"
#include "UAKTutorialTypes.h"

/**
 * 交互引导桥接：把同一份 Tutorials/*.json 变成引擎原生的「引导式教程」。
 *
 * 为什么还要这一层（录制模式 UAK.Tutorial.Run 已经能跑通全部步骤了）：
 *   录制模式是**引擎替你点**，产出的是文档素材——适合学"有什么、长什么样"；
 *   引导模式是**你自己点**，引擎在旁边提示与判定——适合学"手上怎么操作"。
 * 两者共用同一份脚本，所以改一处两边都更新，不会出现"文档教 A、引导教 B"。
 *
 * 为什么不直接写 .uasset 教程资产：
 *   1. 资产是二进制，进不了 review，也没法被自动化用例断言；
 *   2. 运行时 NewObject<UEditorTutorial> 一样能启动，字段是公开的 UPROPERTY；
 *   3. 这样"脚本 → 引导"是一次纯代码变换，随时可重算，不需要人去资产里点。
 *
 * 依赖策略（三层降级，任何一层不满足都不会让 UAK 挂掉）：
 *   编译期：UAK_WITH_INTRO_TUTORIALS —— 引擎里没有 IntroTutorials 源码就不编进来；
 *   运行期：IIntroTutorials::IsAvailable() —— 插件没启用则 Start() 明确报错并建议替代方案；
 *   使用期：步骤默认仍需读者点「下一步」，只有脚本显式写了 advance 才自动推进。
 */
class FUAKTutorialGuided
{
public:
	static FUAKTutorialGuided& Get();

	/** 引擎这一侧到底能不能用（编译进来了 + 插件已加载） */
	bool IsAvailable() const;

	bool IsRunning() const;

	/**
	 * 启动引导。
	 * @param TutorialOrIdOrPath 教程 id（如 01-first-ability）、或脚本完整路径
	 */
	bool Start(const FString& TutorialOrIdOrPath, FString& OutError);
	void Stop();

	/** 注册 UAK.Tutorial.Guided / UAK.Tutorial.GuidedStop */
	static void Register();
	static void Unregister();
};
