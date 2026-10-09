#!/usr/bin/env python3
"""Check that every non-Valve file under game/mod_hl2mp/ has a row in CREDITS.md.

Files come from `git ls-files`. Valve's own files (the SDK template and the files derived from it) are listed in
VALVE below and need no row. The first table in CREDITS.md holds the paths: every `backticked` path is a pattern
in which `*` matches anything (also across `/`) and a trailing `/` matches a directory. Fails if a file has no row,
or a row under game/ matches no file. Run from anywhere: tools/release/credits_audit.py
"""
import fnmatch
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MOD = "game/mod_hl2mp/"

# Valve-origin files: the SDK template, or HL2DM files this mod adapted. Not ours to credit.
VALVE = [
    "game/mod_hl2mp/cfg/skill.cfg",
    "game/mod_hl2mp/cfg/skill_manifest.cfg",
    "game/mod_hl2mp/cfg/valve.rc",
    "game/mod_hl2mp/gameinfo.txt",
    "game/mod_hl2mp/steam.inf",
    "game/mod_hl2mp/maps/dm_lockdown.bsp",
    "game/mod_hl2mp/materials/example_model_material.vmt",
    "game/mod_hl2mp/materials/models/test/*",
    "game/mod_hl2mp/models/test/*",
    "game/mod_hl2mp/shaders/fxc/*",
    "game/mod_hl2mp/resource/*.res",
    "game/mod_hl2mp/resource/mod_hl2mp_english.txt",
    "game/mod_hl2mp/scripts/*",
]
# ...except these, which are ours (inside a Valve-derived directory).
OURS_IN_VALVE_DIRS = ["game/mod_hl2mp/scripts/game_sounds_kart.txt"]


def match(path, pattern):
    if pattern.endswith("/"):
        return path.startswith(pattern)
    return fnmatch.fnmatchcase(path, pattern)


def credit_patterns():
    pats = []
    in_table = False
    for line in (ROOT / "CREDITS.md").read_text().splitlines():
        if line.startswith("| File path |"):
            in_table = True
            continue
        if in_table and not line.startswith("|"):
            break
        if in_table and "EXAMPLE" not in line:
            first_cell = line.split("|")[1]
            pats += re.findall(r"`([^`]+)`", first_cell)
    return pats


def main():
    files = subprocess.run(
        ["git", "ls-files", MOD], cwd=ROOT, capture_output=True, text=True, check=True
    ).stdout.split("\n")
    files = [f for f in files if f]
    pats = credit_patterns()
    missing = []
    used = set()
    for f in files:
        hit = [p for p in pats if match(f, p)]
        used.update(hit)
        is_valve = any(match(f, p) for p in VALVE) and not any(match(f, p) for p in OURS_IN_VALVE_DIRS)
        if not hit and not is_valve:
            missing.append(f)
    stale = [p for p in pats if p.startswith(MOD) and p not in used]
    for f in missing:
        print(f"no CREDITS.md row: {f}")
    for p in stale:
        print(f"CREDITS.md row matches no file: {p}")
    if missing or stale:
        return 1
    print(f"credits audit OK: {len(files)} files under {MOD}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
