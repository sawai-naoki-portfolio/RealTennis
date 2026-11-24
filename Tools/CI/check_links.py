#!/usr/bin/env python3
"""Markdown の相対リンクを検査する。

- リンク先のファイル（またはフォルダ）が存在すれば OK
- 存在しない場合：
  - LINK_CHECK_STRICT=1 のときはエラー
  - そうでなければ、Docs/README.md（ドキュメントの索引）に載っている「予定のドキュメント」なら警告、
    載っていなければエラー（打ち間違いを見つけるため）
- LINK_CHECK_STRICT=1 のときは、索引に載っているドキュメントがすべて存在することも確認する
- 外部 URL（http/https/mailto など）、ページ内アンカー、コードブロックとインラインコードの中は対象外
"""
from __future__ import annotations

import os
import re
import sys
from pathlib import Path
from urllib.parse import unquote

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

ROOT = Path(__file__).resolve().parents[2]
INDEX = ROOT / "Docs" / "README.md"
SKIP_DIRS = {".git", "node_modules", "Binaries", "Intermediate", "Saved", "DerivedDataCache", ".vs"}
LINK_RE = re.compile(r"(?<!!)\[(?P<text>[^\]]*)\]\((?P<target>[^)\s]+)(?:\s+\"[^\"]*\")?\)")
FENCE_RE = re.compile(r"^\s*(```|~~~)")
INLINE_CODE_RE = re.compile(r"`[^`]*`")
SCHEME_RE = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")
STRICT = os.environ.get("LINK_CHECK_STRICT", "0") == "1"


def md_files() -> list[Path]:
    out = []
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for f in filenames:
            if f.lower().endswith(".md"):
                out.append(Path(dirpath) / f)
    return sorted(out)


def iter_links(path: Path):
    in_fence = False
    for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if FENCE_RE.match(line):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        for m in LINK_RE.finditer(INLINE_CODE_RE.sub("", line)):
            yield lineno, m.group("target")


def resolve(src: Path, target: str) -> Path | None:
    if SCHEME_RE.match(target) or target.startswith("#"):
        return None  # 外部 URL・ページ内アンカー
    target = unquote(target.split("#", 1)[0])
    if not target:
        return None
    base = ROOT if target.startswith("/") else src.parent
    return (base / target.lstrip("/")).resolve()


def rel(p: Path) -> str:
    try:
        return p.relative_to(ROOT).as_posix()
    except ValueError:
        return p.as_posix()


def main() -> int:
    declared: set[Path] = set()
    if INDEX.exists():
        for _, t in iter_links(INDEX):
            r = resolve(INDEX, t)
            if r is not None:
                declared.add(r)

    files = md_files()
    errors = warnings = checked = 0
    for f in files:
        for lineno, target in iter_links(f):
            r = resolve(f, target)
            if r is None:
                continue
            checked += 1
            if r.exists():
                continue
            loc = f"file={rel(f)},line={lineno}"
            if not STRICT and r in declared:
                warnings += 1
                print(f"::warning {loc}::予定のドキュメントへのリンク（未作成）: {target}")
            else:
                errors += 1
                print(f"::error {loc}::リンク切れ: {target}")

    if STRICT and INDEX.exists():
        for d in sorted(declared):
            if not d.exists():
                errors += 1
                print(f"::error file={rel(INDEX)}::索引に載っているドキュメントがありません: {rel(d)}")

    mode = "strict" if STRICT else "normal"
    print(f"checked {checked} links in {len(files)} files (mode={mode}): {errors} error(s), {warnings} warning(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
