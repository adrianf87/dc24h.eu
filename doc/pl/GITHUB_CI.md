# CI w GitHub Actions

Testy ncdc z konfliktem interfejsów rezerwują porty na obu adresach loopback i zachowują drugi nasłuch do gotowości klienta. Wyłącznie kolizja EADDRINUSE przy przydziale wybiera nową parę w ograniczonej pętli; sam test nie jest ponawiany. `integration.ncdc_ports` sprawdza deterministyczne kolizje TCP/UDP i sprzątanie.

Workflow `Check status` na gałęzi Etapu IV działa na runnerach GitHub-hosted `ubuntu-22.04`, `ubuntu-24.04` i `ubuntu-26.04`. Wywołują go push do `feature/stage4-webadmin-dedicated-service` i `main`, PR do `main` oraz uruchomienie ręczne. Workflow ma uprawnienia tylko do odczytu i nie zachowuje poświadczeń checkout. `main` nie zmienia się przed scaleniem gałęzi.

Zadanie aplikacji kompiluje C++ Release, uruchamia pełny CTest z wymogiem systemd, buduje i testuje WebAdmin na Node.js 24, wykonuje Chromium/Playwright oraz audyt npm na każdym obrazie Ubuntu. ASan i UBSan także działają na wszystkich trzech obrazach i warunkują check `CI (C++ and WebAdmin)`. Pozostałe trójobrazowe workflow obejmują CodeQL, TSAN/fuzz, Stage III offline i loopback, testy pamięci i błędów dyskowych oraz izolowane testy Stage IV. Publiczne testy Stage III na chronionej gałęzi `main` i w trybie OWNER pozostają na Ubuntu 22.04 i nie modyfikują VPS. Zobacz [Zależności](DEPENDENCIES.md) oraz [Linię beta](BETA.md).

Jedna grupa współbieżności na workflow i ref ma `cancel-in-progress: false`. Zadanie aplikacji ma limit 120 minut, a sanitizery 90 minut. **Bieżące** wyniki repozytorium Twilight są dostępne w [GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml); starsze przebiegi potwierdzają tylko swój SHA. Nie czekamy na zakończenie CI.

Push uruchamia CI; nie czekamy na jego zakończenie.
