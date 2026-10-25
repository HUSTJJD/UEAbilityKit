"""UnLuaNova 相关功能的唯一入口：setup / doctor。

和 Tools/lyra/cli.py 同一套约定：**shell 只是薄入口，逻辑全在 Python**。

    python Tools/unluanova/cli.py setup [--backend Luau|Lua] [--disable-extensions]
    python Tools/unluanova/cli.py doctor [--json] [--strict]

doctor 只读不写，可以直接当 CI 门禁：有 error 退出码 1。
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # Tools/unluanova/

USAGE = """用法：unluanova <子命令> [参数]

  setup    一次把集成写对：子模块 / 后端 ini / stage 路径 / Build.cs 后端
  doctor   体检：子模块、后端一致性（ini ↔ 二进制 ↔ Build.cs）、打包路径、扩展插件

例：
  ./Tools/unluanova.ps1 setup
  ./Tools/unluanova.ps1 doctor
  ./Tools/unluanova.ps1 doctor --strict      # 警告也算失败（新机器接入用）

每个子命令都有 --help。
"""

COMMANDS = {
    "setup": ("setup", "main"),
    "doctor": ("doctor", "main"),
}


def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv or argv[0] in ("-h", "--help", "help"):
        print(USAGE)
        return 0
    if argv[0] not in COMMANDS:
        print(f"[UAK] 未知子命令：{argv[0]}\n", file=sys.stderr)
        print(USAGE, file=sys.stderr)
        return 2

    name, func = COMMANDS[argv[0]]
    module = __import__(name)
    return int(getattr(module, func)(argv[1:]) or 0)


if __name__ == "__main__":
    raise SystemExit(main())
