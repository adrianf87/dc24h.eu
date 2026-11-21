# Lokalne zarządzanie usługą huba — beta-0.1.7

Ta wersja rozwojowa dodaje panel usługi dostępny wyłącznie dla Ownera na stronie **Zarządzanie** w istniejącym React WebAdmin. Wykorzystuje stały, niezależny kontroler systemd z beta-0.1.5 oraz audytowane lokalne operacje z beta-0.1.6. Nie udostępnia publicznej administracji zdalnej.

## Lokalna obsługa

1. Skonfiguruj osobny proces utrzymaniowy, prywatny socket API, konta `dc24h-maintenance`/`dc24h-control` i politykę stałej jednostki `dc24h.service` zgodnie z dokumentem [Operacje na usłudze](SERVICE_OPERATIONS.md). Ta wersja nie instaluje jednostek na hoście.
2. Otwórz WebAdmin **lokalnie**, potwierdź konto aktywnego **Ownera** w Rdzeniu administracyjnym i przejdź do Zarządzania. Administrator i niższe role nie mają dostępu do stanu i operacji usługi.
3. Odśwież stan. `GET /admin-api/v1/management/hub/status` korzysta z prywatnego adaptera Bearer oraz powiązania grantu, sesji i CSRF. API utrzymaniowe C++ odczytuje stałe polecenie `STATUS` przez socket kontrolera z kontrolą tożsamości procesu. Można odczytać wyłącznie jednostkę `dc24h.service`. Panel działa niezależnie od zatrzymanego procesu huba.
4. Start jest dostępny tylko dla stanu `inactive`, Stop i Restart tylko dla `active`. Nieznane, przejściowe, niezaładowane i niedostępne stany blokują operacje. Wybierz operację, przeczytaj ostrzeżenie o rozłączeniu i odzyskiwaniu, zaznacz potwierdzenie i zatwierdź. Przeglądarka wysyła wyłącznie `confirm=START`, `confirm=STOP` lub `confirm=RESTART` na odpowiednią stałą trasę. Serwer ponownie sprawdza rolę Ownera i stan systemd; sam interfejs nie stanowi zabezpieczenia autoryzacji.
5. W czasie działania przyciski są zablokowane. Po potwierdzeniu operacji panel ponownie pobiera stan systemd. Timeout, utrata odpowiedzi i `outcome_unknown` **nie powodują automatycznego ponowienia**. Przed kolejną próbą sprawdź systemd i trwały audyt. Sama odpowiedź sukcesu nie gwarantuje późniejszej dostępności usługi.

## Użytkownicy i historia

Liczba użytkowników online pochodzi wyłącznie z *osobnej obserwacji API statusu huba* nie starszej niż 15 sekund i jest wyświetlana tylko wtedy, gdy kontroler zgłasza aktywną jednostkę. Po Start/Stop/Restart poprzednia obserwacja zostaje unieważniona, a WebAdmin ponownie pobiera telemetrię huba; do tego czasu nie pokazuje liczby użytkowników sprzed operacji. Brak danych jest oznaczony jako niedostępny; nie wyliczamy liczby z systemd ani historycznego maksimum. Odczyt stanu usługi nie pobiera prywatnych danych użytkowników.

Panel prowadzi do **Historii działań**. Istniejąca trasa `/api/v1/audit` wymaga dodatkowo ID wykonawcy i dowodu dostępu; zachowuje strony po 16 wpisów i zdarzenia usługi z migracji v11 (`hub.start`, `hub.stop`, `hub.restart`, cel `service/1`, wynik `started`, `succeeded`, `failed`). Filtr operacji usługi ogranicza tylko *widoczną stronę*, nie wszystkie strony w bazie. Przerwane lub niepewne operacje mogą pozostać w stanie `started` do ręcznej analizy. Panel i audyt nie ujawniają sekretów.

## Granice bezpieczeństwa i ograniczenia

- Stałe trasy huba są przekazywane wyłącznie przez lokalny adapter Vite/Node na loopback. Przykładowa lista Apache HTTPS **nie zawiera** tych tras. Nie rozszerzaj jej ani nie udostępniaj internetowej bramy bez osobnego przeglądu bezpieczeństwa.
- Adapter Node weryfikuje lokalny same-origin, grant/sesję/CSRF, typ formularza, limity danych i odpowiedzi. API C++ niezależnie sprawdza aktualną rolę Ownera, a kontroler sprawdza tożsamość procesu. Nie ma powłoki, dowolnej nazwy usługi ani socketu wskazanego przez przeglądarkę.
- Odczyt systemd jest ograniczony czasem, operacje mają limity i zapisują wynik audytu. API utrzymaniowe ma osobny ograniczony termin na wysłanie odpowiedzi po zakończeniu wolnej, lecz ograniczonej operacji systemd.
- Ogólny kontrakt `POST /management/authorize` pozostaje niewykonującym operacji punktem zgodności. Konfiguracja nadal ma ograniczony edytor, bez pełnego przeładowania na żywo. ADC/ADCS pozostaje bez zmian.
- Beta-0.1.7 nie dodaje migracji: migracja **v11** już zapisuje operacje huba. Etap nie wdraża usługi, nie zmienia uprawnień hosta, nie aktywuje jednostek, nie uruchamia automatycznego odzyskiwania ani nie potwierdza odtwarzania po awarii. Procedura odzyskiwania pozostaje obowiązkiem operatora opisanym w [Operacjach na usłudze](SERVICE_OPERATIONS.md).

Przed scaleniem uruchom `npm run typecheck`, `npm test`, `npm run build`, testy przeglądarkowe WebAdmin oraz pakiety C++ i testów procesowych w obsługiwanej macierzy CI GitHub.
