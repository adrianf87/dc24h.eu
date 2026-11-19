# Logging security — alpha-0.9.10

Secure logs and event recording. The branch starts from main `3237062`. The branch name is `alpha-0.9.10`.

The hub still writes synchronous UTC lines to stderr or to one append file. This increment refuses a log another user can read, and it records a fixed set of security outcomes even when connection logging is off.

## Private file

A new log file is created with `O_EXCL` and `O_NOFOLLOW`, then set to mode `0600`. It must be a regular file, have one link, and be owned by the effective user. An existing file is appended only when it already has mode `0600` and that same owner. The hub does not change the mode of an existing file. A mode such as `0640` or `0644`, another owner, a final symlink, a FIFO, a device or a second hard link is refused. The directory is not created. `--log-file -` still selects stderr. `--check-config` still does not open the file.

A restrictive umask cannot leave a new file without owner write. The file is not rotated, reopened or sent to a remote collector. A slow disk can still block the caller. This does not close residual threat T13.

## Security records

At `info` or `warning`, these outcomes are written even when connection logging is off:

`tls_handshake_failed`, `handshake_timeout`, `security_quota`, `partial_frame_timeout`, `login_rejected`, `login_timeout`, `negotiation_limit`, `identity_limit`, `authentication_revoked`, `authentication_limit`, `kicked`, `banned`.

The line is `Security record=` plus that fixed name. It has no connection id, address, port, nickname, CID, PID, password or ADC text. At most 32 records are written in each one-second window. Further records are counted. One warning, `Suppressed security records=`, is written on the next window or when the logger stops. `error` and `off` do not write these records. Opt-in connection lines keep their own budget and still require `connections=on` with `level=info`.

These records are not an audit trail. They have no actor and no target. Refusals of maintenance, CSRF failures and grant use are not rows and are not these lines. This does not close residual threat T11. Production audit logging is not implemented. Schema v9 is unchanged. No status counter, WebAdmin page, INI key or Apache setting is added. `Require local` stays.

`logging.unit` checks the owner-only mode, a refused existing file, the fixed records and the 32-per-second budget. `logging.process` checks startup refusal of a mode `0644` file and one `partial_frame_timeout` record with no peer text.
