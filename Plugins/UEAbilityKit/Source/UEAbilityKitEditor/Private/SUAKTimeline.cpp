#include "SUAKTimeline.h"
#include "UAKSettings.h"

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
	OnRequestDeleteTasks = InArgs._OnRequestDeleteTasks;
	OnRequestScrollBy = InArgs._OnRequestScrollBy;
	OnRequestRevealRow = InArgs._OnRequestRevealRow;
	OnRequestDuplicateTask = InArgs._OnRequestDuplicateTask;
	OnAction = InArgs._OnAction;

	SetCanTick(true);
	SetClipping(EWidgetClipping::ClipToBounds);

	// 吸附开关是编辑器状态（不是资产属性），从设置里读回来
	if (const UUAKSettings* Settings = GetDefault<UUAKSettings>())
	{
		bSnapEnabled = Settings->bTimelineSnapEnabled;
		bSnapToMarkers = Settings->bTimelineSnapToMarkers;
	}

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

	// 教学提示的淡出要一个"现在"，Tick 里顺手记下来
	LastTickTime = InCurrentTime;

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

	SelectedTasks.Reset();
	SelectedTasks.Add(Task);
	SelectedTask = Task;
	OnTaskSelected.ExecuteIfBound(Task);

	const float StartTime = Task->StartTime;
	SetCursorTime(StartTime);
	ScrollTimeIntoView(StartTime);

	// 轨道多的时候还得把这一行滚进视野：垂直滚动归左栏管，这里只发请求
	if (const UAKAbilityAsset* AssetPtr = Asset.Get())
	{
		for (int32 Index = 0; Index < AssetPtr->Tracks.Num(); ++Index)
		{
			if (AssetPtr->Tracks[Index].Get() == Task->GetOuter())
			{
				OnRequestRevealRow.ExecuteIfBound(Index);
				break;
			}
		}
	}
}

void SUAKTimeline::ClearSelection()
{
	SetSelection(nullptr, false);
}

void SUAKTimeline::DeleteSelection()
{
	if (SelectedTasks.Num() > 1)
	{
		TArray<UAKAbilityTaskBase*> Tasks;
		Tasks.Reserve(SelectedTasks.Num());
		for (const TWeakObjectPtr<UAKAbilityTaskBase>& Item : SelectedTasks)
		{
			if (UAKAbilityTaskBase* Task = Item.Get())
			{
				Tasks.Add(Task);
			}
		}
		OnRequestDeleteTasks.ExecuteIfBound(Tasks);
	}
	else
	{
		OnRequestDeleteTask.ExecuteIfBound();
	}
}

void SUAKTimeline::SelectAllTasks()
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		return;
	}

	SelectedTasks.Reset();

	// 有选中轨道就只选这一条轨道，否则整条技能全选
	const bool bOnlyCurrentTrack = (SelectedTrackIndex != INDEX_NONE);
	for (int32 Row = 0; Row < AssetPtr->Tracks.Num(); ++Row)
	{
		if (bOnlyCurrentTrack && Row != SelectedTrackIndex)
		{
			continue;
		}

		UAKAbilityTrack* Track = AssetPtr->Tracks[Row];
		if (!Track)
		{
			continue;
		}

		for (UAKAbilityTaskBase* Task : Track->Tasks)
		{
			if (Task)
			{
				SelectedTasks.Add(Task);
			}
		}
	}

	if (SelectedTasks.Num() > 0)
	{
		SelectedTask = SelectedTasks[0];
		OnTaskSelected.ExecuteIfBound(SelectedTask.Get());
	}
}

void SUAKTimeline::SetSelection(UAKAbilityTaskBase* Task, bool bAdditive)
{
	if (!Task)
	{
		SelectedTasks.Reset();
		SelectedTask = nullptr;
		OnTaskSelected.ExecuteIfBound(nullptr);
		return;
	}

	if (bAdditive)
	{
		// 再点一次已选中的 = 取消选中
		if (IsTaskSelected(Task))
		{
			SelectedTasks.RemoveAll([Task](const TWeakObjectPtr<UAKAbilityTaskBase>& Item) { return Item.Get() == Task; });
		}
		else
		{
			SelectedTasks.Add(Task);
		}
	}
	else if (!IsTaskSelected(Task))
	{
		// 单选：点的是多选集合之外的新任务才重置集合；
		// 点在集合内则保留整组，这样能直接拖动整组而不用重新框选
		SelectedTasks.Reset();
		SelectedTasks.Add(Task);
	}

	SelectedTask = Task;
	OnTaskSelected.ExecuteIfBound(Task);
}

bool SUAKTimeline::IsTaskSelected(const UAKAbilityTaskBase* Task) const
{
	for (const TWeakObjectPtr<UAKAbilityTaskBase>& Item : SelectedTasks)
	{
		if (Item.Get() == Task)
		{
			return true;
		}
	}
	return false;
}

void SUAKTimeline::CollectTasksInRect(const FVector2D& Start, const FVector2D& End, TArray<UAKAbilityTaskBase*>& OutTasks) const
{
	const float MinX = FMath::Max(TrackHeaderWidth, FMath::Min(Start.X, End.X));
	const float MaxX = FMath::Min(Start.X > End.X ? Start.X : End.X, TrackHeaderWidth + ViewWidth);
	const float MinY = FMath::Min(Start.Y, End.Y);
	const float MaxY = FMath::Max(Start.Y, End.Y);

	if (MaxX <= MinX)
	{
		return;
	}

	const float Time0 = XToTime(MinX);
	const float Time1 = XToTime(MaxX);

	// 标尺区拖动不算框选行，统一从第 0 行起算
	const int32 Row0 = FMath::Max(0, HitTestRow(MinY));
	const int32 Row1 = FMath::Max(0, HitTestRow(MaxY));

	for (const FAKScheduledTask& Scheduled : CachedPlan)
	{
		if (!Scheduled.Task || Scheduled.TrackIndex < Row0 || Scheduled.TrackIndex > Row1)
		{
			continue;
		}

		const float SegMin = FMath::Min(Scheduled.StartTime, Scheduled.EndTime);
		const float SegMax = FMath::Max(Scheduled.StartTime, Scheduled.EndTime);
		if (SegMax >= Time0 && SegMin <= Time1)
		{
			OutTasks.AddUnique(Scheduled.Task);
		}
	}
}

void SUAKTimeline::MoveSelectionBy(float DeltaTime)
{
	if (FMath::IsNearlyZero(DeltaTime))
	{
		return;
	}

	UAKAbilityAsset* AssetPtr = Asset.Get();
	bool bChanged = false;

	for (const TPair<TWeakObjectPtr<UAKAbilityTaskBase>, float>& Pair : MoveSelectionStarts)
	{
		UAKAbilityTaskBase* Task = Pair.Key.Get();
		if (!Task || !CanEditTaskTimes(Task))
		{
			continue;
		}

		const float NewStart = FMath::Max(0.f, SnapTime(Pair.Value + DeltaTime));
		if (!FMath::IsNearlyEqual(NewStart, Task->StartTime))
		{
			Task->Modify();
			Task->StartTime = NewStart;
			bChanged = true;
		}
	}

	if (bChanged && AssetPtr)
	{
		AssetPtr->MarkPackageDirty();
	}
}

void SUAKTimeline::SetSnapEnabled(bool bInEnabled)
{
	bSnapEnabled = bInEnabled;
	if (UUAKSettings* Settings = GetMutableDefault<UUAKSettings>())
	{
		Settings->bTimelineSnapEnabled = bInEnabled;
		Settings->SaveConfig();
	}
}

void SUAKTimeline::SetSnapToMarkers(bool bInEnabled)
{
	bSnapToMarkers = bInEnabled;
	if (UUAKSettings* Settings = GetMutableDefault<UUAKSettings>())
	{
		Settings->bTimelineSnapToMarkers = bInEnabled;
		Settings->SaveConfig();
	}
}

void SUAKTimeline::ZoomToFit()
{
	// 内容末端：技能长度、最后一个任务、最后一个标记，取最大
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
	ContentEnd = FMath::Max(ContentEnd, 0.1f);

	if (ViewWidth > 1.f)
	{
		PixelsPerSecond = FMath::Clamp((ViewWidth - 8.f) / ContentEnd, 20.f, 4000.f);
	}
	ViewOffsetTime = 0.f;
	ClampViewOffset();
}

bool SUAKTimeline::CanEditTaskTimes(const UAKAbilityTaskBase* Task) const
{
	// 复合轨道（Sequence / Parallel / Repeat）的任务时间由轨道算出来，
	// 手动改 StartTime / Duration 没有意义，一律不允许拖（与 OnMouseMove 的旧判断一致）。
	const UAKAbilityTrack* OwnerTrack = Cast<UAKAbilityTrack>(Task->GetOuter());
	return OwnerTrack && OwnerTrack->CompositeMode == EAKTrackCompositeMode::Timeline;
}

bool SUAKTimeline::IsTrackLocked(int32 TrackIndex) const
{
	const UAKAbilityAsset* AssetPtr = Asset.Get();
	return AssetPtr && AssetPtr->Tracks.IsValidIndex(TrackIndex) && AssetPtr->Tracks[TrackIndex]
		&& AssetPtr->Tracks[TrackIndex]->bLocked;
}

int32 SUAKTimeline::FindTrackRow(const UAKAbilityTaskBase* Task) const
{
	const UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !Task)
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < AssetPtr->Tracks.Num(); ++Index)
	{
		if (AssetPtr->Tracks[Index].Get() == Task->GetOuter())
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void SUAKTimeline::MoveTaskToTrack(UAKAbilityTaskBase* Task, int32 FromTrackIndex, int32 ToTrackIndex)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !Task || FromTrackIndex == ToTrackIndex)
	{
		return;
	}

	// 锁住的轨道：既不能把任务挪走，也不能挪进来
	if (IsTrackLocked(FromTrackIndex) || IsTrackLocked(ToTrackIndex))
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
	// （不加 REN_ForceNoResetLoaders：5.x 起该标记已废弃，且会触发 C4996）
	Task->Rename(nullptr, ToTrack, REN_DontCreateRedirectors);

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
	// 吸附总开关关掉后就是"自由拖动"（Sequencer 的 Snap 按钮同一语义）
	if (!bSnapEnabled)
	{
		return Time;
	}

	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		return Time;
	}

	const float Frame = 1.f / static_cast<float>(FMath::Max(1, AssetPtr->FrameRate));
	const float FrameSnapped = FMath::RoundToFloat(Time / Frame) * Frame;

	// 只吸附到帧时到此为止（"对齐到标记"是单独一项，用户可以只要前者）
	if (!bSnapToMarkers)
	{
		return FrameSnapped;
	}

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
	// 屏幕 Y 要加上滚动偏移，才能换回"第几条轨道"
	return static_cast<int32>((Y + ScrollOffset - RulerHeight) / RowHeight);
}

float SUAKTimeline::RowTopY(int32 Row) const
{
	return RulerHeight + Row * RowHeight - ScrollOffset;
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
	// 折叠的轨道只画汇总条，任务不可点（点它也没意义——看不到具体哪一块）
	const UAKAbilityAsset* AssetPtr = Asset.Get();
	if (AssetPtr && AssetPtr->Tracks.IsValidIndex(Row)
		&& AssetPtr->Tracks[Row] && AssetPtr->Tracks[Row]->bCollapsed)
	{
		return nullptr;
	}

	// 用执行计划做命中测试，保证 Sequence / Parallel 下点到的位置与实际运行一致
	const float Time = XToTime(X);

	// 重叠的片段要能点到压在上面那块（Sequencer 会遍历 UnderlappingSegments）：
	// 先取最后命中的（画得最晚 = 视觉上在最上层），再让"已经选中的那条"优先，
	// 这样选中一条后再点重叠区，拿到的还是它，不会被上面的盖掉。
	UAKAbilityTaskBase* TopHit = nullptr;
	bool bSelectedAlsoHit = false;

	for (const FAKScheduledTask& Scheduled : CachedPlan)
	{
		if (Scheduled.TrackIndex != Row || !Scheduled.Task)
		{
			continue;
		}
		if (Time < Scheduled.StartTime || Time > Scheduled.EndTime)
		{
			continue;
		}

		TopHit = Scheduled.Task;
		if (Scheduled.Task == SelectedTask.Get())
		{
			bSelectedAlsoHit = true;
		}
	}

	if (bSelectedAlsoHit)
	{
		return SelectedTask.Get();
	}
	return TopHit;
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

	// 轨道头已经搬到左栏（SUAKTrackOutliner）：这里只管右侧画布
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
		// Ctrl / Shift + 点击 = 加选或减选；直接点则单选
		SetSelection(Hit, MouseEvent.IsControlDown() || MouseEvent.IsShiftDown());
		SelectedTrackIndex = Row;

		DraggingTask = Hit;
		DragMode = Mode;
		DragGrabOffset = XToTime(Local.X) - Hit->StartTime;
		DragStartLocalX = Local.X;
		DragOriginTaskStart = Hit->StartTime;
		DragOriginTrackIndex = Row;
		DragCurrentTrackIndex = Row;
		bAltPendingDuplicate = MouseEvent.IsAltDown();

		// 多选且拖的正是组内任务 → 整组一起动（Alt 复制时只拖副本那一块）
		MoveSelectionStarts.Reset();
		if (!bAltPendingDuplicate && Mode == EAKTimelineDragMode::Move
			&& IsTaskSelected(Hit) && SelectedTasks.Num() > 1)
		{
			for (const TWeakObjectPtr<UAKAbilityTaskBase>& Item : SelectedTasks)
			{
				if (UAKAbilityTaskBase* ItemTask = Item.Get())
				{
					MoveSelectionStarts.Emplace(Item, ItemTask->StartTime);
				}
			}
		}

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
		// 空白：单击 = 定位光标并清空选中；按住拖开一段距离后转成框选（见 OnMouseMove）
		SetSelection(nullptr, false);
		SetCursorTime(SnapTime(XToTime(Local.X)));
		DragMode = EAKTimelineDragMode::Scrub;
		DragStartLocalX = Local.X;
		MarqueeStart = Local;
		MarqueeCurrent = Local;
		bMarqueeAdditive = MouseEvent.IsControlDown() || MouseEvent.IsShiftDown();
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
		// 拖开一段距离说明想框选，而不是定位光标——两者共用"空白处按下"
		if (FMath::Abs(Local.X - MarqueeStart.X) > 6.f || FMath::Abs(Local.Y - MarqueeStart.Y) > 6.f)
		{
			DragMode = EAKTimelineDragMode::Marquee;
			MarqueeCurrent = Local;
			return FReply::Handled();
		}

		// 按住拖动 = 连续 scrub，而不是"点一下跳一下"
		SetCursorTime(SnapTime(XToTime(Local.X)));
		return FReply::Handled();

	case EAKTimelineDragMode::Marquee:
		MarqueeCurrent = Local;
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
	if (!Task)
	{
		return FReply::Handled();
	}

	// ── 换轨道：必须放在"能不能改时间"之前 ──────────────────────────────
	// Sequence / Parallel 轨道上的任务不许改 StartTime（时间由轨道算出来），
	// 但"把它挪到别的轨道"是另一回事，不该被那条规则连坐——
	// 而技能的主轨道往往是 Sequence，连坐的话等于一个任务都挪不动。
	if (DragMode == EAKTimelineDragMode::Move && MoveSelectionStarts.Num() == 0)
	{
		const int32 TargetRow = HitTestRow(Local.Y);
		if (TargetRow != INDEX_NONE && TargetRow != DragCurrentTrackIndex && AssetPtr && AssetPtr->Tracks.IsValidIndex(TargetRow))
		{
			// 源和目的都不能是锁住的轨道；目标轨道的复合模式不限制（移过去由新轨道重新编排）
			if (!IsTrackLocked(DragCurrentTrackIndex) && !IsTrackLocked(TargetRow))
			{
				MoveTaskToTrack(Task, DragCurrentTrackIndex, TargetRow);
				DragCurrentTrackIndex = TargetRow;
				SelectedTrackIndex = TargetRow;
			}
		}
	}

	// 复合轨道：时间由轨道算出来，改 StartTime / Duration 没有意义（换轨道仍然可以）
	if (!CanEditTaskTimes(Task))
	{
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
			SelectedTasks.Reset();
			SelectedTasks.Add(Copy);
			SelectedTask = Copy;
			OnTaskSelected.ExecuteIfBound(Copy);
			ForceRefresh();
		}
	}

	Task->Modify();

	if (DragMode == EAKTimelineDragMode::Move)
	{
		const float NewStart = FMath::Max(0.f, SnapTime(XToTime(Local.X) - DragGrabOffset));

		if (MoveSelectionStarts.Num() > 0)
		{
			// 整组一起动：用主任务的位移驱动其余任务，组内相对关系保持不变
			MoveSelectionBy(NewStart - DragOriginTaskStart);
		}
		else if (!FMath::IsNearlyEqual(NewStart, Task->StartTime))
		{
			// 换轨道在上面统一处理了，这里只管时间
			Task->StartTime = NewStart;
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

	// Shift + 拖动 = 播放头跟着走（Persona 同款）。
	// 没按 Shift 就提示一次——首次操作即教学，成本极低、收益很高。
	const float DraggedTime = (DragMode == EAKTimelineDragMode::Move)
		? FMath::Max(0.f, SnapTime(XToTime(Local.X) - DragGrabOffset))
		: SnapTime(XToTime(Local.X));

	if (MouseEvent.IsShiftDown())
	{
		SetCursorTime(DraggedTime);
	}
	else if (ScrubHintStartTime < 0.0)
	{
		ScrubHintStartTime = LastTickTime;
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

	if (DragMode == EAKTimelineDragMode::Marquee)
	{
		// 框选收尾：矩形里的任务进选中集合
		TArray<UAKAbilityTaskBase*> Picked;
		CollectTasksInRect(MarqueeStart, MarqueeCurrent, Picked);

		if (!bMarqueeAdditive)
		{
			SelectedTasks.Reset();
		}
		for (UAKAbilityTaskBase* Task : Picked)
		{
			SelectedTasks.AddUnique(Task);
		}
		if (Picked.Num() > 0)
		{
			SelectedTask = Picked[0];
			OnTaskSelected.ExecuteIfBound(Picked[0]);
		}
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
	MoveSelectionStarts.Reset();

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

	// 双击空白：兜底缩放到整条技能（Sequencer 的双击 Section 兜底也是 Fit，双击永远不会"没反应"）
	ZoomToFit();
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

	// 移到轨道：拖着改时间的时候没人会想到"再上下挪一下"，所以必须给条能看见的路
	MenuBuilder.AddSubMenu(
		LOCTEXT("Menu_MoveToTrack", "移到轨道"),
		LOCTEXT("Menu_MoveToTrackTip", "把这个任务挪到另一条轨道（也可以直接把片段拖到别的行）"),
		FNewMenuDelegate::CreateLambda([this, HitTask](FMenuBuilder& SubMenuBuilder)
		{
			UAKAbilityAsset* AssetPtr = Asset.Get();
			if (!AssetPtr || !HitTask)
			{
				return;
			}

			const int32 CurrentRow = FindTrackRow(HitTask);
			for (int32 Index = 0; Index < AssetPtr->Tracks.Num(); ++Index)
			{
				UAKAbilityTrack* Track = AssetPtr->Tracks[Index];
				if (!Track || Index == CurrentRow)
				{
					continue;
				}

				const bool bTargetLocked = Track->bLocked;
				const bool bSourceLocked = (CurrentRow != INDEX_NONE) && IsTrackLocked(CurrentRow);

				SubMenuBuilder.AddMenuEntry(
					FText::Format(LOCTEXT("Menu_TrackItem", "{0}{1}"),
						Track->GetDisplayName(),
						bTargetLocked ? LOCTEXT("TrackLockedSuffix", "（已锁定）") : FText::GetEmpty()),
					FText::GetEmpty(),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([this, HitTask, CurrentRow, Index]()
						{
							const FScopedTransaction Transaction(LOCTEXT("MoveTaskToTrack", "移动任务到轨道"));
							MoveTaskToTrack(HitTask, CurrentRow, Index);
						}),
						FCanExecuteAction::CreateLambda([bTargetLocked, bSourceLocked]() { return !bTargetLocked && !bSourceLocked; })));
			}
		}),
		// FUIAction 没有"只给 CanExecute"的重载：Execute 得显式给个空的。
		// 这个版本的 AddSubMenu 后面两个参数（FName / ActionType）没有默认值，必须显式给。
		FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([bHasTask]() { return bHasTask; })),
		NAME_None,
		EUserInterfaceActionType::Button);

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
			FExecuteAction::CreateLambda([this]() { DeleteSelection(); }),
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

	// 裸滚轮 = 滚动轨道列表，交给左栏（与 Sequencer 一致：Ctrl+滚轮才是缩放）
	if (!MouseEvent.IsControlDown())
	{
		OnRequestScrollBy.ExecuteIfBound(-Delta * RowHeight * 2.f);
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

	// 全选（Ctrl+A）：有选中轨道就选这一条轨道，否则全技能
	if (Key == EKeys::A && InKeyEvent.IsControlDown())
	{
		SelectAllTasks();
		return FReply::Handled();
	}

	// 删除（Delete / Backspace）：多选时一次事务删掉整组
	if (Key == EKeys::Delete || Key == EKeys::BackSpace)
	{
		DeleteSelection();
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
		// 粘贴出来的就是新的唯一选中，别让旧的多选集合还挂着
		SelectedTasks.Reset();
		SelectedTasks.Add(Pasted);
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
			LOCTEXT("HoverTaskTip", "{0}\n{1}s → {2}s（时长 {3}s / {4} 帧）\n拖两端改时长 · 上下拖到别的行 = 换轨道 · Alt+拖动复制 · 右键「移到轨道」"),
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
	DrawMarquee(AllottedGeometry, OutDrawElements, LayerId + 4);

	// 一次性教学提示：右下角，3 秒淡出（抄 Persona 的"拖动时提示按 SHIFT"）
	if (ScrubHintStartTime >= 0.0 && (LastTickTime - ScrubHintStartTime) < 3.0)
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();
		const float Alpha = 1.f - static_cast<float>(LastTickTime - ScrubHintStartTime) / 3.f;
		const FSlateFontInfo HintFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);

		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 6,
			AllottedGeometry.ToPaintGeometry(
				UE::Slate::CastToVector2f(FVector2D(260.f, 14.f)),
				FSlateLayoutTransform(UE::Slate::CastToVector2f(FVector2D(Size.X - 268.f, Size.Y - 20.f)))),
			LOCTEXT("ScrubHint", "提示：拖动时按住 Shift，播放头会跟着走"),
			HintFont, ESlateDrawEffect::None, FLinearColor(1.f, 0.9f, 0.55f, FMath::Clamp(Alpha, 0.f, 1.f)));
	}

	return LayerId + 7;
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
	const float Bottom = Geometry.GetLocalSize().Y;

	for (int32 Row = 0; Row < AssetPtr->Tracks.Num(); ++Row)
	{
		UAKAbilityTrack* Track = AssetPtr->Tracks[Row];
		const float Y = RowTopY(Row);

		// 滚出视口的行不画：轨道可以有很多条，靠左栏共享的滚动偏移来定位
		if (Y + RowHeight < RulerHeight || Y > Bottom)
		{
			continue;
		}

		// 行底：静音 / 被 Solo 挤掉的轨道压暗，与左栏状态对上
		FLinearColor RowColor = (Row % 2 == 0) ? ColorBackground : FLinearColor(0.05f, 0.05f, 0.05f);
		if (Track && !Track->IsRunning())
		{
			RowColor *= 0.55f;
		}
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			PaintGeom(Geometry, FVector2D(0, Y), FVector2D(Width, RowHeight - 1.f)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, RowColor);

		// 选中轨道：整行上下各描一条淡蓝边（左栏选中行高亮在右侧的延续）
		if (Row == SelectedTrackIndex)
		{
			const FLinearColor Line(0.35f, 0.65f, 1.f, 0.55f);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
				PaintGeom(Geometry, FVector2D(0, Y), FVector2D(Width, 1.f)),
				FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Line);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
				PaintGeom(Geometry, FVector2D(0, Y + RowHeight - 2.f), FVector2D(Width, 1.f)),
				FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Line);
		}

		// 空轨道：给一句提示，别让人以为编辑器坏了
		if (Track && Track->Tasks.Num() == 0 && !Track->bCollapsed)
		{
			const FSlateFontInfo HintFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
				PaintGeom(Geometry, FVector2D(8.f, Y + 8.f), FVector2D(220.f, 14.f)),
				LOCTEXT("EmptyTrackHint", "空轨道 · 从右侧任务库添加任务"),
				HintFont, ESlateDrawEffect::None, FLinearColor(0.45f, 0.45f, 0.45f));
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

		// 折叠的轨道不展开任务（左栏折叠后这里只画一根汇总条）
		if (Track->bCollapsed)
		{
			continue;
		}

		const float Y = RowTopY(Row);
		// 滚出视口的行不画（轨道可以有很多条）
		if (Y + RowHeight < RulerHeight || Y > ViewHeight)
		{
			continue;
		}

		const FLinearColor TrackColor = UAKColorForTrackType(Track->TrackType);

		{
			const float X0 = TimeToX(Scheduled.StartTime);
			const float X1 = TimeToX(Scheduled.EndTime);
			const float W = FMath::Max(4.f, X1 - X0);
			if (X0 > Width || X1 < TrackHeaderWidth)
			{
				continue;
			}

			// 多选集合里的都按"选中"画，否则拖整组时看不出到底选中了哪几块
			const bool bSelected = IsTaskSelected(Task);
			const bool bHovered = (HoverTask.Get() == Task);
			// 静音 / 被 Solo 挤掉的轨道画成灰的，与左栏状态对上
			FLinearColor Fill = Track->IsRunning() ? TrackColor : FLinearColor(0.3f, 0.3f, 0.3f);
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

	// 折叠轨道：整条画成一根汇总条，写清里面有几个任务
	for (int32 Row = 0; Row < AssetPtr->Tracks.Num(); ++Row)
	{
		UAKAbilityTrack* Track = AssetPtr->Tracks[Row];
		if (!Track || !Track->bCollapsed)
		{
			continue;
		}

		float MinStart = TNumericLimits<float>::Max();
		float MaxEnd = 0.f;
		int32 Count = 0;
		for (const FAKScheduledTask& Scheduled : CachedPlan)
		{
			if (Scheduled.TrackIndex != Row || !Scheduled.Task)
			{
				continue;
			}
			MinStart = FMath::Min(MinStart, Scheduled.StartTime);
			MaxEnd = FMath::Max(MaxEnd, Scheduled.EndTime);
			++Count;
		}
		if (Count == 0)
		{
			continue;
		}

		const float Y = RowTopY(Row);
		if (Y + RowHeight < RulerHeight || Y > ViewHeight)
		{
			continue;
		}

		const float X0 = TimeToX(MinStart);
		const float X1 = TimeToX(MaxEnd);
		const float W = FMath::Max(6.f, X1 - X0);
		if (X0 > Width || X1 < TrackHeaderWidth)
		{
			continue;
		}

		FLinearColor Fill = Track->IsRunning()
			? UAKColorForTrackType(Track->TrackType)
			: FLinearColor(0.35f, 0.35f, 0.35f);
		Fill.A = 0.55f;

		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			PaintGeom(Geometry, FVector2D(X0, Y + 8.f), FVector2D(W, RowHeight - 17.f)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Fill);

		if (W > 44.f)
		{
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
				PaintGeom(Geometry, FVector2D(X0 + 4.f, Y + 8.f), FVector2D(W - 8.f, 14.f)),
				FText::Format(LOCTEXT("CollapsedTrack", "{0} · {1} 个任务"), Track->GetDisplayName(), FText::AsNumber(Count)),
				Font, ESlateDrawEffect::None, FLinearColor(0.9f, 0.9f, 0.9f, 0.85f));
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

void SUAKTimeline::DrawMarquee(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	if (DragMode != EAKTimelineDragMode::Marquee)
	{
		return;
	}

	const FVector2D Min(
		FMath::Min(MarqueeStart.X, MarqueeCurrent.X),
		FMath::Min(MarqueeStart.Y, MarqueeCurrent.Y));
	const FVector2D Max(
		FMath::Max(MarqueeStart.X, MarqueeCurrent.X),
		FMath::Max(MarqueeStart.Y, MarqueeCurrent.Y));
	const FVector2D Size = Max - Min;
	if (Size.X < 1.f || Size.Y < 1.f)
	{
		return;
	}

	const FLinearColor Fill(0.35f, 0.65f, 1.f, 0.15f);
	const FLinearColor Border(0.35f, 0.65f, 1.f, 0.85f);
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
		PaintGeom(Geometry, Min, Size), WhiteBrush, ESlateDrawEffect::None, Fill);

	// 四边：只描边不描心，避免盖住底下的片段
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
		PaintGeom(Geometry, Min, FVector2D(Size.X, 1.f)), WhiteBrush, ESlateDrawEffect::None, Border);
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
		PaintGeom(Geometry, FVector2D(Min.X, Max.Y - 1.f), FVector2D(Size.X, 1.f)), WhiteBrush, ESlateDrawEffect::None, Border);
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
		PaintGeom(Geometry, Min, FVector2D(1.f, Size.Y)), WhiteBrush, ESlateDrawEffect::None, Border);
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
		PaintGeom(Geometry, FVector2D(Max.X - 1.f, Min.Y), FVector2D(1.f, Size.Y)), WhiteBrush, ESlateDrawEffect::None, Border);
}

#undef LOCTEXT_NAMESPACE
