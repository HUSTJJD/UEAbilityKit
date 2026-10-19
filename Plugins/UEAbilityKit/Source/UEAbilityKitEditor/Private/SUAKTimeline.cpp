#include "SUAKTimeline.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"
#include "UAKTypes.h"

#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MenuBuilder.h"
#include "Rendering/DrawElements.h"
#include "ScopedTransaction.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "SUAKTimeline"

namespace
{
	const FLinearColor ColorBackground(0.043f, 0.043f, 0.043f);
	const FLinearColor ColorHeader(0.075f, 0.075f, 0.075f);
	const FLinearColor ColorGrid(0.11f, 0.11f, 0.11f);
	const FLinearColor ColorGridMajor(0.16f, 0.16f, 0.16f);
	const FLinearColor ColorRuler(0.055f, 0.055f, 0.055f);
	const FLinearColor ColorCursor(1.f, 0.85f, 0.2f);

	FLinearColor ColorForTrack(EAKTrackType Type)
	{
		switch (Type)
		{
		case EAKTrackType::Main:			return FLinearColor(0.85f, 0.85f, 0.90f);
		case EAKTrackType::Effect:			return FLinearColor(0.95f, 0.35f, 0.35f);
		case EAKTrackType::Presentation:	return FLinearColor(0.40f, 0.80f, 0.95f);
		case EAKTrackType::Query:			return FLinearColor(0.55f, 0.85f, 0.45f);
		case EAKTrackType::Motion:			return FLinearColor(0.95f, 0.75f, 0.30f);
		case EAKTrackType::Rule:			return FLinearColor(0.75f, 0.50f, 0.95f);
		case EAKTrackType::Custom:
		default:							return FLinearColor(0.60f, 0.60f, 0.65f);
		}
	}
}

void SUAKTimeline::Construct(const FArguments& InArgs)
{
	Asset = InArgs._Asset;
	OnTaskSelected = InArgs._OnTaskSelected;
	OnTrackSelected = InArgs._OnTrackSelected;
	OnCursorTimeChanged = InArgs._OnCursorTimeChanged;
	OnRequestDeleteTask = InArgs._OnRequestDeleteTask;
	OnRequestDuplicateTask = InArgs._OnRequestDuplicateTask;
	OnAction = InArgs._OnAction;

	SetCanTick(true);
	SetCanSupportFocus(true);
	SetClipping(EWidgetClipping::ClipToBounds);

	RebuildPlanIfNeeded();
}

void SUAKTimeline::RebuildPlanIfNeeded()
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		CachedPlan.Reset();
		LastPlanSignature.Empty();
		return;
	}

	// 用"轨道设置 + 任务时间"做指纹，避免每次 Tick 都重建
	FString Signature;
	for (const UAKAbilityTrack* Track : AssetPtr->Tracks)
	{
		if (!Track) { continue; }
		Signature += FString::Printf(TEXT("%d|%d|%d|%.3f|%.3f|"),
			static_cast<int32>(Track->CompositeMode), Track->RepeatCount,
			Track->bEnabled ? 1 : 0, Track->RepeatInterval, Track->StartDelay);
		for (const UAKAbilityTaskBase* Task : Track->Tasks)
		{
			if (!Task) { continue; }
			Signature += FString::Printf(TEXT("%s:%.3f:%.3f;"), *Task->GetClass()->GetName(), Task->StartTime, Task->Duration);
		}
	}

	if (Signature == LastPlanSignature)
	{
		return;
	}

	LastPlanSignature = Signature;
	AssetPtr->BuildExecutionPlan(CachedPlan, /*bOnlyEnabledTracks=*/false);
}

void SUAKTimeline::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	ViewWidth = FMath::Max(0.f, AllottedGeometry.GetLocalSize().X - TrackHeaderWidth);
	ViewHeight = AllottedGeometry.GetLocalSize().Y;

	PlanRefreshTimer += InDeltaTime;
	if (PlanRefreshTimer >= 0.25f)
	{
		PlanRefreshTimer = 0.f;
		RebuildPlanIfNeeded();
	}

	// 播放：按真实时间推进光标（与运行时一样受 PlayRate 影响）
	if (bPlaying)
	{
		const float Length = GetAssetLength();
		float Next = CursorTime + InDeltaTime * PlaybackRate;

		if (Next >= Length)
		{
			if (bLoopPlayback)
			{
				Next = 0.f;
			}
			else
			{
				Next = Length;
				Pause();
			}
		}

		SetCursorTime(Next);
	}
}

float SUAKTimeline::GetAssetLength() const
{
	const UAKAbilityAsset* AssetPtr = Asset.Get();
	return AssetPtr ? FMath::Max(AssetPtr->Length, 0.001f) : 1.f;
}

void SUAKTimeline::Play()
{
	if (CursorTime >= GetAssetLength())
	{
		CursorTime = 0.f;
	}
	bPlaying = true;
}

void SUAKTimeline::Pause()
{
	bPlaying = false;
}

void SUAKTimeline::TogglePlay()
{
	bPlaying ? Pause() : Play();
}

void SUAKTimeline::Stop()
{
	bPlaying = false;
	SetCursorTime(0.f);
}

void SUAKTimeline::SetCursorTime(float InTime)
{
	CursorTime = FMath::Max(0.f, InTime);
	OnCursorTimeChanged.ExecuteIfBound(CursorTime);
}

void SUAKTimeline::StepFrame(int32 Frames)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	const float FrameRate = AssetPtr ? FMath::Max(1, AssetPtr->FrameRate) : 60;
	SetCursorTime(CursorTime + static_cast<float>(Frames) / static_cast<float>(FrameRate));
}

float SUAKTimeline::SnapTime(float Time) const
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		return Time;
	}
	const float Frame = 1.f / static_cast<float>(FMath::Max(1, AssetPtr->FrameRate));
	return FMath::RoundToFloat(Time / Frame) * Frame;
}

float SUAKTimeline::TimeToX(float Time) const
{
	return TrackHeaderWidth + Time * PixelsPerSecond;
}

float SUAKTimeline::XToTime(float X) const
{
	return (X - TrackHeaderWidth) / PixelsPerSecond;
}

int32 SUAKTimeline::HitTestRow(float Y) const
{
	if (Y < RulerHeight)
	{
		return INDEX_NONE;
	}
	return static_cast<int32>((Y - RulerHeight) / RowHeight);
}

UAKAbilityTaskBase* SUAKTimeline::HitTestTask(float X, int32 Row) const
{
	// 用执行计划做命中测试，保证 Sequence / Parallel / Repeat 下点到的位置与实际运行一致
	const float Time = XToTime(X);
	for (const FAKScheduledTask& Scheduled : CachedPlan)
	{
		if (Scheduled.TrackIndex != Row || !Scheduled.Task)
		{
			continue;
		}
		if (Time >= Scheduled.StartTime && Time <= Scheduled.EndTime)
		{
			return Scheduled.Task;
		}
	}
	return nullptr;
}

FReply SUAKTimeline::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	// 点轨道头：色条区 = 切换整轨开关（UAK 独有的整轨静音）；名称区 = 选中轨道改复合设置
	if (Local.X < TrackHeaderWidth)
	{
		const int32 Row = HitTestRow(Local.Y);
		UAKAbilityAsset* AssetPtr = Asset.Get();
		if (AssetPtr && AssetPtr->Tracks.IsValidIndex(Row) && AssetPtr->Tracks[Row])
		{
			UAKAbilityTrack* Track = AssetPtr->Tracks[Row];
			if (Local.X <= 12.f)
			{
				Track->Modify();
				Track->bEnabled = !Track->bEnabled;
				AssetPtr->MarkPackageDirty();
			}
			else
			{
				SelectedTrackIndex = Row;
				OnTrackSelected.ExecuteIfBound(Track, Row);
			}
		}
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	// 标尺区：移动光标
	const int32 Row = HitTestRow(Local.Y);
	if (Row == INDEX_NONE)
	{
		SetCursorTime(SnapTime(XToTime(Local.X)));
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	// 右键：弹出上下文菜单
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		UAKAbilityTaskBase* RightHit = (Row == INDEX_NONE) ? nullptr : HitTestTask(Local.X, Row);
		if (RightHit)
		{
			SelectedTask = RightHit;
			OnTaskSelected.ExecuteIfBound(RightHit);
		}
		if (Row != INDEX_NONE)
		{
			SelectedTrackIndex = Row;
		}

		ShowContextMenu(MouseEvent, MyGeometry, RightHit, Row);
		return FReply::Handled();
	}

	UAKAbilityTaskBase* Hit = HitTestTask(Local.X, Row);

	// Alt + 拖拽：复制一份再拖（复制动作由编辑器完成，这里只接管拖动）
	if (Hit && MouseEvent.IsAltDown())
	{
		if (UAKAbilityTaskBase* Copy = OnRequestDuplicateTask.Execute())
		{
			Hit = Copy;
		}
	}

	if (Hit)
	{
		SelectedTask = Hit;
		OnTaskSelected.ExecuteIfBound(Hit);

		DraggingTask = Hit;
		DragGrabOffset = XToTime(Local.X) - Hit->StartTime;
		bDragging = true;

		// 一次拖动 = 一次可撤销的事务（撤销粒度而不是每帧一个）
		DragTransaction = MakeUnique<FScopedTransaction>(LOCTEXT("MoveTask", "移动任务"));
		if (UAKAbilityAsset* AssetPtr = Asset.Get())
		{
			AssetPtr->Modify();
		}
	}
	else
	{
		SelectedTask = nullptr;
		OnTaskSelected.ExecuteIfBound(nullptr);
		SetCursorTime(SnapTime(XToTime(Local.X)));
	}

	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SUAKTimeline::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bDragging)
	{
		return FReply::Unhandled();
	}

	UAKAbilityTaskBase* Task = DraggingTask.Get();
	if (!Task)
	{
		return FReply::Unhandled();
	}

	// 复合轨道（Sequence / Parallel）的任务时间由轨道计算，不允许手动改绝对时间
	if (const UAKAbilityTrack* OwnerTrack = Cast<UAKAbilityTrack>(Task->GetOuter()))
	{
		if (OwnerTrack->CompositeMode != EAKTrackCompositeMode::Timeline)
		{
			return FReply::Handled();
		}
	}

	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const float NewStart = FMath::Max(0.f, SnapTime(XToTime(Local.X) - DragGrabOffset));

	if (!FMath::IsNearlyEqual(NewStart, Task->StartTime))
	{
		Task->Modify();
		Task->StartTime = NewStart;
		if (UAKAbilityAsset* AssetPtr = Asset.Get())
		{
			AssetPtr->MarkPackageDirty();
		}
	}

	return FReply::Handled();
}

FReply SUAKTimeline::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bDragging = false;
	DraggingTask = nullptr;

	// 结束拖动事务（析构即提交）
	DragTransaction.Reset();

	return FReply::Handled().ReleaseMouseCapture();
}

void SUAKTimeline::ForceRefresh()
{
	LastPlanSignature.Empty();
	RebuildPlanIfNeeded();
}

TSharedRef<SWidget> SUAKTimeline::GenerateContextMenu(UAKAbilityTaskBase* HitTask, int32 Row)
{
	const bool bHasTask = (HitTask != nullptr);

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	MenuBuilder.BeginSection(TEXT("Task"), LOCTEXT("MenuSection_Task", "任务"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_DuplicateTask", "复制任务"),
		LOCTEXT("Menu_DuplicateTaskTip", "复制当前选中的任务（Ctrl+D）"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { OnRequestDuplicateTask.ExecuteIfBound(); }),
			FCanExecuteAction::CreateLambda([bHasTask]() { return bHasTask; })));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_DeleteTask", "删除任务"),
		LOCTEXT("Menu_DeleteTaskTip", "删除当前选中的任务（Delete）"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { OnRequestDeleteTask.ExecuteIfBound(); }),
			FCanExecuteAction::CreateLambda([bHasTask]() { return bHasTask; })));
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(TEXT("Track"), LOCTEXT("MenuSection_Track", "轨道"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_AddTrack", "添加轨道"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { OnAction.ExecuteIfBound(EAKToolbarAction::AddTrack); })));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_RemoveTrack", "删除当前轨道"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { OnAction.ExecuteIfBound(EAKToolbarAction::RemoveTrack); })));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_TrackUp", "上移轨道"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { OnAction.ExecuteIfBound(EAKToolbarAction::MoveTrackUp); })));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_TrackDown", "下移轨道"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { OnAction.ExecuteIfBound(EAKToolbarAction::MoveTrackDown); })));
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(TEXT("Asset"), LOCTEXT("MenuSection_Asset", "资产"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_AddMarker", "在此处添加标记"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { OnAction.ExecuteIfBound(EAKToolbarAction::AddMarker); })));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_SelectAsset", "资产属性"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { OnAction.ExecuteIfBound(EAKToolbarAction::SelectAsset); })));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_GenFromMontage", "从 Montage 生成"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { OnAction.ExecuteIfBound(EAKToolbarAction::GenerateFromMontage); })));
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SUAKTimeline::ShowContextMenu(const FPointerEvent& MouseEvent, const FGeometry& MyGeometry, UAKAbilityTaskBase* HitTask, int32 Row)
{
	FWidgetPath WidgetPath = MouseEvent.GetEventPath() ? *MouseEvent.GetEventPath() : FWidgetPath();

	FSlateApplication::Get().PushMenu(
		SharedThis(this),
		WidgetPath,
		GenerateContextMenu(HitTask, Row),
		MouseEvent.GetScreenSpacePosition(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}

FReply SUAKTimeline::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const float Delta = MouseEvent.GetWheelDelta();
	PixelsPerSecond = FMath::Clamp(PixelsPerSecond * (1.f + Delta * 0.1f), 20.f, 4000.f);
	return FReply::Handled();
}

FReply SUAKTimeline::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	// 播放 / 暂停
	if (Key == EKeys::SpaceBar)
	{
		TogglePlay();
		return FReply::Handled();
	}

	// 单帧步进
	if (Key == EKeys::Right)
	{
		StepFrame(InKeyEvent.IsControlDown() ? 10 : 1);
		return FReply::Handled();
	}
	if (Key == EKeys::Left)
	{
		StepFrame(InKeyEvent.IsControlDown() ? -10 : -1);
		return FReply::Handled();
	}

	// 复制（Ctrl+D）
	if (Key == EKeys::D && InKeyEvent.IsControlDown())
	{
		OnRequestDuplicateTask.ExecuteIfBound();
		return FReply::Handled();
	}

	// 删除（Delete / Backspace）
	if (Key == EKeys::Delete || Key == EKeys::BackSpace)
	{
		OnRequestDeleteTask.ExecuteIfBound();
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

int32 SUAKTimeline::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	ViewWidth = FMath::Max(0.f, AllottedGeometry.GetLocalSize().X - TrackHeaderWidth);
	ViewHeight = AllottedGeometry.GetLocalSize().Y;

	LayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	DrawRuler(AllottedGeometry, OutDrawElements, LayerId);
	DrawTrackRows(AllottedGeometry, OutDrawElements, LayerId + 1);
	DrawSegments(AllottedGeometry, OutDrawElements, LayerId + 2);
	DrawCursor(AllottedGeometry, OutDrawElements, LayerId + 3);

	return LayerId + 4;
}

void SUAKTimeline::DrawRuler(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	// 标尺底
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
		Geometry.ToPaintGeometry(FVector2D(0, 0), FVector2D(Geometry.GetLocalSize().X, RulerHeight)),
		FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, ColorRuler);

	const float Width = Geometry.GetLocalSize().X;
	const float StartTime = XToTime(TrackHeaderWidth);
	const float EndTime = XToTime(Width);

	// 目标：屏幕上每 ~80px 一个刻度
	float Step = 0.1f;
	while (Step * PixelsPerSecond < 80.f)
	{
		Step *= (FMath::IsNearlyEqual(Step, 0.1f) ? 5.f : 2.f);
		if (Step > 60.f) { break; }
	}

	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	const float FirstTick = FMath::FloorToFloat(StartTime / Step) * Step;

	for (float T = FirstTick; T <= EndTime; T += Step)
	{
		const float X = TimeToX(T);
		if (X < TrackHeaderWidth - 1.f || X > Width)
		{
			continue;
		}

		const bool bMajor = FMath::IsNearlyEqual(FMath::Fmod(T, Step * 5.f), 0.f, Step * 0.01f);
		const FLinearColor Color = bMajor ? ColorGridMajor : ColorGrid;

		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			Geometry.ToPaintGeometry(FVector2D(X, RulerHeight - (bMajor ? 10.f : 5.f)), FVector2D(1.f, bMajor ? 10.f : 5.f)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color);

		if (bMajor)
		{
			UAKAbilityAsset* AssetPtr = Asset.Get();
			const int32 FrameRate = AssetPtr ? FMath::Max(1, AssetPtr->FrameRate) : 60;
			const FText Label = bShowFrames
				? FText::AsNumber(FMath::RoundToInt(T * FrameRate))
				: FText::Format(LOCTEXT("TimeLabel", "{0}s"), FText::AsNumber(FMath::RoundToFloat(T * 100.f) / 100.f));

			FSlateDrawElement::MakeText(OutDrawElements, LayerId,
				Geometry.ToPaintGeometry(FVector2D(X + 3.f, 3.f), FVector2D(60.f, 14.f)),
				Label, Font, ESlateDrawEffect::None, FLinearColor(0.65f, 0.65f, 0.65f));
		}
	}
}

void SUAKTimeline::DrawTrackRows(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		return;
	}

	const float Width = Geometry.GetLocalSize().X;
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);

	for (int32 Row = 0; Row < AssetPtr->Tracks.Num(); ++Row)
	{
		UAKAbilityTrack* Track = AssetPtr->Tracks[Row];
		const float Y = RulerHeight + Row * RowHeight;

		// 行底
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			Geometry.ToPaintGeometry(FVector2D(0, Y), FVector2D(Width, RowHeight - 1.f)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None,
			(Row % 2 == 0) ? ColorBackground : FLinearColor(0.05f, 0.05f, 0.05f));

		// 轨道头
		const FLinearColor TrackColor = Track ? ColorForTrack(Track->TrackType) : FLinearColor::Gray;
		const FLinearColor HeaderColor = Track && !Track->bEnabled ? FLinearColor(0.05f, 0.05f, 0.05f) : ColorHeader;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			Geometry.ToPaintGeometry(FVector2D(0, Y), FVector2D(TrackHeaderWidth - 1.f, RowHeight - 1.f)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, HeaderColor);

		// 轨道色条 + 开关方块
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
			Geometry.ToPaintGeometry(FVector2D(4.f, Y + 6.f), FVector2D(6.f, RowHeight - 13.f)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None,
			Track && Track->bEnabled ? TrackColor : FLinearColor(0.25f, 0.25f, 0.25f));

		if (Track)
		{
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
				Geometry.ToPaintGeometry(FVector2D(18.f, Y + 7.f), FVector2D(TrackHeaderWidth - 24.f, 16.f)),
				Track->GetDisplayName(), Font, ESlateDrawEffect::None,
				Track->bEnabled ? FLinearColor(0.85f, 0.85f, 0.85f) : FLinearColor(0.4f, 0.4f, 0.4f));
		}

		// 时间轴网格
		const float StartTime = XToTime(TrackHeaderWidth);
		const float EndTime = XToTime(Width);
		float Step = 0.1f;
		while (Step * PixelsPerSecond < 80.f)
		{
			Step *= (FMath::IsNearlyEqual(Step, 0.1f) ? 5.f : 2.f);
			if (Step > 60.f) { break; }
		}
		for (float T = FMath::FloorToFloat(StartTime / Step) * Step; T <= EndTime; T += Step)
		{
			const float X = TimeToX(T);
			if (X < TrackHeaderWidth || X > Width) { continue; }
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
				Geometry.ToPaintGeometry(FVector2D(X, Y), FVector2D(1.f, RowHeight - 1.f)),
				FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, ColorGrid);
		}
	}
}

void SUAKTimeline::DrawSegments(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		return;
	}

	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	const float Width = Geometry.GetLocalSize().X;

	// 按执行计划绘制：复合轨道展开后的真实时间，做到"看到的即跑出来的"
	for (const FAKScheduledTask& Scheduled : CachedPlan)
	{
		const int32 Row = Scheduled.TrackIndex;
		if (!AssetPtr->Tracks.IsValidIndex(Row))
		{
			continue;
		}

		UAKAbilityTrack* Track = AssetPtr->Tracks[Row];
		if (!Track)
		{
			continue;
		}

		UAKAbilityTaskBase* Task = Scheduled.Task;
		if (!Task)
		{
			continue;
		}

		const float Y = RulerHeight + Row * RowHeight;
		const FLinearColor TrackColor = ColorForTrack(Track->TrackType);

		{
			const float X0 = TimeToX(Scheduled.StartTime);
			const float X1 = TimeToX(Scheduled.EndTime);
			const float W = FMath::Max(4.f, X1 - X0);
			if (X0 > Width || X1 < TrackHeaderWidth)
			{
				continue;
			}

			const bool bSelected = (SelectedTask.Get() == Task);
			FLinearColor Fill = Track->bEnabled ? TrackColor : FLinearColor(0.3f, 0.3f, 0.3f);
			Fill.A = bSelected ? 0.95f : 0.7f;

			FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
				Geometry.ToPaintGeometry(FVector2D(X0, Y + 4.f), FVector2D(W, RowHeight - 9.f)),
				FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Fill);

			if (bSelected)
			{
				// 选中描边
				const FLinearColor Outline(1.f, 1.f, 1.f, 0.9f);
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
					Geometry.ToPaintGeometry(FVector2D(X0, Y + 4.f), FVector2D(W, 1.f)),
					FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Outline);
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
					Geometry.ToPaintGeometry(FVector2D(X0, Y + RowHeight - 5.f), FVector2D(W, 1.f)),
					FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Outline);
			}

			if (W > 30.f)
			{
#if WITH_EDITOR
				const FText Label = Task->GetTaskDisplayName();
#else
				const FText Label = FText::FromString(
					Task->GetClass()->GetName().Replace(TEXT("UAKAbilityTask_"), TEXT("")));
#endif
				FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
					Geometry.ToPaintGeometry(FVector2D(X0 + 4.f, Y + 7.f), FVector2D(W - 8.f, 14.f)),
					Label, Font, ESlateDrawEffect::None, FLinearColor(0.05f, 0.05f, 0.05f));
			}
		}
	}

	// Markers（命名时间点）
	for (const FAKTimeMarker& Marker : AssetPtr->Markers)
	{
		const float X = TimeToX(Marker.Time);
		if (X < TrackHeaderWidth || X > Width) { continue; }

		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
			Geometry.ToPaintGeometry(FVector2D(X, RulerHeight), FVector2D(1.f, ViewHeight - RulerHeight)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.4f, 0.75f, 0.95f, 0.55f));

		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
			Geometry.ToPaintGeometry(FVector2D(X + 3.f, RulerHeight + 2.f), FVector2D(80.f, 12.f)),
			FText::FromName(Marker.Name), Font, ESlateDrawEffect::None, FLinearColor(0.4f, 0.75f, 0.95f));
	}
}

void SUAKTimeline::DrawCursor(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	const float X = TimeToX(CursorTime);
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
		Geometry.ToPaintGeometry(FVector2D(X, 0.f), FVector2D(1.5f, ViewHeight)),
		FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, ColorCursor);
}

#undef LOCTEXT_NAMESPACE
