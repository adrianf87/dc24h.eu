# Logs / Dzienniki

This directory holds **sanitized diagnostic evidence** for `dc24h.eu-twilight`, not live application secrets or private state.

- [`tests/`](tests/) — reports from reproducible tests (including Debian 13, ncdc, ADC/ADCS, CI and WebAdmin).
- Include test date (UTC), source commit SHA, OS/client versions, commands, expected/actual results and a link to the relevant Issue or PR.
- Do **not** commit passwords, TOTP seeds/codes, session or service tokens, API credentials, private keys, database dumps, personal data or unredacted user/client identifiers.
- Ordinary `*.log` files are ignored by the repository. Only explicitly reviewed `logs/tests/*.redacted.log` files may be committed after removing secrets and private data. Do not use `git add -f` for raw logs.
- Logs can be useful evidence, but their presence is **not** proof of a PASS or a production GO. Verify their origin and source SHA.

## PL

Katalog na **oczyszczone logi diagnostyczne**. Wyniki testów umieszczaj w [`tests/`](tests/) wraz z datą, SHA źródła, środowiskiem, poleceniami i statusem testów. Przed dodaniem do GitHub usuń hasła, tokeny, klucze, dane użytkowników i inne informacje prywatne. Oryginalne pełne logi przechowuj poza publicznym repozytorium.
