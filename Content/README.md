# Content

示例工程的内容目录。

仓库只提交**源码**，不提交二进制资产——技能资产（`UAKAbilityAsset`）、GE、
GameplayCue、角色与动画需要你在编辑器里创建。

最小可行步骤：

1. **Content/Abilities/** 新建 `Ability Asset`（右键 → Miscellaneous → Ability Asset），
   加一条 Effect 轨道，放一个 `ApplyGameplayEffect` 任务
2. **Content/GE/** 新建：
   - `GE_Cooldown_Sample`：Has Duration 5s，Granted Tags = `Cooldown.Sample`
   - `GE_Cost_Sample`：Instant，Modifiers: Mana Additive -20
   - `GE_Damage_Sample`：Instant，Modifiers: Health Additive -30
3. **Content/Abilities/** 新建 `BP_SampleAbility` 继承 `UAKDemoAbility`，
   填 `AbilityAsset` / `SampleCooldownEffect` / `SampleCostEffect`
4. 新建 `BP_DemoCharacter` 继承 `AAKDemoCharacter`，把 `BP_SampleAbility` 填进 `Default Abilities`
5. 新建一个关卡，把 `BP_DemoCharacter` 放进去并设为 `GameDefaultMap`

## Script/

`Content/Script/UAKDemoCharacter.lua` 是 UnLuaNova 绑定脚本（由 `AAKDemoCharacter::GetModuleName` 指定）。
脚本只负责项目特定的扩展：输入绑定、UI 联动、自定义引导条件。
详见 [../docs/examples/04-scripting-with-unlua.md](../docs/examples/04-scripting-with-unlua.md)。

## 更多

- 工程结构与迁移记录：[../docs/examples/03-sample-project.md](../docs/examples/03-sample-project.md)
- 多人接入：[../docs/examples/01-minimal-multiplayer-setup.md](../docs/examples/01-minimal-multiplayer-setup.md)
- 冷却 / 消耗 / 堆叠：[../docs/examples/02-cooldown-cost-stacking.md](../docs/examples/02-cooldown-cost-stacking.md)
