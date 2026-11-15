#!/usr/bin/env python3
"""Provision once as an administrator; CI only checks packages as its own user."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

COMMON = ['build-essential', 'cmake', 'python3', 'libutf8proc-dev',
          'libgcrypt20-dev', 'libssl-dev', 'libmariadb-dev']
SERVER = ['openssl', 'ncdc', 'mariadb-server', 'apache2']
# Chromium dependencies from the locked Playwright Ubuntu 24.04 x64 catalog.
BROWSER = ['xvfb', 'fonts-noto-color-emoji', 'fonts-unifont', 'libfontconfig1',
           'libfreetype6', 'xfonts-cyrillic', 'xfonts-scalable', 'fonts-liberation',
           'fonts-ipafont-gothic', 'fonts-wqy-zenhei', 'fonts-tlwg-loma-otf',
           'fonts-freefont-ttf', 'libasound2t64', 'libatk-bridge2.0-0t64',
           'libatk1.0-0t64', 'libatspi2.0-0t64', 'libcairo2', 'libcups2t64',
           'libdbus-1-3', 'libdrm2', 'libgbm1', 'libglib2.0-0t64', 'libnspr4',
           'libnss3', 'libpango-1.0-0', 'libx11-6', 'libxcb1', 'libxcomposite1',
           'libxdamage1', 'libxext6', 'libxfixes3', 'libxkbcommon0', 'libxrandr2']


def packages(scope):
    return COMMON + (SERVER if scope in ('server', 'all') else []) + (
        BROWSER if scope in ('webadmin', 'all') else [])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['check', 'list', 'install'])
    parser.add_argument('scope', choices=['server', 'webadmin', 'all'])
    args = parser.parse_args()
    required = packages(args.scope)
    if args.action == 'list':
        print('\n'.join(required))
        return 0
    if args.action == 'install':
        # Explicit administrator action only; never called by a workflow.
        release = dict(line.split('=', 1) for line in Path('/etc/os-release').read_text().splitlines()
                       if '=' in line)
        if release.get('ID', '').strip('"') != 'ubuntu' or release.get('VERSION_ID', '').strip('"') != '24.04':
            parser.error('installation is supported only on Ubuntu 24.04')
        if os.geteuid() != 0:
            parser.error('installation requires an administrator; CI must use check')
        subprocess.run(['apt-get', 'update'], check=True)
        subprocess.run(['apt-get', 'install', '-y', *required], check=True)
        return 0
    missing = []
    for package in required:
        status = subprocess.run(['dpkg-query', '-W', '-f=${db:Status-Status}', package],
                                capture_output=True, text=True, check=False)
        if status.returncode or status.stdout != 'installed':
            missing.append(package)
    if missing:
        print('Runner dependencies are missing: ' + ', '.join(missing), file=sys.stderr)
        print('An administrator must run once on server-747585786, from a reviewed checkout:\n'
              '  sudo python3 scripts/ci/runner_dependencies.py install all\n'
              'See doc/us/CI_DEBIAN13.md. CI does not request sudo or a password.', file=sys.stderr)
        return 1
    print('Runner dependencies are installed (' + args.scope + ').')
    return 0


if __name__ == '__main__':
    sys.exit(main())
