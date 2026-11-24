"""Exercise real ADC/ADCS on a disposable loopback-only hub with bounded probes.

This is an Ubuntu CI smoke measurement, not a production load or security audit.
"""

import json
import os
from pathlib import Path
import platform
import re
import selectors
import signal
import subprocess
import sys
import tempfile
import time
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import stage4_endurance_gate


ROOT = Path(__file__).resolve().parents[1]
ADC_READY = re.compile(rb"Network listening on 127\.0\.0\.1:([0-9]+)")
ADCS_READY = re.compile(rb"ADCS listening on 127\.0\.0\.1:([0-9]+)")


class UbuntuVmHubProbe(unittest.TestCase):
    def test_real_adc_and_verified_adcs(self):
        binary = Path(sys.argv[1]).resolve()
        self.assertTrue(binary.is_file(), "Built hub executable is required")
        with tempfile.TemporaryDirectory(prefix="dc24h-ubuntu-vm-") as directory:
            root = Path(directory)
            certificate = root / "certificate.pem"
            key = root / "private.pem"
            subprocess.run([
                "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                "-days", "1", "-subj", "/CN=localhost",
                "-addext", "basicConstraints=critical,CA:FALSE",
                "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1",
                "-addext", "keyUsage=digitalSignature,keyEncipherment",
                "-addext", "extendedKeyUsage=serverAuth",
                "-keyout", str(key), "-out", str(certificate),
            ], capture_output=True, check=True, timeout=20)
            key.chmod(0o600)
            configuration = root / "hub.ini"
            configuration.write_text(
                "[network]\nlisten=127.0.0.1:0\nadcs_listen=127.0.0.1:0\n"
                "[tls]\ncertificate=certificate.pem\nprivate_key=private.pem\n",
                encoding="utf-8",
            )
            hub = subprocess.Popen(
                [str(binary), "--config", str(configuration)],
                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, start_new_session=True,
            )
            try:
                with selectors.DefaultSelector() as selector:
                    selector.register(hub.stderr, selectors.EVENT_READ)
                    startup_log = bytearray()
                    deadline = time.monotonic() + 15
                    while not (ADC_READY.search(startup_log) and ADCS_READY.search(startup_log)):
                        remaining = deadline - time.monotonic()
                        self.assertGreater(remaining, 0, "Timed out waiting for both loopback listeners")
                        self.assertIsNone(hub.poll(), "Disposable hub exited during startup")
                        self.assertTrue(selector.select(remaining), "No startup readiness from hub")
                        data = os.read(hub.stderr.fileno(), 8192)
                        self.assertTrue(data, "Disposable hub closed its startup log")
                        startup_log.extend(data)
                        self.assertLess(len(startup_log), 128 * 1024, "Unexpectedly large startup log")

                ports = {
                    "adc": int(ADC_READY.search(startup_log).group(1)),
                    "adcs": int(ADCS_READY.search(startup_log).group(1)),
                }
                samples = {}
                for profile, port in ports.items():
                    command = [
                        sys.executable, str(ROOT / "scripts/stage4_perf_probe.py"),
                        "--profile", profile, "--port", str(port),
                        "--requests", "12", "--rate", "4", "--concurrency", "2",
                        "--timeout", "3", "--target-pid", str(hub.pid), "--ack-isolated",
                    ]
                    if profile == "adcs":
                        command.extend(["--ca-file", str(certificate)])
                    result = subprocess.run(command, capture_output=True, text=True, timeout=35)
                    self.assertEqual(result.returncode, 0, f"{profile}: {result.stderr.strip()}")
                    report = json.loads(result.stdout)
                    self.assertEqual(report["profile"], profile)
                    self.assertEqual(report["requests"], 12)
                    self.assertEqual(report["success"], 12)
                    self.assertEqual(report["failure"], 0)
                    self.assertFalse(report["capacity_claim"])
                    self.assertIsNotNone(report["p95_ms"])
                    samples[profile] = report

                # Stage IV endurance is mandatory on hosted Ubuntu CI only.
                # Do not run this sustained test on an operator's local hub.
                endurance = None
                if os.environ.get("GITHUB_ACTIONS") == "true":
                    self.assertEqual(os.environ.get("RUNNER_ENVIRONMENT"), "github-hosted")
                    self.assertEqual(os.environ.get("RUNNER_OS"), "Linux")
                    os_image = "ubuntu-" + platform.freedesktop_os_release()["VERSION_ID"]
                    windows = {}
                    for profile, port in ports.items():
                        windows[profile] = []
                        for _ in range(stage4_endurance_gate.WINDOWS):
                            command = [
                                sys.executable, str(ROOT / "scripts/stage4_perf_probe.py"),
                                "--profile", profile, "--port", str(port),
                                "--requests", str(stage4_endurance_gate.REQUESTS_PER_WINDOW),
                                "--rate", "8", "--concurrency", "4", "--timeout", "3",
                                "--target-pid", str(hub.pid), "--ack-isolated",
                            ]
                            if profile == "adcs":
                                command.extend(["--ca-file", str(certificate)])
                            result = subprocess.run(command, capture_output=True, text=True, timeout=35)
                            self.assertEqual(result.returncode, 0,
                                             f"{profile} endurance failed (stderr suppressed)")
                            windows[profile].append(json.loads(result.stdout))
                    endurance = stage4_endurance_gate.compare(os_image, windows)
                    self.assertTrue(endurance["ci_regression_gate_passed"],
                                    "Hosted Ubuntu Stage IV endurance exceeded bounded regression limits")
                    endurance["source_sha"] = stage4_endurance_gate.required_source_sha(
                        os.environ.get("SOURCE_SHA"))

                destination = os.environ.get("DC24H_VM_REPORT_DIR")
                if destination:
                    report_dir = Path(destination)
                    self.assertTrue(report_dir.is_dir(), "Report directory must exist")
                    report_path = report_dir / "loopback-probes.json"
                    with report_path.open("x", encoding="utf-8") as report_file:
                        json.dump({"schema": 1, "evidence": "isolated-ubuntu-vm-only", "profiles": samples},
                                  report_file, indent=2, sort_keys=True)
                        report_file.write("\n")
                    if endurance is not None:
                        with (report_dir / "stage4-endurance.json").open("x", encoding="utf-8") as file:
                            json.dump(endurance, file, indent=2, sort_keys=True)
                            file.write("\n")
                        # Preserve only the numeric, synthetic three-window
                        # ADC/ADCS reports; no client transcript or secret.
                        with (report_dir / "beta07-adc-adcs-windows.json").open("x", encoding="utf-8") as file:
                            json.dump({"schema": 1, "source_sha": endurance["source_sha"],
                                       "os_image": os_image, "windows": windows}, file, sort_keys=True)
                            file.write("\n")
                for profile, report in samples.items():
                    print(f"{profile}: {report['success']}/{report['requests']} successful, "
                          f"p95={report['p95_ms']} ms (isolated VM; no capacity claim)")
            finally:
                if hub.poll() is None:
                    os.killpg(hub.pid, signal.SIGTERM)
                try:
                    hub.communicate(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(hub.pid, signal.SIGKILL)
                    hub.communicate(timeout=5)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit("Usage: stage4_ubuntu_vm_probe_tests.py PATH_TO_BUILT_HUB")
    unittest.main(argv=[sys.argv[0]])
