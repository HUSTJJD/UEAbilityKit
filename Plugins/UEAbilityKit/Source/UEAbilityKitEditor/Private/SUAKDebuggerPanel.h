#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

/**
 * 调试面板的一行（已扁平化成文本，避免 UI 每帧去解 UObject）。
 *
 * 为什么不直接把 FAKDebugEntry 塞进 SListView：
 * UE 5.8 的 SListView 只接受 UObject 指针 / TSharedPtr / FName 作为 ItemType，
 * 而且面板每秒刷 10 次，值类型行比持有弱引用更安全。
 */
struct FAKDebuggerRow
{
	FString OwnerName;
	FString AbilityName;
	FString TimeText;
	FString StateText;
	TArray<FString> TaskLines;
};

/**
 * UAK 调试面板（对标 AbleDebugger 的 UMG 列表，但这里是编辑器 Slate 面板）。
 *
 * 数据源：UEditor 侧从 PIE 世界的 UAKDebugSubsystem 取快照，每 0.1 秒刷新。
 * 没进 PIE 时显示提示，而不是空白——避免"不知道是没数据还是坏了"。
 */
class SUAKDebuggerPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUAKDebuggerPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<ITableRow> GenerateRow(TSharedPtr<FAKDebuggerRow> Item, const TSharedRef<STableViewBase>& OwnerTable);
	EActiveTimerReturnType Refresh(double InCurrentTime, float InDeltaTime);
	void RebuildRows();

	FText GetStatusText() const;
	FText GetToggleText() const;
	FReply OnToggleDrawClicked();

	TArray<TSharedPtr<FAKDebuggerRow>> Rows;
	TSharedPtr<SListView<TSharedPtr<FAKDebuggerRow>>> ListView;
	TSharedPtr<STextBlock> StatusText;

	bool bDrawEnabled = false;
	int32 ActiveCount = 0;
};
