# Development

**Current integration — alpha-0.2.8:** [Guest login](ADC_LOGIN.md) attaches the state/SUP/SID/INF/Tiger stack to the actual C++ TCP/TLS reactor with bounded ordered output, nickname/CID admission, public presence and complete ncdc NORMAL/reconnect tests. Historical stage descriptions below retain their original scope; statements that transport attachment/output/login were still planned describe those earlier versions. Registered-user authentication, chat, search and transfer dispatch remain planned.

The alpha-0.2 baseline and prioritized acceptance gates are recorded in [the alpha-0.1 audit](AUDIT_ALPHA_0.1.md). Numbered branches still start from current main. `alpha-0.2` marks the reviewed audit commit. Use `DC24H_REQUIRE_INTEGRATION_TESTS=ON` for full validation; keep project.metadata, npm lock metadata, runtime/frontend version and both documentation trees aligned.

Create each new release branch from the current `main`, preserving existing history and code. alpha-0.1.4 was created from main and merged the reviewed `chatgpt-alpha-0.1.4` history; fixes and validation follow on the release branch. Main is not automatically changed by this work.

Keep each module in its own `src/` and `include/dc24h/` directory. Use C++20 without compiler extensions, RAII, explicit ownership and bounded data structures. Network operations stay in the reactor thread; new modules must define their startup/shutdown dependencies. UI and database logic must not be coupled into Core. Public headers describe lifetime requirements.

Default code and documentation language is EN/US UTF-8. Polish documentation lives in `doc/pl`; English in `doc/us`. Only the root index links to both trees; duplicate root module files were removed. Keep the ordered built-in catalogs in `languages/us/messages.hpp` and `languages/pl/messages.hpp` aligned. Translate operator text only, never protocol payloads. Internal/system diagnostics without catalog entries use the English fallback.

Before significant commits: build, run appropriate CTest suites, update documentation and CHANGELOG, inspect Git diff and exclude secrets/build artifacts. Meaningful checks must also work with Release assertions disabled. Every stage gets a descriptive commit. Include source version and commit SHA in bug reports. Do not claim planned modules or unverified OS/CI results are complete.

alpha-0.1.5 starts from main after the explicit alpha-0.1.4 merge. The configuration/logging sources and tests from chatgpt-alpha-0.1.5 were reviewed and adapted, preserving main TLS and i18n fixes. No en/ duplicate language directory is imported.

alpha-0.1.6 starts from main after alpha-0.1.5. The logging-specific commits in chatgpt-alpha-0.1.6 were reviewed and adapted; main's safer INI/file handling, TLS scheduling and US/PL support are retained. Use typed ConnectionEvent values and keep all payload/peer data out of operational logs.

alpha-0.1.7 starts from main after the alpha-0.1.6 merge. The security sources/tests in chatgpt-alpha-0.1.7 were reviewed and adapted. Retain bounded admission state, canonical source identities, one release per slot and absolute per-frame deadlines; never log the source keys or payloads. Security is independent of the planned authentication module.

## Database increment

alpha-0.1.8 starts from main after alpha-0.1.7. Keep SQL handles and credentials out of Core/Network, use bound values, and keep applied SQL migrations immutable. Extend migration recovery before adding non-idempotent DDL. Disposable database tests must use their own data directory/socket and `--no-defaults`; never connect to a developer database.

## API increment

alpha-0.1.9 starts from main after the alpha-0.1.8 merge. Preserve the narrow bodyless HTTP contract and local-only access controls. Do not add mutations, body parsers, Apache exposure or browser token storage without explicit authorization/role/audit design. Providers must be thread safe, bounded and outlive API destruction. Use disposable private Unix sockets/tokens in tests, and verify both ncdc counters and MariaDB status through the real API.

## WebAdmin increment

alpha-0.1.10 starts from main after alpha-0.1.9. Keep browser-safe schema/formatting separate from the server-only Vite adapter, maintain EN/PL key parity, never include secrets or node_modules/dist/test results in Git, and update package-lock with dependency changes. Verify the frontend build, unit/browser/live C++ integration and existing server tests. Administrative mutations and production web authentication require a separate reviewed design.

## Apache2 increment

alpha-0.1.11 starts from main after alpha-0.1.10. Keep host activation, real certificates and service reloads separate from example authoring/testing. Test the exact shipped TLS/access/header policy with temporary paths/ports and no host config. A successful apache2 -t is insufficient to claim certificate/key correspondence; exercise real initialization. Keep both documentation languages and version-sensitive fixtures aligned.

## ADC Parser increment

alpha-0.2.2 starts from main after the explicit alpha-0.2/alpha-0.2.1 merge. Keep BASE argument syntax separate from authorization, identity checks and execution. Preserve opaque extension layouts, field order and duplicates. NamedParameterView borrows a decoded token; never retain it after mutation/destruction. Add release-active syntax fixtures and full-result stream comparisons for new grammar rules, then verify TCP, TLS and both ncdc transports.

## ADC State — alpha-0.2.3

This branch starts from main at `4c98e10` after the parser merge. Maintain the [state contract](ADC_STATE.md), one noncopyable instance per future connection, explicit semantic approvals, unchanged-state rejection and terminal disconnect. Keep state tests active in Release; update the command/state and direction/source matrices with every new command. Never infer authentication from a parsed frame or advance on an enqueued response. Transport integration/output, validation and unique SID allocation remain separate acceptance work.

## SUP — alpha-0.2.4

Branch from main at `1dd0b9f` after the ADC State merge. Keep support parsing, protocol sequencing and identity/hash computation separate. Prefer ProtocolConnection over manual low-level protocol approval for future transport integration. New hash algorithms require an explicit registry/metadata and selection/reply-order tests; unknown feature names are never guessed to be hashes. Preserve transactional updates, immutable session hash, bounded cumulative support and deterministic local replies. ncdc fixture success is not live hub login. See [ADC_SUP.md](ADC_SUP.md).

## SID — alpha-0.2.5

Branch from current main at `350a43f`. Use one shared SidPool for one hub and unique move-only leases; never add release-by-SID or copyable ownership. Preserve allocation-free acquire/release, the 20-bit codec, full namespace and word-boundary tests, and one-thread lifetime rules. SidSession supersedes arbitrary caller-assigned ISID for future transport integration. Test cleanup before bind, terminal responses, destruction and reuse; do not call completed-send observation for queue insertion. Do not infer user identity from SID ownership. See [ADC_SID.md](ADC_SID.md).

## INF — alpha-0.2.6

Branch from main at `8552406` after the SID merge. Build with libgcrypt20-dev; extend known fields only with explicit type/domain/budget and cancellation tests. Preserve canonical base32/TIGER1 byte order, source/IP binding, immutable CID, no PD in retained/exported metadata and candidate-before-state commit. Use ClientSession for future attachment; lower-level state admission alone does not validate INF. Keep registry uniqueness/admission separate and never auto-grant NORMAL from a parsed INF. New cryptographic dependencies must be present in both CI jobs and installation guides. See [ADC_INF.md](ADC_INF.md).

## CID/PID — alpha-0.2.7

Branch from main at `9d3443a` after the INF merge. Use verify_tiger_identity for private PID input; do not duplicate string-encoded digest comparisons in field consumers. Extend public verified values without adding PID storage. Preserve exact TIGER1/canonical-tail semantics, direct failure clearing, cleanup on all exits and no fallback/global crypto changes. Linker fault wrappers are test-only. Tests must cover malformed encodings, mismatch, unavailable provider, recovery and actual ncdc pairs. See [ADC_IDENTITY.md](ADC_IDENTITY.md).
