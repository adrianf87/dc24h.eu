# Session Manager — alpha-0.3.2

This increment starts from main `ca00266992dd866bdd30ff68e911c981c6665a62`, including Session Core. `users::SessionManager` in the independent dc24h_users library is the single owner of registered live session models. LoginHub owns one manager and its protocol peers borrow those models. TCP/TLS resource ownership, public SID/CID/nickname indexes, queues, validation and wire behavior remain in their existing modules.

## Registration and lifetime

The manager has an explicit capacity of 1–65535 entries. LoginHub uses its existing connection limit. Every registered transport is included: accepted ADCS before TLS success, negotiating ADC, identified admissions awaiting welcome completion, ready clients and closing sessions. Closing entries occupy capacity until cleanup. This is not the count of admitted public users or ready clients.

`add` accepts a nonzero SessionId, transport and supplied acceptance time. Success returns a borrowed Session pointer; typed errors distinguish stale/nonincreasing IDs and full capacity. IDs must increase across the manager lifetime, including after removal. A capacity rejection commits neither an ID nor an entry; IDs supplied by Network still come from its never-reused sequence. Allocation exceptions propagate without inserting a partial entry or advancing the watermark. Registration does not start negotiation: the adapter retains that decision.

An ordered map owns unique_ptr models. Inserting or removing another entry cannot invalidate a model reference. Only removing that model or destroying the manager invalidates it. Live access is restricted to the owner reactor thread; there are no locks, callbacks, cross-thread handles or persistent identifiers. The manager cannot be copied or moved.

`find` returns null for absent IDs; const access yields immutable models. `remove` returns missing or not_closed instead of silently discarding a live model. The transport/protocol owner first removes routing indexes, cancels output, disconnects protocol state and finalizes the model. LoginHub then destroys the peer and removes the closed model, without retaining tombstones. Repeated hub disconnect is harmless. A failed protocol-peer construction closes/removes its newly registered model and keeps that ID consumed. Destruction releases memory without performing socket I/O or hidden callbacks.

## Read access

`snapshot` returns an owning copy or nullopt. `page(after, limit)` returns copies in ascending numeric SessionId order strictly after the exclusive cursor; zero starts the list. Limits are 1–256, default 64; invalid limits throw invalid_argument before copying. An empty page marks the current end. Use the final returned ID as the next cursor. Gaps and a removed cursor are supported, and UINT64_MAX never wraps. Each page is a current owner-thread read; multiple pages are not a frozen transaction, so new registrations/removals may change subsequent results.

`counts` derives total and all six phase counts from the actual models in O(n), without duplicated transition counters. Closed entries are visible to a standalone manager until explicitly removed; the hub removes them in the same cleanup call. Registration, lookup and removal are O(log n); a page is O(log n + limit), excluding bounded identity copying. No unbounded full-registry snapshot API is added. The registry has bounded entry count and each model retains only the existing bounded public identity and lifecycle data.

`LoginHub::sessions()` exposes const registry queries to internal callers. This is not an HTTP endpoint or WebAdmin feature. No new configuration, log messages, credentials, translations, SQL schema, dependencies or API JSON fields are required. Existing US/PL runtime catalogs remain unchanged. Registered accounts, roles, bans, user profile modeling, persistence, distributed registries and remote administrative mutation remain separate work.

## Verification

`users.manager` covers capacity and ID rejection, missing/live/closed removal, accurate phase counts, stable addresses, owning pages, exclusive cursors with gaps, maximum cursor/ID, invalid page limits, 2000-lifetime churn and composition with real LoginHub models. The adapter tests also cover protocol-construction rollback, stale IDs after failed setup and empty registry after ADC/ADCS cleanup. Existing live mixed transport and real ncdc login/reconnect gates continue to exercise the integrated registry.

Build and strict test commands are in [testing](TESTING.md). See [Session Core](SESSION_CORE.md) for transitions, clocks, identity bounds and cleanup responsibilities. No remote CI, other OS or production-load result is inferred from local tests.
