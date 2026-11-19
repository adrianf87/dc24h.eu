# CI configuration

`Check status` on this branch uses the custom runner `server-747585786`.
That host starts each test container only for its check, with 16 CPU threads and
32 GiB RAM, then stops it. See [GitHub Actions CI](GITHUB_CI.md). Branch
`contabo-docker-v0.1` is not merged into this branch or into `main`.
