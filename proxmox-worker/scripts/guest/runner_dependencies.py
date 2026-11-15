#!/usr/bin/python3 -I
"""Install test dependencies only inside marked disposable Ubuntu CI clones."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

COMMON = ['build-essential', 'cmake', 'python3', 'libutf8proc-dev',
          'libgcrypt20-dev', 'libssl-dev', 'libmariadb-dev']
SERVER = ['openssl', 'ncdc', 'mariadb-server', 'apache2']
BROWSER = ['xvfb', 'fonts-noto-color-emoji', 'fonts-unifont', 'libfontconfig1',
           'libfreetype6', 'xfonts-cyrillic', 'xfonts-scalable', 'fonts-liberation',
           'fonts-ipafont-gothic', 'fonts-wqy-zenhei', 'fonts-tlwg-loma-otf',
           'fonts-freefont-ttf', 'libasound2t64', 'libatk-bridge2.0-0t64',
           'libatk1.0-0t64', 'libatspi2.0-0t64', 'libcairo2', 'libcups2t64',
           'libdbus-1-3', 'libdrm2', 'libgbm1', 'libglib2.0-0t64', 'libnspr4',
           'libnss3', 'libpango-1.0-0', 'libx11-6', 'libxcb1', 'libxcomposite1',
           'libxdamage1', 'libxext6', 'libxfixes3', 'libxkbcommon0', 'libxrandr2']
OS_RELEASE = Path('/etc/os-release')
PRODUCT_SERIAL = Path('/sys/class/dmi/id/product_serial')


def ubuntu_version():
    release = dict(line.split('=', 1) for line in OS_RELEASE.read_text().splitlines()
                   if '=' in line)
    version = release.get('VERSION_ID', '').strip('"')
    if release.get('ID', '').strip('"') != 'ubuntu' or version not in ('22.04', '24.04'):
        raise ValueError('only Ubuntu 22.04 / 24.04 are supported')
    return version


def packages(scope, version):
    browser = BROWSER if version == '24.04' else [item.replace('t64', '') for item in BROWSER]
    compiler = ['g++-12'] if version == '22.04' else []
    return COMMON + compiler + (SERVER if scope in ('server', 'all') else []) + (
        browser if scope in ('webadmin', 'all') else [])


def assert_clone(version):
    expected = {'24.04': 'dc24h-ci-clone-200', '22.04': 'dc24h-ci-clone-201'}
    if PRODUCT_SERIAL.read_text().strip() != expected[version]:
        raise ValueError('refusing package installation outside the matching CI clone; source images stay clean')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['check', 'list', 'install'])
    parser.add_argument('scope', choices=['server', 'webadmin', 'all'])
    args = parser.parse_args()
    try:
        version = ubuntu_version()
    except (ValueError, OSError) as error:
        parser.error(str(error))
    required = packages(args.scope, version)
    if args.action == 'list':
        print('\n'.join(required))
        return 0
    if args.action == 'install':
        if os.geteuid() != 0:
            parser.error('use the installed root-owned helper through its limited sudo rule')
        try:
            assert_clone(version)
        except (ValueError, OSError) as error:
            parser.error(str(error))
        env = {'PATH': '/usr/sbin:/usr/bin:/sbin:/bin', 'LC_ALL': 'C',
               'DEBIAN_FRONTEND': 'noninteractive', 'NEEDRESTART_MODE': 'a'}
        subprocess.run(['/usr/bin/apt-get', '-o', 'DPkg::Lock::Timeout=300', 'update'], check=True, env=env)
        subprocess.run(['/usr/bin/apt-get', '-o', 'DPkg::Lock::Timeout=300',
                        'install', '-y', *required], check=True, env=env)
        return 0
    missing = []
    for package in required:
        status = subprocess.run(['dpkg-query', '-W', '-f=${db:Status-Status}', package],
                                capture_output=True, text=True, check=False)
        if status.returncode or status.stdout != 'installed':
            missing.append(package)
    if missing:
        print('Runner dependencies are missing: ' + ', '.join(missing), file=sys.stderr)
        print('Run the provisioning job inside the disposable clone first:\n'
              '  sudo -n /usr/local/sbin/dc24h-ci-dependencies install all\n'
              'Do not install test packages in source VM 100 or 101.', file=sys.stderr)
        return 1
    print('Runner dependencies are installed (' + args.scope + ', Ubuntu ' + version + ').')
    return 0


if __name__ == '__main__':
    sys.exit(main())
