# Beta 0.9.13–0.9.16 — Etap IV: niezawodny audyt i prywatna diagnostyka

Zakres: [Issue #242](https://github.com/adrianf87/dc24h.eu-high-starter/issues/242), wspólna gałąź `beta-0.9.x-stage-full`, roboczy PR #243. Etap IV jest oddzielnym zadaniem; nie oznacza akceptacji Etapów II/III ani zgody na migrację audytu. Zachowujemy ochronę `main` i zasadę oddzielnej decyzji OWNER przed wdrożeniem.

## 0.9.13 — projekt ciągłości audytu, bez uruchamiania rotacji

Aktywny dziennik ma nadal **8 MiB**, jest ograniczony i blokuje autoryzowane operacje po błędzie. Istniejące narzędzie zachowuje archiwum **bez** zerowania źródła i bez zwalniania limitu. Nowy zapis od numeru ostatniego rekordu nie jest obecnie obsługiwany w produkcyjnej bramce.

Proponowana procedura, której **nie wolno wykonać na produkcji bez osobnej zgody OWNER**:

1. Zatrzymać wszystkie procesy zapisujące, zweryfikować ich zatrzymanie i utrzymać blokadę konserwacyjną. Potwierdzić spójną generację prywatnych pięciu magazynów oraz kopii MariaDB.
2. Zweryfikować cały łańcuch, inode, skrót i rozmiar źródła. W prywatnym katalogu utworzyć nowe archiwum, wykonać `fsync` pliku i katalogu, nie usuwać oryginału. Brak lub niezgodność archiwum kończy procedurę odmową.
3. Zatwierdzić niezależnym kluczem OWNER punkt kontrolny: skrót archiwum, końcowy numer i skrót łańcucha, generację, skrót poprzedniego punktu i identyfikator klucza. Odrębnie potwierdzić monotoniczny rejestr blokujący cofnięcia.
4. Dopiero po atomowym zapisie metadanych i zatwierdzeniu przez OWNER utworzyć wersjonowany nowy dziennik kontynuujący numerację i poprzedni skrót. Każdy przerwany krok, ponownie użyta generacja, brak klucza lub `fsync` z błędem oznacza **odmowę autoryzacji**, nie pusty reset.

W kodzie testowym znajduje się **wyłącznie prototyp w pamięci** weryfikujący dwa kolejne archiwa. Nie jest importowany przez bramkę, nie modyfikuje plików produkcyjnych i nie daje prawa uruchomienia rotacji.

## 0.9.14 — klucz i zabezpieczenie przed podrobieniem

Prototyp wiąże HMAC-SHA256 ze skrótem całego archiwum, końcówką łańcucha, numeracją, poprzednim punktem i generacją. Testy odrzucają podmianę na plik tej samej długości ze świadomie przeliczonym zwykłym SHA-256, obcy klucz, brak archiwum i cofnięcie generacji. Klucz w testach jest **sztuczny**.

Przed włączeniem rozwiązania OWNER musi osobno ustalić: przechowywanie klucza poza pięcioma magazynami, rotację/utratę klucza, odporny na rollback rejestr generacji, migrację v1→v2, procedurę awaryjnego odzyskania, zgodność z kopią zapasową i zasady wyłączności zapisu. Sam HMAC nie zapewnia niezaprzeczalności. Przejęcie klucza albo równoczesna podmiana lokalnego rejestru i plików przez uprzywilejowaną osobę pozostaje ryzykiem.

## 0.9.15 — bezpieczne testy błędów dysku

Dodany workflow dla **GitHub-hosted Ubuntu 22.04, 24.04, 26.04**, bez Debiana i self-hosted Xeon:

- Tworzy tymczasowy `tmpfs` o wielkości **4 MiB** i napełnia go aż do rzeczywistego błędu jądra **ENOSPC**. Test wymaga odmowy zapisu audytu i blokady kolejnych prób.
- Z wykorzystaniem kompilowanej tylko do testów biblioteki `LD_PRELOAD` zwraca **EIO** przy `fsync` dokładnego testowego pliku; wynik nie może być uznany za trwały sukces.
- Zabija testowy proces `SIGKILL` po poprawnym `fsync`, następnie sprawdza łańcuch po ponownym otwarciu. Nie symuluje to awarii **w trakcie** `fsync`.
- Ponawia testy uprawnień `0600`, symlinków, podmiany inode, uszkodzonych rekordów i limitu.

Katalog i montowanie są izolowane w `$RUNNER_TEMP`, chronione znacznikiem i kontrolą typu `tmpfs`, sprzątane po pracy. **Nie publikować surowego audytu, kluczy ani kopii magazynów**. Jeśli GitHub runner odmówi montowania, test ma zakończyć się błędem — nie wolno oznaczyć ENOSPC jako zaliczone. Lokalnie bez montowania jest pomijany.

## 0.9.16 — stan C++/Node i dostępność EN/PL

Powtórzone testy świeżości C++, restartu i prywatnej historii. Rozszerzony test przeglądarkowy sprawdza, że przy wstrzymanym odświeżaniu po wyczerpaniu **45-sekundowego** limitu znikają również **zachowane zdarzenia historyczne** — nie tylko aktywne liczniki. Komunikaty EN i PL informują o nieaktualności. Klawiatura i lokalne potwierdzenie nie wysyłają operacji do huba ani nie przyznają uprawnień.

## Odbiór i dalsze kroki

Nie uruchamiać produkcyjnej rotacji, zmiany formatu, migracji kluczy, release, merge ani wdrożenia. Wymagane są wyniki CI na **dokładnym SHA** dla GitHub Ubuntu 22/24/26 oraz pełna integracja z pozostałymi etapami (C++, Node, Chromium, VM/systemd, odtwarzanie, narzędzia bezpieczeństwa). Samo uruchomienie CI nie oznacza akceptacji. Zgodnie z OWNER po wypchnięciu zmian **zakończyć pracę bez oczekiwania i odpytywania o wyniki**.
