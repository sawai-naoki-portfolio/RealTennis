"""UE の自動テストの報告（-ReportExportPath の index.json）を読み、失敗・テストなしなら CI を止める。

  py -I Tools/CI/check_automation_report.py <ReportExportPath> [--prefix Tennis.]

結果は 1 行の要約と、失敗したテストの名前とエラーを出す（CI のログで理由が読めるように）。
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

PASSING_STATES = {"Success"}


def summarize(report: dict, prefix: str) -> tuple[int, list[str]]:
    """prefix で始まるテストの数と、失敗の説明の一覧を返す（純粋な関数）。"""
    tests = [t for t in report.get("tests", []) if str(t.get("fullTestPath", "")).startswith(prefix)]
    failures = []
    for test in tests:
        if test.get("state") not in PASSING_STATES:
            errors = [entry.get("event", {}).get("message", "") for entry in test.get("entries", [])
                      if entry.get("event", {}).get("type") == "Error"]
            failures.append(f"{test.get('fullTestPath')}: {test.get('state')} {'; '.join(e for e in errors if e)}".rstrip())
    return len(tests), failures


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report_dir", type=Path)
    parser.add_argument("--prefix", default="Tennis.")
    args = parser.parse_args(argv)

    index = args.report_dir / "index.json"
    if not index.is_file():
        print(f"automation: report not found: {index}")
        return 2
    report = json.loads(index.read_text(encoding="utf-8-sig"))
    total, failures = summarize(report, args.prefix)
    print(f"automation: {total - len(failures)}/{total} passed (prefix={args.prefix})")
    for failure in failures:
        print(f"  FAIL {failure}")
    if total == 0:
        print("automation: no tests matched the prefix")
        return 1
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
