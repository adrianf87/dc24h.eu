# dc24h.eu-twilight — Etap V: dokumentacja, GitHub, pochodzenie i komunikacja

**Stan: dokumentacja PL/EN i ustawienia GitHub zweryfikowane 2026-11-25; końcowy CI oraz formalny odbiór OWNER oczekują.** Jest to etap **P1 dokumentacyjny**; nie aktualizuje wersji wykonywalnej, API ani prywatnego stanu. [Zadanie #1](https://github.com/adrianf87/dc24h.eu-twilight/issues/1) · [Draft PR #9](https://github.com/adrianf87/dc24h.eu-twilight/pull/9). W pełnym rebrandingu **Etap VI nadal pozostaje NO-GO**.

## Co jest prawdziwym stanem projektu?

| Obszar | Bieżący stan | Źródło / zastrzeżenie |
| --- | --- | --- |
| Nazwa i repozytorium | [`adrianf87/dc24h.eu-twilight`](https://github.com/adrianf87/dc24h.eu-twilight) | Odrębny projekt rozwojowy z chronionym `main`. |
| Kod / wersja | `beta-0.9.20`, źródło oparte na High Starter RC1 | [`v0.9.20-rc.1`](https://github.com/adrianf87/dc24h.eu-high-starter/releases/tag/v0.9.20-rc.1), commit źródłowy `e4fcb8b5849d4afa4671f97a2ee9c3e63b085a18`. |
| Odbiór pierwotnego RC1 | Tak — **tylko High Starter, tylko nieprodukcyjna ocena** | [PR #243](https://github.com/adrianf87/dc24h.eu-high-starter/pull/243) + [#244](https://github.com/adrianf87/dc24h.eu-high-starter/pull/244) + [#245](https://github.com/adrianf87/dc24h.eu-high-starter/pull/245); zachowujemy oryginalne URL-e i dowody. |
| Rebranding Twilight I–IV | Wdrożony w kodzie chronionego `main` | [PR #2–#7 w repo Twilight](https://github.com/adrianf87/dc24h.eu-twilight/pulls?q=is%3Apr+is%3Aclosed). Test pakietów/systemd był **syntetyczny**, nie dotyczył istniejącego serwera. |
| Etap V i VI | Prace w tym samym [Draft PR #9](https://github.com/adrianf87/dc24h.eu-twilight/pull/9) | Etap V: dokumentacja i ustawienia GitHub sprawdzone; końcowy CI i odbiór OWNER oczekują. Etap VI: wydanie NO-GO. |

## Aktualne punkty wejścia dla użytkownika i operatora

- [README repozytorium](../../README.md): bieżąca marka, historia Alpha/Beta w rozwijanych menu, badge **aktualnego `main`**, pięć workflow i ograniczenia wydania.
- [Instalacja PL](INSTALLATION.md) / [EN](../us/INSTALLATION.md): klonowanie **Twilight**, katalog `dc24h.eu-twilight`, nowa binarka `./build/dc24h-twilight` i wyłącznie próbny listener `127.0.0.1`.
- [Konfiguracja](CONFIGURATION.md), [API](API.md), [MariaDB](DATABASE_MIGRATIONS.md), [WebAdmin](WEBADMIN.md), [TLS](ADCS_TLS.md) i [indeks dokumentacji](../README.md) mają bieżące ścieżki tam, gdzie opisują obecny kod. Operacje migracji bazy nadal wymagają osobnego upoważnienia.
- [GitHub Actions Twilight](https://github.com/adrianf87/dc24h.eu-twilight/actions) i [przewodnik CI PL](GITHUB_CI.md) / [EN](../us/GITHUB_CI.md): odnośniki do własnego repo; badge `main` **nie** oznacza zaakceptowania finalnego SHA PR.
- [Etap IV — mapa nazw i rollback](TWILIGHT_STAGE_IV.md) / [EN](../us/TWILIGHT_STAGE_IV.md) oraz [Etap VI — pełna kwalifikacja](TWILIGHT_STAGE_VI.md) / [EN](../us/TWILIGHT_STAGE_VI.md).

## Wyjątki: czego nie wolno przepisywać w ramach dokumentacji

1. **Historia:** dawne wersje Alpha/Beta, opisy „Draft PR #243”, linki [High Starter](https://github.com/adrianf87/dc24h.eu-high-starter), autentyczne przebiegi CI i starsze sekcje `CHANGELOG.md` są utrzymane jako historyczne zapisy **z daty ich powstania**. Od aktualnego stanu Twilight są oddzielone notą pochodzenia oraz nowym wpisem changelogu. Nie przenosić numerów PR ani nie zmieniać linków dowodowych na nowe repo.
2. **Identyfikatory techniczne:** `namespace dc24h`, wewnętrzne cele CMake, klucze `DC24H_*`, ścieżki API, nazwy tabel MariaDB i elementy protokołu ADC/ADCS są celowo stabilne. Ich zmiana byłaby migracją, a nie korektą tekstu.
3. **Infrastruktura:** `dc24h.eu` jako host, TLS/certyfikaty, WebAuthn `rpID`/`Origin`, konta `dc24h`, `/etc/dc24h` oraz `/var/lib/dc24h` pozostają bez zmian bez odrębnej zgody. Marketingowy `rpName` i `server.name` API v1 mają już nową nazwę, przy ścisłej odmowie starego `server.name`.
4. **`LICENSE`, podpisy commitów i historia Gita:** bez zmian. Oryginalny tag `v0.9.20-rc.1` nie jest przesuwany ani przedstawiany jako nowe Twilight RC.

## GitHub About i ustawienia — wynik kontroli 2026-11-25

Poniższe elementy **nie są częścią commita**. Zostały sprawdzone bezpośrednio w interfejsie GitHub, nie wywnioskowane z zawartości PR:

- **Description (zapisana i sprawdzona):** `dc24h.eu-twilight — independent C++20 ADC/ADCS hub and React WebAdmin development fork; non-production release qualification.`
- **Topics (zapisane i sprawdzone):** `dc24h`, `twilight`, `direct-connect`, `adc`, `adcs`, `cpp20`, `react`, `webadmin`, `linux`.
- **Website URL:** puste; nie dopisano domeny ani nie zmieniono DNS/TLS.
- **Ochrona gałęzi:** `main` domyślny, ruleset **Main Branch Protection** **Active**, lista bypass pusta; wymagany PR, aktualna gałąź oraz 12 status checks; force-push i usunięcie gałęzi zablokowane.
- **GitHub App ChatGPT Codex Connector:** wybrane trzy repozytoria (`dc24h.eu-high-starter`, `dc24h.eu`, `dc24h.eu-twilight`); odczyt checks, commit statuses i metadata oraz zapis Actions, code, issues, pull requests i workflows.
- **GitHub App Cursor:** ma dostęp do **All repositories** (obecnych i przyszłych), odczyt administration, commit statuses, deployments, metadata, packages i pages oraz zapis Actions, checks, code, discussions, issues, merge queues, pull requests i workflows. **To szeroki zakres dostępu, wymagający świadomej decyzji OWNER o ewentualnym ograniczeniu**; nie zmieniano uprawnień.
- **Kopiowanie repo:** Git mirror zachowuje commity/tagi, ale nie odtwarza automatycznie Issues, PR, Releases, sekretów, ustawień, rulesets ani uprawnień aplikacji. Własna historia Twilight jest w [Issue #1](https://github.com/adrianf87/dc24h.eu-twilight/issues/1) i PR #2–#9; oryginalna historia pozostała u źródła.

## Odbiór Etapu V — warunki do zaznaczenia w Issue #1

1. Audyt **1498/1498 lokalnych odnośników Markdown** w aktywnej dokumentacji i negatywne testy dokumentów wykonano. Zestaw kontroli CI dla **ostatecznego SHA** PR #9 nadal wymaga zakończenia; audyt nie certyfikuje wszystkich zewnętrznych URL-i.
2. GitHub About/topics/ruleset/uprawnienia aplikacji sprawdzono w GitHub. Ryzyko szerokich uprawnień Cursor przekazano do oceny OWNER; sam opis repo nie dowodzi ustawień.
3. Ocena przez OWNER treści PL/EN, changelogu, mapy wyjątków oraz prawdziwości historycznych linków. **Odrębna zgoda na merge**, bez nowego tagu ani produkcji.

**Decyzja obecna:** prace dokumentacyjne i konfiguracja About wykonane; **odbiór formalny nadal oczekuje na końcowy CI i OWNER**. **NO-GO dla nowego RC i wdrożenia.**
