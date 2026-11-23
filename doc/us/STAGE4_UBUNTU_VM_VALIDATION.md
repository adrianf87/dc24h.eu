# Stage IV: isolated Ubuntu VM validation

This independent test branch is `test/stage4-ubuntu-vm-validation`, based on
`main` after the beta-0.4.13–0.4.16 Stage IV merge. The branch is for testing;
it does not deploy the project, enable public WebAdmin or authorize a release.

## GitHub-hosted runners

The [Stage IV isolated Ubuntu VM validation](../../.github/workflows/stage4-ubuntu-vm-validation.yml)
workflow runs the same operational suite on fresh GitHub-hosted Ubuntu **22.04**,
**24.04** and **26.04** VMs. On each runner it:

1. Installs pinned-in-scope C++20, MariaDB server/client, OpenSSL and Apache2
   dependencies with the existing bounded apt-mirror installer.
2. Builds the actual C++ server and all CTest fixtures in Release mode, failing
   configuration if integration dependencies are missing.
3. Runs the process-level database backup/restore, migration and fault/restart
   resilience checks against a **disposable Unix-socket-only MariaDB instance**.
4. Tests a **disposable Apache2 loopback HTTPS gateway**, including TLS and
   negative proxy/access cases, plus API and ADCS TLS regressions.
5. Starts the real C++ ADC and ADCS listeners on `127.0.0.1` and ephemeral
   ports, uses a throwaway self-signed localhost certificate, and checks 12
   bounded handshakes per protocol with certificate verification for ADCS.
6. Installs locked WebAdmin packages, builds React/TypeScript/Vite, installs
   Chromium and runs browser E2E, frontend unit and C++ API integration checks.

The suite writes only non-secret latency/success aggregates and a CTest JUnit
report to a seven-day artifact named `stage4-ubuntu-vm-<runner>`.

## Run and interpret

Push this branch (or open a pull request to `main`) to start the matrix.
The workflow has a 90-minute timeout per VM; the apt installer has separate
network deadlines. All three jobs must finish successfully. A red, missing,
cancelled or timed-out job is **not a pass**; inspect job logs and rerun after
fixing the cause. The workflow does not automatically merge or deploy.

Local reproduction on a host with the required test services installed:

```bash
cmake -S . -B build/Stage4VM -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build/Stage4VM --parallel 2
ctest --test-dir build/Stage4VM --output-on-failure -R '^beta\.04\.stage4\.vm\.probe$'
```

The ADC/ADCS probe is deliberately constrained to 12 connections at 4/s.
It is a smoke measurement, **not** a throughput/capacity benchmark or a claim
about the target Intel Xeon D-1521 with 31,997 MiB RAM.

### Evidence boundary and remaining gates

- Ubuntu VM results are **not** measurements on the target Debian 13 Trixie
  server and do not establish Debian 12/13 deployment compatibility.
- Disposable MariaDB backup/recovery tests are **not** a production-sized
  backup restore, measured target RPO/RTO or a disaster-recovery sign-off.
- This automated workflow does **not** replace an independent security audit,
  operator review, public-access decision, or manual release authorization.
- Stage IV issue [#223](https://github.com/adrianf87/dc24h.eu-high-starter/issues/223)
  must retain any such outstanding gates until real evidence is attached.

Do not point the VM scripts or load probe at a public listener, shared
database or real administrator credentials.
