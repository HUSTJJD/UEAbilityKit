-- UAKDemoCharacter.lua
--
-- UnLuaNova 绑定脚本（由 AAKDemoCharacter::GetModuleName 指定）。
--
-- 分工原则：
--   UAK 的核心（执行器、任务、编辑器、序列化）是纯 C++，不依赖任何脚本系统；
--   Lua 只负责**项目特定**的部分：输入绑定后的业务动作、UI 联动、一次性玩法逻辑，
--   以及 UAK 刻意留出的扩展点（自定义引导条件）。
--
-- 这样既能享受脚本的迭代速度，又不会让框架核心被脚本语义绑死。

local M = {}

-- 技能槽说明：索引与 C++ 侧 DefaultAbilities 的顺序一致
local SlotNames = {
    [0] = "近战挥砍",
    [1] = "引导火球",
}

-- 按键按下状态：引导技能需要「按住」才算数
local HeldSlots = {}

local ActivatedCount = 0

function M:ReceiveBeginPlay()
    -- 角色初始化：这里只做 Lua 侧的事，ASC / 属性 / 技能授予在 C++ 里完成
    print("[UAKDemo] Character BeginPlay")
end

function M:ReceiveEndPlay()
    HeldSlots = {}
end

--- C++ 侧把输入绑定到这个事件（BlueprintImplementableEvent）。
-- 为什么输入绑定不放 Lua：绑定 InputComponent / EnhancedInput 的 API 不是 UFUNCTION，
-- 脚本调不到；而"按下去该干什么"是项目逻辑，正应该留在脚本里。
function M:OnAbilityInput(SlotIndex, bPressed)
    HeldSlots[SlotIndex] = bPressed

    -- 告诉 C++ 这个槽位是否还按着：UAKDemoChanneling_InputHeld 会来问它
    self:SetChannelingInputHeld(SlotIndex, bPressed)

    if not bPressed then
        return
    end

    local SlotName = SlotNames[SlotIndex] or ("槽 " .. tostring(SlotIndex))
    print(string.format("[UAKDemo] 输入：%s", SlotName))

    -- 走 GAS 的正常激活流程（含冷却 / 消耗 / 预测），不绕过 CommitAbility
    self:TryActivateAbilityByIndex(SlotIndex)
end

--- UAK 不内置输入系统，引导维持条件由项目自己回答。
-- C++ 侧用 UAKChanneling_CustomCondition 转发到这里。
function M:IsChannelingInputHeld(SlotIndex)
    -- 槽 0（近战）不需要维持；槽 1（火球）要求按住不放
    if SlotIndex == 0 then
        return true
    end
    return HeldSlots[SlotIndex] == true
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

--- 技能开始 / 结束的 Lua 钩子，便于做 UI 与音效联动
function M:OnAbilityStarted(AbilityName)
    ActivatedCount = ActivatedCount + 1
    -- 例如：显示读条 UI
    print(string.format("[UAKDemo] Started: %s (第 %d 次)", tostring(AbilityName), ActivatedCount))
end

function M:OnAbilityEnded(AbilityName, bWasCancelled)
    -- 例如：隐藏读条 UI；bWasCancelled 为真时播放"被打断"反馈
    if bWasCancelled then
        print(string.format("[UAKDemo] Cancelled: %s", tostring(AbilityName)))
    else
        print(string.format("[UAKDemo] Ended: %s", tostring(AbilityName)))
    end
end

return M
