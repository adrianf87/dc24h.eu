"""Test persistent enable/disable and service activation on a disposable hosted VM."""

import os
from pathlib import Path
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from tls_renewal import Host, Renewal, TIMER, command


class TimerFixture(Host):
    def __init__(self, directory):
        self.state_dir = Path(directory)
        self.timer = self.state_dir.name + ".timer"

    def systemctl(self, *args, **kwargs):
        return super().systemctl(*(self.timer if arg == TIMER else arg for arg in args), **kwargs)


def main():
    if (os.geteuid() != 0 or os.getenv("GITHUB_ACTIONS") != "true" or
            os.getenv("RUNNER_ENVIRONMENT") != "github-hosted"):
        raise SystemExit("Run this persistent timer fixture only on a disposable GitHub-hosted VM")
    templates = Path(__file__).resolve().parents[1] / "config" / "tls-renewal"
    command(["/usr/bin/systemd-analyze", "verify", *(str(path) for path in sorted(templates.glob("dc24h-*")))],
            timeout=15)
    with tempfile.TemporaryDirectory(prefix="dc24h-ci-tls-", dir="/run") as directory:
        host = TimerFixture(directory)
        root = host.state_dir
        name = root.name
        timer = Path("/etc/systemd/system") / host.timer
        service = Path("/etc/systemd/system") / (name + ".service")
        fired = root / "fired"
        data = (templates / TIMER).read_text().replace("dc24h-tls-renewal.service", service.name)
        data = data.replace("OnCalendar=*-*-* 03,15:00:00 UTC", "OnCalendar=*-*-* *:*:00/3 UTC")
        data = data.replace("RandomizedDelaySec=30min", "RandomizedDelaySec=0\nAccuracySec=1s")
        assert "Persistent=true" in data
        try:
            with service.open("x") as output:
                output.write(f"[Service]\nType=oneshot\nExecStart=/usr/bin/touch {fired}\n"
                             f"PrivateNetwork=yes\nProtectSystem=strict\nReadWritePaths={root}\n")
            with timer.open("x") as output:
                output.write(data)
            host.systemctl("daemon-reload", timeout=10)
            renewal = Renewal(host)
            assert renewal.schedule() == ("disabled", None)
            renewal.set_schedule("enable")
            renewal.set_schedule("enable")
            schedule, due = renewal.schedule()
            assert schedule == "active" and isinstance(due, int)
            link = Path("/etc/systemd/system/timers.target.wants") / host.timer
            assert link.is_symlink() and link.resolve() == timer
            deadline = time.monotonic() + 15
            while not fired.exists() and time.monotonic() < deadline:
                time.sleep(0.25)
            assert fired.exists(), "the real systemd timer did not activate its isolated unit"
            renewal.set_schedule("disable")
            renewal.set_schedule("disable")
            assert renewal.schedule() == ("disabled", None) and not link.is_symlink()
            print("PASS persistent systemd timer enable, firing, and idempotent disable")
        finally:
            try:
                host.systemctl("disable", "--now", TIMER, timeout=10)
                host.systemctl("clean", "--what=state", TIMER, timeout=10)
            finally:
                timer.unlink(missing_ok=True)
                service.unlink(missing_ok=True)
                host.systemctl("daemon-reload", timeout=10)


if __name__ == "__main__":
    main()
