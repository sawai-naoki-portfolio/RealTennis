"""check_automation_report.py の単体テスト。"""
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_automation_report as report  # noqa: E402


def make_test(path, state, errors=()):
    return {"fullTestPath": path, "state": state,
            "entries": [{"event": {"type": "Error", "message": m}} for m in errors]}


class SummarizeTest(unittest.TestCase):
    def test_counts_only_tests_with_the_prefix(self):
        data = {"tests": [make_test("Tennis.Sim.A", "Success"), make_test("Engine.B", "Fail")]}
        self.assertEqual(report.summarize(data, "Tennis."), (1, []))

    def test_reports_failures_with_their_errors(self):
        data = {"tests": [make_test("Tennis.Sim.A", "Fail", ["Expected condition to be true."]),
                          make_test("Tennis.Sim.B", "Success")]}
        total, failures = report.summarize(data, "Tennis.")
        self.assertEqual(total, 2)
        self.assertEqual(failures, ["Tennis.Sim.A: Fail Expected condition to be true."])

    def test_not_run_counts_as_failure(self):
        _, failures = report.summarize({"tests": [make_test("Tennis.Sim.A", "NotRun")]}, "Tennis.")
        self.assertEqual(len(failures), 1)


class MainTest(unittest.TestCase):
    def run_main(self, data):
        with tempfile.TemporaryDirectory() as tmp:
            # UE は BOM 付きの UTF-8 で書く
            (Path(tmp) / "index.json").write_text(json.dumps(data), encoding="utf-8-sig")
            return report.main([tmp])

    def test_all_passing_is_success(self):
        self.assertEqual(self.run_main({"tests": [make_test("Tennis.Sim.A", "Success")]}), 0)

    def test_a_failure_stops_ci(self):
        self.assertEqual(self.run_main({"tests": [make_test("Tennis.Sim.A", "Fail")]}), 1)

    def test_no_matching_tests_stops_ci(self):
        self.assertEqual(self.run_main({"tests": []}), 1)

    def test_missing_report_stops_ci(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(report.main([tmp]), 2)


if __name__ == "__main__":
    unittest.main()
