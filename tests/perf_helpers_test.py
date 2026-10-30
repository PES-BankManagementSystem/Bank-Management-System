import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts" / "perf"))

from perf_common import generated_history, safe_database_path, summarize  # noqa: E402


class PerformanceHelpersTest(unittest.TestCase):
    def test_percentiles_and_summary_use_nearest_rank(self):
        result = summarize([1.0, 2.0, 3.0, 4.0], 5.0)
        self.assertEqual(result["p95_ms"], 4.0)
        self.assertEqual(result["p99_ms"], 4.0)
        self.assertEqual(result["median_ms"], 2.5)
        self.assertEqual(result["verdict"], "Pass")

    def test_generator_is_repeatable_and_has_long_histories(self):
        self.assertEqual(generated_history(0), generated_history(0))
        self.assertEqual(len(generated_history(0)), 300)
        self.assertEqual(len(generated_history(5)), 12)

    def test_database_guard_requires_temp_directory_and_perf_filename(self):
        with self.assertRaises(ValueError):
            safe_database_path("/not-a-temp-dir/bms-perf-test.sqlite")
        with self.assertRaises(ValueError):
            safe_database_path(Path(__import__("tempfile").gettempdir()) / "production.sqlite")


if __name__ == "__main__":
    unittest.main()
