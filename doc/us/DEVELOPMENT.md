# Development

Create each new release branch from the current `main`, preserving existing history and code. alpha-0.1.4 was created from main and merged the reviewed `chatgpt-alpha-0.1.4` history; fixes and validation follow on the release branch. Main is not automatically changed by this work.

Keep each module in its own `src/` and `include/dc24h/` directory. Use C++20 without compiler extensions, RAII, explicit ownership and bounded data structures. Network operations stay in the reactor thread; new modules must define their startup/shutdown dependencies. UI and database logic must not be coupled into Core. Public headers describe lifetime requirements.

Default code and documentation language is EN/US UTF-8. Polish documentation lives in `doc/pl`; English in `doc/us`. Only the root index links to both trees; duplicate root module files were removed. Keep the ordered built-in catalogs in `languages/us/messages.hpp` and `languages/pl/messages.hpp` aligned. Translate operator text only, never protocol payloads. Internal/system diagnostics without catalog entries use the English fallback.

Before significant commits: build, run appropriate CTest suites, update documentation and CHANGELOG, inspect Git diff and exclude secrets/build artifacts. Meaningful checks must also work with Release assertions disabled. Every stage gets a descriptive commit. Include source version and commit SHA in bug reports. Do not claim planned modules or unverified OS/CI results are complete.

alpha-0.1.5 starts from main after the explicit alpha-0.1.4 merge. The configuration/logging sources and tests from chatgpt-alpha-0.1.5 were reviewed and adapted, preserving main TLS and i18n fixes. No en/ duplicate language directory is imported.

alpha-0.1.6 starts from main after alpha-0.1.5. The logging-specific commits in chatgpt-alpha-0.1.6 were reviewed and adapted; main's safer INI/file handling, TLS scheduling and US/PL support are retained. Use typed ConnectionEvent values and keep all payload/peer data out of operational logs.

alpha-0.1.7 starts from main after the alpha-0.1.6 merge. The security sources/tests in chatgpt-alpha-0.1.7 were reviewed and adapted. Retain bounded admission state, canonical source identities, one release per slot and absolute per-frame deadlines; never log the source keys or payloads. Security is independent of the planned authentication module.
