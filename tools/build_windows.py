#!/usr/bin/env python3
"""Generate assets on Windows, then compile with the JieLi toolchain in WSL."""
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import build  # noqa: E402


def main():
    os.chdir(ROOT)
    print("== Generating firmware headers & assets (Windows) ==")
    build.generate()
    wsl_src = subprocess.check_output(
        ["wsl", "-d", "Ubuntu", "--exec", "wslpath", "-a", ROOT.as_posix()],
        text=True,
    ).strip()
    print(f"== Compiling firmware in WSL Ubuntu ({wsl_src}) ==")
    code = "import sys; sys.path.insert(0, 'tools'); import build; build.generate = lambda: None; sys.exit(build.main())"
    subprocess.run(
        [
            "wsl",
            "-d",
            "Ubuntu",
            "--cd",
            wsl_src,
            "--exec",
            "env",
            "JIELI_TOOLCHAIN=/home/fred/.jieli/toolchain",
            "AC79_SDK=/home/fred/fw-AC79_AIoT_SDK",
            "python3",
            "-c",
            code,
            *sys.argv[1:],
        ],
        check=True,
    )


if __name__ == "__main__":
    main()
