"""Real owner CLI against the parent's disposable MariaDB, never production."""
from pathlib import Path
import re
import subprocess


def exercise(binary, config, folder, sql):
    proof = folder / 'owner-account.password'
    proof.write_text('Owner CLI private password 42!\n')
    proof.chmod(0o600)
    wrong = folder / 'owner-wrong.password'
    wrong.write_text('Wrong CLI private password 42!\n')
    wrong.chmod(0o600)

    def call(*args, success=False):
        result = subprocess.run([binary, '--config', str(config), *map(str, args)],
                                text=True, capture_output=True, timeout=20)
        assert (result.returncode == 0) == success, result.stderr
        for secret in ('Owner CLI private password', 'Wrong CLI private password', '$dc24h-scrypt$', str(proof)):
            assert secret not in result.stdout + result.stderr
        return result

    flags = ['--bootstrap', 'FirstOwnerCLI', '--password-file', proof]
    call(*flags)  # Explicit stopped-hub acknowledgement is mandatory.
    proof.chmod(0o644)
    call(*flags, '--confirm-stopped')
    proof.chmod(0o600)
    link = folder / 'owner-linked.password'
    link.symlink_to(proof)
    call('--bootstrap', 'FirstOwnerCLI', '--password-file', link, '--confirm-stopped')
    call(*flags, '--confirm-stopped', '--revision', '1')  # Unknown combination.
    assert sql("SELECT COUNT(*) FROM dc24h.dc24h_accounts WHERE role='owner'") == '0'
    result = call(*flags, '--confirm-stopped', success=True)
    owner = re.search(r'owner_id=(\d+) owner_revision=1', result.stdout)[1]
    call(*flags, '--confirm-stopped')
    assert sql("SELECT COUNT(*) FROM dc24h.dc24h_accounts WHERE role='owner'") == '1'
    sql("INSERT INTO dc24h.dc24h_accounts(name,role,status,created_at,updated_at) "
        f"SELECT 'OwnerCLISuccessor','administrator','active',created_at,updated_at FROM dc24h.dc24h_accounts WHERE id={owner};")
    target = sql("SELECT id FROM dc24h.dc24h_accounts WHERE name='OwnerCLISuccessor'")
    sql("INSERT INTO dc24h.dc24h_account_credentials(account_id,verifier,created_at) "
        f"SELECT {target},verifier,created_at FROM dc24h.dc24h_account_credentials WHERE account_id={owner};")
    flags = ['--transfer', owner, '--revision', '1', '--to', target, '--to-revision', '1',
             '--password-file', proof, '--target-password-file', wrong, '--confirm-stopped']
    call(*flags)
    assert sql(f"SELECT role FROM dc24h.dc24h_accounts WHERE id={owner}") == 'owner'
    flags[-2] = proof
    result = call(*flags, success=True)
    assert f'former_owner_id={owner} former_owner_revision=2' in result.stdout
    assert f'owner_id={target} owner_revision=2' in result.stdout
    assert sql(f"SELECT CONCAT(role,':',status) FROM dc24h.dc24h_accounts WHERE id={owner}") == 'administrator:disabled'
    assert sql(f"SELECT CONCAT(role,':',status) FROM dc24h.dc24h_accounts WHERE id={target}") == 'owner:active'
    call(*flags)  # Stale IDs/revisions never replay a successful write.
    assert sql("SELECT COUNT(*) FROM dc24h.dc24h_accounts WHERE role='owner' AND status='active'") == '1'
    # Empty/current migration history and serving config must not be altered.
    assert sql('SELECT COUNT(*) FROM dc24h.dc24h_schema_migrations') == '7'
