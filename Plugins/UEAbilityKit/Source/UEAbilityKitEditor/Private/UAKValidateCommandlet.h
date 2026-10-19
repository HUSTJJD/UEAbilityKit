#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "UAKValidateCommandlet.generated.h"

/**
 * CI 校验命令：把"配错"挡在合入之前。
 *
 * 用法：
 *   UnrealEditor-Cmd.exe <Project> -run=UAKValidate -path=/Game/Abilities -strict -json [-unattended]
 *
 * 参数：
 *   -path=<内容路径>   扫描路径，默认 /Game
 *   -strict            存在 Error 时返回非零退出码
 *   -json              同时导出 Json 到 Saved/UAK/
 */
UCLASS()
class UEABILITYKITEDITOR_API UAKValidateCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UAKValidateCommandlet();

	virtual int32 Main(const FString& Params) override;
};
