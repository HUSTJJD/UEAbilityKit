-- UAKDemoCharacter.lua
--
-- UnLuaNova 绑定脚本（由 AAKDemoCharacter::GetModuleName 指定）。
--
-- 分工原则：
--   UAK 的核心（执行器、任务、编辑器、序列化）是纯 C++，不依赖任何脚本系统；
--   Lua 只负责**项目特定**的部分：输入绑定、UI 联动、一次性玩法逻辑，
--   以及 UAK 刻意留出的扩展点（自定义任务、自定义引导条件）。
--
-- 这样既能享受脚本的迭代速度，又不会让框架核心被脚本语义绑死。

local M = {}

-- 缓存，避免每帧查表
local ActivatedCount = 0

function M:ReceiveBeginPlay()
    -- 角色初始化：这里只做 Lua 侧的事，ASC / 属性 / 技能授予在 C++ 里完成
    print("[UAKDemo] Character BeginPlay")

    -- 示例：监听技能激活（C++ 侧通过 UnLuaNova 的 LuaFunction / Delegate 回调到这里）
    self.OnAbilityActivated = function(AbilityName)
        ActivatedCount = ActivatedCount + 1
        print(string.format("[UAKDemo] Ability activated: %s (total %d)", tostring(AbilityName), ActivatedCount))
    end
end

function M:ReceiveEndPlay()
    self.OnAbilityActivated = nil
end

--- 激活一个技能（从 Lua 侧触发，走 GAS 的正常激活与预测流程）
-- @param AbilityClass UClass* —— UAKDemoAbility 的蓝图子类
function M:ActivateAbility(AbilityClass)
    local ASC = self:GetAbilitySystemComponent()
    if ASC == nil then
        print("[UAKDemo] No AbilitySystemComponent")
        return false
    end

    return ASC:TryActivateAbilityByClass(AbilityClass)
end

--- 示例：用 Lua 实现的"引导维持条件"——按住某个输入才算成立
--
-- UAK 不内置输入系统（按键绑定是项目相关的）。
-- 推荐做法：C++ 侧派生 UAKChanneling_CustomCondition，
-- 在 IsSatisfied 里通过 UnLuaNova 调用到这个 Lua 函数。
--
-- @return boolean true 表示条件仍然成立
function M:IsChannelingInputHeld()
    -- 伪代码：实际项目用 EnhancedInput 查询 Action 状态
    -- return UE.UAKDemoInput.IsActionHeld(self, "IA_Channel")
    return true
end

--- 示例：技能开始 / 结束的 Lua 钩子，便于做 UI 与音效联动
function M:OnAbilityStarted(AbilityName)
    -- 例如：显示读条 UI
    print("[UAKDemo] Started: " .. tostring(AbilityName))
end

function M:OnAbilityEnded(AbilityName, bWasCancelled)
    -- 例如：隐藏读条 UI；bWasCancelled 为真时播放"被打断"反馈
    if bWasCancelled then
        print("[UAKDemo] Cancelled: " .. tostring(AbilityName))
    else
        print("[UAKDemo] Ended: " .. tostring(AbilityName))
    end
end

return M
