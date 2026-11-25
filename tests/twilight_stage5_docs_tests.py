"""Bounded Stage V current-doc/provenance review; NOT a release or OWNER GO.

Source histories intentionally retain old repositories and executable names;
the check is restricted to present-day operator entry points.
"""

from pathlib import Path
import re
import shutil
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
CURRENT_GUIDES = (
    "README.md", "config/README.md", "doc/pl/INSTALLATION.md",
    "doc/us/INSTALLATION.md", "doc/pl/CONFIGURATION.md", "doc/us/CONFIGURATION.md",
    "doc/pl/API.md", "doc/us/API.md", "doc/pl/DATABASE_MIGRATIONS.md",
    "doc/us/DATABASE_MIGRATIONS.md", "doc/pl/ACCOUNT_REPOSITORY.md",
    "doc/us/ACCOUNT_REPOSITORY.md", "doc/pl/ADCS_TLS.md", "doc/us/ADCS_TLS.md",
    "doc/pl/LOGGING.md", "doc/us/LOGGING.md", "doc/pl/NETWORK.md",
    "doc/pl/WEBADMIN.md", "doc/us/WEBADMIN.md",
)
CURRENT_HEADINGS = (
    "doc/README.md", "doc/pl/README.md", "doc/us/README.md", "webadmin/README.md",
)
STAGE_V_DOCS = ("doc/pl/TWILIGHT_STAGE_V.md", "doc/us/TWILIGHT_STAGE_V.md")
LEGACY_COMMAND = re.compile(r"(?:\./|\.\./)build/dc24h(?!-twilight)\b")


def read(root, relative):
    return (Path(root) / relative).read_text(encoding="utf-8")


def check_current_docs(root):
    for path in CURRENT_GUIDES:
        body = read(root, path)
        if LEGACY_COMMAND.search(body):
            raise ValueError("Old build artifact in current documentation: " + path)
    for path in CURRENT_HEADINGS:
        if "dc24h.eu-twilight" not in read(root, path)[:1400]:
            raise ValueError("Twilight identity missing in current index: " + path)
    for path in ("doc/us/INSTALLATION.md", "doc/pl/INSTALLATION.md"):
        body = read(root, path)
        if ("git clone --branch main https://github.com/adrianf87/dc24h.eu-twilight.git"
                not in body or "cd dc24h.eu-twilight" not in body or
                "./build/dc24h-twilight --" not in body or
                "git clone --branch main https://github.com/adrianf87/dc24h.eu-high-starter.git"
                in body):
            raise ValueError("Installation guide would fetch/run the source repository")
    landing = read(root, "README.md")
    early = landing.split("<summary>Beta</summary>", 1)[0]
    if ("High Starter v0.9.20-rc.1 pre-release" not in early or
            "Stage V source candidate" in early or
            "github.com/adrianf87/dc24h.eu-twilight/actions/workflows/build.yml" not in early or
            "github.com/adrianf87/dc24h.eu-high-starter/releases/tag/v0.9.20-rc.1"
            not in early or
            "<summary>Alpha</summary>" not in landing):
        raise ValueError("Main README merges historic RC1 with active Twilight status")
    if ("v0.9.20-rc.1" not in read(root, "CHANGELOG.md")[:2600] or
            "No tag" not in read(root, "CHANGELOG.md")[:2600]):
        raise ValueError("New changelog lacks safe source provenance and release limit")
    for path in STAGE_V_DOCS:
        content = read(root, path)
        if (not all(key in content for key in (
                "dc24h.eu-twilight", "dc24h.eu-high-starter",
                "v0.9.20-rc.1", "Draft PR #9",
                "rpID", "Origin", "LICENSE", "main")) or
                len(content) > 20000):
            raise ValueError("Stage V doc lacks a bounded, explicit provenance exception")
        directory = (Path(root) / path).parent
        for link in re.findall(r"\]\(([^)]+)\)", content):
            if link.startswith(("https://", "http://", "#")):
                continue
            target = (directory / link.split("#", 1)[0]).resolve()
            if not target.is_file():
                raise ValueError("Broken relative Stage V link: " + path + " " + link)
    return True


class StageFiveDocsTests(unittest.TestCase):
    def test_current_documentation_and_provenance(self):
        self.assertTrue(check_current_docs(ROOT))

    def test_active_documentation_local_links_resolve(self):
        # Historical URLs remain untouched; these are local file references in
        # current operator/README and Stage V docs, without network access.
        checked = 0
        for name in dict.fromkeys(CURRENT_GUIDES + CURRENT_HEADINGS + STAGE_V_DOCS):
            document = ROOT / name
            for raw in re.findall(r"\]\(([^)]+)\)", document.read_text(encoding="utf-8")):
                target = raw.strip().split(" ", 1)[0].split("#", 1)[0]
                if not target or re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target):
                    continue
                if target.startswith("//"):
                    continue
                with self.subTest(document=name, link=raw):
                    self.assertTrue((document.parent / target).is_file())
                checked += 1
        self.assertGreaterEqual(checked, 100)

    def test_incorrect_clone_or_legacy_binary_refused(self):
        for path, old, new in (
                ("doc/pl/INSTALLATION.md", "dc24h.eu-twilight.git", "dc24h.eu-high-starter.git"),
                ("doc/us/INSTALLATION.md", "./build/dc24h-twilight --help", "./build/dc24h --help"),
                ("README.md", "./build/dc24h-twilight --version", "./build/dc24h --version"),
                ("README.md", "High Starter v0.9.20-rc.1 pre-release",
                 "Stage V source candidate"),
        ):
            with self.subTest(path=path), tempfile.TemporaryDirectory() as tmp:
                temp = Path(tmp)
                for name in set(CURRENT_GUIDES + CURRENT_HEADINGS + STAGE_V_DOCS +
                                ("CHANGELOG.md",)):
                    dest = temp / name
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(ROOT / name, dest)
                altered = read(temp, path)
                self.assertIn(old, altered)
                (temp / path).write_text(altered.replace(old, new, 1), encoding="utf-8")
                with self.assertRaises(ValueError):
                    check_current_docs(temp)

    def test_source_history_licensing_and_hostnames_are_not_rebranded(self):
        historical = read(ROOT, "README.md")
        self.assertIn("github.com/adrianf87/dc24h.eu-high-starter/issues/242",
                      historical)
        self.assertIn("github.com/adrianf87/dc24h.eu-high-starter/pull/243",
                      historical)
        self.assertIn("GNU GENERAL PUBLIC LICENSE", read(ROOT, "LICENSE")[:150])
        for path in STAGE_V_DOCS:
            content = read(ROOT, path)
            self.assertIn("/etc/dc24h", content)
            self.assertIn("/var/lib/dc24h", content)


if __name__ == "__main__":
    unittest.main()
