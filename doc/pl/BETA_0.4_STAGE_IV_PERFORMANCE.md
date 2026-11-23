# Beta 0.4.14 — pomiary wydajności w Etapie IV

To instrukcja **ręcznych pomiarów na odizolowanej instalacji**, nie deklaracja przepustowości produkcyjnej. Brak dostępu do serwera docelowego Debian 13 Trixie (Intel Xeon D-1521, 8 wątków sprzętowych, 31 997 MiB RAM). Wszystkie rzeczywiste pomiary są oznaczone jako **NOT MEASURED**.

## Warunki pomiaru

1. Przygotuj osobną, usuwalną instalację Debian 13 z testową MariaDB, kontami testowymi, ADC/ADCS dostępnymi tylko po loopback oraz prywatnym Apache2 HTTPS. Nie używaj kont ani danych produkcyjnych. Nie udostępniaj WebAdmin publicznie.
2. Zapisz dokładny SHA, wersje systemu, jądra, kompilatora, C++/Node, MariaDB/Apache2, certyfikat CA, częstotliwość CPU, RAM, limity procesów, porty i parametry obciążenia. Ustal progi akceptacji **przed** wykonaniem testów.
3. Wykonaj pomiar bezczynności, a następnie osobne, powtarzalne profile. Obserwuj CPU/RSS, obciążenie MariaDB i błędy. Narzędzie odczytuje jedynie RSS przed/po z `/proc/PID/status`, nie maksymalne RSS i nie pełny monitoring hosta.
4. Dla prywatnego API przygotuj osobny plik sesji o trybie `0600`, zawierający wyłącznie `dc24h_remote=<64 małe znaki szesnastkowe>`. Nigdy nie umieszczaj pliku, tokenu, ciasteczka ani prywatnej odpowiedzi w raporcie/CI. Po pomiarze usuń dane sesji.

Przykładowe polecenia na **odizolowanym serwerze docelowym**, z zatwierdzonym certyfikatem CA i rzeczywistymi lokalnymi portami:

```sh
python3 scripts/stage4_perf_probe.py --profile adc --port 411 --requests 100 --rate 5 --concurrency 2 --ack-isolated
python3 scripts/stage4_perf_probe.py --profile adcs --port 1511 --ca-file /private/localhost-ca.pem --requests 100 --rate 5 --concurrency 2 --ack-isolated
python3 scripts/stage4_perf_probe.py --profile webadmin --port 8443 --ca-file /private/localhost-ca.pem --requests 100 --rate 5 --concurrency 2 --ack-isolated
python3 scripts/stage4_perf_probe.py --profile api --port 8443 --ca-file /private/localhost-ca.pem --cookie-file /private/session-cookie --requests 50 --rate 2 --concurrency 2 --ack-isolated
```

Narzędzie dopuszcza wyłącznie `localhost`/`127.0.0.1`, wymaga `--ack-isolated`, ogranicza liczbę prób do 1000, tempo do 20/s, współbieżność do 8 i timeout do 5 s. ADC/ADCS wysyła wyłącznie początkowe `HSUP ADBASE ADTIGR` i wymaga poprawnej, kompletnej odpowiedzi `ISUP` z `ADBASE` (komunikat błędu ADC nie jest sukcesem); HTTPS pobiera stronę `/remote.html` o typie HTML, a API tylko autoryzowany, **odczytowy** `/remote/v1/summary` ze schematem JSON 1. Odmowa lub przeterminowana sesja to błąd, nie sukces. Walidacji TLS nie wolno wyłączać przez `--insecure`.

JSON wynikowy zawiera opóźnienia p50/p95/p99, liczbę sukcesów/odmów, rzeczywistą przepustowość i opcjonalne RSS. Zadany limit tempa rozpoczęcia żądań nie jest rzeczywistą przepustowością. Zapisz wyniki i obserwacje hosta bez sekretów, a następnie poproś drugiego operatora o niezależny przegląd.

| Profil | Stan |
| --- | --- |
| ADC oraz ADCS | **NOT MEASURED** |
| HTTPS WebAdmin i prywatne API | **NOT MEASURED** |
| CPU, RAM/RSS, MariaDB, wskaźnik błędów | **NOT MEASURED** |
| Progi wydajności i dopuszczalna pojemność | **NOT MEASURED** — decyzja operatora wymagana |

Testy jednostkowe narzędzia i zielone GitHub CI **nie zastępują** tych rzeczywistych pomiarów. Dokument źródłowy: [EN/US](../us/BETA_0.4_STAGE_IV_PERFORMANCE.md). Decyzja wydania: [Etap IV](BETA_0.4_STAGE_IV.md).
