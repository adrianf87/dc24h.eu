from pathlib import Path
from tempfile import TemporaryDirectory
from subprocess import run

with TemporaryDirectory(prefix='dc24h-stage1-private-tmp-', dir='/tmp') as stage:
    folder = Path(stage)
    folder.chmod(0o755)
    sentinel = folder / 'sentinel'
    sentinel.touch(mode=0o644)
    assert sentinel.is_file()
    run(['systemd-run', '--quiet', '--pipe', '--wait', '--collect',
         '-pUser=nobody', '-pGroup=nogroup', '-pNoNewPrivileges=yes', '-pPrivateTmp=yes',
         '-pPrivateNetwork=yes', '-pProtectSystem=strict', '-pProtectHome=yes',
         '-pCapabilityBoundingSet=', '-pRuntimeMaxSec=10s', '/usr/bin/python3', '-c', '''
from pathlib import Path
from sys import argv
from tempfile import TemporaryFile
mounts = {parts[4]: parts for parts in (line.split() for line in Path('/proc/self/mountinfo').read_text().splitlines())}
assert not Path(argv[1]).exists(), 'Host sentinel visible in private /tmp'
with TemporaryFile(dir='/tmp'):
    pass
print('PASS PrivateTmp: host sentinel invisible and private /tmp writable')
assert '/root' in mounts, 'ProtectHome mount missing'
assert 'inaccessible' in mounts['/root'][3] or not list(Path('/root').iterdir()), 'Home contents visible'
print('PASS ProtectHome: /root covered by inaccessible or empty mount')
for path in ('/etc', '/usr'):
    entry = mounts.get(path, mounts['/'])
    assert 'ro' in entry[5].split(','), f'{path} is not mounted read-only'
print('PASS ProtectSystem: /etc and /usr mounted read-only')
''', str(sentinel)], check=True, timeout=15)
