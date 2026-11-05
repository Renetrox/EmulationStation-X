#!/usr/bin/env python3
"""Exercise the embedded query with isolated RetroPie-Setup fixtures."""
import pathlib
import subprocess
import tempfile
import unittest

SOURCE = pathlib.Path(__file__).resolve().parents[1] / 'es-app/src/guis/GuiPortsBrowser.cpp'
QUERY = SOURCE.read_text().split('R"ESX(', 1)[1].split(')ESX"', 1)[0]


class CompatibilityQuery(unittest.TestCase):
    def run_query(self, failure=False):
        with tempfile.TemporaryDirectory() as tmp:
            modules = pathlib.Path(tmp) / 'scriptmodules'
            modules.mkdir()
            (modules / 'system.sh').write_text('''
test_chroot() { :; }
get_platform() { :; }
get_os_version() { getDepends lsb-release; rm forbidden-cleanup; }
conf_memory_vars() { :; }
conf_build_vars() { :; }
setup_env() { echo MUST_NOT_RUN; exit 99; }
''')
            (modules / 'helpers.sh').write_text('getDepends() { echo MUST_NOT_INSTALL; exit 99; }\n')
            (modules / 'inifuncs.sh').write_text('')
            (modules / 'packages.sh').write_text('''
declare -A __mod_info
rp_registerAllModules() {
    __mod_id=(supported blocked notaport)
    __mod_info[supported/type]=ports
    __mod_info[supported/enabled]=1
    __mod_info[blocked/type]=ports
    __mod_info[blocked/enabled]=0
    __mod_info[notaport/type]=emulators
}
''' if not failure else 'rp_registerAllModules() { exit 2; }\n')
            return subprocess.run(['bash', '-c', QUERY, 'esx', tmp], capture_output=True, text=True)

    def test_query_uses_registered_states_and_skips_setup(self):
        result = self.run_query()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, 'ESX_PORT\tsupported\t1\nESX_PORT\tblocked\t0\nESX_DONE\n')

    def test_failure_has_no_completion_marker(self):
        result = self.run_query(True)
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('ESX_DONE', result.stdout)


if __name__ == '__main__':
    unittest.main()
