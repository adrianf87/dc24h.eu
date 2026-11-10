# Nick Manager — alpha-0.3.6

**Bieżący etap — alpha-0.3.11:** [User Disconnect](USER_DISCONNECT.md) kończy cykl życia i zwalnia kolejkę, stan protokołu/SID oraz rejestry odchodzącego użytkownika przed publicznym QUI. Błąd alokacji powiadomienia nie przerywa sprzątania źródła; odbiorcy bez możliwości przyjęcia powiadomienia są zamykani. Zachowano własne ISTA → IQUI i zachowanie przy wyłączeniu. Starsze opisy zachowują zakres historyczny.

Etap powstał bezpośrednio z main `cb49cdb9e1e714f3b548be6a842063c58890eaf6`, zawierającego już alpha-0.3.1–alpha-0.3.5. Zachowano main i historyczne gałęzie. Nowa gałąź alpha-0.3.6 nie jest scalana automatycznie.

## Własność i walidacja

`users::NickManager` jest niekopiowalnym i nieprzenoszalnym rejestrem jednego reaktora, prywatnie należącym do SessionManager. Zastępuje jego surową mapę nicków; indeksy SID/CID i własność Session/User pozostają w SessionManager. LoginHub nie utrzymuje drugiego rejestru. Samodzielny NickManager można testować bez gniazd, TLS, ADC, bazy danych i zegara.

Jedna rezerwacja łączy sprawdzoną nazwę z niezerowym lokalnym SessionId. Odwrotny indeks właścicieli wymusza jeden nick na sesję i umożliwia usuwanie po identyfikatorze czasu życia. Jawny limit wynosi 1–65535; integracja korzysta z limitu sesji. Sesja przed przyjęciem może nie mieć nicka; rezerwacja nie oznacza uwierzytelnionego konta.

Tekstowe przeciążenia `reserve` i `rename` wywołują istniejące `Nickname::create`. Przeciążenia przyjmujące sprawdzoną wartość wykorzystują tę walidację. Nazwa musi być niepusta, mieć najwyżej 256 bajtów UTF-8, być już w NFC i nie zawierać spacji ASCII, znaków sterujących, formatujących ani separatorów linii/akapitów. Nie ma normalizacji ani składania wielkości liter: `Name` i `name` są różne. Zachowano dotychczasową politykę. Pozostałe odstępy Unicode podlegają istniejącym kategoriom znaków. Pusta wartość Nickname po przeniesieniu jest odrzucana.

ADC zachowuje ostrzejszy limit 64 bajtów NI. Parser strumienia odrzuca błędny UTF-8 i ramki poza NFC przed przetwarzaniem INF; kolejna poprawna ramka pozwala kontynuować. INF odrzuca dodatkowe zabronione znaki oraz puste/anulowane wymagane nazwy i sprawdza pozostałe pola przed projekcją User. Ogólna fabryka nazwy nie zastępuje kontroli protokołu.

[Specyfikacja ADC, INF/NI oraz STA](https://adc.sourceforge.io/ADC.html#_inf) wymaga unikalności nicka i pozwala hubowi ograniczyć dopuszczalny zestaw znaków. Ten etap zachowuje dotychczasowe dokładne porównanie, kody błędów i politykę znaków.

## Kontrakt rejestru

| Operacja | Zachowanie |
| --- | --- |
| `reserve(id, name)` | Waliduje i rezerwuje nową parę; istniejący właściciel, zajęty nick i pełny rejestr dają typowane błędy. Powtórne reserve właściciela zwraca owner_exists. |
| `rename(id, name)` | Wymaga istniejącego właściciela; zmiana na własny nick działa także przy pełnym rejestrze. Kolizja lub błędna nazwa zachowuje stary nick. |
| `owner(name)` | Zwraca opcjonalny SessionId po dokładnym wyszukiwaniu bez tymczasowej alokacji. Puste/z NUL/za długie zapytanie nie ma właściciela. To wyszukiwanie, nie osobny pełny walidator. |
| `release(id)` | Usuwa wyłącznie parę danego właściciela, zwraca false przy braku. Powtórne sprzątanie starej sesji nie zwolni nazwy przejętej przez nowy czas życia. |
| `size()` / `capacity()` | Liczba rezerwacji i skonfigurowany limit. |

`NickResult` to opcjonalny NickError: invalid_nickname, duplicate_nickname, owner_exists, missing lub capacity_reached. Błędny limit konstruktora rzuca invalid_argument. Błąd alokacji przekazuje bad_alloc i zachowuje oba indeksy. Walidacja surowego tekstu poprzedza kontrolę rejestru; nie określono ogólnego pierwszeństwa wielu równoczesnych błędów. Na zewnątrz nie wychodzą referencje do nazw ani iteratory map.

Reserve najpierw alokuje wpis nazwy, potem odwrotny wpis właściciela; błąd drugiego wycofuje pierwszy. Rename alokuje nowy nick przed przestawieniem iteratora właściciela i usunięciem starego wpisu. Końcowe operacje nie alokują i nie rzucają. Operacje mają O(log n), ograniczone ciągi i najwyżej jedną nazwę oraz dwa węzły map na rezerwację. Indeks właścicieli przechowuje iterator, nie kopię nicka. Limit wpisów nie jest dokładnym budżetem pamięci. Const nie zapewnia synchronizacji; wymagany jest jeden właściciel. Samodzielny wywołujący nie może używać ponownie identyfikatora nadal opisującego stare połączenie.

## Integracja sesji i protokołu

SessionManager nadal sprawdza cykl życia, monotoniczny czas, poprawność User i niezmienność SID/CID przed zmianą nazwy. Przyjęcie przygotowuje SID/CID i wpisy NickManager przed nierzuconym zatwierdzeniem Session; wyjątek cofa wszystkie klucze. Aktualizacja samego profilu nie zmienia rejestru. Nowa nazwa jest zatwierdzana w NickManager, po czym następuje sprawdzone, nierzucone przeniesienie User. Publiczne `SessionManager::rename` tworzy kandydata zachowującego profil i używa tej samej ścieżki. Rejestr nie jest udostępniany do zewnętrznej modyfikacji.

Rezerwacje trwają w identified, ready, closing i closed do końcowego `remove`. Nowy użytkownik nie przejmie nicka sesji wysyłającej dane końcowe. Po poprawnej zmianie stary nick jest dostępny od razu; po usunięciu sesji dostępny jest ostatni nick. SID/CID nie zmieniają się przy zmianie nazwy. Kopie snapshot zachowują pierwotną tożsamość/profil.

Istniejąca bramka kolizji NI w LoginHub pyta SessionManager, a przez niego NickManager, przed zatwierdzeniem stanu ADC. Wszystkie poprawne aktualizacje INF używają tego rejestru. Zajęty nick daje dotychczasowe ISTA 222; błędne nazwy zachowują mapowanie STA/QUI. Poprawne publiczne aktualizacje trafiają do nadawcy i przyjętych klientów; odrzucone pola nie są rozsyłane. Wyjście końcowe i sprzątanie zachowują istniejące limity.

Nie dodano zależności, opcji konfiguracji, migracji SQL, tras API, komunikatów ani kluczy tłumaczeń. Domyślny język to EN/US; istniejące katalogi languages/us i languages/pl nadal obowiązują. Trwałe przydziały nicków, konta, rezerwowane nazwy operatorów, wykrywanie znaków podobnych, porównanie bez wielkości liter i administracyjne API zmiany nicka pozostają planami.

## Weryfikacja

`users.nicknames` obejmuje limity, UTF-8/NFC/kategorie znaków, polskie nazwy, wielkość liter, kolizje właścicieli/nazw, pełny rejestr, zmianę na własny i obcy nick, stare sprzątanie, ponowne użycie i 1000 czasów życia. Ograniczony budżet alokacji sprawdza każdy punkt alokacji C++ w reserve, rename i przyjęciu SessionManager, w tym wycofanie i ponowienie. Nie wstrzykuje błędów do alokatora C biblioteki utf8proc i nie stanowi pełnej kampanii błędów całego systemu.

`users.nicknames.process` sprawdza rzeczywiste ADC/ADCS, mieszane transporty, zmianę nicka/profilu, własny nick, granicę 64 bajtów ADC, ponowne użycie starej nazwy przed rozłączeniem właściciela, kolizje, brak rozsyłania odrzuconych pól, sprzątanie/reconnect i kontynuację po ramce poza NFC. Istniejące users.registry sprawdza bramki czasu/cyklu życia i rezerwacje do usunięcia zamkniętej sesji. Rzeczywisty ncdc zmienia polską nazwę dc24hŻółw na dc24hŁoś i ponownie osiąga NORMAL przez ADC oraz ADCS. ncdc stosuje `/nick` po reconnect; zmianę NI w działającej sesji sprawdzają klienci testowi protokołu.

Polecenia i wyniki regresji opisuje [TESTING.md](TESTING.md). Wyniki lokalne nie potwierdzają wydajności produkcyjnej, zdalnego CI ani każdego wymienionego systemu operacyjnego.
