# MariaDB

Status alpha-0.1.7: brak połączeń SQL, sterownika i migracji wykonywalnych.
Katalog `database/migrations/` rezerwuje miejsce dla kolejnego etapu.

Plan schematu obejmuje konta i hashe haseł, role/uprawnienia, konfigurację,
bany/blokady, wybrane zdarzenia i dane administracyjne. Migracje będą
numerowane i wersjonowane; migracji zastosowanych nie należy zmieniać.
Należy uwzględnić właściwości transakcyjne konkretnych operacji DDL MariaDB
oraz kopię zapasową przed migracją produkcyjną.

Przyszły adapter wymaga RAII dla połączeń, parametryzacji zapytań, minimalnych
uprawnień konta bazy i bezpiecznego hashowania haseł. Core nie zależy obecnie
od dostępności MariaDB. Dane dostępowe nie mogą trafić do kodu ani logów.
