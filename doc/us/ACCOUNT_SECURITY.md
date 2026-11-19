# Account security — alpha-0.9.6

Passwords, GPA/PAS and secret protection. The branch starts from main `22bc5bc`. The branch name is `alpha-0.9.6`.

The hub already stores the account password as a fixed scrypt verifier and a separate AES-256-GCM ADC password. It issues one Tiger GPA per connection and limits repeated attempts. This increment adds two fixed rules. They are not optional and they are not INI keys.

## Distinct passwords

Enrollment refuses an ADC password that is the same byte string as the account password proved in that call. Equal lengths are compared with `CRYPTO_memcmp`. A different length, a different case or one extra character is a different password. The check runs only after the scrypt proof succeeds, so a wrong proof stays a proof failure. The diagnostic does not include either password.

Rows already stored are not rewritten. Startup does not compare the two passwords, because the account password is not available as plaintext. An account-password change still deletes the ADC credential. The next enrollment applies this rule. There is no schema migration.

## Sealed until admission

`prepare` identifies a guest, a denial or an account. It does not decrypt. The reactor decrypts the ADC password only in `open_challenge`, after the account ban check and the per-account attempt budget both admit the connection. An account-banned or account-throttled attempt keeps the existing generic refusal and receives no GPA. The decrypted buffer is wiped when that challenge is issued. Startup still decrypts each stored record once, to reject a bad envelope before listen, and then wipes that copy.

IP admission still runs before `prepare`. A denied identity, a disabled account, a missing credential, or plain ADC while `require_tls` is on, still returns before decryption. A test double that issues its challenge inside `prepare` keeps that behavior. The production directory does not.

## Unchanged

GPA/PAS remains Tiger(password concatenated with the challenge), one challenge per connection, consumed on every attempt. Refusal stays the generic `ISTA 223 Authentication failed`. Logs still omit passwords, GPA/PAS payloads and ciphertext. `require_tls` still defaults to on. `require_tls=off` still lets a captured PAS be attacked offline. Attempt buckets still reset on process restart and still belong to one hub process. This does not close residual threats T02 or T03. Schema v9 is unchanged. No status counter, WebAdmin page, public password endpoint, CLI password argument or Apache setting is added. Automatic key rotation is not implemented. HTTP/2, OCSP and automatic certificate renewal remain unimplemented. A public authenticated gateway remains separate work.

`accounts.account_security` checks that an exhausted account budget and an account ban do not open the sealed secret, that another account still receives GPA, and that a guest and a mismatched identity do not open it. `accounts.adc_authentication.process` checks the identical-password refusal, enrollment of a case-different ADC password, a sealed `prepare`, and one-shot `open_challenge`.
