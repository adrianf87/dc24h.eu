# CI configuration

`Check status` uses the custom runner `server-747585786`.
That host starts each test container only for its check, with 16 CPU threads and
32 GiB RAM, then stops it. See [GitHub Actions CI](GITHUB_CI.md).
