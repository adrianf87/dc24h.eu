# Connection Security — alpha-0.6.8

**Weryfikacja lokalna alpha-0.6.8 (2026-11-14):** Przeszły wszystkie 159/159 testów CTest w Debug, Release i ASan/UBSan, w tym odmowy aktywnego/pasywnego ncdc i wcześniejsze poprawne handshake klientów zwykłych/TLS. Końcowe kompilacje nie mają ostrzeżeń po poprawce pomocnika alokacji testu statystyk; ten test ponownie przeszedł w trzech trybach. Poprawne są build/typecheck WebAdmin, 12 testów Node, 10 testów Chromium i integracja API C++. Przeszły kontrole metadanych, UTF-8, zgodności języków/linków, składni zmienionych plików Python, przykładowego INI i białych znaków. Nie oczekiwano na GitHub Actions; pełna macierz wspieranych systemów i obciążenie produkcyjne pozostają niesprawdzone.

Etap utworzony bezpośrednio z main `236b58877a317c19bcb1574c2b4545d3091c44fe` po Transfer Tokens. Dodaje jawne uprawnienia połączeń, zaufane nadania związane z życiem sesji i ponowną autoryzację oraz ujednolica/sprawdza walidację adresu i portu. Gałąź numerowana jest publikowana osobno od main, z zachowaniem kodu i historii.

## Granice IP, portu i SID

Dotychczasowy adapter ADC INF wiąże liczbowe I4/I6 z adresem klienta otrzymanym od jądra przy logowaniu i aktualizacjach. I40.0.0.0 / I6:: zastępuje znormalizowanym adresem klienta. Obcy adres lub inna rodzina są odrzucane; uprawnienia transferów nie omijają tej kontroli. Nie dodano DNS, nadpisywania adresu, sondowania NAT ani gniazda klientów otwieranego przez hub. [Specyfikacja ADC](https://adc.sourceforge.io/ADC.html), rozdziały 5.3.4 i 5.3.8–5.3.9, opisuje kontrolę własności IP i koordynację aktywną/pasywną.

`users::usable_address(AddressBytes, AddressFamily)` zawiera teraz wspólną regułę binarnego adresu nasłuchu bez alokacji, wcześniej osadzoną w public_address. Adres nieokreślony, multicast, ograniczony broadcast IPv4, mapowany IPv6 i link-local IPv6 wymagający zakresu nie dają nasłuchu. IPv4 wymaga czterech bajtów adresu i dwunastu zer dopełnienia. Adresy prywatne/LAN i loopback pozostają poprawne; nieznane rodziny są odrzucane. To kontrola struktury nasłuchu, nie uniwersalna polityka globalnej routowalności/wyjścia ani potwierdzenie osiągalności. Broadcastu podsieci nie da się rozpoznać bez jej prefiksu.

`public_address` używa tej reguły bez zmiany przyjmowanych profili i zachowania Search. `ClientCapabilities::listener` również ją stosuje, więc ręcznie zbudowana natywna migawka nie wprowadzi nieokreślonego ani niekanonicznie dopełnionego adresu.

`transfers::valid_connection_port(int64_t)` jest wspólny dla Request i ADC CTM. Port nasłuchu klienta musi mieścić się w 1..65535. Parser odrzuca błędną składnię i przepełnienie liczby 64-bitowej ze znakiem; kodek semantyczny odrzuca zero, wartości ujemne i za duże. Poprawny zapis 00001 pozostaje bez zmian. Reguła nie zmienia portu 0 nasłuchu huba (port tymczasowy) ani portu UDP 0 (wyłączenie). RCM nadal nie zawiera portu.

Adapter ADC wymaga poprawnych czteroznakowych SID base32, sprawdza własność źródła/stan i wybiera dokładne życie gotowego celu. Brak, zamykanie lub oczekiwanie na powitanie celu, połączenie do siebie i stare plany pozostają odrzucane. Token, publiczne CT/SU/AP i deklaracja operatora nie wybierają odbiorcy ani nie nadają uprawnień. Niezależny od protokołu Core zachowuje wcześniejsze ograniczone ogólne klucze sesji; składnia SID ADC pozostaje w adapterze.

## Model uprawnień

`transfers/security.hpp` definiuje `Permissions { initiate, receive }`, `SecuritySettings { ctm_enabled, rcm_enabled, guests }`, typowany PermissionError i authorize bez alokacji.

| Warunek | Znaczenie |
| --- | --- |
| `ctm_enabled` | Globalne dopuszczenie CTM, również odpowiedzi na RCM |
| `rcm_enabled` | Globalne dopuszczenie RCM |
| `initiate` źródła | Może wysłać to żądanie koordynacji |
| `receive` celu | Może otrzymać to żądanie koordynacji |

Wszystkie przełączniki domyślnie mają wartość true, zachowując wcześniejsze zachowanie gości. Nadanie sesyjne nie włącza globalnie wyłączonego polecenia. Brak uprawnień źródła lub celu blokuje oryginał i początkowe echo E; jedynie źródło dostaje ogólne odwracalne ISTA 125 z poleceniem w FC. Opis nie ujawnia protokołu, portu, tokena, SID ani IP. Dotychczasowe kontrole źródła/stanu/ramki/adresu zachowują pierwszeństwo.

Uprawnienie dotyczy kierunku jednego żądania, nie roli klienta/serwera gniazda: pasywne źródło RCM wymaga initiate, aktywny cel receive. Odpowiedź CTM to nowe żądanie wymagające odpowiednich uprawnień nowego źródła i celu. Receive źródła nie blokuje echa jego własnego przyjętego żądania.

## Konfiguracja i zaufane nadania

```ini
[transfers]
ctm_enabled = on
rcm_enabled = on
guest_initiate = on
guest_receive = on
```

Dozwolone są wyłącznie małe on/off. Nieznane klucze/sekcje, duplikaty i błędne wartości odrzuca kontrola konfiguracji. Ustawienia przechodzą z Configuration przez Network Options i LoginLimits do Transfer Core. Działają od startu: zmiana wymaga restartu. Nie dodano przełącznika CLI, endpointu HTTP ani kontrolki WebAdmin. Schemat API stanu nie zawiera nowych ustawień/liczników transferów.

`LoginHub::set_transfer_permissions(lifetime_id, Permissions)` jest zaufanym API wątku właściciela dla istniejącego gotowego połączenia. Zwraca false dla nieistniejących, negocjujących, zamykanych i zakończonych ID. Nadania są w Peer, nie w publicznym INF, CID/SID ani globalnej mapie. Rozłączenie je usuwa, a nowe połączenie otrzymuje domyślne uprawnienia gościa. Role kont, trwałe nadania, administracja przeglądarkowa i uprawnienia SQL pozostają przyszłymi pracami.

Natywny `Core(sessions, settings, authority)` przyjmuje pożyczony PermissionSource. Musi on żyć dłużej niż Core i zapewniać odczyty bez alokacji i wyjątków dla dokładnego życia gotowej sesji. Brak wpisu oznacza odmowę. Brak authority jawnie używa uprawnień gościa po sprawdzeniu gotowości, zachowując zgodność istniejących wywołań. Core i LoginHub pozostają nieprzenoszalne. Zależne komponenty C++ wymagają przebudowania; stabilne binarne ABI nie jest obiecywane.

## Wysyłka, cofnięcie i zakres

Transfer Core sprawdza initiate źródła i receive celu przy przygotowaniu oraz ponownie tuż przed wywołaniem odbiorcy. Cofnięcie nadania pomiędzy tymi punktami zużywa/anuluje próbę bez wysyłki. Przywrócenie nadania pozwala na nowe żądanie, nie odtworzenie kopii starego planu. Nadal obowiązują kontrole adresu/aktywności i życia sesji; plan nie przechodzi po cichu do innej rodziny ani użytkownika.

Dla E najpierw podejmowana jest próba celu. Przed echem osobno sprawdzane są gotowość/własność źródła, rola adresowa, globalne dopuszczenie polecenia i aktualne initiate źródła. Cofnięcie initiate w funkcji odbiorcy celu anuluje echo. Późniejsza zmiana uprawnień celu nie cofa już przyjętego wpisu jego kolejki ani nie anuluje nadal poprawnego niezależnego echa źródła. Cofnięcie uprawnień nie usuwa ramek już zakolejkowanych i nie zamyka zestawionych połączeń klientów.

Autoryzacja nie dodaje alokacji do wysyłki. Plan zachowuje domenę i jedną próbę, ograniczenia kolejek i izolację awarii odbiorców. Uprawnienia dotyczą koordynacji CTM/RCM. Dotychczasowe statusy STA, uprawnienia Search, uwierzytelnianie, firewall/polityka wyjścia, listy IP, limity szybkości transferów, zaufanie/pinning TLS, NAT, korelacja tokenów i bajty plików są poza etapem. Nadal działają wcześniejsze ogólne limity ramek/przyjęć/wyjścia. Dane połączeń i nadania nie trafiają do logów, SQL, API/WebAdmin ani liczników Chat/Search.

## Weryfikacja

`transfers.security` sprawdza adresy LAN/loopback oraz niepoprawne bajty/dopełnienie/rodziny, skrajne porty, błędne SID, wszystkie kombinacje uprawnień, globalne wyłączenie, brak wpisu authority, domyślne nadania bez authority, cofnięcie przed wysyłką, jedną próbę po przywróceniu, cofnięcie echa w funkcji odbiorcy, niezależne echo, zaufane nadania, niezaufane metadane, gotowość i brak dziedziczenia po rozłączeniu. Autoryzacja i odmowa wysyłki są sprawdzane z wyłączonymi alokacjami C++.

`transfers.security.process` obejmuje IPv4/IPv6, mieszane ADC/ADCS, cztery przełączniki INI, oba polecenia/zakresy, prywatne odmowy, deklaracje klienta, poprawne porty z zerami i błędne/przepełnione porty, brak celu i cel własny, obcy adres INF, podrobione źródło SID, dalszą komunikację i prywatność API/logów. Przepełnienie liczby jest odrzucane przez parser bez odpowiedzi; błąd semantyczny portu daje odwracalny ogólny status. Sprawdzane są on/off i duplikaty INI na rzeczywistym programie.

Cztery scenariusze rzeczywistego ncdc (`adc.connectionsecurity.ncdc`, `adc.connectionsecurity.active.ncdc`, `adcs.connectionsecurity.ncdc`, `adcs.connectionsecurity.active.ncdc`) sprawdzają odmowę pasywnego RCM i aktywnego CTM przez oba transporty huba. Klient wyświetla odwracalny błąd, obserwator nie dostaje żądania, a czat i zamknięcie działają. Pozostają wcześniejsze poprawne handshake klientów zwykłych i TLS z weryfikacją certyfikatów.

Zobacz [Konfiguracja](CONFIGURATION.md), [Testy](TESTING.md), [Transfer Core](TRANSFER_CORE.md), [Connection Routing](CONNECTION_ROUTING.md), [Client Capabilities](CLIENT_CAPABILITIES.md) i [Transfer Tokens](TRANSFER_TOKENS.md).
