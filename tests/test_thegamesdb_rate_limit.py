"""TheGamesDB limiter regression test; uses only C++11 standard headers."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
INCLUDE = ROOT / "es-app" / "src"
HEADER = INCLUDE / "scrapers" / "TheGamesDBRateLimit.h"
SOURCE_MAIN = r"""
#include "scrapers/TheGamesDBRateLimit.h"
#include <cassert>
void signal429FromAnotherTranslationUnit();
int main() {
    assert(TheGamesDBRateLimit::isApiUrl("https://api.thegamesdb.net/v1/Games/ByGameName"));
    assert(!TheGamesDBRateLimit::isApiUrl("https://api.thegamesdb.net.evil.example/"));
    assert(!TheGamesDBRateLimit::isApiUrl("https://www.screenscraper.fr/"));
    assert(TheGamesDBRateLimit::remainingSeconds() == 0);
    signal429FromAnotherTranslationUnit();
    const long remaining = TheGamesDBRateLimit::remainingSeconds();
    assert(remaining >= 119 && remaining <= 121);
    TheGamesDBRateLimit::noteHttp429();
    assert(TheGamesDBRateLimit::remainingSeconds() >= 119);
}
"""
SOURCE_OTHER = r"""
#include "scrapers/TheGamesDBRateLimit.h"
void signal429FromAnotherTranslationUnit() {
    TheGamesDBRateLimit::noteHttp429();
}
"""

class TestTheGamesDBRateLimit(unittest.TestCase):
    def test_session_scope_shared_across_translation_units(self):
        self.assertTrue(HEADER.is_file())
        with tempfile.TemporaryDirectory() as tmp:
            p = pathlib.Path(tmp)
            main = p / "main.cpp"
            other = p / "other.cpp"
            binary = p / "test_rate_limit"
            main.write_text(SOURCE_MAIN)
            other.write_text(SOURCE_OTHER)
            subprocess.run([
                "g++", "-std=c++11", "-pthread", "-I", str(INCLUDE),
                str(main), str(other), "-o", str(binary)
            ], check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)

if __name__ == "__main__":
    unittest.main()
