# CI configuration

`Check status` uses the public GitHub-hosted runners described in [GitHub Actions CI](GITHUB_CI.md).
C++ Debug, C++ Release and WebAdmin run on each image using available logical CPUs; two additional ASan/UBSan security builds run on Ubuntu 24.04.
