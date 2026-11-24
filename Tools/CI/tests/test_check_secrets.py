"""check_secrets.py の単体テスト。

テストの中に本物らしいトークンの文字列を書かない（このファイル自体が検査に引っかかるため）。
偽の値は実行時に組み立てる。
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_secrets as cs  # noqa: E402

INI = "Config/DefaultEngine.ini"
HEX32 = "0123456789ABCDEF" * 2


def fake(prefix: str, n: int, ch: str = "a") -> str:
    return prefix + ch * n


class ScanTextTest(unittest.TestCase):
    def test_detects_github_token(self):
        text = "token = " + fake("gh" + "p_", 36) + "\n"
        found = cs.scan_text("notes.md", text)
        self.assertEqual([f.kind for f in found], ["GitHub token"])
        self.assertEqual(found[0].line, 1)

    def test_detects_aws_key_on_correct_line(self):
        text = "a\nb\nkey=" + fake("AK" + "IA", 16, "B") + "\n"
        found = cs.scan_text("x.txt", text)
        self.assertEqual(len(found), 1)
        self.assertEqual(found[0].line, 3)

    def test_detects_private_key_header(self):
        text = "-----BEGIN " + "RSA PRIVATE KEY-----\n"
        self.assertEqual([f.kind for f in cs.scan_text("k.pem", text)], ["Private key"])

    def test_config_token_with_value_is_detected(self):
        found = cs.scan_text(INI, "[Section]\nSecurityToken=" + HEX32 + "\n")
        self.assertEqual([f.kind for f in found], ["Config credential"])

    def test_config_token_with_value_crlf_is_detected(self):
        found = cs.scan_text(INI, "SecurityToken=" + HEX32 + "\r\nX=1\r\n")
        self.assertEqual(len(found), 1)

    def test_config_empty_or_auto_token_is_allowed(self):
        for value in ("", "[AUTO]"):
            with self.subTest(value=value):
                self.assertEqual(cs.scan_text(INI, f"SecurityToken={value}\n"), [])

    def test_config_empty_value_does_not_reach_next_line_crlf(self):
        self.assertEqual(cs.scan_text(INI, "SecurityToken=\r\nbIncludeInShipping=False\r\n"), [])

    def test_config_pattern_only_applies_to_ini(self):
        self.assertEqual(cs.scan_text("Docs/a.md", "Password=hunter2\n"), [])

    def test_finding_is_masked(self):
        token = fake("gh" + "p_", 36)
        found = cs.scan_text("a.md", token)
        self.assertNotIn(token, found[0].masked)
        self.assertTrue(found[0].masked.startswith(token[:4]))

    def test_clean_text_has_no_findings(self):
        self.assertEqual(cs.scan_text("README.md", "# RealTennis\nno secrets here\n"), [])


if __name__ == "__main__":
    unittest.main()
