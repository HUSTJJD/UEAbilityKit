"""UnLuaNova 集成现状的只读快照。

只回答"工程现在是什么样"，不做判断也不改文件：判断在 doctor.py，修复在 setup.py。
这样三件事（看 / 判 / 修）各自独立，doctor 可以当 CI 门禁跑而永远不产生副作用。

关键约定（与 docs/examples/04 一致）：
- 插件本体在 submodule 的内层：Plugins/UnLuaNova/Plugins/UnLuaNova/
- 后端是**编译期**选择，ini（编辑器设置）与二进制（UnrealEditor-Luau.dll / -Lua.dll）
  必须一致；使用方 Build.cs 还得再列一次后端模块
- 脚本不是资产，必须显式 stage，且路径相对**工程 Content 目录**

后端隔离布局（Plugins/UnLuaNova/Tools/SwitchBackend.ps1）：
- 普通 target 把 DLL 写在 Binaries/Win64/ 根目录；示例工程的
  UnLuaNovaDemo<Backend>{Editor,Server,''} target 通过 ModuleRules.BinariesSubFolder
  把同一批 DLL 写进 Binaries/Win64/Backends/<Backend>/，两套产物可以并存。
- 谁生效由 *.modules 清单决定，不看 DLL 有多少份。非活动的清单被改名为
  *.modules.disabled，FModuleManager 递归扫描时看不见它。
- 因此「两个后端 DLL 同时存在」不再自动是错误；「两个后端清单同时 live」才是。
- 这里有两层 ini：外层工程用 REPO_ROOT/Config/DefaultUnLuaNova.ini（Build.cs 的
  编译期开关），示例工程用 SUBMODULE_ROOT/Config/DefaultUnLuaNova.ini（隔离 target
  下退化为运行时标签，由 SwitchBackend.ps1 维护）。doctor 各自按用途读。
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
ISOLATED_ROOT = BINARIES_DIR / "Backends"
MANIFEST_SUFFIX = ".modules"
DISABLED_SUFFIX = ".disabled"

# 外层工程自己的配置（Build.cs 的编译期开关）；示例工程另有一份，见 DEMO_INI。
UNLUANOVA_INI = REPO_ROOT / "Config" / "DefaultUnLuaNova.ini"
EDITOR_INI = UNLUANOVA_INI
# 示例工程的 ini：隔离 target 下 LuaBackend 只是运行时标签，由 SwitchBackend.ps1 写。
DEMO_INI = SUBMODULE_ROOT / "Config" / "DefaultUnLuaNova.ini"
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


def backend_from_demo_ini() -> str | None:
    """示例工程隔离布局下，ini 的 LuaBackend 是 SwitchBackend.ps1 维护的运行时标签。"""
    return read_ini_key(DEMO_INI, INI_SECTION, "LuaBackend")


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


# ── 二进制 / 后端隔离布局 ──────────────────────────────────────────────────
def isolated_root(backend: str) -> Path:
    return ISOLATED_ROOT / backend


def _dll_backends(root: Path) -> list[str]:
    """某个目录里 *-<Backend>.dll 对应哪些后端。

    DLL 名的前缀跟 AppName 走：Editor 是 UnrealEditor-Lua.dll，Game/Server 是
    UnLuaNovaDemoLua-Lua.dll 之类，所以只能按后缀认，不能拼死一个名字。
    """
    if not root.is_dir():
        return []
    return [b for b in BACKENDS if any(root.glob(f"*-{b}.dll"))]


def module_manifests(root: Path) -> dict[str, list[str]]:
    """目录里的 live / disabled 模块清单（只看文件名，不解析内容）。"""
    live: list[str] = []
    disabled: list[str] = []
    if not root.is_dir():
        return {"live": live, "disabled": disabled}
    for path in sorted(root.iterdir()):
        if not path.is_file():
            continue
        name = path.name
        if name.endswith(MANIFEST_SUFFIX + DISABLED_SUFFIX):
            disabled.append(name[: -len(DISABLED_SUFFIX)])
        elif name.endswith(MANIFEST_SUFFIX):
            live.append(name)
    return {"live": live, "disabled": disabled}


def backend_layout() -> dict:
    """只读快照：普通/隔离产物、清单状态，以及当前真正生效的后端。

    active 的取值：
      'Lua' / 'Luau'  该后端的隔离清单 live（可能还有 root 清单 live，那是
                      SwitchBackend.ps1 为没有隔离产物的模块 root 特意保留的）
      'Default'       只有 root 清单 live，走普通单 target 布局
      'ambiguous'     两个后端的隔离清单同时 live，模块解析不确定
      None            一个 live 清单都没有
    """
    root_manifests = module_manifests(BINARIES_DIR)
    per_backend: dict[str, dict] = {}
    for backend in BACKENDS:
        sub = isolated_root(backend)
        per_backend[backend] = {
            "dlls": _dll_backends(sub),
            **module_manifests(sub),
        }
    live_isolated = [b for b in BACKENDS if per_backend[b]["live"]]
    if len(live_isolated) > 1:
        active = "ambiguous"
    elif len(live_isolated) == 1:
        active = live_isolated[0]
    elif root_manifests["live"]:
        active = "Default"
    else:
        active = None
    return {
        "ordinary": _dll_backends(BINARIES_DIR),
        "isolated": [b for b in BACKENDS if per_backend[b]["dlls"]],
        "root": root_manifests,
        "per_backend": per_backend,
        "active": active,
    }


def built_backends() -> list[str]:
    """所有能找到后端 DLL 的地方：根目录 + Backends/<Backend>/，都要认。"""
    layout = backend_layout()
    found = set(layout["ordinary"])
    for backend in BACKENDS:
        if layout["per_backend"][backend]["dlls"]:
            found.add(backend)
    return [b for b in BACKENDS if b in found]


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
    """优先宿主工程的 .uproject；这个仓库也可以只作为插件仓库存在，此时回退到
    submodule 自带的示例工程，让只读体检仍能跑完而不是中途退出。"""
    found = sorted(REPO_ROOT.glob("*.uproject"))
    if found:
        return found[0]
    demo = sorted(SUBMODULE_ROOT.glob("*.uproject"))
    if demo:
        return demo[0]
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
