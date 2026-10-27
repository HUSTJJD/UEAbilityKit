#include "SUAKTimeline.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"
#include "UAKTypes.h"

#include "Dom/JsonObject.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Rendering/DrawElements.h"
#include "Serialization/JsonSerializer.h"
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

	/**
	 * 画一块矩形要的 PaintGeometry。
	 *
	 * 引擎 5.2 起 FGeometry::ToPaintGeometry(偏移, 尺寸) 被标记弃用，新签名是
	 * (局部尺寸, 布局变换)——参数顺序反了，而且尺寸/偏移收窄成 float。
	 * 这里收口成"偏移 + 尺寸"，调用点不用再关心这两件事。
	 * 布局变换的偏移与旧版的 LocalOffset 同为局部空间，scale 为 1 时两者等价。
	 */
	FPaintGeometry PaintGeom(const FGeometry& Geometry, const FVector2D& Offset, const FVector2D& Size)
	{
		return Geometry.ToPaintGeometry(
			UE::Slate::CastToVector2f(Size),
			FSlateLayoutTransform(UE::Slate::CastToVector2f(Offset)));
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
	SetClipping(EWidgetClipping::ClipToBounds);

	// tooltip 跟着悬停走：片段显示"几点到几点、多长"，空白处显示"光标在哪一帧"
	SetToolTipText(TAttribute<FText>::Create(
		TAttribute<FText>::FGetter::CreateSP(this, &SUAKTimeline::GetHoverTooltipText)));

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

	// 播放：按真实时间推进光标（与运行时一样受资产的 PlayRate 影响）
	if (bPlaying)
	{
		const float Length = GetAssetLength();
		PlaybackRate = (Asset.Get() != nullptr) ? FMath::Max(0.01f, Asset.Get()->PlayRate) : 1.f;
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
	// 两端都钳：光标不能跑到 0 之前，也不能越过技能末尾（否则"此刻生效"的统计会说谎）
	CursorTime = FMath::Clamp(InTime, 0.f, GetAssetLength());
	OnCursorTimeChanged.ExecuteIfBound(CursorTime);
}

float SUAKTimeline::GetViewEndTime() const
{
	return ViewOffsetTime + (PixelsPerSecond > 0.f ? ViewWidth / PixelsPerSecond : 1.f);
}

void SUAKTimeline::ClampViewOffset()
{
	const float VisibleTime = (PixelsPerSecond > 0.f) ? ViewWidth / PixelsPerSecond : 1.f;

	// 内容末端 = 技能长度与最后一个任务结束时间取大，再留一点余量
	float ContentEnd = GetAssetLength();
	for (const FAKScheduledTask& Scheduled : CachedPlan)
	{
		ContentEnd = FMath::Max(ContentEnd, Scheduled.EndTime);
	}
	if (const UAKAbilityAsset* AssetPtr = Asset.Get())
	{
		for (const FAKTimeMarker& Marker : AssetPtr->Markers)
		{
			ContentEnd = FMath::Max(ContentEnd, Marker.Time);
		}
	}
	ContentEnd += VisibleTime * 0.05f;

	const float MaxOffset = FMath::Max(0.f, ContentEnd - VisibleTime * 0.9f);
	ViewOffsetTime = FMath::Clamp(ViewOffsetTime, 0.f, MaxOffset);
}

void SUAKTimeline::ScrollTimeIntoView(float Time)
{
	const float VisibleTime = (PixelsPerSecond > 0.f) ? ViewWidth / PixelsPerSecond : 1.f;
	if (Time < ViewOffsetTime + VisibleTime * 0.05f || Time > ViewOffsetTime + VisibleTime * 0.95f)
	{
		// 落在视野外就把它放到左侧 1/4 处，前后都留得下上下文
		ViewOffsetTime = Time - VisibleTime * 0.25f;
		ClampViewOffset();
	}
}

void SUAKTimeline::FocusTask(UAKAbilityTaskBase* Task)
{
	if (!Task)
	{
		return;
	}

	SelectedTask = Task;
	OnTaskSelected.ExecuteIfBound(Task);

	const float StartTime = Task->StartTime;
	SetCursorTime(StartTime);
	ScrollTimeIntoView(StartTime);
}

bool SUAKTimeline::CanEditTaskTimes(const UAKAbilityTaskBase* Task) const
{
	// 复合轨道（Sequence / Parallel / Repeat）的任务时间由轨道算出来，
	// 手动改 StartTime / Duration 没有意义，一律不允许拖（与 OnMouseMove 的旧判断一致）。
	const UAKAbilityTrack* OwnerTrack = Cast<UAKAbilityTrack>(Task->GetOuter());
	return OwnerTrack && OwnerTrack->CompositeMode == EAKTrackCompositeMode::Timeline;
}

void SUAKTimeline::MoveTaskToTrack(UAKAbilityTaskBase* Task, int32 FromTrackIndex, int32 ToTrackIndex)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !Task || FromTrackIndex == ToTrackIndex)
	{
		return;
	}

	UAKAbilityTrack* FromTrack = AssetPtr->Tracks.IsValidIndex(FromTrackIndex) ? AssetPtr->Tracks[FromTrackIndex] : nullptr;
	UAKAbilityTrack* ToTrack = AssetPtr->Tracks.IsValidIndex(ToTrackIndex) ? AssetPtr->Tracks[ToTrackIndex] : nullptr;
	if (!FromTrack || !ToTrack)
	{
		return;
	}

	FromTrack->Modify();
	ToTrack->Modify();
	Task->Modify();

	FromTrack->Tasks.Remove(Task);
	ToTrack->Tasks.Add(Task);

	// 任务是内联子对象：改父级要连 Outer 一起改，否则保存与 GC 会认错父亲
	Task->Rename(nullptr, ToTrack, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);

	AssetPtr->MarkPackageDirty();
	ForceRefresh();
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
	const float FrameSnapped = FMath::RoundToFloat(Time / Frame) * Frame;

	// Marker 吸附：在 6px 范围内优先吸附到 Marker。
	// Marker 是语义锚点（起手 / 命中 / 收招），比等距网格更值得对齐——
	// 这一条让"把 GE 挪到出招那一帧"变成一次拖动就能完成的事。
	const float Threshold = 6.f / FMath::Max(1.f, PixelsPerSecond);
	float Best = FrameSnapped;
	float BestDelta = Threshold;

	for (const FAKTimeMarker& Marker : AssetPtr->Markers)
	{
		const float Delta = FMath::Abs(Marker.Time - FrameSnapped);
		if (Delta <= BestDelta)
		{
			Best = Marker.Time;
			BestDelta = Delta;
		}
	}

	return Best;
}

float SUAKTimeline::TimeToX(float Time) const
{
	return TrackHeaderWidth + (Time - ViewOffsetTime) * PixelsPerSecond;
}

float SUAKTimeline::XToTime(float X) const
{
	return ViewOffsetTime + (X - TrackHeaderWidth) / PixelsPerSecond;
}

int32 SUAKTimeline::HitTestRow(float Y) const
{
	if (Y < RulerHeight)
	{
		return INDEX_NONE;
	}
	return static_cast<int32>((Y - RulerHeight) / RowHeight);
}

int32 SUAKTimeline::HitTestMarker(float X) const
{
	const UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		return INDEX_NONE;
	}

	// 标记线只有 1px 宽，给 ±5px 的手感范围
	for (int32 Index = 0; Index < AssetPtr->Markers.Num(); ++Index)
	{
		if (FMath::Abs(TimeToX(AssetPtr->Markers[Index].Time) - X) <= 5.f)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

EAKTimelineDragMode SUAKTimeline::HitTestSegmentEdge(float X, int32 Row, UAKAbilityTaskBase*& OutTask, const FAKScheduledTask*& OutScheduled) const
{
	OutTask = nullptr;
	OutScheduled = nullptr;

	UAKAbilityTaskBase* Task = HitTestTask(X, Row);
	if (!Task)
	{
		return EAKTimelineDragMode::None;
	}

	const FAKScheduledTask* Scheduled = CachedPlan.FindByPredicate(
		[Task, Row](const FAKScheduledTask& Item) { return Item.Task == Task && Item.TrackIndex == Row; });
	if (!Scheduled)
	{
		return EAKTimelineDragMode::None;
	}

	OutTask = Task;
	OutScheduled = Scheduled;

	if (!CanEditTaskTimes(Task))
	{
		return EAKTimelineDragMode::Move;
	}

	const float X0 = TimeToX(Scheduled->StartTime);
	const float X1 = TimeToX(Scheduled->EndTime);
	const float Width = X1 - X0;
	// 短片段按比例收窄把手，避免整块都变成"拉伸区"而无法整体移动
	const float Edge = FMath::Max(2.f, FMath::Min(6.f, Width * 0.3f));

	if (FMath::Abs(X - X0) <= Edge && X <= (X0 + X1) * 0.5f)
	{
		return EAKTimelineDragMode::TrimLeft;
	}
	if (FMath::Abs(X - X1) <= Edge)
	{
		return EAKTimelineDragMode::TrimRight;
	}
	return EAKTimelineDragMode::Move;
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
	const FKey Button = MouseEvent.GetEffectingButton();
	const bool bLeft = (Button == EKeys::LeftMouseButton);
	const bool bRight = (Button == EKeys::RightMouseButton);
	const bool bMiddle = (Button == EKeys::MiddleMouseButton);

	if (!bLeft && !bRight && !bMiddle)
	{
		return FReply::Unhandled();
	}

	// 右键：整块画布都能弹菜单。
	// 注意别在这之前 return——就是"只认左键"的早退把上下文菜单变成了死代码。
	if (bRight)
	{
		const int32 MenuRow = HitTestRow(Local.Y);
		UAKAbilityTaskBase* RightHit = (MenuRow == INDEX_NONE) ? nullptr : HitTestTask(Local.X, MenuRow);

		// 菜单里"在此处…"要的是点击位置的时间，所以先把光标挪过去
		SetCursorTime(SnapTime(XToTime(Local.X)));
		ContextMarkerIndex = HitTestMarker(Local.X);

		if (RightHit)
		{
			SelectedTask = RightHit;
			OnTaskSelected.ExecuteIfBound(RightHit);
		}
		if (MenuRow != INDEX_NONE)
		{
			SelectedTrackIndex = MenuRow;
		}

		ShowContextMenu(MouseEvent, MyGeometry, RightHit, MenuRow);
		return FReply::Handled();
	}

	// 中键：拖着平移视图
	if (bMiddle)
	{
		DragMode = EAKTimelineDragMode::Pan;
		DragStartLocalX = Local.X;
		DragStartViewOffset = ViewOffsetTime;
		return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
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
				// 静音也是一次编辑：不包事务的话 Ctrl+Z 撤不回来
				const FScopedTransaction Transaction(LOCTEXT("ToggleTrack", "轨道开关"));
				Track->Modify();
				Track->bEnabled = !Track->bEnabled;
				AssetPtr->MarkPackageDirty();
				ForceRefresh();
			}
			else
			{
				SelectedTrackIndex = Row;
				OnTrackSelected.ExecuteIfBound(Track, Row);
			}
		}
		return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
	}

	const int32 Row = HitTestRow(Local.Y);

	// 标尺区：定位光标；按住不放是连续 scrub，按在标记线上则是拖标记
	if (Row == INDEX_NONE)
	{
		SetCursorTime(SnapTime(XToTime(Local.X)));

		const int32 MarkerIndex = HitTestMarker(Local.X);
		DraggingMarkerIndex = MarkerIndex;
		DragMode = (MarkerIndex != INDEX_NONE) ? EAKTimelineDragMode::Marker : EAKTimelineDragMode::Scrub;
		DragStartLocalX = Local.X;

		if (MarkerIndex != INDEX_NONE)
		{
			DragTransaction = MakeUnique<FScopedTransaction>(LOCTEXT("MoveMarker", "移动标记"));
			if (UAKAbilityAsset* AssetPtr = Asset.Get())
			{
				AssetPtr->Modify();
			}
		}

		return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
	}

	UAKAbilityTaskBase* Hit = nullptr;
	const FAKScheduledTask* HitScheduled = nullptr;
	const EAKTimelineDragMode Mode = HitTestSegmentEdge(Local.X, Row, Hit, HitScheduled);

	if (Hit && HitScheduled)
	{
		SelectedTask = Hit;
		OnTaskSelected.ExecuteIfBound(Hit);
		SelectedTrackIndex = Row;

		DraggingTask = Hit;
		DragMode = Mode;
		DragGrabOffset = XToTime(Local.X) - Hit->StartTime;
		DragStartLocalX = Local.X;
		DragOriginTrackIndex = Row;
		DragCurrentTrackIndex = Row;
		bAltPendingDuplicate = MouseEvent.IsAltDown();

		// 一次拖动 = 一次可撤销的事务。Alt+拖拽把"复制"也并进同一个事务，
		// 所以事务先开好，复制时告诉编辑器别再开一个。
		const FText TransactionLabel = bAltPendingDuplicate
			? LOCTEXT("DuplicateAndMoveTask", "复制并移动任务")
			: (Mode == EAKTimelineDragMode::Move ? LOCTEXT("MoveTask", "移动任务") : LOCTEXT("TrimTask", "调整任务时长"));

		DragTransaction = MakeUnique<FScopedTransaction>(TransactionLabel);
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
		DragMode = EAKTimelineDragMode::Scrub;
		DragStartLocalX = Local.X;
	}

	return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
}

FReply SUAKTimeline::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	// 悬停状态每次都更新：高亮、拉伸把手、tooltip 都读它
	HoverLocalPos = Local;
	HoverRow = HitTestRow(Local.Y);
	HoverMarkerIndex = HitTestMarker(Local.X);
	HoverTask = (HoverRow == INDEX_NONE) ? nullptr : HitTestTask(Local.X, HoverRow);

	if (DragMode == EAKTimelineDragMode::None)
	{
		return FReply::Unhandled();
	}

	UAKAbilityAsset* AssetPtr = Asset.Get();

	switch (DragMode)
	{
	case EAKTimelineDragMode::Pan:
		ViewOffsetTime = DragStartViewOffset - (Local.X - DragStartLocalX) / PixelsPerSecond;
		ClampViewOffset();
		return FReply::Handled();

	case EAKTimelineDragMode::Scrub:
		// 按住拖动 = 连续 scrub，而不是"点一下跳一下"
		SetCursorTime(SnapTime(XToTime(Local.X)));
		return FReply::Handled();

	case EAKTimelineDragMode::Marker:
		if (AssetPtr && AssetPtr->Markers.IsValidIndex(DraggingMarkerIndex))
		{
			const float NewTime = FMath::Max(0.f, SnapTime(XToTime(Local.X)));
			if (!FMath::IsNearlyEqual(NewTime, AssetPtr->Markers[DraggingMarkerIndex].Time))
			{
				AssetPtr->Markers[DraggingMarkerIndex].Time = NewTime;
				AssetPtr->MarkPackageDirty();
			}
		}
		return FReply::Handled();

	default:
		break;
	}

	UAKAbilityTaskBase* Task = DraggingTask.Get();
	if (!Task || !CanEditTaskTimes(Task))
	{
		// 复合轨道：时间由轨道算出来，拖动无效（事务白开也不改数据，抬手即撤）
		return FReply::Handled();
	}

	// Alt+拖拽：真的挪动了才复制——否则 Alt 点一下也会凭空多一份副本
	if (bAltPendingDuplicate && FMath::Abs(Local.X - DragStartLocalX) > 3.f)
	{
		bAltPendingDuplicate = false;
		if (UAKAbilityTaskBase* Copy = OnRequestDuplicateTask.Execute(/*bMergeIntoActiveTransaction=*/true))
		{
			DraggingTask = Copy;
			Task = Copy;
			SelectedTask = Copy;
			OnTaskSelected.ExecuteIfBound(Copy);
			ForceRefresh();
		}
	}

	Task->Modify();

	if (DragMode == EAKTimelineDragMode::Move)
	{
		const float NewStart = FMath::Max(0.f, SnapTime(XToTime(Local.X) - DragGrabOffset));
		if (!FMath::IsNearlyEqual(NewStart, Task->StartTime))
		{
			Task->StartTime = NewStart;
		}

		// 跨轨道拖拽：鼠标换到别的行就把任务挪过去
		const int32 TargetRow = HitTestRow(Local.Y);
		if (TargetRow != INDEX_NONE && TargetRow != DragCurrentTrackIndex && AssetPtr && AssetPtr->Tracks.IsValidIndex(TargetRow))
		{
			UAKAbilityTrack* TargetTrack = AssetPtr->Tracks[TargetRow];
			if (TargetTrack && TargetTrack->CompositeMode == EAKTrackCompositeMode::Timeline)
			{
				MoveTaskToTrack(Task, DragCurrentTrackIndex, TargetRow);
				DragCurrentTrackIndex = TargetRow;
				SelectedTrackIndex = TargetRow;
			}
		}
	}
	else if (DragMode == EAKTimelineDragMode::TrimRight)
	{
		// 起点钉住，只改时长
		const float NewEnd = FMath::Max(Task->StartTime, SnapTime(XToTime(Local.X)));
		Task->Duration = NewEnd - Task->StartTime;
	}
	else
	{
		// TrimLeft：终点钉住，改起点与时长
		const float EndTime = Task->GetEndTime();
		const float NewStart = FMath::Clamp(SnapTime(XToTime(Local.X)), 0.f, EndTime);
		Task->StartTime = NewStart;
		Task->Duration = EndTime - NewStart;
	}

	if (AssetPtr)
	{
		AssetPtr->MarkPackageDirty();
	}

	return FReply::Handled();
}

FReply SUAKTimeline::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// 只有真正拖动的那个键抬起才收尾：右键菜单弹着时不会被误伤
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton
		&& MouseEvent.GetEffectingButton() != EKeys::MiddleMouseButton)
	{
		return FReply::Unhandled();
	}

	if (DragMode == EAKTimelineDragMode::Move
		|| DragMode == EAKTimelineDragMode::TrimLeft
		|| DragMode == EAKTimelineDragMode::TrimRight
		|| DragMode == EAKTimelineDragMode::Marker)
	{
		ForceRefresh();
	}

	DragMode = EAKTimelineDragMode::None;
	DraggingTask = nullptr;
	DraggingMarkerIndex = INDEX_NONE;
	bAltPendingDuplicate = false;

	// 结束拖动事务（析构即提交）
	DragTransaction.Reset();

	return FReply::Handled().ReleaseMouseCapture();
}

FReply SUAKTimeline::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const int32 Row = HitTestRow(Local.Y);

	// 双击标尺：在该处放一个命名标记（对齐 Notify 的关键帧最常用的动作）
	if (Row == INDEX_NONE)
	{
		SetCursorTime(SnapTime(XToTime(Local.X)));
		OnAction.ExecuteIfBound(EAKToolbarAction::AddMarker);
		return FReply::Handled();
	}

	UAKAbilityTaskBase* Hit = HitTestTask(Local.X, Row);
	if (Hit)
	{
		// 双击片段：光标落到起点并进入视野，Inspector 已经在看它了
		FocusTask(Hit);
		return FReply::Handled();
	}

	SetCursorTime(SnapTime(XToTime(Local.X)));
	return FReply::Handled();
}

void SUAKTimeline::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	HoverLocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
}

void SUAKTimeline::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	HoverTask = nullptr;
	HoverRow = INDEX_NONE;
	HoverMarkerIndex = INDEX_NONE;
}

FCursorReply SUAKTimeline::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	const FVector2D Local = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());

	if (Local.X < TrackHeaderWidth || Local.Y < RulerHeight)
	{
		return FCursorReply::Unhandled();
	}

	// 悬停在标记线上 → 左右箭头（可拖）；在片段两端 → 拉伸；否则默认
	if (HoverMarkerIndex != INDEX_NONE)
	{
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	}

	UAKAbilityTaskBase* Hit = nullptr;
	const FAKScheduledTask* HitScheduled = nullptr;
	switch (HitTestSegmentEdge(Local.X, HitTestRow(Local.Y), Hit, HitScheduled))
	{
	case EAKTimelineDragMode::TrimLeft:
	case EAKTimelineDragMode::TrimRight:
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	case EAKTimelineDragMode::Move:
		return FCursorReply::Cursor(EMouseCursor::CardinalCross);
	default:
		return FCursorReply::Unhandled();
	}
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
		LOCTEXT("Menu_DuplicateTask", "原地复制任务"),
		LOCTEXT("Menu_DuplicateTaskTip", "复制当前选中的任务（Ctrl+D）"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				// 复制型委托带返回值：拿到副本后直接选中，省一次点击
				if (UAKAbilityTaskBase* Copy = OnRequestDuplicateTask.Execute(/*bMergeIntoActiveTransaction=*/false))
				{
					OnTaskSelected.ExecuteIfBound(Copy);
				}
			}),
			FCanExecuteAction::CreateLambda([bHasTask]() { return bHasTask; })));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_CopyTask", "复制任务"),
		LOCTEXT("Menu_CopyTaskTip", "把任务复制进剪贴板（Ctrl+C）"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { CopySelectedTask(); }),
			FCanExecuteAction::CreateLambda([bHasTask]() { return bHasTask; })));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_PasteTask", "粘贴任务"),
		LOCTEXT("Menu_PasteTaskTip", "把剪贴板里的任务粘到光标处（Ctrl+V）"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { PasteTask(); }),
			FCanExecuteAction::CreateLambda([this]() { return HasCopiedTask(); })));
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
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			// 光标在右键时就已移到点击处，"此处"就是它
			OnAction.ExecuteIfBound(EAKToolbarAction::AddMarker);
		})));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_RemoveMarker", "删除此标记"),
		LOCTEXT("Menu_RemoveMarkerTip", "删掉右键点中的这个标记"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				OnAction.ExecuteIfBound(EAKToolbarAction::RemoveMarker);
				ContextMarkerIndex = INDEX_NONE;
			}),
			FCanExecuteAction::CreateLambda([this]() { return ContextMarkerIndex != INDEX_NONE; })));
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
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const float Delta = MouseEvent.GetWheelDelta();

	// Shift+滚轮：横向平移（缩放大了以后靠它找内容）
	if (MouseEvent.IsShiftDown())
	{
		const float VisibleTime = (PixelsPerSecond > 0.f) ? ViewWidth / PixelsPerSecond : 1.f;
		ViewOffsetTime -= Delta * VisibleTime * 0.15f;
		ClampViewOffset();
		return FReply::Handled();
	}

	// 缩放：以光标下的时间为锚点，放大时目标不会跑出视野
	const float AnchorTime = XToTime(FMath::Max(Local.X, TrackHeaderWidth));
	const float NewPixelsPerSecond = FMath::Clamp(PixelsPerSecond * (1.f + Delta * 0.1f), 20.f, 4000.f);
	if (!FMath::IsNearlyEqual(NewPixelsPerSecond, PixelsPerSecond))
	{
		PixelsPerSecond = NewPixelsPerSecond;
		ViewOffsetTime = AnchorTime - (FMath::Max(Local.X, TrackHeaderWidth) - TrackHeaderWidth) / PixelsPerSecond;
		ClampViewOffset();
	}

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

	// 单帧步进：方向键与 , . 都认（ Sequencer 用户习惯方向键，剪辑软件习惯逗号句号 ）
	if (Key == EKeys::Right || Key == EKeys::Period)
	{
		StepFrame(InKeyEvent.IsControlDown() ? 10 : 1);
		return FReply::Handled();
	}
	if (Key == EKeys::Left || Key == EKeys::Comma)
	{
		StepFrame(InKeyEvent.IsControlDown() ? -10 : -1);
		return FReply::Handled();
	}

	// 首尾：Home / End
	if (Key == EKeys::Home)
	{
		SetCursorTime(0.f);
		ScrollTimeIntoView(0.f);
		return FReply::Handled();
	}
	if (Key == EKeys::End)
	{
		SetCursorTime(GetAssetLength());
		ScrollTimeIntoView(GetAssetLength());
		return FReply::Handled();
	}

	// 复制（Ctrl+D）
	if (Key == EKeys::D && InKeyEvent.IsControlDown())
	{
		if (UAKAbilityTaskBase* Copy = OnRequestDuplicateTask.Execute(/*bMergeIntoActiveTransaction=*/false))
		{
			OnTaskSelected.ExecuteIfBound(Copy);
		}
		return FReply::Handled();
	}

	// 复制（Ctrl+C）：存成 Json，跨撤销/重做依然有效
	if (Key == EKeys::C && InKeyEvent.IsControlDown())
	{
		CopySelectedTask();
		return FReply::Handled();
	}

	// 粘贴（Ctrl+V）：粘到当前轨道的光标处
	if (Key == EKeys::V && InKeyEvent.IsControlDown())
	{
		PasteTask();
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

void SUAKTimeline::CopySelectedTask()
{
	UAKAbilityTaskBase* Task = SelectedTask.Get();
	if (!Task)
	{
		return;
	}

	// 用任务自己的 ExportJson：与文件序列化同一套字段，粘贴端无需另写解析
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Task->ExportJson(Json);

	FString OutString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutString);
	if (FJsonSerializer::Serialize(Json, Writer))
	{
		TaskClipboard = MoveTemp(OutString);
	}
}

void SUAKTimeline::PasteTask()
{
	if (TaskClipboard.IsEmpty() || !OnRequestPasteTask.IsBound())
	{
		return;
	}

	if (UAKAbilityTaskBase* Pasted = OnRequestPasteTask.Execute(TaskClipboard))
	{
		SelectedTask = Pasted;
		OnTaskSelected.ExecuteIfBound(Pasted);
		ForceRefresh();
	}
}

FText SUAKTimeline::GetHoverTooltipText() const
{
	const UAKAbilityAsset* AssetPtr = Asset.Get();
	const int32 FrameRate = AssetPtr ? FMath::Max(1, AssetPtr->FrameRate) : 60;

	if (UAKAbilityTaskBase* Task = HoverTask.Get())
	{
		// 复合轨道下"看到的时间"是执行计划算出来的，tooltip 必须说同一套时间
		float StartTime = Task->StartTime;
		float EndTime = Task->GetEndTime();
		if (const FAKScheduledTask* Scheduled = CachedPlan.FindByPredicate(
				[Task](const FAKScheduledTask& Item) { return Item.Task == Task; }))
		{
			StartTime = Scheduled->StartTime;
			EndTime = Scheduled->EndTime;
		}

		return FText::Format(
			LOCTEXT("HoverTaskTip", "{0}\n{1}s → {2}s（时长 {3}s / {4} 帧）\n拖两端改时长 · Alt+拖动复制 · 拖到别的行换轨道"),
			Task->GetTaskDisplayName(),
			FText::AsNumber(FMath::RoundToFloat(StartTime * 1000.f) / 1000.f),
			FText::AsNumber(FMath::RoundToFloat(EndTime * 1000.f) / 1000.f),
			FText::AsNumber(FMath::RoundToFloat((EndTime - StartTime) * 1000.f) / 1000.f),
			FText::AsNumber(FMath::Max(0, FMath::RoundToInt((EndTime - StartTime) * FrameRate))));
	}

	if (HoverMarkerIndex != INDEX_NONE && AssetPtr && AssetPtr->Markers.IsValidIndex(HoverMarkerIndex))
	{
		const FAKTimeMarker& Marker = AssetPtr->Markers[HoverMarkerIndex];
		return FText::Format(LOCTEXT("HoverMarkerTip", "标记 {0} @ {1}s\n拖动改时间 · 右键可删除"),
			FText::FromName(Marker.Name),
			FText::AsNumber(FMath::RoundToFloat(Marker.Time * 1000.f) / 1000.f));
	}

	const float HoverTime = FMath::Max(0.f, XToTime(HoverLocalPos.X));
	return FText::Format(LOCTEXT("HoverTimeTip", "{0}s（第 {1} 帧）"),
		FText::AsNumber(FMath::RoundToFloat(HoverTime * 1000.f) / 1000.f),
		FText::AsNumber(FMath::RoundToInt(HoverTime * FrameRate)));
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
		PaintGeom(Geometry, FVector2D(0, 0), FVector2D(Geometry.GetLocalSize().X, RulerHeight)),
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
			PaintGeom(Geometry, FVector2D(X, RulerHeight - (bMajor ? 10.f : 5.f)), FVector2D(1.f, bMajor ? 10.f : 5.f)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color);

		if (bMajor)
		{
			UAKAbilityAsset* AssetPtr = Asset.Get();
			const int32 FrameRate = AssetPtr ? FMath::Max(1, AssetPtr->FrameRate) : 60;
			const FText Label = bShowFrames
				? FText::AsNumber(FMath::RoundToInt(T * FrameRate))
				: FText::Format(LOCTEXT("TimeLabel", "{0}s"), FText::AsNumber(FMath::RoundToFloat(T * 100.f) / 100.f));

			FSlateDrawElement::MakeText(OutDrawElements, LayerId,
				PaintGeom(Geometry, FVector2D(X + 3.f, 3.f), FVector2D(60.f, 14.f)),
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
			PaintGeom(Geometry, FVector2D(0, Y), FVector2D(Width, RowHeight - 1.f)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None,
			(Row % 2 == 0) ? ColorBackground : FLinearColor(0.05f, 0.05f, 0.05f));

		// 轨道头
		const FLinearColor TrackColor = Track ? ColorForTrack(Track->TrackType) : FLinearColor::Gray;
		const FLinearColor HeaderColor = Track && !Track->bEnabled ? FLinearColor(0.05f, 0.05f, 0.05f) : ColorHeader;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			PaintGeom(Geometry, FVector2D(0, Y), FVector2D(TrackHeaderWidth - 1.f, RowHeight - 1.f)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, HeaderColor);

		// 轨道色条 + 开关方块
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
			PaintGeom(Geometry, FVector2D(4.f, Y + 6.f), FVector2D(6.f, RowHeight - 13.f)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None,
			Track && Track->bEnabled ? TrackColor : FLinearColor(0.25f, 0.25f, 0.25f));

		if (Track)
		{
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
				PaintGeom(Geometry, FVector2D(18.f, Y + 7.f), FVector2D(TrackHeaderWidth - 24.f, 16.f)),
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
				PaintGeom(Geometry, FVector2D(X, Y), FVector2D(1.f, RowHeight - 1.f)),
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
			const bool bHovered = (HoverTask.Get() == Task);
			FLinearColor Fill = Track->bEnabled ? TrackColor : FLinearColor(0.3f, 0.3f, 0.3f);
			Fill.A = bSelected ? 0.95f : (bHovered ? 0.85f : 0.7f);

			FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
				PaintGeom(Geometry, FVector2D(X0, Y + 4.f), FVector2D(W, RowHeight - 9.f)),
				FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Fill);

			if (bHovered && !bSelected)
			{
				// 悬停描边：没选中时也让人看清"鼠标正压在这一块上"
				const FLinearColor HoverOutline(1.f, 1.f, 1.f, 0.45f);
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
					PaintGeom(Geometry, FVector2D(X0, Y + 4.f), FVector2D(W, 1.f)),
					FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, HoverOutline);
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
					PaintGeom(Geometry, FVector2D(X0, Y + RowHeight - 5.f), FVector2D(W, 1.f)),
					FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, HoverOutline);
			}

			// 拉伸把手：悬停或正在拖的时候才画，两端各一条——看见它就知道能改时长
			if ((bHovered || DraggingTask.Get() == Task) && CanEditTaskTimes(Task) && W > 12.f)
			{
				const FLinearColor HandleColor(1.f, 1.f, 1.f, 0.75f);
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2,
					PaintGeom(Geometry, FVector2D(X0, Y + 4.f), FVector2D(2.f, RowHeight - 9.f)),
					FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, HandleColor);
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2,
					PaintGeom(Geometry, FVector2D(X1 - 2.f, Y + 4.f), FVector2D(2.f, RowHeight - 9.f)),
					FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, HandleColor);
			}

			if (bSelected)
			{
				// 选中描边
				const FLinearColor Outline(1.f, 1.f, 1.f, 0.9f);
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
					PaintGeom(Geometry, FVector2D(X0, Y + 4.f), FVector2D(W, 1.f)),
					FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Outline);
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
					PaintGeom(Geometry, FVector2D(X0, Y + RowHeight - 5.f), FVector2D(W, 1.f)),
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
					PaintGeom(Geometry, FVector2D(X0 + 4.f, Y + 7.f), FVector2D(W - 8.f, 14.f)),
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
			PaintGeom(Geometry, FVector2D(X, RulerHeight), FVector2D(1.f, ViewHeight - RulerHeight)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.4f, 0.75f, 0.95f, 0.55f));

		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
			PaintGeom(Geometry, FVector2D(X + 3.f, RulerHeight + 2.f), FVector2D(80.f, 12.f)),
			FText::FromName(Marker.Name), Font, ESlateDrawEffect::None, FLinearColor(0.4f, 0.75f, 0.95f));
	}
}

void SUAKTimeline::DrawCursor(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	// 鼠标位置细线：按下之前就知道会落在哪一格（拖动时不画，避免和光标打架）
	if (DragMode == EAKTimelineDragMode::None && HoverLocalPos.X > TrackHeaderWidth)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			PaintGeom(Geometry, FVector2D(HoverLocalPos.X, RulerHeight), FVector2D(1.f, ViewHeight - RulerHeight)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, 0.10f));
	}

	const float X = TimeToX(CursorTime);
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
		PaintGeom(Geometry, FVector2D(X, 0.f), FVector2D(1.5f, ViewHeight)),
		FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, ColorCursor);
}

#undef LOCTEXT_NAMESPACE
