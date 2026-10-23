"""Lyra 相关功能的唯一入口：import / clean / patch / verify。

历史上 import / clean / patch / verify 是四个独立脚本（且各有一份 ps1 + sh），
现在合成一个入口，用子命令区分；实现分别在同目录的 importer.py / clean.py /
patch.py / verify.py 里，shell 只负责转发。

    python Tools/lyra/cli.py import --lyra-path <Lyra 目录>
    python Tools/lyra/cli.py clean --keep-assets
    python Tools/lyra/cli.py patch --check
    python Tools/lyra/cli.py verify --self-test

shell 入口（Tools/lyra.ps1 / lyra.sh）只是把参数原样转发到这里。
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # Tools/lyra/

USAGE = """用法：lyra <子命令> [参数]

  import   把 Lyra 拷进本工程并打补丁（先清 → 拷 → 打补丁 → 校验）
  clean    清除从 Lyra 拷进来的内容，回到未导入状态
  patch    打 / 撤销 / 检查 Lyra 补丁（copy → patch）
  verify   校验有没有人手改过 Lyra

例：
  ./lyra.ps1 import --lyra-path <Lyra 目录> --engine-root <引擎目录>
  ./lyra.ps1 clean --keep-assets
  ./lyra.ps1 patch --check
  ./lyra.ps1 verify

每个子命令都有 --help：./lyra.ps1 import --help
"""

COMMANDS = {
    "import": ("importer", "main"),
    "clean": ("clean", "main"),
    "patch": ("patch", "main"),
    "verify": ("verify", "main"),
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
