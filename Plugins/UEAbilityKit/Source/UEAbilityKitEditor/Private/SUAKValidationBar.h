#pragma once

#include "CoreMinimal.h"
#include "UAKTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class UAKAbilityAsset;
class UAKAbilityTaskBase;

/**
 * 常驻校验条（UAK 与 Able 的一处刻意差异：Able 是校验完弹窗，这里边改边报）。
 *
 * 规则由各任务自己的 Validate() 提供，资产级规则由 UAKAbilityAsset::Validate 提供。
 */
class SUAKValidationBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUAKValidationBar) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UAKAbilityAsset>, Asset)
		SLATE_EVENT(FSimpleDelegate, OnIssueDoubleClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void Refresh();
	int32 GetErrorCount() const { return ErrorCount; }
	int32 GetWarningCount() const { return WarningCount; }

private:
	TSharedRef<ITableRow> GenerateRow(TSharedPtr<FAKValidationIssue> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnRowDoubleClicked(TSharedPtr<FAKValidationIssue> Item);
	FText GetHeaderText() const;

	TSharedPtr<STextBlock> HeaderText;

	TWeakObjectPtr<UAKAbilityAsset> Asset;
	FSimpleDelegate OnIssueDoubleClicked;

	// UE 5.8 起 SListView 的 ItemType 只能是 UObject 指针 / TSharedPtr / TSharedRef / FName
	TArray<TSharedPtr<FAKValidationIssue>> Issues;
	TSharedPtr<SListView<TSharedPtr<FAKValidationIssue>>> ListView;

	int32 ErrorCount = 0;
	int32 WarningCount = 0;

	float RefreshTimer = 0.f;
	/** 资产内容指纹，变化时才重算 */
	FString LastSignature;
};
