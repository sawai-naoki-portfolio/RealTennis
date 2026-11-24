#!/usr/bin/env python3
"""リポジトリに追跡されているテキストファイルから、秘密情報らしい文字列を探す（開発ルール集（非公開） §5）。

- 見つけた値は伏せ字にして出力する（ログに秘密情報を残さない）
- 対象は `git ls-files` のファイル。読めないファイル（バイナリ）は飛ばす
- 誤検知を消すときは、パターンを直す（例外のリストは作らない。KISS）
"""
from __future__ import annotations

import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

ROOT = Path(__file__).resolve().parents[2]

# UE の Config で、値が入っていてはいけないキー（エディタが自動生成するトークン・資格情報）
CONFIG_SECRET_KEYS = "SecurityToken|Password|ApiKey|ClientSecret|Secret|AccessToken"

# (名前, 正規表現, 対象の拡張子。None ならすべて)
PATTERNS: list[tuple[str, re.Pattern[str], tuple[str, ...] | None]] = [
    ("GitHub token", re.compile(r"\b(?:gh[pousr]_[A-Za-z0-9]{36,}|github_pat_[A-Za-z0-9_]{60,})\b"), None),
    ("AWS access key", re.compile(r"\bAKIA[0-9A-Z]{16}\b"), None),
    ("Slack token", re.compile(r"\bxox[abprs]-[A-Za-z0-9-]{10,}\b"), None),
    ("Private key", re.compile(r"-----BEGIN (?:RSA |EC |DSA |OPENSSH |ENCRYPTED )?PRIVATE KEY-----"), None),
    # 行の中の空白だけを許す（[ \t]）。改行をまたいで次の行の値を拾わない。CRLF の \r も許す
    ("Config credential", re.compile(
        rf"^[ \t]*(?:{CONFIG_SECRET_KEYS})[ \t]*=[ \t]*(?!\[AUTO\])([^\s]+)[ \t]*\r?$",
        re.IGNORECASE | re.MULTILINE), (".ini",)),
]


@dataclass(frozen=True)
class Finding:
    path: str
    line: int
    kind: str
    masked: str


def mask(value: str) -> str:
    """先頭の 4 文字だけを残して伏せる。"""
    return value[:4] + "…" if len(value) > 4 else "…"


def scan_text(path: str, text: str) -> list[Finding]:
    findings: list[Finding] = []
    suffix = Path(path).suffix.lower()
    for kind, pattern, suffixes in PATTERNS:
        if suffixes is not None and suffix not in suffixes:
            continue
        for m in pattern.finditer(text):
            value = m.group(m.lastindex or 0)
            line = text.count("\n", 0, m.start()) + 1
            findings.append(Finding(path, line, kind, mask(value)))
    return findings


def tracked_files() -> list[str]:
    out = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, check=True, capture_output=True).stdout
    return [p for p in out.decode("utf-8").split("\0") if p]


def main() -> int:
    findings: list[Finding] = []
    files = tracked_files()
    for rel in files:
        try:
            # newline="" で改行を変換しない（CRLF のファイルもそのまま検査する）
            with open(ROOT / rel, encoding="utf-8", newline="") as fh:
                text = fh.read()
        except (UnicodeDecodeError, FileNotFoundError, IsADirectoryError):
            continue
        findings.extend(scan_text(rel, text))
    for f in findings:
        print(f"::error file={f.path},line={f.line}::秘密情報の可能性（{f.kind}）: {f.masked}")
    print(f"secrets: scanned {len(files)} files, {len(findings)} finding(s)")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
