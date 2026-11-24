#!/usr/bin/env python3
"""Unreal のプロジェクトファイル（.uproject）を検査する。

- JSON として読めること
- EngineAssociation が想定の版（5.8）であること
- Plugins の各要素に Name と Enabled（bool）があること
- Modules がある場合、各モジュールの Source/<Name>/<Name>.Build.cs が存在すること
- Config/DefaultEngine.ini があること
"""
import json
import sys
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

ROOT = Path(__file__).resolve().parents[2]
EXPECTED_ENGINE = "5.8"

projects = sorted(ROOT.glob("*.uproject"))
if not projects:
    print("skip: .uproject がありません")
    sys.exit(0)

errors = 0
for p in projects:
    name = p.name
    try:
        data = json.loads(p.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as e:
        print(f"::error file={name},line={e.lineno}::JSON として読めません: {e.msg}")
        errors += 1
        continue
    eng = data.get("EngineAssociation")
    if eng != EXPECTED_ENGINE:
        print(f"::error file={name}::EngineAssociation が {EXPECTED_ENGINE} ではありません: {eng!r}")
        errors += 1
    for i, pl in enumerate(data.get("Plugins", [])):
        if not isinstance(pl.get("Name"), str) or not isinstance(pl.get("Enabled"), bool):
            print(f"::error file={name}::Plugins[{i}] に Name または Enabled がありません: {pl}")
            errors += 1
    for m in data.get("Modules", []):
        mod = m.get("Name")
        build = ROOT / "Source" / str(mod) / f"{mod}.Build.cs"
        if not build.exists():
            print(f"::error file={name}::モジュール {mod} の Build.cs がありません: {build.relative_to(ROOT).as_posix()}")
            errors += 1
    print(f"{name}: engine={eng}, plugins={len(data.get('Plugins', []))}, modules={len(data.get('Modules', []))}")

if not (ROOT / "Config" / "DefaultEngine.ini").exists():
    print("::error::Config/DefaultEngine.ini がありません")
    errors += 1

sys.exit(1 if errors else 0)
