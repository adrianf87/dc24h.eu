"""Compile real SQL catalogs in private fixtures; no compiler/server required."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CatalogTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='dc24h-catalog-')
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.source = self.folder / 'migrations'
        shutil.copytree(ROOT / 'database/migrations', self.source)
        self.manifest = json.loads((self.source / 'manifest.json').read_text())
        self.script = self.folder / 'catalog.cmake'
        self.script.write_text('cmake_minimum_required(VERSION 3.22)\n'
                               f'include("{ROOT / "cmake/Migrations.cmake"}")\n'
                               f'dc24h_generate_migrations("{self.source}" "{self.folder / "generated.hpp"}")\n')

    def generate(self, success=True):
        (self.source / 'manifest.json').write_text(json.dumps(self.manifest))
        result = subprocess.run(['cmake', '-P', str(self.script)], capture_output=True, timeout=10)
        self.assertEqual(result.returncode == 0, success, result.stderr.decode())
        return (self.folder / 'generated.hpp').read_text() if success else ''

    def test_exact_bytes_and_literal_cpp_template_characters(self):
        a = "INSERT INTO fixture VALUES ('ż; @name@ $<CONFIG> ${unchanged}')\n"
        b = "UPDATE fixture SET value='second;step'\n"
        self.manifest['migrations'].append({'version': 4, 'name': 'fixture',
                                           'files': ['0004_fixture_01.sql', '0004_fixture_02.sql']})
        for suffix, text in [('01', a), ('02', b)]:
            (self.source / f'0004_fixture_{suffix}.sql').write_text(text)
        generated = self.generate()
        baseline = (self.source / '0001_schema_migrations.sql').read_bytes()
        self.assertIn(hashlib.sha256(baseline).hexdigest(), generated)
        joined = b''.join(str(len(s.encode())).encode() + b':' + s.encode() for s in [a, b])
        self.assertIn(hashlib.sha256(joined).hexdigest(), generated)
        self.assertIn(a, generated)
        self.assertIn(b, generated)

    def test_rejects_gaps_and_traversal(self):
        self.manifest['migrations'][1]['version'] = 3
        self.generate(False)
        self.manifest['migrations'][1]['version'] = 2
        self.manifest['migrations'][1]['version'] = '2'
        self.generate(False)
        self.manifest['migrations'][1]['version'] = 2
        self.manifest['migrations'][1]['files'] = ['../0002_migration_state.sql']
        self.generate(False)

    def test_rejects_empty_oversized_delimiter_and_unlisted_sql(self):
        path = self.source / '0002_migration_state.sql'
        original = path.read_text()
        for text in ['', 'x' * 65537, ')DC24H_MIG"']:
            with self.subTest(text_length=len(text)):
                path.write_text(text)
                self.generate(False)
        path.write_text(original)
        (self.source / '0003_forgotten.sql').write_text('SELECT 1')
        self.generate(False)

    def test_rejects_missing_duplicate_and_symlink_files(self):
        path = self.source / '0002_migration_state.sql'
        text = path.read_text()
        path.unlink()
        self.generate(False)

        target = self.folder / 'external.sql'
        target.write_text(text)
        path.symlink_to(target)
        self.generate(False)
        path.unlink()
        path.write_text(text)
        self.manifest['migrations'][1]['files'] *= 2
        self.generate(False)

    def test_rejects_nul_and_crlf_before_compiler_normalization(self):
        path = self.source / '0002_migration_state.sql'
        for text in [b'SELECT 1\r\n', b'SELECT 1\x00']:
            with self.subTest(source=text):
                path.write_bytes(text)
                self.generate(False)


if __name__ == '__main__':
    unittest.main()
