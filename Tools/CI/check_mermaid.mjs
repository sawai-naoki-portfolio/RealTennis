#!/usr/bin/env node
// Markdown 内の ```mermaid ブロックを mermaid-cli（mmdc）で描画し、構文エラーを検出する。
// 使い方：node Tools/CI/check_mermaid.mjs [file.md ...]（引数なしならリポジトリ内のすべての .md）
// 環境変数：MMDC（mmdc のパス）、CHROME_PATH（ローカルで既存の Chrome を使う場合）
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const SKIP = new Set([".git", "node_modules", "Binaries", "Intermediate", "Saved", "DerivedDataCache", ".vs"]);
const isWin = process.platform === "win32";

function findMmdc() {
  if (process.env.MMDC) return process.env.MMDC;
  const local = path.join(ROOT, "Tools", "CI", "node_modules", ".bin", isWin ? "mmdc.cmd" : "mmdc");
  return fs.existsSync(local) ? local : "mmdc";
}

function walk(dir, out = []) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    if (SKIP.has(e.name)) continue;
    const p = path.join(dir, e.name);
    if (e.isDirectory()) walk(p, out);
    else if (e.name.toLowerCase().endsWith(".md")) out.push(p);
  }
  return out;
}

function blocks(file) {
  const text = fs.readFileSync(file, "utf8").replace(/\r\n/g, "\n");
  const re = /^```mermaid\n([\s\S]*?)^```/gm;
  const out = [];
  let m;
  while ((m = re.exec(text))) out.push({ code: m[1], line: text.slice(0, m.index).split("\n").length + 1 });
  return out;
}

const mmdc = findMmdc();
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "mmd-"));
const pcfg = path.join(tmp, "puppeteer.json");
const cfg = { args: ["--no-sandbox", "--disable-setuid-sandbox"] };
if (process.env.CHROME_PATH) cfg.executablePath = process.env.CHROME_PATH;
fs.writeFileSync(pcfg, JSON.stringify(cfg));

const files = (process.argv.length > 2 ? process.argv.slice(2).map((f) => path.resolve(f)) : walk(ROOT)).sort();
let total = 0;
let failed = 0;
for (const file of files) {
  const rel = path.relative(ROOT, file).split(path.sep).join("/");
  blocks(file).forEach((b, i) => {
    total++;
    const inF = path.join(tmp, `b${total}.mmd`);
    fs.writeFileSync(inF, b.code);
    const quote = (s) => (isWin ? `"${s}"` : s);
    const r = spawnSync(quote(mmdc), ["-q", "-p", quote(pcfg), "-i", quote(inF), "-o", quote(path.join(tmp, `b${total}.svg`))], {
      encoding: "utf8",
      shell: isWin,
      timeout: 180000,
    });
    if (r.status === 0) {
      console.log(`ok   ${rel}:${b.line} (#${i + 1})`);
    } else {
      failed++;
      const msg = `${r.stderr || ""}${r.stdout || ""}${r.error ? r.error.message : ""}`
        .split("\n")
        .map((s) => s.trim())
        .filter(Boolean)
        .slice(0, 4)
        .join(" | ");
      console.log(`::error file=${rel},line=${b.line}::Mermaid の描画に失敗: ${msg}`);
    }
  });
}
console.log(`mermaid: ${total} diagram(s), ${failed} failure(s)`);
fs.rmSync(tmp, { recursive: true, force: true });
process.exit(failed ? 1 : 0);
