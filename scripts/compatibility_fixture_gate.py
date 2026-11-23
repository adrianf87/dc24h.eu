#!/usr/bin/env python3
"""Fail Debian smoke CI if integration fixtures silently disappear.

This is not a systemd host check. Docker without a running systemd manager
cannot supply acceptance evidence for sandboxed service lifecycle behavior.
"""

import argparse
import json
import subprocess


REQUIRED = {
    "database.process", "database.resilience.process", "database.migrations.process",
    "apache2.https", "apache2.remote.gateway", "api.process", "adc.process",
    "adcs.process", "beta.04.stage4.vm.probe", "project.metadata",
}


def check_fixture_names(names):
    names = set(names)
    missing = REQUIRED - names
    if missing:
        raise ValueError("Required Debian test fixtures missing: " + ", ".join(sorted(missing)))
    return len(names)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    args = parser.parse_args()
    result = subprocess.run(["ctest", "--test-dir", args.build_dir, "--show-only=json-v1"],
                            capture_output=True, text=True, check=True, timeout=30)
    tests = json.loads(result.stdout)["tests"]
    count = check_fixture_names(item["name"] for item in tests)
    print(f"Required Debian integration fixtures registered ({count} total tests); "
          "Docker is not a systemd-host acceptance result")


if __name__ == "__main__":
    main()
