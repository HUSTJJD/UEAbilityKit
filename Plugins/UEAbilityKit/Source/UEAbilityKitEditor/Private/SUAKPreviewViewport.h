#pragma once

#include "CoreMinimal.h"
#include "PreviewScene.h"
#include "SEditorViewport.h"
#include "EditorViewportClient.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UAKAbilityAsset;
class UUAKPreviewRunner;

/**
 * 预览视口客户端：在独立的 FPreviewScene 中驱动预览角色与 Montage。
 *
 * 设计取舍：视口是**独立 Tab**（可关闭），而不是像 Able 那样常驻右上角。
 * 这样时间轴能占满主工作区，也避免"不预览时白白占着一块屏幕"。
 */
class FAKPreviewViewportClient : public FEditorViewportClient
{
public:
	FAKPreviewViewportClient(FPreviewScene& InPreviewScene, const TSharedRef<SEditorViewport>& InViewportWidget);

	// FEditorViewportClient
	virtual void Tick(float DeltaSeconds) override;
	virtual FSceneInterface* GetScene() const override { return PreviewScene->GetScene(); }
	virtual FLinearColor GetBackgroundColor() const override;

	/** 重建预览角色（切换资产或预览类时） */
	void RefreshPreviewActor(class UAKAbilityAsset* Asset);

	/** 相机回到侧视：站着不动的角色从这个角度最容易看清动作 */
	void ResetCamera();

	/** 地面：建一次，之后重建预览角色不动它 */
	void EnsureFloor();

	/**
	 * 真跑一次施放：重建角色 → 装 ASC → 从 0 跑到资产结束（Montage / Cue / 位移都算）。
	 *
	 * 返回 false 表示没跑起来（没有角色或没有资产），调用方回退到只播 Montage。
	 */
	bool PlayPreviewAbility(class UAKAbilityAsset* Asset);

	/** 播放 / 停止预览 Montage */
	void PlayPreviewMontage(class UAnimMontage* Montage);
	void StopPreview();

	void SetPlaying(bool bInPlaying) { bPlaying = bInPlaying; }
	bool IsPlaying() const { return bPlaying; }
	void SetLooping(bool bInLooping) { bLooping = bInLooping; }

private:
	FPreviewScene* PreviewScene = nullptr;
	TWeakObjectPtr<AActor> PreviewActor;

	/** 地面：没有它就没有投影与透视参照，看不出位移和跳跃 */
	TWeakObjectPtr<AActor> FloorActor;

	/** 一次施放的执行器（预览专用；用强引用持有，避免被 GC 收走） */
	TStrongObjectPtr<UUAKPreviewRunner> Runner;

	float PreviewTime = 0.f;
	bool bPlaying = false;
	bool bLooping = false;
};

/** 预览 Tab 的控件 */
class SUAKEditorViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SUAKEditorViewport) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UAKAbilityAsset>, Asset)
	SLATE_END_ARGS()

	SUAKEditorViewport();
	~SUAKEditorViewport();

	void Construct(const FArguments& InArgs);
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

	void OnAssetChanged(UAKAbilityAsset* Asset);

	/** 预览角色：当前那只是谁（资产没指定就显示"默认（XXX）"） */
	FText GetPreviewCharacterLabel() const;

	/**
	 * 预览角色下拉：跟随工程默认 / 指定一只。
	 * 选中的类写进资产的 PreviewCharacterClass —— 每个技能可以自己指定，
	 * 跟着资产一起保存，别人打开这个技能看到的就是同一只角色。
	 */
	TSharedRef<SWidget> MakePreviewCharacterMenu();
	void OnPreviewCharacterPicked(UClass* PickedClass);

	/** 预览控制（由外层 Tab 的工具条调用） */
	FReply OnPlayClicked();
	FReply OnStopClicked();
	void ToggleLooping();
	void ResetCamera();
	const FSlateBrush* GetLoopBrush() const;
	bool IsLooping() const { return bLooping; }

private:

	TSharedPtr<FPreviewScene> PreviewScene;
	TSharedPtr<FAKPreviewViewportClient> ViewportClient;
	TWeakObjectPtr<UAKAbilityAsset> Asset;
	bool bLooping = false;
};
