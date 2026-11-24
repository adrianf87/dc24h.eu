# Beta 0.7.9–0.7.12 — Etap III: spójne kopie i odtwarzanie, pakiet `.deb`, rollback

Zakres: Etap III [#238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238), gałąź `beta-0.7.x-stage-full`, ten sam roboczy PR #239. To **próba na izolowanych danych testowych**, a nie gotowość produkcyjna. Nie instaluj pakietu CI na serwerze operatora; nie przeprowadzaj odtwarzania na żywej bazie ani katalogu bramki.

## 0.7.9 — punkt spójności i sześć składników

1. Zatrzymaj wszystkie procesy mogące zapisywać do MariaDB i katalogu WebAdmin. Blokada odczytu MariaDB **nie zatrzymuje** zapisów do plików.
2. Na zatwierdzonym, odizolowanym hoście utrzymaj blokadę odczytu MariaDB przez cały eksport SQL (`mariadb-dump`) i kopię prywatnych plików.
3. Skopiuj **razem** bazę ze schematem v11 i komplet: `mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json`, `rate-limits.json`. Wszystkie pliki i katalogi zawierające kopie muszą należeć do wykonującego użytkownika i nie udostępniać danych grupie ani innym użytkownikom; nie akceptuj dowiązań symbolicznych ani częściowych zapisów.
4. Uruchom `scripts/stage4_private_recovery_drill.py snapshot` na świeżym katalogu, następnie `scripts/stage1_recovery_bundle.py pair` z **dokładnym SHA commita**. Manifest `recovery-pair.json` wiąże skróty SHA-256 eksportu SQL i prywatnego manifestu; manifest prywatny wiąże pięć plików. Nie modyfikuj zestawu po sparowaniu.

Potwierdzenie zatrzymania zapisów pozostaje odpowiedzialnością operatora: sam manifest nie dowodzi wspólnej generacji danych. Nie publikuj eksportu ani plików prywatnych jako artefaktów CI.

## 0.7.10 — preflight przed zmianami

Po zatrzymaniu procesów, na **nowych, pustych celach** wykonaj:

```sh
python3 scripts/beta07_recovery_preflight.py \
  --mariadb-dump /secure/offline/state.sql \
  --private-snapshot /secure/offline/snapshot \
  --pair-directory /secure/offline/pair \
  --commit-sha EXACT_40_HEX_SOURCE_SHA \
  --fresh-sql-destination /secure/rehearsal/new-mariadb-data \
  --fresh-private-destination /secure/rehearsal/new-webadmin-state \
  --ack-isolated
```

Preflight **nie dokonuje żadnego importu ani przywrócenia**. Odmawia w razie błędnego SHA, niezgodnych sum, brakujących lub dodatkowych plików, niepoprawnej struktury MFA/passkey/replay/rate-limit, przerwanego łańcucha audytu, niekompletnych 11 wpisów migracji, niedozwolonych uprawnień, podmienionego pliku SQL i istniejącego/nakładającego się celu. Wynik nie ujawnia SQL, kont, sekretów ani lokalnych ścieżek. Status `schema_v11_verified_after_import=false` jest **zamierzony**: konieczny jest jeszcze osobny import na świeżej MariaDB i kontrola `--database-status` z binarki C++.

Odtwarzanie kopiuj tylko do nowo utworzonych, odizolowanych lokalizacji: SQL do świeżej instancji MariaDB, pięć plików przez `stage4_private_recovery_drill.py restore`. Przed udostępnieniem usług sprawdź schemat v11, konta i uprawnienia C++, odwołane passkeys, epoch, replay, łańcuch audytu i limity prób. Przy odmowie **zachowaj stare cele bez zmian**; nigdy nie „naprawiaj” kopii pustymi plikami.

## 0.7.11–0.7.12 — próbny dpkg i rollback

- `scripts/beta07_ci_deb.py` tworzy wyłącznie prywatny pakiet `dc24h-high-starter-ci-fixture` na `amd64`. Pakiet posiada binarkę C++, plik pochodzenia i nieaktywną jednostkę systemd z warunkiem niemożliwym do spełnienia w zatwierdzonym teście. Nie ma skryptów instalacyjnych w **zwykłej** wersji, nie włącza ani nie uruchamia usługi, nie zawiera konfiguracji produkcyjnej.
- Od Etapu V nazwy `0.5.16+ci1` i `0.7.20+ci1` odzwierciedlają historyczną i bieżącą binarkę **użytą w teście**, bez tworzenia pakietu produkcyjnego. Wersja `0.7.20+ci2` jest tylko celowo wadliwym, poprawnym archiwum z `preinst` kończącym się kodem 41. Sprawdza odmowę **przed rozpakowaniem**, nie wszystkie możliwe awarie `postinst` lub migracji. W czasie pierwotnej pracy nad Etapem III bieżąca binarka raportowała jeszcze beta-0.6.16.
- Na jednorazowym GitHub-hosted Ubuntu 22.04/24.04/26.04 test instaluje poprzedni pakiet, aktualizuje do bieżącego, odrzuca uszkodzony plik `.deb`, wywołuje kontrolowany błąd `preinst`, potwierdza nienaruszoną binarkę i stan dpkg, a następnie rzeczywiście instaluje poprzednią wersję. Po przejściu używa sparowanego SQL v11 i pięciu plików w nowej bazie i nowym katalogu; porównuje uprawnienia, kredencjały, cofnięcia i utrzymanie danych po restarcie. Pakiet testowy zostaje usunięty.
- Ta próba **nie dowodzi** kompatybilności produkcyjnej instalacji ani odtworzenia pod obciążeniem. Osiągnięcie parametrów RPO/RTO i pełny test operacyjny wymagają osobnego zatwierdzenia.

## Kryterium zamknięcia

Etap wymaga zielonych wyników na dokładnym końcowym SHA w trzech profilach GitHub-hosted Ubuntu. Zgodnie z instrukcją OWNER po wypchnięciu commita i uruchomieniu CI **kończymy pracę, nie czekając na wyniki**. `CI pending` nie oznacza zaliczenia. PR pozostaje `draft`, bez merge, wdrożenia i dotykania usług produkcyjnych.
