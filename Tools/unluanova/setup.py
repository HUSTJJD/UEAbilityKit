"""unluanova setup —— 一次把"必须手写两处"的地方写对。

对应 docs/examples/04 里四处"知道才会写对"的坑：

1. 子模块（含嵌套的 Lua / Luau）没拉全
2. ini 没钉住 LuaBackend，与编译出来的后端分叉
3. 使用方 Build.cs 忘了再列一次后端模块（UBT 不递归传递链接项）
4. 打包 stage 路径漏了插件运行时模块那条（编辑器正常、打包后 require 失败）

每条都幂等：已经对的就不动，只报告；需要写文件时先打印再写（--dry-run 只打印）。

    python Tools/unluanova/setup.py [--backend Luau|Lua] [--no-submodule]
                                    [--disable-extensions] [--dry-run]
切了 --backend 之后**必须重编**，ini 与二进制不能分叉。
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # Tools/
sys.path.insert(0, str(Path(__file__).resolve().parent))      # Tools/unluanova/

import common  # noqa: E402
import state  # noqa: E402


class Planner:
    """收集要做的改动，最后统一落地；--dry-run 时只走收集。"""

    def __init__(self, dry_run: bool) -> None:
        self.dry_run = dry_run
        self.changed: list[str] = []
        self.skipped: list[str] = []

    def write(self, path: Path, old: str, new: str, what: str) -> None:
        common.step(f"{what} → {state.rel(path)}")
        if self.dry_run:
            return
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(new, encoding="utf-8")
        self.changed.append(state.rel(path))

    def skip(self, what: str) -> None:
        common.step(f"{what}（已经是对的，跳过）")
        self.skipped.append(what)


# ── 1) 子模块 ──────────────────────────────────────────────────────────────
def ensure_submodule(plan: Planner) -> None:
    if state.submodule_checked_out():
        plan.skip("子模块 Plugins/UnLuaNova 已就位")
    else:
        common.step("初始化子模块（含嵌套的 Lua / Luau）")
        if not plan.dry_run:
            common.git(state.REPO_ROOT, "submodule", "update", "--init", "--recursive")

    if state.submodule_checked_out():
        missing = [k for k, v in state.third_party_present().items() if not v]
        if missing:
            common.step(f"补齐后端源码：{', '.join(missing)}")
            if not plan.dry_run:
                common.git(state.SUBMODULE_ROOT, "submodule", "update", "--init", "--recursive")


# ── 2) 后端 ini ────────────────────────────────────────────────────────────
def _set_ini_key(text: str, section: str, key: str, value: str) -> str:
    """幂等地把 section 下的 key 设成 value：有就改，没有就插，section 没有就建。"""
    lines = text.splitlines()
    sec_idx = -1
    for i, line in enumerate(lines):
        s = line.strip()
        if s.startswith("[") and s.endswith("]") and s[1:-1].strip() == section:
            sec_idx = i
            break

    entry = f"{key}={value}"

    if sec_idx >= 0:
        for j in range(sec_idx + 1, len(lines)):
            s = lines[j].strip()
            if s.startswith("[") and s.endswith("]"):
                break
            if "=" in s:
                k = s.partition("=")[0].strip()
                if k.lower() == key.lower():
                    lines[j] = entry
                    return "\n".join(lines) + "\n"
        lines.insert(sec_idx + 1, entry)
        return "\n".join(lines) + "\n"

    head = text.rstrip("\n")
    return f"{head}\n\n[{section}]\n{entry}\n" if head else f"[{section}]\n{entry}\n"


def ensure_backend_ini(plan: Planner, backend: str | None) -> str:
    want = backend or state.backend_from_ini() or state.DEFAULT_BACKEND
    if want not in state.BACKENDS:
        common.fail(f"后端只能是 {' / '.join(state.BACKENDS)}，收到 {want}")

    old = state.EDITOR_INI.read_text(encoding="utf-8-sig") if state.EDITOR_INI.is_file() else ""
    new = _set_ini_key(old, state.INI_SECTION, "LuaBackend", want)
    if new.strip() == old.strip():
        plan.skip(f"ini 已钉住 LuaBackend={want}")
    else:
        plan.write(state.EDITOR_INI, old, new, f"写入 LuaBackend={want}")
        common.warn("改了后端就要重编：ini 与二进制分叉会出现『目录叫 lua、跑的是 Luau』")
    return want


# ── 3) stage 路径 ──────────────────────────────────────────────────────────
PACKAGING_SECTION = "/Script/UnrealEd.ProjectPackagingSettings"


def ensure_stage_paths(plan: Planner) -> None:
    old = state.GAME_INI.read_text(encoding="utf-8-sig") if state.GAME_INI.is_file() else ""
    lines = old.splitlines()
    present = set(state.stage_paths())

    to_add = [p for p in state.REQUIRED_STAGE_PATHS if p not in present]
    if not to_add:
        plan.skip("stage 路径已齐全")
        return

    entries = [f'+DirectoriesToAlwaysStageAsUFS=(Path="{p}")' for p in to_add]

    sec_idx = -1
    for i, line in enumerate(lines):
        s = line.strip()
        if s.startswith("[") and s.endswith("]") and s[1:-1].strip() == PACKAGING_SECTION:
            sec_idx = i
            break

    if sec_idx >= 0:
        end = len(lines)
        for j in range(sec_idx + 1, len(lines)):
            if lines[j].strip().startswith("[") and lines[j].strip().endswith("]"):
                end = j
                break
        lines[end:end] = entries
        new = "\n".join(lines) + "\n"
    else:
        head = old.rstrip("\n")
        body = "\n".join(entries)
        new = f"{head}\n\n[{PACKAGING_SECTION}]\n{body}\n" if head else f"[{PACKAGING_SECTION}]\n{body}\n"

    plan.write(state.GAME_INI, old, new, f"补 stage 路径 {len(to_add)} 条")


# ── 4) 使用方 Build.cs 的后端模块 ───────────────────────────────────────────
ADD_RANGE = re.compile(
    r"(PrivateDependencyModuleNames\.AddRange\(\s*(?:new\s+string\[\]|new\[\])\s*\{\s*\n)")


def ensure_build_cs_backend(plan: Planner, backend: str) -> None:
    consumers = state.find_consumer_build_cs()
    if not consumers:
        plan.skip("没有使用方模块引用 UnLuaNova")
        return

    for path in consumers:
        text = path.read_text(encoding="utf-8-sig")
        got = state.backend_from_build_cs(path)
        if got == backend:
            plan.skip(f"{state.rel(path)} 已列 {backend}")
            continue
        if got is not None:
            # 列的后端与期望不同：改字面量比插一行安全
            new = text.replace(f'"{got}"', f'"{backend}"', 1)
            plan.write(path, text, new, f"{state.rel(path)}：{got} → {backend}")
            continue

        m = ADD_RANGE.search(text)
        if not m:
            common.warn(f"{state.rel(path)}：找不到 PrivateDependencyModuleNames.AddRange，"
                        f"请手动加上 \"{backend}\"（不敢猜你的代码结构）")
            continue

        insert_at = m.end()
        new = (text[:insert_at]
               + f'\t\t\t"{backend}",   // {state.MODULE_MARKER} 后端模块，须与 ini 的 LuaBackend 一致\n'
               + text[insert_at:])
        plan.write(path, text, new, f"{state.rel(path)}：补后端模块 {backend}")


# ── 5) 扩展插件 ────────────────────────────────────────────────────────────
def disable_extensions(plan: Planner) -> None:
    """把三个扩展插件在 .uproject 里显式写成 false。

    上游 .uplugin 的 EnabledByDefault 现在就是 false（没写 = 没开），所以这一条是
    "钉死"而不是"救火"：防止别处（模板工程、同事的分支）把它们带起来。
    """
    path = state.uproject_path()
    text = path.read_text(encoding="utf-8-sig")
    new = text

    for name in state.EXTENSION_PLUGINS:
        obj = re.search(r"\{[^{}]*\"Name\"\s*:\s*\"%s\"[^{}]*\}" % name, new)
        if obj:
            block = obj.group(0)
            if re.search(r'"Enabled"\s*:\s*false', block):
                continue
            fixed = re.sub(r'"Enabled"\s*:\s*true', '"Enabled": false', block)
            new = new.replace(block, fixed, 1)
            continue

        m = re.search(r'("Plugins"\s*:\s*\[)', new)
        if not m:
            common.warn(f"{state.rel(path)}：找不到 Plugins 数组，无法自动关闭 {name}")
            continue
        entry = f'\n\t{{\n\t\t"Name": "{name}",\n\t\t"Enabled": false\n\t}},'
        new = new[:m.end()] + entry + new[m.end():]

    if new == text:
        plan.skip("扩展插件已是关闭状态")
    else:
        plan.write(path, text, new, "关闭不需要的扩展插件")


# ── 入口 ───────────────────────────────────────────────────────────────────
USAGE = """用法：unluanova setup [选项]

  --backend Luau|Lua   指定后端（默认沿用现有 ini，没有就 Luau）；切完必须重编
  --no-submodule       跳过子模块初始化
  --disable-extensions 把 LuaSocket / LuaRapidjson / LuaProtobuf 在 .uproject 里显式关掉
                       （上游默认就是关的；这条是"钉死"，防止别处把它们带起来）
  --dry-run            只打印要做什么，不改文件

例：
  ./Tools/unluanova.ps1 setup
  ./Tools/unluanova.ps1 setup --backend Lua --disable-extensions
"""


def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if "-h" in argv or "--help" in argv:
        print(USAGE)
        return 0

    dry_run = "--dry-run" in argv
    backend = None
    if "--backend" in argv:
        i = argv.index("--backend")
        if i + 1 >= len(argv):
            common.fail("--backend 后面要跟 Luau 或 Lua")
        backend = argv[i + 1]

    plan = Planner(dry_run)
    common.info("UnLuaNova 集成 setup" + ("（dry-run，不改文件）" if dry_run else ""))

    if "--no-submodule" not in argv:
        ensure_submodule(plan)

    want = ensure_backend_ini(plan, backend)
    ensure_stage_paths(plan)
    ensure_build_cs_backend(plan, want)

    if "--disable-extensions" in argv:
        disable_extensions(plan)

    if dry_run:
        common.info("dry-run 结束，没有写任何文件")
        return 0

    if plan.changed:
        common.info(f"改了 {len(plan.changed)} 个文件：{', '.join(plan.changed)}")
        if any(p.endswith(".ini") for p in plan.changed):
            common.warn("ini 变了就重新编译，别让 ini 与二进制分叉")
    else:
        common.info("没有需要改的地方")
    common.info("下一步：跑 ./Tools/unluanova.ps1 doctor 复检")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
