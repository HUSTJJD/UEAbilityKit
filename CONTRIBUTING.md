# 贡献指南（Contributing）

感谢你愿意为 UEAbilityKit（UAK）出力。动手前请务必先读完**第 1 节 CLA** —— 这是本项目能同时提供免费授权与商业授权的法律前提。

- 许可与授权边界：[LICENSE](LICENSE)（PolyForm Small Business 1.0.0）、[LICENSE-COMMERCIAL.md](LICENSE-COMMERCIAL.md)
- 第三方与商标声明：[NOTICE](NOTICE)
- 贡献者许可协议：[CLA.md](CLA.md)

---

## 1. 贡献者许可协议（CLA）—— 必读，不可跳过

本项目是「**源码可得 + 商业授权**」双轨模式：小团队免费，超过规模/营收门槛的主体需付费授权。
要把包含你贡献的代码合法地卖给企业客户，维护者必须持有**每一位贡献者**的再授权许可。因此：

> **所有外部贡献都必须先签署 CLA，未签署的 PR 不会被合并。**

- 个人：在首个 PR 中回复 `I have read the CLA Document and I hereby sign and accept.`（也可由 CLA 机器人引导）。
- 企业 / 组织：由授权代表签署企业 CLA（CCLA），邮件至 `__________`，主题 `[UAK CCLA]`。
- 我们**不接受**仅以 DCO（`Signed-off-by`）替代 CLA：DCO 只证明来源合法，不授予维护者对外商业再授权的权利。
- 你保留贡献的著作权；贡献无偿，不参与商业授权收入分成。

条款全文见 [CLA.md](CLA.md)。

## 2. 参与方式

| 场景 | 渠道 |
|---|---|
| Bug、崩溃、编译问题 | GitHub Issues（附 UE 版本、平台、复现步骤与日志） |
| 新功能 / 设计提案 | 先开 Issue 讨论，**重大架构改动必须提前对齐**，避免返工 |
| 文档、示例、翻译 | 直接 PR |
| 安全问题 | 勿开公开 Issue，邮件至 `__________` |

## 3. 开发流程

1. Fork 仓库 → 从 `main` 切分支：`feat/xxx`、`fix/xxx`、`docs/xxx`；
2. 本地编译 + 跑测试通过；
3. 提交 PR，填写模板（`.github/PULL_REQUEST_TEMPLATE.md`），勾选 CLA；
4. Review：至少 1 位维护者批准后合并；合并采用 squash。

## 4. 构建与测试

需要先设置引擎路径环境变量 `$env:UE_ENGINE_ROOT`（或在脚本参数中显式传 `-EngineRoot`）。

```powershell
./Tools/build.ps1                        # UAKDemoEditor / Win64 / Development
./Tools/build.ps1 -Target UAKDemo -Configuration Shipping
./Tools/run_tests.ps1                    # 全量：Editor + Game + DS/多客户端，默认带截图/录屏
./Tools/run_tests.ps1 -Stage Editor -Filter UAK.Unit -SkipBuild
./Tools/run_tests.ps1 -Headless          # 没 GPU：只跑断言，不出素材
./Tools/run_tests.ps1 -Commit            # 跑完把结论提交（CI 再发布到 Pages）
```

Linux / macOS 使用 `Tools/run_tests.sh`（参数同义）。报告产物落在 `docs/reports/` 与 `docs/showcase.md`。

**PR 要求**：编译零新增警告；新增逻辑必须有对应的 UE Automation 测试；报告与文档需一并更新。

## 5. 代码规范

- 遵循 Unreal Engine 编码规范：`U` / `A` / `F` / `I` 前缀、`b` 开头布尔、`In` / `Out` 参数命名、`UPROPERTY` / `UFUNCTION` 标注齐全；
- 运行时模块不得依赖编辑器模块；编辑器代码一律放在 Editor 模块；
- 配置保持**可 diff 的纯数据**（Json），不要把逻辑硬编码进资产；
- 新增 Task / Trigger 需同时提供：参数结构、验证器规则、示例配置与文档；
- 不引入 Copyleft（GPL / LGPL / AGPL）代码，不引入与本项目模式冲突的第三方许可代码；引用第三方代码必须在 PR 中标明来源、许可与出处。

## 6. 提交信息

一行主题 + 可选正文，主题采用约定式前缀：

```
feat(timeline): 支持在轨道上批量对齐关键帧
fix(trace): 修正多实例技能溯源串号
docs(examples): 补充冲刺技能的 Json 示例
refactor(ability): 拆分 UAKAbilityAsset 的序列化逻辑
```

## 7. 行为准则

保持技术讨论就事论事。禁止人身攻击、歧视性言论与无关广告。维护者对争议有最终裁决权，情节严重者可封禁。

---

## 8. 常见问题

**Q：我签了 CLA，我的代码会被拿去卖钱吗？**
A：会包含在对企业的商业授权中，这正是 CLA 的目的；但贡献本身无偿、无分成，你的署名会保留在 Git 历史与文档中。

**Q：我的公司不同意签 CLA？**
A：那就请不要向本仓库提交代码。你可以自行 fork 使用（须遵守 PolyForm Small Business 条款并保留 `Required Notice:`）。

**Q：我提交的 PR 一直没被合并？**
A：CLA 未签署、与路线图冲突、缺少测试是最常见的三个原因。见 [docs/roadmap.md](docs/roadmap.md)。
