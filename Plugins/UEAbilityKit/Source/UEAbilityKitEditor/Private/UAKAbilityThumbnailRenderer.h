#pragma once

#include "CoreMinimal.h"
#include "ThumbnailRendering/ThumbnailRenderer.h"
#include "UAKAbilityThumbnailRenderer.generated.h"

/**
 * Ability Asset 的缩略图渲染器。
 *
 * 不真的渲染 3D 场景（那需要完整预览管线），而是画一张"技能结构示意图"：
 * 用轨道色条与任务时间块表达这条技能的形状，让内容浏览器里一眼能区分技能。
 */
UCLASS()
class UEABILITYKITEDITOR_API UAKAbilityThumbnailRenderer : public UThumbnailRenderer
{
	GENERATED_BODY()

public:
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height,
		FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily) override;

	virtual bool CanVisualizeAsset(UObject* Object) override { return true; }
};
