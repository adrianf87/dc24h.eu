# User Core — alpha-0.3.5

**Bieżący etap — alpha-0.3.7:** [User Registry](USER_REGISTRY.md) utrzymuje ograniczone indeksy SID/CID i odwrotny indeks właścicieli prywatnie w SessionManager. Przyjęcie jest transakcyjne między tym rejestrem, NickManager i modelem Session. Wyszukiwanie User tylko do odczytu wskazuje z obu kluczy ten sam aktualny model; rezerwacje trwają do usunięcia sesji. Starsze opisy modułów zachowują zakres historyczny.

Etap bazuje na main `ba12aa3a96ea70e2b64ca3b11a519bd862e7f2f4`, po scaleniu alpha-0.3.4 Session Manager. Rozwija publiczny model gościa z alpha-0.3.3: sprawdzony typ nicku, wspólne reguły Unicode i jawną projekcję ADC. Manager nadal posiada zarejestrowane sesje i indeksy unikalności. Main i historyczne gałęzie są zachowane; ta gałąź nie scala się automatycznie.

## Tożsamość i sprawdzone nazwy

Session opisuje jeden czas życia transportu, a User — Identity i Profile przyjętego gościa. Session posiada opcjonalny User, nieobecny przed przyjęciem. SessionManager posiada zarejestrowane Session oraz indeksy SID/CID/nick. Odczyt modeli jest const; zmiany przechodzą przez bramki managera. User/Identity/Profile są samodzielnymi kopiowalnymi wartościami, nie kontami.

Identity posiada ograniczone teksty SID/CID i wartość `users::Nickname`. Limity SID/CID pozostają 64/128 bajtów, wartości niepuste i bez NUL. Ich ogólna fabryka nie dowodzi składni ADC, własności dzierżawy, dowodu Tiger, konta ani unikalności. Adapter dostarcza rzeczywisty czteroznakowy Sid i kanoniczny publiczny CID z ClientInfo::VerifiedIdentity. SID jest ponownie używalną dzierżawą protokołu; SessionId rozróżnia czasy życia. CID nie uwierzytelnia konta.

Nickname nie ma domyślnego ani publicznego konstruktora bez sprawdzenia. `Nickname::create` wymaga niepustego tekstu do 256 bajtów UTF-8, już w NFC, bez kontrolek/znaków formatu i separatorów wiersza/akapitu. Spacja ASCII jest zabroniona w nazwie, zgodnie z dotychczasową polityką tekstową ADC. Nie ma cichej normalizacji, ignorowania wielkości liter ani porównań zależnych od locale. Ogólny limit 256 bajtów jest szerszy niż niezmieniony limit nicku ADC — 64 bajty; sama fabryka modelu nie spełnia wszystkich zasad ADC. Znaki nie-ASCII podlegają dotychczasowym kategoriom Unicode, bez rozszerzania zakazu na wszystkie białe znaki.

`Identity::create(sid,cid,text)` używa tego typu. Wariant z Nickname zachowuje ograniczenia SID/CID bez ponownej walidacji nazwy. `nickname_value()` zwraca const Nickname, a `nickname()` zachowuje dotychczasowy dostęp const do tekstu. Istnieje jedna własna kopia nicku. `Nickname::valid()` tanio sprawdza obecność/limit skonstruowanej lub przeniesionej wartości, bez ponownej alokacji Unicode. Dostęp tylko do odczytu uniemożliwia ominięcie fabryki przez obsługiwane API.

## Wspólny tekst i zależność budowania

`dc24h_text` udostępnia `text::valid_public_text(value, nickname)` z istniejącą zależnością utf8proc. Wydziela wcześniejsze sprawdzenia UTF-8/NFC/kategorii z walidatora INF; ADC i Users korzystają z tej samej funkcji. Puste fragmenty są poprawne dla opcjonalnych pól profilu, a Nickname osobno wymaga obecności. Odrzucane są błędne/nadmiarowe kodowania UTF-8, surogaty, tekst poza NFC, kontrolki ASCII/C1, znaki formatu i separatory wiersza/akapitu. Limit wejścia wynosi 4096 bajtów przed alokacją normalizacji. Najpierw sprawdzane są limity konkretnego pola.

Fabryki Profile stosują teraz te reguły do każdego publicznego pola tekstowego, zamykając wcześniejszą lukę bezpośredniego modelu, który mógł przyjąć niepoprawne dane nie-ASCII po samym sprawdzeniu bajtów/kontrolek. Polityka protokołu się nie zmienia: ADC już sprawdzało te zasady. Błędne wartości zwracają nullopt; błąd alokacji propaguje wyjątek przed zmianą modelu, bez wymuszonego zakończenia procesu ani udawanego wyniku walidacji. Pamięć normalizacji ogranicza limit wejścia i przetwarzanie biblioteki; nie zadeklarowano dokładnego budżetu bajtów ani audytu wymuszania awarii.

Users zależy od neutralnej biblioteki tekstu, prywatnie połączonej z utf8proc. ADC używa tej samej biblioteki. Nie ma zależności Users od ADC, TLS, kryptografii, gniazd, SQL ani logowania. To nowa zależność wewnętrznych celów, bez nowego pakietu systemowego; dotychczasowy `libutf8proc-dev` pozostaje wymagany.

## Profil i zmiany wartości

ProfileData zachowuje jawną listę pól:

| Pole | Projekcja ADC | Limit / znaczenie |
| --- | --- | --- |
| description | DE | 2048 bajtów |
| email | EM | 254 bajty, publiczna deklaracja zamiast potwierdzonych danych kontaktowych |
| application / version | AP / VE | 128 bajtów każde |
| locale | LO | 64 bajty, deklaracja zamiast wyboru języka serwera |
| shared_bytes / shared_files / slots | SS / SF / SL | Opcjonalne nieujemne int64, także zero i INT64_MAX |
| features | SU | Do 128 unikalnych czteroznakowych funkcji wielkich liter/cyfr |

Tekst ma poprawne NFC UTF-8 bez zabronionych kategorii; zwykłe spacje pozostają dozwolone w profilu. Pusty tekst/lista i brak liczby oznaczają brak deklaracji, odróżniony od zera. Łączny limit tekstu/tokenów 4096 bajtów oraz limity pól pozostają bez zmian. Deklaracje zasobów, email i funkcji nie są zweryfikowanymi faktami.

`User::with_nickname(Nickname)` tworzy samodzielnego kandydata, zachowując SID, CID i cały profil. Nie zmienia źródła ani nie rezerwuje nazwy. Samodzielne rename w Session i zarządzane rename w SessionManager korzystają z tej ścieżki. Nie potrzebują uprzywilejowanego dostępu do prywatnych pól User/Identity. Żywa zmiana nadal wymaga fazy/czasu, niezmiennych SID/CID i unikalności managera. Błędny tekst lub zajęty nick pozostawia User i indeksy bez zmian. Istniejące identify(User), update_user i przyjęcie Identity pozostają dostępne.

Snapshot kopiuje tożsamość/profil z tego samego User i przetrwa aktualizacje, zamknięcie oraz usunięcie źródła. Pożyczone referencje do modelu/profilu/nazwy dotyczą wyłącznie wątku właściciela i wygasają przy zastąpieniu lub zniszczeniu. Nie dodano rewizji użytkownika, publikacji między wątkami, trwałego konta ani endpointu HTTP profilu.

## Jawna projekcja zwalidowanego ADC

`adc::user_from_info(const ClientInfo&, Sid)` jest publicznym adapterem w adc/user.hpp/cpp. Przyjmuje typ już zwalidowanych metadanych, pobiera CID z VerifiedIdentity i niezerowy typowany Sid od wywołującego, tworzy sprawdzony nick oraz projektuje tylko wymienione pola. Zwraca opcjonalny User; brak tożsamości/nazwy, zerowy SID lub niespełnione ograniczenia modelu powodują odrzucenie. Nie liczy ponownie PID, nie odczytuje poświadczeń, nie przydziela dzierżawy SID ani nie rozstrzyga unikalności/gotowości.

LoginHub używa adaptera po zbudowaniu ograniczonego powitania i po zaakceptowanych scalonych aktualizacjach INF. Pominięte pola zachowują scalone wartości, jawne kasowanie je usuwa. Adapter pożycza ClientInfo i zwraca samodzielne wartości. PID/PD, PAS/hasło, adres IP, I4/I6, porty, dowolne rozszerzenia, RF/referrer, uprawnienia CT, konto/role i bany nie trafiają do User/Profile. ClientInfo zachowuje zwalidowane metadane protokołu potrzebne do dotychczasowego przekazywania; serializacja i kody statusów są niezmienione.

Nie potrzeba migracji SQL, pól JSON API, konfiguracji ani nowych komunikatów runtime. Istniejące katalogi US/PL są aktualne. Uwierzytelnianie kont, wiele sesji konta, role, bany, tożsamość trwała/rozproszona, czat/wyszukiwanie/transfery i mutacje administracyjne pozostają osobnymi etapami.

## Weryfikacja

`users.values` sprawdza typowaną konstrukcję, błędne/nadmiarowe/ucięte UTF-8, surogaty, niekanoniczną normalizację, NUL/kontrolki/format/separatory w nazwach i każdym polu tekstu profilu, granice bajtów UTF-8, własność kandydata zmiany nicku, odrzucanie bez zmian modelu/indeksów, projekcję zweryfikowanego CID/SID oraz kasowanie pól. Sprawdza też zachowanie ostrzejszego limitu nicku ADC — 64 bajty. Dotychczasowe testy managera/modelu/tożsamości pozostają wymagane.

Rzeczywisty ncdc loguje się i łączy ponownie jako `dc24hŻółw` w ADC i ADCS. Test tekstowego transportu sprawdza publiczne INF; oba transporty osiągają NORMAL, zachowują pojedynczą gotowość/sprzątanie każdego połączenia i nie ujawniają nicku w logach. ADCS pozostaje szyfrowane. Zobacz [testowanie](TESTING.md), [Session Core](SESSION_CORE.md), [Session Manager](SESSION_MANAGER.md), [INF](ADC_INF.md) i [dowód tożsamości](ADC_IDENTITY.md). Testy lokalne nie dowodzą innych systemów, zdalnego CI, obciążenia ani audytu bezpieczeństwa.
