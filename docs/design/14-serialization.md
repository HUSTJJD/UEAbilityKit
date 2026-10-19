# 序列化设计（已定稿，M3）

## 1. 为什么要有它

Able 的资产是 uasset 二进制，不可 diff，社区被迫外挂 Json + Git 双轨维护。
UAK 把"可读配置 ↔ 运行时双向"作为一等能力（这也是 AbilityKit 的核心思想之一）。

## 2. 三条规则

1. **只导出逻辑字段**：`Preview*` 等 `WITH_EDITORONLY_DATA` 字段不进 Json。
2. **强类型而非属性表 dump**：每个任务用 `ExportJson` / `ImportJson` 写自己的字段，
   字段名稳定、可 review——这正是"强类型 Action Schema"的落地。
3. **版本可迁移**：资产带 `Version`、Json 带 `version`；未来版本拒绝静默失败。

## 3. 格式

```json
{
  "version": 1,
  "asset": "Fireball",
  "length": 1.5,
  "playRate": 1.0,
  "frameRate": 60,
  "loop": false,
  "abilityTags": "(Ability.Fire)",
  "markers": [{ "name": "Cast", "time": 0.3 }],
  "tracks": [
    {
      "name": "Effect", "type": "Effect", "enabled": true,
      "tasks": [
        {
          "$type": "UAKAbilityTask_ApplyGameplayEffect",
          "startTime": 0.30, "duration": 0.0,
          "effectClass": "/Game/GE/GE_Damage.GE_Damage_C",
          "level": 1.0,
          "setByCaller": { "Damage": 120 }
        }
      ]
    }
  ]
}
```

- 资源引用导出为**路径字符串**，导入时用 `LoadClass` / `LoadObject`。
- 任务用 `$type` 定位类：先查 `/Script/UEAbilityKit.<Name>`，再按路径当蓝图类加载。

## 4. 已知限制

- `FGameplayAttribute`（Branch 任务）由 UProperty 指针构成，**无法从字符串安全重建**，
  导入时不恢复，由校验器报出来避免静默丢失。
- `FGameplayTag` 按名字 `RequestGameplayTag`，标签不存在时不会报错（返回空 Tag），
  校验器需检查 Tag 有效性。
- 蓝图派生的任务类：导出的是路径，导入要求该蓝图仍存在。

## 5. 使用场景

| 场景 | 用法 |
|---|---|
| Code Review | Json 进 Git，技能改动在 PR 里可读 |
| CI | `UAKValidateCommandlet -json` 一次导出全量 |
| 批量改数值 | 改 Json → 导入 |
| 跨项目复用 | 拷贝 Json（路径引用需随项目调整） |
