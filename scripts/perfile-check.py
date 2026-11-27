#!/usr/bin/env python3
"""Compiles every hand-written .cpp of this project on its own, with the flags
Unreal's build tool uses.

Unreal builds a module amalgamated, which lets a .cpp borrow an include from a
neighbour; the editor, and this check, compile each file alone. Run it before
claiming a change builds cleanly:

    scripts/perfile-check.py            regenerate the database if missing, then check
    scripts/perfile-check.py --regen    regenerate, then check
    scripts/perfile-check.py --db-only  regenerate the database and stop

The database is .vscode/compile_commands.json, which also drives IntelliSense.
UE_ROOT names the engine (default ~/UnrealEngine/UE_5.9).
"""
import glob
import json
import os
import platform
import re
import shlex
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UE_ROOT = os.environ.get("UE_ROOT", os.path.expanduser("~/UnrealEngine/UE_5.9"))
PROJECT = os.path.join(ROOT, "RpgDnd5eUnreal.uproject")
DB_DIR = os.path.join(ROOT, ".vscode")
DB = os.path.join(DB_DIR, "compile_commands.json")
GENERATED = os.path.join(ROOT, "Plugins", "RpgWire", "Source", "RpgWire", "Private", "pb")


def regenerate():
    # The engine ships its .NET under a version and a platform: DotNet/<version>/<platform>/dotnet.
    arch = {"x86_64": "x64", "aarch64": "arm64"}.get(platform.machine(), platform.machine())
    dotnet = sorted(glob.glob(os.path.join(UE_ROOT, "Engine", "Binaries", "ThirdParty", "DotNet", "*", "linux-" + arch, "dotnet")))
    if not dotnet:
        sys.exit("perfile-check: no Unreal at %s (set UE_ROOT)" % UE_ROOT)
    ubt = os.path.join(UE_ROOT, "Engine", "Binaries", "DotNET", "UnrealBuildTool", "UnrealBuildTool.dll")
    os.makedirs(DB_DIR, exist_ok=True)
    subprocess.run([dotnet[0], ubt, "-Mode=GenerateClangDatabase", "-Project=" + PROJECT, "-OutputDir=" + DB_DIR,
                    "-Exclude=" + GENERATED + os.sep, "RpgDnd5eUnrealEditor", "Development", "Linux"],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main(argv):
    if "--db-only" in argv or "--regen" in argv or not os.path.exists(DB):
        regenerate()
    if "--db-only" in argv:
        print("perfile-check: wrote .vscode/compile_commands.json")
        return 0
    shown = max((int(a) for a in argv if a.isdigit()), default=0)
    plain = lambda s: re.sub(r"\x1b\[[0-9;]*m", "", s)
    bad = 0
    for entry in json.load(open(DB)):
        if "/Intermediate/" in entry["file"] or ".pb." in entry["file"]:
            continue
        command = entry.get("arguments") or shlex.split(entry["command"])
        started = time.time()
        run = subprocess.run(command + ["-fsyntax-only", "-Qunused-arguments"], cwd=entry["directory"],
                             capture_output=True, text=True)
        errors = [l for l in plain(run.stderr).splitlines() if ": error: " in l]
        short = os.path.relpath(entry["file"], ROOT)
        bad += run.returncode != 0
        print("  %-78s %s %d error(s)  %.0fs" % (short, "ok  " if run.returncode == 0 else "FAIL", len(errors), time.time() - started))
        for line in errors[:shown]:
            print("       " + line[:220])
    print("perfile-check: %s" % ("every hand-written file compiles alone" if not bad else "%d file(s) fail alone" % bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
