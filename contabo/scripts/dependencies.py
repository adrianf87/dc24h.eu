#!/usr/bin/env python3
"""Provision the single persistent Contabo CI host, or check its packages."""
import argparse
import os
from pathlib import Path
import socket
import subprocess
import sys

PACKAGES = [
    'git', 'ca-certificates', 'build-essential', 'cmake', 'python3',
    'libutf8proc-dev', 'libgcrypt20-dev', 'libssl-dev', 'libmariadb-dev',
    'openssl', 'ncdc', 'mariadb-server', 'apache2',
    'xvfb', 'fonts-noto-color-emoji', 'fonts-unifont', 'libfontconfig1',
    'libfreetype6', 'xfonts-cyrillic', 'xfonts-scalable', 'fonts-liberation',
    'fonts-ipafont-gothic', 'fonts-wqy-zenhei', 'fonts-tlwg-loma-otf',
    'fonts-freefont-ttf', 'libasound2t64', 'libatk-bridge2.0-0t64',
    'libatk1.0-0t64', 'libatspi2.0-0t64', 'libcairo2', 'libcups2t64',
    'libdbus-1-3', 'libdrm2', 'libgbm1', 'libglib2.0-0t64', 'libnspr4',
    'libnss3', 'libpango-1.0-0', 'libx11-6', 'libxcb1', 'libxcomposite1',
    'libxdamage1', 'libxext6', 'libxfixes3', 'libxkbcommon0', 'libxrandr2',
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('check', 'install'))
    args = parser.parse_args()
    release = dict(line.split('=', 1) for line in Path('/etc/os-release').read_text().splitlines()
                   if '=' in line)
    if (release.get('ID', '').strip('"'), release.get('VERSION_ID', '').strip('"')) != ('ubuntu', '24.04'):
        parser.error('this worker requires Ubuntu 24.04')
    if socket.gethostname() != 'server-747585786':
        parser.error('refusing to provision/check another host')
    if args.action == 'install':
        if os.geteuid() != 0:
            parser.error('run install once as administrator using sudo')
        env = {'PATH': '/usr/sbin:/usr/bin:/sbin:/bin', 'LC_ALL': 'C',
               'DEBIAN_FRONTEND': 'noninteractive', 'NEEDRESTART_MODE': 'l'}
        # Tests launch their own isolated MariaDB/Apache instances. Prevent newly
        # installed packages from starting system daemons; retain an existing policy.
        policy = Path('/usr/sbin/policy-rc.d')
        created_policy = False
        try:
            try:
                with policy.open('x') as handle:
                    created_policy = True
                    handle.write('#!/bin/sh\nexit 101\n')
                policy.chmod(0o755)
            except FileExistsError:
                pass
            subprocess.run(['apt-get', '-o', 'DPkg::Lock::Timeout=300', 'update'], check=True, env=env)
            subprocess.run(['apt-get', '-o', 'DPkg::Lock::Timeout=300', 'install',
                            '-y', '--no-install-recommends', *PACKAGES], check=True, env=env)
        finally:
            if created_policy:
                policy.unlink()
    missing = []
    for package in PACKAGES:
        result = subprocess.run(['dpkg-query', '-W', '-f=${db:Status-Status}', package],
                                capture_output=True, text=True, check=False)
        if result.returncode or result.stdout != 'installed':
            missing.append(package)
    if missing:
        print('Missing worker packages: ' + ', '.join(missing), file=sys.stderr)
        print('Administrator: sudo python3 contabo/scripts/dependencies.py install', file=sys.stderr)
        return 1
    print('Contabo worker packages are installed (Ubuntu 24.04).')
    return 0


if __name__ == '__main__':
    sys.exit(main())
