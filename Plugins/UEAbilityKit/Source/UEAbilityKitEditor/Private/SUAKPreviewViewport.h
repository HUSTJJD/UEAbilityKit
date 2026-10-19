#pragma once

#include "CoreMinimal.h"
#include "PreviewScene.h"
#include "SEditorViewport.h"
#include "EditorViewportClient.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UAKAbilityAsset;

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

	/** 播放 / 停止预览 Montage */
	void PlayPreviewMontage(class UAnimMontage* Montage);
	void StopPreview();

	void SetPlaying(bool bInPlaying) { bPlaying = bInPlaying; }
	bool IsPlaying() const { return bPlaying; }
	void SetLooping(bool bInLooping) { bLooping = bInLooping; }

private:
	FPreviewScene* PreviewScene = nullptr;
	TWeakObjectPtr<AActor> PreviewActor;
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

	/** 预览控制（由外层 Tab 的工具条调用） */
	FReply OnPlayClicked();
	FReply OnStopClicked();
	void ToggleLooping();
	const FSlateBrush* GetLoopBrush() const;
	bool IsLooping() const { return bLooping; }

private:

	TSharedPtr<FPreviewScene> PreviewScene;
	TSharedPtr<FAKPreviewViewportClient> ViewportClient;
	TWeakObjectPtr<UAKAbilityAsset> Asset;
	bool bLooping = false;
};
