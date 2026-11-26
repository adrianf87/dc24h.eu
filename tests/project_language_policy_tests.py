#!/usr/bin/env python3
"""Regression tests for the explicit Stage II US-only documentation policy."""

from pathlib import Path
import unittest

from project_checks import US_ONLY_DOCUMENTS, documentation_language_mismatches


class ProjectLanguagePolicyTests(unittest.TestCase):
    def test_stage_ii_runbook_may_be_us_only(self):
        us = {Path("README.md"), Path("PUBLIC_STAGE_II_DEBIAN13.md")}
        pl = {Path("README.md")}
        self.assertEqual(documentation_language_mismatches(us, pl), ([], []))

    def test_unrelated_us_only_document_remains_rejected(self):
        us = {Path("README.md"), Path("UNREVIEWED_NEW_GUIDE.md")}
        pl = {Path("README.md")}
        self.assertEqual(documentation_language_mismatches(us, pl),
                         ([Path("UNREVIEWED_NEW_GUIDE.md")], []))

    def test_pl_only_document_remains_rejected(self):
        us = {Path("README.md")}
        pl = {Path("README.md"), Path("UNREVIEWED_PL_GUIDE.md")}
        self.assertEqual(documentation_language_mismatches(us, pl),
                         ([], [Path("UNREVIEWED_PL_GUIDE.md")]))

    def test_nested_name_does_not_inherit_exception(self):
        us = {Path("nested/PUBLIC_STAGE_II_DEBIAN13.md")}
        self.assertEqual(documentation_language_mismatches(us, set()), (sorted(us), []))

    def test_exception_is_exact_and_single_file(self):
        self.assertEqual(US_ONLY_DOCUMENTS, frozenset({Path("PUBLIC_STAGE_II_DEBIAN13.md")}))

    def test_matching_localized_pairs_still_pass(self):
        common = {Path("README.md"), Path("PUBLIC_STAGE_II_DEBIAN13.md")}
        self.assertEqual(documentation_language_mismatches(common, common), ([], []))


if __name__ == "__main__":
    unittest.main()
