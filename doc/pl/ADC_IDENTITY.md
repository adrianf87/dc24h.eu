# CID/PID — alpha-0.2.7

## Zakres

Podstawowe sprawdzanie związku Tiger powstało w [INF](ADC_INF.md) w alpha-0.2.6. Ten etap wydziela je do `adc/tiger_identity.hpp`, dodaje typy publiczne, bezpośrednie bufory, porównanie binarne i osobne testy błędów. INF używa wspólnego weryfikatora i zachowuje wynik publiczny. Semantyka akceptowanych kanonicznych par pozostaje zgodna.

Źródła: [identyfikacja/hash sesji ADC](https://adc.sourceforge.io/ADC.html), [TIGR](https://adc.sourceforge.io/ADC-EXT.html) i [cykl hash Libgcrypt](https://www.gnupg.org/documentation/manuals/gcrypt/Working-with-hash-algorithms.html). CID jest Tiger/192 z **odkodowanego PID 24-bajtowego**, nie jego 39-znakowego zapisu. Nadal używany jest GCRY_MD_TIGER1/kolejność NESSIE. Nie używa się Tiger2, historycznej kolejności GCRY_MD_TIGER, prefiksu liścia TTH ani drzewa pliku.

Moduł sprawdza samą parę, nie własność konta, globalną unikalność CID/nicku, uprawnienia i hasło. Nie generuje PID i nie podłącza się do rzeczywistego odbiorczego Network. Rejestr/dopuszczenie, ograniczone wyjście i pełne logowanie pozostają osobnymi etapami.

## Typy i wejście kanoniczne

`TigerCid::parse(text)` zwraca optional TigerCid dla dokładnie 39 wielkich znaków base32 bez paddingu. Trzy nieużyte bity końca muszą być zerowe: ostatnim znakiem może być tylko A/I/Q/Y. Odrzucane są małe litery, odstępy, padding, NUL, znaki spoza ASCII, zła długość i niezerowe bity końca. Każde 24 bajty, także zero, mają poprawne kodowanie; zgodność formatu nie dowodzi związku PID. `text()` daje własny string; `bytes()` pożycza publiczne 24 bajty na czas życia obiektu.

`verify_tiger_identity(cid, pid, hash)` zwraca variant<VerifiedIdentity, IdentityError>. Akceptuje tylko wynegocjowany SessionHash::tiger192; inna wartość enum jest odrzucana przed parsowaniem/hash, bez fallback. Sprawdzany jest CID, następnie format PID, dostawca/kontekst i porównanie. Błędna para nie tworzy obiektu sukcesu.

VerifiedIdentity nie ma domyślnego/publicznego konstruktora wartości i zawiera wyłącznie TigerCid. Można go bezpiecznie kopiować/przenosić jako publiczne dane po weryfikacji. Nie zapisuje PID, widoku wejścia, wskaźnika sesji ani roli. Oznacza jedynie, że moduł poprawnie sprawdził podaną parę w danym momencie. Nie jest przenoszalnym tokenem autoryzacji. Zmiana oryginalnych stringów nie zmienia CID wyniku.

Stałe IdentityError: invalid_cid, invalid_pid, mismatch, unsupported_hash, crypto_unavailable. `identity_status_code` proponuje odpowiednio 243, 227, 227, 247 i 210 przyszłemu dispatcherowi; nie wysyła odpowiedzi/disconnect i nie zwraca wejścia. Błędny format jest odrzucany przed utworzeniem kontekstu. Kontrola długości ogranicza nadmiar przed dekodowaniem; weryfikator przetwarza tylko 24 bajty PID.

## Prywatne bufory i dostawca

Weryfikator pożycza tekst i używa dwóch lokalnych 24-bajtowych buforów bez kopiowania/przenoszenia: odkodowanego PID i obliczonego skrótu. `decode_tiger_base32(text, destination)` zapisuje od razu bufor, bez prywatnego optional zwracanego przez wartość w tej ścieżce. Każdy błąd dekodowania czyści cały cel, także częściowy wynik. Cel nie może nakładać się na tekst wejścia.

`tiger192_into(bytes, destination)` oblicza hash do bufora właściciela. Sprawdza algorytm, otwiera posiadany kontekst Libgcrypt, finalizuje, kopiuje wynik i zamyka kontekst po każdym udanym otwarciu. Brak algorytmu/błąd otwarcia/odczytu czyści cel i zwraca false. Poprawne obliczanie dopuszcza alias wejścia/wyjścia, bo wejście jest zużyte przed zapisem wyniku. Brak zastępczego algorytmu i zmian globalnych trybów/FIPS.

Końcowe porównanie obejmuje dokładnie 24 bajty przez OpenSSL CRYPTO_memcmp. Używa stałoczasowej funkcji biblioteki; walidacja formatu, inicjalizacja, dostawca, kolejność błędów i cały handshake **nie** są deklarowane jako stałoczasowe. Etap nie zawiera benchmarku czasowego ani pełnego audytu kanałów bocznych.

Destruktory czyszczą oba prywatne bufory po każdym zwykłym powrocie, także mismatch i błędzie dostawcy. Zamknięcie kontekstu Libgcrypt sprząta jego stan. Dotyczy to własnych buforów/cyklu kontekstu: nie obiecujemy wymazania Message/stringów dekodera właściciela, rejestrów, zrzutów i dowolnych kopii kompilatora/bibliotek. Oryginalny zakodowany PID nadal należy do wywołującego; nie wolno go logować/przekazywać.

Funkcje hash/dekodowania optional z alpha-0.2.6 pozostają dostępne i używają wersji bezpośrednich. To ogólne narzędzia bajtowe; wywołujący odpowiada za zwrócone tablice i ich czas życia. PID sprawdzać dedykowanym weryfikatorem. Brak nowej zależności poza Libgcrypt/OpenSSL.

## Integracja INF

InfValidator używa TigerCid dla formatu ID i verify_tiger_identity dla pierwszego związku. Poprawny ClientInfo zachowuje opcjonalny VerifiedIdentity obok dotychczasowego ID; `identity()` zwraca własną opcjonalną kopię. Opublikowane poprawne snapshoty zawierają wynik. Zwykłe aktualizacje go zachowują; stały ID i zakaz kolejnego PD uniemożliwiają zmianę tożsamości w miejscu. Nowe połączenie wymaga własnej weryfikacji początkowej.

Budżet 4096 bajtów nazw/wartości ClientInfo jest bez zmian; wynik dodaje stałe 24 bajty plus narzut optional/kontenera, nigdy prywatne dane. `public_message` nadal eksportuje tylko znane poprawne pola, bez PD i dodatkowego pola wyniku. Publiczny wynik nie omija dopuszczenia/hasła ClientSession. Błąd sprawdzania/dostawcy nie zmienia snapshotu, stanu ani rezerwacji SID.

Brak zmian HTTP, SQL, TLS, INI i katalogów komunikatów. Rzeczywisty Network nadal używa samego parsera; sukces fixture nie oznacza logowania huba.

## Weryfikacja

`adc.identity` sprawdza referencyjne bajty empty/abc i alias wyjścia, wszystkie zamiany na bajty spoza alfabetu w każdej pozycji, 32 możliwości ostatniego znaku, złe długości, czyszczenie błędnego dekodowania, 1000 deterministycznych par i zmianę jednego bitu PID. Obejmuje własność wyniku, błędy/statusy, nieobsługiwany hash, zachowanie wyniku INF i stały CID. To funkcjonalne testy deterministyczne, nie kampania fuzzingu.

`adc.identity.failure` linkuje osobny program z wrapperami GNU-compatible --wrap dla gcry_md_algo_info, gcry_md_open i gcry_md_read. Wstrzykuje brak algorytmu oraz błędy otwarcia/odczytu, sprawdza wyzerowane wyjście, brak zatwierdzenia tożsamości/INF, utrzymany oczekujący SID, odzyskanie po usunięciu awarii i sprzątanie. Wrappery dotyczą tylko tego programu; produkcja nie ma wstrzykiwanego dostawcy, preload i zmian hosta. Błąd inicjalizacji wersji i dowolne awarie alokacji pozostają nietestowanymi gałęziami.

`adc.identity.ncdc` uruchamia prawdziwy ncdc z tymczasowym TCP loopback i TLS Pythona. Sterownik C++ wykonuje modułowe SUP/SID, odrzuca zmieniony jednym bitem kanoniczny CID bez zmiany IDENTIFY, następnie przyjmuje prawdziwą parę, sprawdza typowany wynik i eksport bez PD. Celowo pozostaje w niezatwierdzonym IDENTIFY i zwalnia SID po disconnect. Rzeczywiste testy transportu C++ TCP/OpenSSL i fixture INF/SID/SUP są osobne. Zobacz [TESTING.md](TESTING.md).
