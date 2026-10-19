# 校验设计（已定稿，M3）

## 1. 与 Able 的关键差异

| | Able | UAK |
|---|---|---|
| 规则归属 | 外部 Validator 按 Task 类绑定 | **Task 自带 `Validate()`**，内聚 |
| 触发时机 | 手动点按钮，结果弹窗 | 改动即增量重算，常驻底栏 |
| 目标 | 事后报错 | **让错误无法被配置出来** |

## 2. 两级规则

### 资产级（`UAKAbilityAsset::Validate`）
- 长度必须 > 0（Error）
- 没有任何轨道（Warning）
- 没有任何任务（Warning）

### 任务级（各 Task 的 `Validate`，基类提供通用规则）
- 任务结束时间超出技能长度（Warning）
- 勾选 `bNeedsTick` 却是单帧任务（Warning）
- `ApplyGameplayEffect` 未指定 GE（Error）
- `PlayMontage` 未指定 Montage（Error）
- `SendGameplayCue` 的 Cue Tag 无效（Error）
- `Branch` 未指定目标轨道 / 目标轨道不存在（Error）

## 3. 三个消费方

| 消费方 | 行为 |
|---|---|
| `SUAKValidationBar`（编辑器底栏） | 用"任务名+时间"指纹判断变化，0.5s 节流增量重算 |
| `UAKValidateCommandlet`（CI） | 全量扫描，`-strict` 时按错误数返回非零退出码 |
| Json 导入 | 导入后可立即校验，发现 Tag/引用丢失 |

## 4. 规则编写约定

```cpp
void UMyTask::Validate(const UAKAbilityAsset* Asset, FAKValidationResult& Out) const
{
    Super::Validate(Asset, Out);   // 必须先调 Super
    ...
    Out.Add(EAKValidationSeverity::Error, Message, const_cast<UMyTask*>(this));
}
```

- 必须调用 `Super::Validate`，保证通用规则不丢。
- 传 `this` 以便 UI 双击定位（基类方法是 const，需 `const_cast`）。
- 严重度：Error = 运行时一定不对；Warning = 可能配错了；Info = 提示。

## 5. 待补（M5）

- Tag 有效性检查（当前 RequestGameplayTag 会静默返回空 Tag）。
- 资源引用存在性检查（GE / Montage / Cue 路径是否有效）。
- "查询任务之后没有效果任务"这类跨任务的模式检查。
- 规则集可由项目扩展（注册表而非硬编码）。
