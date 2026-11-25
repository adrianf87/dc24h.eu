# dc24h.eu-twilight — Etap VI: kwalifikacja wydania i decyzja OWNER

**Stan: ROZPOCZĘTY — NO-GO dla wydania.** Ta gałąź dostarcza pierwszy, ograniczony dowód tożsamości źródła i kontraktów. **Nie** potwierdza kompletnej kwalifikacji, akceptacji OWNER ani uprawnień do tagowania, wdrożenia lub migracji. Obowiązuje zakres Issue #1, a otwarte czynności Etapu V pozostają blokadą do osobnego odbioru.

## Granice rebrandingu i wyjątki

| Kategoria | Oczekiwanie i dowód | Granica |
| --- | --- | --- |
| Marka w CLI i interfejsie | `dc24h.eu-twilight` w `src/main.cpp`, komunikatach PL/US, przykładzie INI i aktywnych punktach WebAdmin; `scripts/twilight_stage6_preflight.py`. | Przegląd statyczny wybranych plików, **nie** pełny audyt wszystkich ekranów, logów i linków. |
| Status API v1 | C++ publikuje `server.name=dc24h.eu-twilight`; oba walidatory WebAdmin odrzucają starą nazwę. | To zmiana niekompatybilna: starszy klient/status może wymagać równoczesnej aktualizacji. Nie włączać niejawnej akceptacji starej nazwy. |
| Passkey i zewnętrzne pochodzenie | Marketingowy `rpName` jest nowy; weryfikacja `rpID`, `expectedOrigin` i brama Origin pozostają ścisłe. | `rpID`, publiczny Origin, domena DNS, TLS i certyfikaty **nie** zmieniają się z nazwą produktu. Brak odbioru fizycznych/synchronizowanych passkeys. |
| Systemd i tożsamość instalacji | Nowa nazwa jednostki, `Conflicts=dc24h.service`, wymagany znacznik OWNER i dotychczasowe konto `dc24h`. | Konta/UID/GID, `/etc/dc24h`, `/var/lib/dc24h`, `/run/dc24h`, prywatny stan i schemat MariaDB nie są migrowane. |
| GitHub | Pięć workflow zawiera przejrzane warunki `github.repository`; historyczne repozytoria są dozwolone tylko jako świadome wyjątki. | Historia commitów, odsyłacze do pierwotnego projektu i dowody RC1 mogą zachowywać dawną nazwę. Nie oznaczają nowego wydania Twilight. |
| Stabilne identyfikatory | `namespace dc24h`, `include/dc24h`, klucze `DC24H_*`, protokoły ADC/ADCS, ścieżki API oraz nazwy DB pozostają techniczne. | Nie przeprowadzać kosmetycznej migracji protokołu ani stanu bez osobnej specyfikacji i zgody. |

## Pierwsza bramka (zakres tego PR)

- Skrypt `scripts/twilight_stage6_preflight.py` jest związany z rzeczywistym `git rev-parse HEAD`, akceptuje wyłącznie 40-znakowy SHA i GitHub-hosted Ubuntu **22.04, 24.04, 26.04**. Wymusza wybrane kontrakty widoczne dla użytkownika, API, Origin/RP ID, systemd, npm i repozytorium.
- Testy `tests/twilight_stage6_preflight_tests.py` sprawdzają odmowy przy zmianie nazwy API, wyłączeniu weryfikacji Origin/RP, usunięciu znacznika migracji lub konfliktu systemd, zmianie repozytorium i nazwie npm oraz złym SHA/OS.
- Macierz C++ Debug/Release w `.github/workflows/build.yml` uruchamia testy i zapisuje zwięzły raport `twilight-stage6-preflight.json` w istniejącym artefakcie GitHub Actions. Walidator ZIP jawnie zna ten raport, ale nie wymaga go od **historycznych** artefaktów RC1.
- Raport zawsze deklaruje **`status: NO-GO`**, brak zweryfikowania kompletnego finalnego SHA, brak niezależnych hashy ZIP, brak akceptacji Etapu V i OWNER. Zielony test potwierdza tylko zachowanie ograniczonego kontraktu, **nie** gotowość wydania.
- Skrypt **nie** pobiera sekretów, nie modyfikuje stanu prywatnego, nie wywołuje `systemctl`, `dpkg`, nie uruchamia publicznego listenera ani nie wdraża projektu.

## Warunki kompletnego odbioru Etapu VI (jeszcze niespełnione)

1. **Zamknąć i odebrać Etap V**: dokumentację, publiczne opisy repozytorium/README, changelog i spójność komunikacji. Uzasadnić wszystkie zamierzone stare identyfikatory i wyeliminować niezamierzony stary branding.
2. **Zamrozić końcowy HEAD** konkretnego Draft PR. Zebrać zielone wyniki **dla tego samego SHA** ze wszystkich wymaganych workflow, jobów i macierzy: C++ Debug/Release, WebAdmin/Playwright, CodeQL, Ubuntu VM/systemd 22.04/24.04/26.04, ASan/UBSan, TSAN/fuzz, magazyn stanu i audyt zależności. Nie akceptować `skipped`, anulowanych, nieudanych ani pochodzących z obcego SHA jako wykonanych testów.
3. **Niezależnie pobrać artefakty ZIP** z GitHub API, porównać SHA-256 z metadanymi, sprawdzić kompletność wymaganych raportów oraz ich `source_sha`, nazwę repozytorium, workflow, próbę wykonania i semantykę dowodów. Sam zielony znaczek CI ani lokalny test nie zastępuje weryfikacji.
4. **Rozszerzyć testy negatywne i audyt ręczny**: fałszywe `server.name`, błędny repo/SHA, złe Origin i `rpID`, stare konfiguracje/klienci, odmowa migracji bez OWNER, granice sesji/TLS. Zachować zamierzone odmowy; nie rozluźniać walidatorów dla pozornej kompatybilności.
5. **Odebrać procedurę instalacji i rollbacku na reprezentatywnym hoście**. Historyczny syntetyczny test pakietów i systemd (Etap IV, PR #7) **nie** jest dowodem migracji realnego hosta. Brak potwierdzenia wydajności Debian/Xeon, fizycznego sprzętu i klientów DC++/AirDC++, SLA, RPO/RTO, cold-startu prywatnego stanu, atomowej migracji v1→v2 i polityki ciągłości audytu. Każdy z tych punktów wymaga odrębnego odbioru lub jawnego wyłączenia z zakresu wydania.
6. **Przedstawić OWNER pakiet decyzji**: repo, branch, finalny SHA, lista workflow/checków i artefaktów, istotne wyjątki, otwarte ryzyka, deklarowany zakres RC oraz osobne wnioski `MERGE GO/NO-GO` i `RELEASE GO/NO-GO`. Zgoda na merge nie oznacza zgody na tag, pre-release, produkcję, migrację ani publiczny listener.

## Format decyzji końcowej — do wypełnienia po kwalifikacji

| Pole | Wartość obecna |
| --- | --- |
| Repo / gałąź | `adrianf87/dc24h.eu-twilight` / `rebrand/stage-vi-release-qualification` |
| Finalny SHA po stabilizacji PR | **niezweryfikowany** |
| Hosted CI na finalnym SHA | **niezweryfikowane** |
| ZIP / SHA-256 i komplet raportów | **niezweryfikowane** |
| Etap V i ręczne przypadki negatywne | **nieodebrane** |
| Host produkcyjny / rollback i limity | **nieodebrane** |
| Decyzja OWNER o merge | **oczekuje osobnej zgody** |
| Decyzja OWNER o nowym tagu/RC | **oczekuje osobnej zgody — NO-GO** |

Nie nadpisywać `v0.9.20-rc.1` (historyczny High Starter). Każdy przyszły tag Twilight ma być nowym, jawnie zatwierdzonym wydaniem **tylko do wskazanego zakresu oceny**, o ile OWNER podejmie taką decyzję. W żadnym wypadku obecny Etap VI nie daje produkcyjnego GO.
