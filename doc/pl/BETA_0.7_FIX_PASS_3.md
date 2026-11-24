# Beta 0.7.x — Fix Pass 3: kopie offline i rollback pakietu (0.7.9–0.7.12)

**Status:** przeprowadzono przegląd Etapu III i lokalne testy regresyjne. Pełne CI na nowym SHA wymaga osobnej weryfikacji. **Brak akceptacji; NO-GO dla merge, wydania i wdrożenia.** Wyłącznie Fix Pass 3, wspólna gałąź `beta-0.7.x-stage-full`, szkic PR #239, Issue #238.

## Potwierdzone błędy i naprawy

1. Sprawdzenie uprawnień katalogu dotąd wykonywało `lstat` tylko na katalogu końcowym. Ścieżka mogła zawierać dowiązanie symboliczne w katalogu nadrzędnym i nadal przejść kontrolę. Teraz każdy składnik ścieżki jest sprawdzany przez `lstat`; symlink lub element niebędący katalogiem powoduje odmowę **bez utworzenia celu odtwarzania**.
2. Podczas odczytu pliku prywatnego kontrolowany był tylko jego rozmiar po operacji. Równoczesna podmiana zawartości o tej samej długości lub ścieżki do nowego pliku mogła pozostać niewykryta na poziomie pojedynczego odczytu. Dodano porównanie rozmiaru, czasów modyfikacji/zmiany oraz pary urządzenie/inode. Nadal obowiązują sumy manifestu i całej pary SQL + pięć plików. **Zatrzymanie wszystkich zapisów jest konieczne**; testy plików nie zapewniają atomowości działającej usługi.

Nowe przypadki negatywne odrzucają symlink w ścieżce źródła, symlink w katalogu nadrzędnym nowego celu oraz modyfikację pliku o tym samym rozmiarze podczas odczytu.

## Wyniki przeglądu czterech patchy

| Patch | Sprawdzono | Wynik / ograniczenie |
| --- | --- | --- |
| **0.7.9** | Manifest pary SQL v11 + pięć plików, sumy i uprawnienia, zmiany w trakcie kopiowania | **Naprawiono symlinki w ścieżce i odczyt modyfikowanego pliku.** Sam hash nie dowodzi, że procesy zapisu były zatrzymane |
| **0.7.10** | Read-only preflight, puste cele, brak/niespójność plików, wersja migracji, odmowy bez ujawniania treści | **Dodano odmowę celu z pośrednim symlinkiem.** Po imporcie na świeżą MariaDB wymagana jest kontrola `--database-status` z C++ |
| **0.7.11** | Testowy `dc24h-high-starter-ci-fixture`, pochodzenie binarki, nieaktywna jednostka systemd, odmowa nadpisania | Nie potwierdzono kolejnego błędu lokalnie; pakiet nie służy do wdrożenia |
| **0.7.12** | Poprzednia/bieżąca binarka, uszkodzony `.deb`, błąd `preinst`, stan `dpkg`, ponowna instalacja starszej wersji i świeże odtworzenie | Wymaga uruchomienia na jednorazowym Ubuntu CI; nie obejmuje żywej usługi ani wszystkich awarii `postinst` |

## Kontrola końcowa

Lokalne testy: `stage4_private_recovery_tests.py`, `beta07_recovery_preflight_tests.py`, `stage1_recovery_bundle_tests.py`, `beta07_ci_deb_tests.py`, a także `git diff --check`. Pełne CI musi objąć ten sam końcowy SHA na **GitHub-hosted Ubuntu 22.04/24.04/26.04**: C++, WebAdmin/Chromium, CodeQL, ASan/UBSan, systemd, ADC/ADCS/TLS, MariaDB/odtwarzanie, `dpkg` i benchmarki. Wynik `pending` lub błąd **blokuje akceptację**, nie zakończenie pracy po uruchomieniu CI.

Nie udostępniaj SQL, prywatnych plików ani pakietu CI, nie stosuj na produkcji, nie zmieniaj `main` ani statusu PR na gotowy do merge. Po push **kończymy bez oczekiwania na CI**. Fix Pass 4 wymaga osobnego polecenia OWNER.
