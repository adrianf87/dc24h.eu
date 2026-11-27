# CI w GitHub Actions

Testy ncdc z konfliktem interfejsów rezerwują porty na obu adresach loopback i zachowują drugi nasłuch do gotowości klienta. Wyłącznie kolizja EADDRINUSE przy przydziale wybiera nową parę w ograniczonej pętli; sam test nie jest ponawiany. `integration.ncdc_ports` sprawdza deterministyczne kolizje TCP/UDP i sprzątanie.

Workflow `Check status` na gałęzi Etapu IV działa wyłącznie na runnerach GitHub-hosted `ubuntu-22.04`. Wywołują go push do `feature/stage4-webadmin-dedicated-service` i `main`, PR do `main` oraz uruchomienie ręczne. Workflow ma uprawnienia tylko do odczytu i nie zachowuje poświadczeń checkout. `main` nie zmienia się przed scaleniem gałęzi.

Zadanie aplikacji kompiluje C++ Release, uruchamia pełny CTest z wymogiem systemd, buduje i testuje WebAdmin na Node.js 24, wykonuje Chromium/Playwright oraz audyt npm. Dwa dodatkowe zadania na Ubuntu 22.04 wykonują ASan i UBSan; oba warunkują check `CI (C++ and WebAdmin)`. Pozostałe workflow Ubuntu 22.04 obejmują CodeQL, TSAN/fuzz, Stage III, testy pamięci i błędów dyskowych oraz izolowane testy Stage IV. Testy publicznego ADCS wymagają osobnej bramki OWNER i nie modyfikują VPS. Zobacz [Zależności](DEPENDENCIES.md) oraz [Linię beta](BETA.md).

Jedna grupa współbieżności na workflow i ref ma `cancel-in-progress: false`. Zadanie aplikacji ma limit 120 minut, a sanitizery 90 minut. **Bieżące** wyniki repozytorium Twilight są dostępne w [GitHub Actions](https://github.com/adrianf87/dc24h.eu-twilight/actions/workflows/build.yml); starsze przebiegi potwierdzają tylko swój SHA. Nie czekamy na zakończenie CI.

Push uruchamia CI; nie czekamy na jego zakończenie.
