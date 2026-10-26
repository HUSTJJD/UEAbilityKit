-- UAKDemoCharacter.lua
--
-- UnLuaNova 绑定脚本（由 Config/DefaultUnLuaNova.ini 的 ClassModuleBindings 指定：
-- AKDemoCharacter → UAKDemoCharacter，蓝图子类一并覆盖）。
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
--
-- 函数名必须**与 UFUNCTION 同名**：覆写是按 UFunction 的名字去 Lua 表里找的
-- （UUnLuaNovaManager::Bind 里的 UEFunctions.Find(LuaFuncName)），`K2_` 前缀不会被去掉。
-- 所以这里叫 K2_OnAbilityInput，不是 OnAbilityInput —— 写错了不会报错，只是永远不触发。
--
-- 为什么输入绑定不放 Lua：绑定 InputComponent / EnhancedInput 的 API 不是 UFUNCTION，
-- 脚本调不到；而"按下去该干什么"是项目逻辑，正应该留在脚本里。
function M:K2_OnAbilityInput(SlotIndex, bPressed)
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
--
-- 注意方向：真正被每帧问的是 **C++ 的条件对象** `UAKDemoChanneling_InputHeld`，
-- 它读的是角色上的 C++ 状态（由上面的 SetChannelingInputHeld 写入），**不经过 Lua**。
-- 下面这个函数只是把同一份判断在 Lua 侧留了个可读的副本（不是 UFUNCTION，不会被覆写），
-- 去掉它引导照样工作 —— 判定留在 C++，是为了能校验、能进 Json、缺脚本时也不会卡死示例。
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

--- 技能开始 / 结束的 Lua 钩子，便于做 UI 与音效联动（同样按 UFUNCTION 名，K2_ 前缀要保留）
function M:K2_OnAbilityStarted(AbilityName)
    ActivatedCount = ActivatedCount + 1
    -- 例如：显示读条 UI
    print(string.format("[UAKDemo] Started: %s (第 %d 次)", tostring(AbilityName), ActivatedCount))
end

function M:K2_OnAbilityEnded(AbilityName, bWasCancelled)
    -- 例如：隐藏读条 UI；bWasCancelled 为真时播放"被打断"反馈
    if bWasCancelled then
        print(string.format("[UAKDemo] Cancelled: %s", tostring(AbilityName)))
    else
        print(string.format("[UAKDemo] Ended: %s", tostring(AbilityName)))
    end
end

return M
