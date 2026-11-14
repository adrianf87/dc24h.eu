# GitHub-hosted Ubuntu CI; Debian runner retired

On this branch, `.github/workflows/build.yml` is the only active workflow. C++20 builds and all CTest integrations run on GitHub-hosted **Ubuntu 22.04 and Ubuntu 24.04**, each in **Debug and Release** (`fail-fast: false`). Required integration tools include ncdc, MariaDB and Apache2. The WebAdmin job remains on GitHub-hosted **Ubuntu 24.04** with Node 22, production build/typecheck, unit tests, Chromium tests and the real C++ API integration.

The Proxmox/Debian 13 VM start/test/shutdown and connection-check workflows have been removed where present. Contabo/Debian workflows, where present, are archived under `.contabo/workflows/` for local tooling tests and are not GitHub Actions workflows. No Debian container, self-hosted runner, SSH VM control or external-host discovery/reinstallation is part of active CI. The obsolete custom-runner label configuration is removed.

This is a CI configuration change. Debian remains a supported application platform; historical local Debian test results remain valid records. Application code, the Ubuntu workflow and its complete test commands are unchanged. Runner registrations, repository secrets, external machines and historical Actions runs are not modified by these commits.

Push and pull-request events run `build.yml`. Workflow YAML, the Ubuntu/build-type matrix and job targets were checked locally. Remote execution results must be checked in GitHub Actions; removing a workflow does not erase old failed checks.
