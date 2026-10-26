"""UnLuaNova 集成现状的只读快照。

只回答"工程现在是什么样"，不做判断也不改文件：判断在 doctor.py，修复在 setup.py。
这样三件事（看 / 判 / 修）各自独立，doctor 可以当 CI 门禁跑而永远不产生副作用。

关键约定（与 docs/examples/04 一致）：
- 插件本体在 submodule 的内层：Plugins/UnLuaNova/Plugins/UnLuaNova/
- 后端是**编译期**选择，ini（编辑器设置）与二进制（UnrealEditor-Luau.dll / -Lua.dll）
  必须一致；使用方 Build.cs 还得再列一次后端模块
- 脚本不是资产，必须显式 stage，且路径相对**工程 Content 目录**
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # Tools/
import common  # noqa: E402

REPO_ROOT = common.repo_root()

SUBMODULE_ROOT = REPO_ROOT / "Plugins" / "UnLuaNova"
PLUGIN_ROOT = SUBMODULE_ROOT / "Plugins" / "UnLuaNova"
UPLUGIN = PLUGIN_ROOT / "UnLuaNova.uplugin"

THIRD_PARTY_ROOT = PLUGIN_ROOT / "Source" / "ThirdParty"
LUA_LIB_DIR = THIRD_PARTY_ROOT / "LuaLib" / "Lua"
LUAU_LIB_DIR = THIRD_PARTY_ROOT / "LuauLib" / "Luau"

BINARIES_DIR = PLUGIN_ROOT / "Binaries" / "Win64"

# 全插件只有一个配置文件
UNLUANOVA_INI = REPO_ROOT / "Config" / "DefaultUnLuaNova.ini"
EDITOR_INI = UNLUANOVA_INI
GAME_INI = REPO_ROOT / "Config" / "DefaultGame.ini"
INI_SECTION = "/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings"
RUNTIME_INI_SECTION = "/Script/UnLuaNova.UnLuaNovaSettings"

INTELLISENSE_DIR = PLUGIN_ROOT / "IntelliSense"

BACKENDS = ("Luau", "Lua")
DEFAULT_BACKEND = "Luau"

# 扩展插件：上游 .uplugin 的 EnabledByDefault 是 false，宿主 .uproject 里没写就是**没开**
EXTENSION_PLUGINS = ("LuaSocket", "LuaRapidjson", "LuaProtobuf")

# 打包时必须 stage 的两条（相对工程 Content 目录）
REQUIRED_STAGE_PATHS = (
    "Script",
    "../Plugins/UnLuaNova/Plugins/UnLuaNova/Content/Script",
)

MODULE_MARKER = "[UnLuaNova] auto-managed"


# ── ini ────────────────────────────────────────────────────────────────────
def read_ini_key(path: Path, section: str, key: str) -> str | None:
    """极简 ini 读取：只要 section 下的 Key=Value，够用且不引入新依赖。"""
    try:
        text = path.read_text(encoding="utf-8-sig")
    except OSError:
        return None

    current = None
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith((";", "#", "//")):
            continue
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1].strip()
            continue
        if current == section and "=" in line:
            k, _, v = line.partition("=")
            if k.strip().lower() == key.lower():
                return v.strip()
    return None


def backend_from_ini() -> str | None:
    return read_ini_key(UNLUANOVA_INI, INI_SECTION, "LuaBackend")


# ── 数据驱动绑定 ───────────────────────────────────────────────────────────
# 两个键必须成对出现才生效：定位器给出模块名之后，"要不要真的绑"由 bDataDrivenBinding 决定。
# 少了开关，映射表配了也不会绑 —— 不报错、不警告，是最容易白忙一场的一处。
def has_class_module_bindings() -> bool:
    try:
        text = UNLUANOVA_INI.read_text(encoding="utf-8-sig")
    except OSError:
        return False
    return re.search(r"^\s*\+?ClassModuleBindings\s*=", text, re.MULTILINE) is not None


def data_driven_binding_enabled() -> bool:
    value = read_ini_key(UNLUANOVA_INI, RUNTIME_INI_SECTION, "bDataDrivenBinding")
    return value is not None and value.lower() in ("true", "1", "yes")


# ── 二进制 ─────────────────────────────────────────────────────────────────
def built_backends() -> list[str]:
    """按 Binaries/Win64 里实际存在的后端 DLL 判断编译出来的后端。"""
    found = []
    for backend in BACKENDS:
        dll = BINARIES_DIR / f"UnrealEditor-{backend}.dll"
        if dll.is_file():
            found.append(backend)
    return found


# ── 打包 stage ─────────────────────────────────────────────────────────────
def stage_paths() -> list[str]:
    try:
        text = GAME_INI.read_text(encoding="utf-8-sig")
    except OSError:
        return []
    return re.findall(r'DirectoriesToAlwaysStageAsUFS=\(Path="([^"]+)"\)', text)


def missing_stage_paths() -> list[str]:
    present = set(stage_paths())
    return [p for p in REQUIRED_STAGE_PATHS if p not in present]


# ── 使用方 Build.cs ─────────────────────────────────────────────────────────
def find_consumer_build_cs() -> list[Path]:
    """引用了 UnLuaNova 的 Build.cs —— 它们都必须再列一次后端模块。"""
    roots = [REPO_ROOT / "Plugins" / "UEAbilityKit" / "Source", REPO_ROOT / "Source"]
    found: list[Path] = []
    for root in roots:
        if not root.is_dir():
            continue
        for path in root.rglob("*.Build.cs"):
            try:
                text = path.read_text(encoding="utf-8-sig")
            except OSError:
                continue
            if '"UnLuaNova"' in text:
                found.append(path)
    return sorted(found)


def backend_from_build_cs(path: Path) -> str | None:
    """Build.cs 里声明的后端：取 UnLuaNova 之后出现的那个后端字面量。"""
    text = path.read_text(encoding="utf-8-sig")
    idx = text.find('"UnLuaNova"')
    tail = text[idx:] if idx >= 0 else text
    for backend in BACKENDS:
        if f'"{backend}"' in tail:
            return backend
    return None


# ── 子模块 ─────────────────────────────────────────────────────────────────
def submodule_checked_out() -> bool:
    return UPLUGIN.is_file()


def third_party_present() -> dict[str, bool]:
    def _has(d: Path) -> bool:
        return d.is_dir() and any(d.iterdir())

    return {"Lua": _has(LUA_LIB_DIR), "Luau": _has(LUAU_LIB_DIR)}


def has_script_dir() -> bool:
    return (REPO_ROOT / "Content" / "Script").is_dir()


def intellisense_generated() -> bool:
    return (INTELLISENSE_DIR / "EmmyLua").is_dir() or (INTELLISENSE_DIR / "Luau").is_dir()


# ── .uproject ──────────────────────────────────────────────────────────────
def uproject_path() -> Path:
    return common.find_uproject(REPO_ROOT)


def uproject_plugins() -> dict[str, bool]:
    """已声明插件 → 是否启用；没声明的不出现在字典里。"""
    path = uproject_path()
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError):
        return {}
    out: dict[str, bool] = {}
    for entry in data.get("Plugins", []):
        name = entry.get("Name")
        if isinstance(name, str):
            out[name] = bool(entry.get("Enabled", True))
    return out


def rel(path: Path) -> str:
    try:
        return str(path.relative_to(REPO_ROOT)).replace("\\", "/")
    except ValueError:
        return str(path)
