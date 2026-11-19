# Dziennik audytu — alpha-0.8.9

Ten etap zapisuje udane działania konserwacyjne. Punktem wyjścia jest main `e88e596`. Nie ma automatycznego scalenia. Nazwa gałęzi to `alpha-0.8.9`.

Schemat v9 dodaje tabelę dopisywania `dc24h_audit_events` w `database/migrations/0009_audit_events.sql`. Migracje 1–8 pozostają bez zmian. Aktualizację wykonuje jawne polecenie migracji bazy po kopii zapasowej. Zwykły start nie migruje schematu. Konserwacja kont i banów wymaga bieżącego katalogu, więc te zapisy czekają na zastosowanie wersji 9.

Każde zatwierdzone działanie konserwacyjne wstawia jeden wiersz w tej samej transakcji SQL, przed zatwierdzeniem. Nieudany zapis wycofuje całe działanie. Zapisane fakty to identyfikator zdarzenia, UTC `occurred_at`, stały kod działania, identyfikator konta aktora, rodzaj celu (`account` albo `ban`), identyfikator celu i wynik `succeeded`.

Kody działań to `accounts.create`, `accounts.activate`, `accounts.disable`, `accounts.password`, `accounts.reset`, `accounts.role`, `ownership.bootstrap`, `ownership.transfer`, `bans.create` i `bans.revoke`. Rejestracja poza Admin Core, przeładowanie katalogu oraz kick i wyciszenie na żywo nie dodają wierszy. Odmowy nie dodają wierszy. Hasła, dowody, nazwy kont i dowolny tekst powodu bana nie są kolumnami.

`audit.read` jest zaimplementowanym uprawnieniem. Ma je moderator i wyższe role. Konto VIP ma je tylko wtedy, gdy maska VIP zawiera ten klucz. `configuration.manage` i `hub.manage` pozostają niezaimplementowane.

`POST /api/v1/audit` zwraca jedną stronę 16 wierszy dla udowodnionego czytelnika. Pola formularza to dokładnie `actor_id`, `proof` i `after`. `after` może mieć wartość `0`. Konto musi być aktywne, hasło musi pasować, a rola musi zezwalać na `audit.read`. Odczyt nie wstawia wiersza i nie zmienia liczników Admin Core. `GET /api/v1/audit` jest odrzucane. Pełna strona ustawia `next_after` na ostatni identyfikator. Krótsza strona ustawia `next_after` na null. Identyfikatory w JSON są napisami. Czasy są liczbami. Prywatny adapter konserwacji przyjmuje ten sam POST pod `/admin-api/v1/audit` i usuwa dodatkowe pola, zanim przeglądarka zobaczy treść.

WebAdmin pokazuje historię pod `#audit` po angielsku i po polsku. Strona jest tylko do odczytu. Hasło pozostaje w pamięci tylko na czas żądania następnej strony, pole jest czyszczone po wysłaniu, a wartość nie trafia do pamięci przeglądarki. Zatrzymaj serwujące huby przed zapisem konserwacji, a potem uruchom je ponownie. Samo przeglądanie historii nie wymaga tego restartu.

Kick i wyciszenie na żywo pozostają licznikami sesji. Zapis konfiguracji i administracja działającego huba pozostają w planach.
