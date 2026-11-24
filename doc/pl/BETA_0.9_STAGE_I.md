# beta-0.9.x — Etap I (0.9.1–0.9.4), przewodnik operatora

Status: **rozpoczęty; nieodebrany**. [Zadanie #242](https://github.com/adrianf87/dc24h.eu-high-starter/issues/242) i [Draft PR #243](https://github.com/adrianf87/dc24h.eu-high-starter/pull/243). Kod na gałęzi `beta-0.9.x-stage-full` nie został scalony ani wdrożony.

## 0.9.1 — Zgodność klientów ADC/ADCS

Jedynym klientem z wykonanymi testami procesowymi i zweryfikowanymi hashami pakietu APT oraz binarki na GitHub-hosted Ubuntu 22.04/24.04/26.04 pozostaje **ncdc**. DC++ i AirDC++ są oznaczone jako **nieprzetestowane**; EiskaltDC++ jest wyłącznie kandydatem do późniejszego sprawdzenia rzeczywistej binarki. Symulowane gniazda Python nie zastępują niezależnego klienta. Raport `beta09-stage1-interoperability.json` uczciwie podaje `stage1_complete: false` do czasu kwalifikacji drugiego klienta.

## 0.9.2 — Cofnięcie uprawnień

Hub C++ używa migawki kont wczytanej podczas startu; zapisu w MariaDB **nie wolno** traktować jako natychmiastowego unieważnienia działających sesji. Poprawna procedura w środowisku odizolowanym:

1. Zatrzymaj zapis kont i nowe logowania w kontrolowanym oknie administracyjnym.
2. W transakcji zmień status/revision dokładnego konta, zatwierdź i zweryfikuj wynik; `ROLLBACK` nie publikuje żadnej zmiany.
3. Zatrzymaj hub, rozłącz istniejące sesje i uruchom go ponownie na zatwierdzonej migawce.
4. Zweryfikuj odmowę logowania wyłączonego konta na ADC i ADCS. W razie awarii restartu zachowaj stan **NO-GO**, bez cofania decyzji o odebraniu dostępu.
5. Ponowne nadanie dostępu wymaga nowej zatwierdzonej transakcji, kontrolowanego restartu i świeżego GPA/PAS.

Automatyczne odświeżanie polityki, generacje i unieważnianie bez restartu wymagają osobnej decyzji Ownera — **nie zostały wdrożone**.

## 0.9.3 — Transfery i NAT

Testy loopback obejmują SCH/RES (TTH), CTM/RCM, aktywne/pasywne profile i IPv4/IPv6 oraz odmowę niezgodnej rodziny adresowej. Nie gwarantują działania przez CGNAT, NAT symetryczny, zapory wielu maszyn ani publiczny IPv6. Nie dopuszczaj obejścia kontroli tokenu, SID, adresata czy limitu prób w trybie zgodności.

## 0.9.4 — Cykl życia zaufania ADCS

Testowana jest poprawna ścieżka CA i SAN `localhost`, odmowa niewłaściwego hosta/SNI i obcego CA, TLS 1.2/1.3 oraz istniejące przypadki rotacji, wygaśnięcia i niedopasowanego klucza. Nie wyłączaj weryfikacji nazwy i CA po stronie klienta; nie stosuj fallbacku do nieszyfrowanego ADC. To nie jest deklaracja serwerowej listy dozwolonych SNI.

## Odbiór i bezpieczeństwo

Wymagane są wyłącznie GitHub-hosted Ubuntu 22.04, 24.04 i 26.04, pełne testy na ostatecznym SHA, brak niepowodzeń oraz oddzielny Fix Pass 1. Nie publikuj surowych logów, haseł, tokenów, prywatnych kluczy ani danych użytkowników. CI uruchomione po push jest **oczekujące**, nie zaliczone. Nie ma zgody na merge, produkcyjne wydanie, migrację ani wdrożenie.

[Opis techniczny EN/US](../us/BETA_0.9_STAGE_I.md).
