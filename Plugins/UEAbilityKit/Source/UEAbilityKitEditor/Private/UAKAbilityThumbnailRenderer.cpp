#include "UAKAbilityThumbnailRenderer.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"
#include "UAKTypes.h"

#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"

void UAKAbilityThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height,
	FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	UAKAbilityAsset* Asset = Cast<UAKAbilityAsset>(Object);
	if (!Asset || !Canvas)
	{
		return;
	}

	// 背景
	FCanvasTileItem Background(FVector2D(X, Y), FVector2D(Width, Height), FLinearColor(0.08f, 0.08f, 0.09f));
	Background.Draw(Canvas);

	// 轨道色条：按轨道类型取色，纵向排列，横向按任务时间占比绘制时间块
	const float Margin = 4.f;
	const float TrackHeight = (Asset->Tracks.Num() > 0)
		? (Height - Margin * 2.f) / static_cast<float>(Asset->Tracks.Num())
		: 0.f;

	const float Length = FMath::Max(Asset->Length, 0.001f);
	float TrackY = Y + Margin;

	for (const UAKAbilityTrack* Track : Asset->Tracks)
	{
		if (!Track || TrackHeight <= 0.f)
		{
			continue;
		}

		FLinearColor TrackColor = FLinearColor(0.6f, 0.6f, 0.65f);
		switch (Track->TrackType)
		{
		case EAKTrackType::Main:			TrackColor = FLinearColor(0.85f, 0.85f, 0.90f); break;
		case EAKTrackType::Effect:			TrackColor = FLinearColor(0.95f, 0.35f, 0.35f); break;
		case EAKTrackType::Presentation:	TrackColor = FLinearColor(0.40f, 0.80f, 0.95f); break;
		case EAKTrackType::Query:			TrackColor = FLinearColor(0.55f, 0.85f, 0.45f); break;
		case EAKTrackType::Motion:			TrackColor = FLinearColor(0.95f, 0.75f, 0.30f); break;
		case EAKTrackType::Rule:			TrackColor = FLinearColor(0.75f, 0.50f, 0.95f); break;
		default: break;
		}

		if (!Track->bEnabled)
		{
			TrackColor *= 0.4f;
		}

		// 轨道底
		FCanvasTileItem TrackBar(FVector2D(X + Margin, TrackY),
			FVector2D(Width - Margin * 2.f, TrackHeight - 2.f), TrackColor * 0.25f);
		TrackBar.Draw(Canvas);

		// 任务时间块
		for (const UAKAbilityTaskBase* Task : Track->Tasks)
		{
			if (!Task)
			{
				continue;
			}

			const float StartPct = FMath::Clamp(Task->StartTime / Length, 0.f, 1.f);
			const float EndPct = FMath::Clamp(Task->GetEndTime() / Length, 0.f, 1.f);
			const float BarWidth = FMath::Max(2.f, (EndPct - StartPct) * (Width - Margin * 2.f));
			const float BarX = X + Margin + StartPct * (Width - Margin * 2.f);

			FCanvasTileItem TaskBar(FVector2D(BarX, TrackY), FVector2D(BarWidth, TrackHeight - 2.f), TrackColor);
			TaskBar.Draw(Canvas);
		}

		TrackY += TrackHeight;
	}

	// 底部：技能名
	const FString Name = Asset->DisplayName.IsEmpty() ? Asset->GetName() : Asset->DisplayName.ToString();
	FCanvasTextItem TextItem(FVector2D(X + Margin, Y + Height - 14.f), FText::FromString(Name),
		GEngine->GetSmallFont(), FLinearColor(0.85f, 0.85f, 0.85f));
	TextItem.ShadowColor = FLinearColor::Black;
	TextItem.bOutlined = true;
	TextItem.Draw(Canvas);
}
