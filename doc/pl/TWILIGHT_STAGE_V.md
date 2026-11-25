# dc24h.eu-twilight — Etap V: dokumentacja, GitHub, pochodzenie i komunikacja

**Stan: przygotowany w Draft PR #9 do kontroli CI i odrębnej akceptacji OWNER.** Jest to etap **P1 dokumentacyjny**; nie aktualizuje wersji wykonywalnej, API ani prywatnego stanu. [Zadanie #1](https://github.com/adrianf87/dc24h.eu-twilight/issues/1) · [PR #9](https://github.com/adrianf87/dc24h.eu-twilight/pull/9). W pełnym rebrandingu **Etap VI nadal pozostaje NO-GO**.

## Co jest prawdziwym stanem projektu?

| Obszar | Bieżący stan | Źródło / zastrzeżenie |
| --- | --- | --- |
| Nazwa i repozytorium | [`adrianf87/dc24h.eu-twilight`](https://github.com/adrianf87/dc24h.eu-twilight) | Odrębny projekt rozwojowy z chronionym `main`. |
| Kod / wersja | `beta-0.9.20`, źródło oparte na High Starter RC1 | [`v0.9.20-rc.1`](https://github.com/adrianf87/dc24h.eu-high-starter/releases/tag/v0.9.20-rc.1), commit źródłowy `e4fcb8b5849d4afa4671f97a2ee9c3e63b085a18`. |
| Odbiór pierwotnego RC1 | Tak — **tylko High Starter, tylko nieprodukcyjna ocena** | [PR #243](https://github.com/adrianf87/dc24h.eu-high-starter/pull/243) + [#244](https://github.com/adrianf87/dc24h.eu-high-starter/pull/244) + [#245](https://github.com/adrianf87/dc24h.eu-high-starter/pull/245); zachowujemy oryginalne URL-e i dowody. |
| Rebranding Twilight I–IV | Wdrożony w kodzie chronionego `main` | [PR #2–#7 w repo Twilight](https://github.com/adrianf87/dc24h.eu-twilight/pulls?q=is%3Apr+is%3Aclosed). Test pakietów/systemd był **syntetyczny**, nie dotyczył istniejącego serwera. |
| Etap V i VI | Prace w tym samym [Draft PR #9](https://github.com/adrianf87/dc24h.eu-twilight/pull/9) | Testy CI, odbiór dokumentacji i końcowa decyzja OWNER pozostają odrębne. Nie jest to nowe wydanie. |

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

## GitHub About i ustawienia — checklista ręcznej kontroli

Poniższe elementy **nie są częścią commita** i nie mogą zostać uznane za wykonane na podstawie samego PR. Potwierdzenie wymagane osobno na GitHub po zalogowaniu jako OWNER:

- **Description (proponowana):** `dc24h.eu-twilight — independent C++20 ADC/ADCS hub and React WebAdmin development fork; non-production release qualification.`
- **Topics (proponowane):** `dc24h`, `twilight`, `direct-connect`, `adc`, `adcs`, `cpp20`, `react`, `webadmin`, `linux`.
- **Website URL:** nie wpisywać niezatwierdzonego publicznego hosta. Publiczna domena `dc24h.eu` i certyfikaty nie zmieniają właściciela przez rebranding.
- **Default branch:** `main` (potwierdzona w metadanych GitHub); ochrona i wymagane kontrole mają pozostać aktywne, bez bypass. Uprawnienia GitHub App i ewentualne branch rulesets wymagają weryfikacji w Settings przed formalnym odbiorem.
- **Kopiowanie repo:** Git mirror zachowuje commity/tagi, ale nie odtwarza automatycznie Issues, PR, Releases, sekretów, ustawień, rulesets ani uprawnień aplikacji. Własna historia Twilight jest w [Issue #1](https://github.com/adrianf87/dc24h.eu-twilight/issues/1) i PR #2–#9; oryginalna historia pozostała u źródła.

## Odbiór Etapu V — warunki do zaznaczenia w Issue #1

1. Zielony CI i audyt aktywnych odnośników dla **ostatecznego SHA** PR #9; negatywne testy dokumentów odrzucają stare clone/binary i pomylenie RC1 ze stanem Twilight.
2. Ręczne sprawdzenie GitHub About/topics/ruleset/permissions i zaprotokołowanie wyniku (także jeśli metadata wymaga poprawy). Sam opis repo nie dowodzi konfiguracji repo.
3. Ocena przez OWNER treści PL/EN, changelogu, mapy wyjątków oraz prawdziwości historycznych linków. **Odrębna zgoda na merge**, bez nowego tagu ani produkcji.

**Decyzja obecna:** dokumentacja gotowa do review, odbiór formalny oczekuje. **NO-GO dla nowego RC i wdrożenia.**
