"""unluanova doctor —— 把 docs/examples/04 里那些"知道才会写对"的地方变成可自检的结论。

判据全部来自 state.py 的只读快照，本文件不写任何文件，所以可以放心当 CI 门禁：
有 error 就退出码 1，warn 只提示。

    python Tools/unluanova/doctor.py [--json] [--strict]
    --strict：warn 也算失败（给"新接入的机器"用）
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import state  # noqa: E402

OK, WARN, ERROR = "ok", "warn", "error"


class Report:
    def __init__(self) -> None:
        self.items: list[dict[str, str]] = []

    def add(self, check: str, level: str, detail: str, hint: str = "") -> None:
        self.items.append({"check": check, "level": level, "detail": detail, "hint": hint})

    def count(self, level: str) -> int:
        return sum(1 for i in self.items if i["level"] == level)

    def failed(self, strict: bool) -> bool:
        return self.count(ERROR) > 0 or (strict and self.count(WARN) > 0)


def inspect() -> Report:
    r = Report()

    # 1) 子模块
    if not state.submodule_checked_out():
        r.add("submodule", ERROR, "Plugins/UnLuaNova 没有内容",
              "git submodule update --init --recursive（或跑 ./Tools/unluanova.ps1 setup）")
    else:
        r.add("submodule", OK, f"插件已就位：{state.rel(state.UPLUGIN)}")

        # 2) 后端源码（子模块的嵌套子模块最容易忘）
        tp = state.third_party_present()
        missing = [k for k, v in tp.items() if not v]
        if missing:
            r.add("third-party", ERROR, f"后端源码缺失：{', '.join(missing)}",
                  "git submodule update --init --recursive")
        else:
            r.add("third-party", OK, "Lua / Luau 后端源码都在")

    # 3) 后端 ini
    ini_backend = state.backend_from_ini()
    if ini_backend is None:
        r.add("backend-ini", WARN,
              f"{state.rel(state.EDITOR_INI)} 里没有 LuaBackend，走隐式默认 {state.DEFAULT_BACKEND}",
              f"显式写死：[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings] LuaBackend={state.DEFAULT_BACKEND}")
    elif ini_backend not in state.BACKENDS:
        r.add("backend-ini", ERROR, f"LuaBackend 的值不认识：{ini_backend}",
              "只能是 Luau 或 Lua")
    else:
        r.add("backend-ini", OK, f"ini 钉住后端：{ini_backend}")

    # 4) 二进制与 ini 是否分叉
    built = state.built_backends()
    if len(built) > 1:
        r.add("backend-binary", ERROR, f"两个后端 DLL 同时存在：{', '.join(built)}",
              "切后端必须重编；删掉旧 DLL 再编，避免 include 路径里出现两份 lua.h")
    elif not built:
        r.add("backend-binary", WARN, "还没有后端 DLL（未编译？）", "编译一次后再跑 doctor")
    elif ini_backend and built[0] != ini_backend:
        r.add("backend-binary", ERROR,
              f"ini 写的是 {ini_backend}，编译出来的是 {built[0]}（分叉）",
              "改 ini 后必须重编；两者不一致会出现『目录叫 lua、跑的是 Luau』")
    else:
        r.add("backend-binary", OK, f"编译后端：{built[0]}")

    want = ini_backend or (built[0] if built else state.DEFAULT_BACKEND)

    # 5) 使用方 Build.cs 必须再列一次后端模块
    consumers = state.find_consumer_build_cs()
    if not consumers:
        r.add("build-cs", OK, "没有使用方模块引用 UnLuaNova")
    for path in consumers:
        got = state.backend_from_build_cs(path)
        if got is None:
            r.add("build-cs", ERROR, f"{state.rel(path)} 引用了 UnLuaNova 却没列后端模块",
                  "UBT 依赖不递归传递链接项，少了它 lua_* 会 LNK2019 或运行期踩到错误实现")
        elif want and got != want:
            r.add("build-cs", ERROR, f"{state.rel(path)} 列的后端是 {got}，期望 {want}",
                  "UnLuaNova.Build.cs 与本文件读的是同一个 ini 键，分叉说明 ini 改过但没重编")
        else:
            r.add("build-cs", OK, f"{state.rel(path)} 后端模块：{got}")

    # 6) 打包 stage 路径
    missing = state.missing_stage_paths()
    if missing:
        r.add("stage", ERROR, f"DefaultGame.ini 缺少 stage 路径：{', '.join(missing)}",
              "漏了插件运行时模块那条：编辑器里一切正常，打包后 require 才失败")
    else:
        r.add("stage", OK, f"stage 路径齐全（{len(state.REQUIRED_STAGE_PATHS)} 条必需）")

    # 7) 扩展插件（嵌套布局会被一起发现并编译）
    plugins = state.uproject_plugins()
    enabled_ext = [p for p in state.EXTENSION_PLUGINS if plugins.get(p, True)]
    if enabled_ext:
        r.add("extensions", WARN, f"扩展插件会被一起编译：{', '.join(enabled_ext)}",
              "不需要就在 .uproject 里设 \"Enabled\": false，或跑 setup --disable-extensions")
    else:
        r.add("extensions", OK, "扩展插件已关闭")

    # 8) 工程脚本目录
    if state.has_script_dir():
        r.add("script-dir", OK, "Content/Script 存在")
    else:
        r.add("script-dir", WARN, "没有 Content/Script", "脚本落点缺省是工程 Content/Script/")

    # 9) IntelliSense 产物（编辑器里导出过一次才会有）
    if state.intellisense_generated():
        r.add("intellisense", OK, "IntelliSense 产物已生成")
    else:
        r.add("intellisense", WARN, f"{state.rel(state.INTELLISENSE_DIR)} 还没有产物",
              "编辑器工具栏导出，或 UnrealEditor-Cmd <uproject> -run=UnLuaNovaIntelliSense -BP=1")

    return r


def render(r: Report, as_json: bool) -> None:
    if as_json:
        # ensure_ascii=True：Windows 控制台是 GBK，直接打中文会 UnicodeEncodeError；
        # JSON 的 \uXXXX 转义任何环境都能安全打印与重定向
        print(json.dumps({"items": r.items}, ensure_ascii=True, indent=2))
        return

    # 只用 ASCII：Windows 控制台默认 GBK，'✔'/'→' 这类字符会直接 UnicodeEncodeError
    icon = {OK: "[ok]", WARN: "[!!]", ERROR: "[XX]"}
    for item in r.items:
        print(f"  {icon[item['level']]} {item['check']:<15} {item['detail']}")
        if item["hint"]:
            print(f"      -> {item['hint']}")
    print()
    print(f"[UAK] doctor：{r.count(OK)} 项正常 / {r.count(WARN)} 项警告 / {r.count(ERROR)} 项错误")


def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    as_json = "--json" in argv
    strict = "--strict" in argv

    if not as_json:
        print("[UAK] UnLuaNova 集成体检\n")

    r = inspect()
    render(r, as_json)
    return 1 if r.failed(strict) else 0


if __name__ == "__main__":
    raise SystemExit(main())
