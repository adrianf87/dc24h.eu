# Development

Create each new release branch from the current `main`, preserving existing history and code. alpha-0.1.4 was created from main and merged the reviewed `chatgpt-alpha-0.1.4` history; fixes and validation follow on the release branch. Main is not automatically changed by this work.

Keep each module in its own `src/` and `include/dc24h/` directory. Use C++20 without compiler extensions, RAII, explicit ownership and bounded data structures. Network operations stay in the reactor thread; new modules must define their startup/shutdown dependencies. UI and database logic must not be coupled into Core. Public headers describe lifetime requirements.

Default code and documentation language is EN/US UTF-8. Polish documentation lives in `doc/pl`; English in `doc/us`. The root module documents link to both. Keep the ordered built-in catalogs in `languages/us/messages.hpp` and `languages/pl/messages.hpp` aligned. Translate operator text only, never protocol payloads. Internal/system diagnostics without catalog entries use the English fallback.

Before significant commits: build, run appropriate CTest suites, update documentation and CHANGELOG, inspect Git diff and exclude secrets/build artifacts. Meaningful checks must also work with Release assertions disabled. Every stage gets a descriptive commit. Include source version and commit SHA in bug reports. Do not claim planned modules or unverified OS/CI results are complete.
