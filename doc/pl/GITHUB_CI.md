# CI w GitHub Actions

Testy ncdc z konfliktem interfejsów rezerwują porty na obu adresach loopback i zachowują drugi nasłuch do gotowości klienta. Wyłącznie kolizja EADDRINUSE przy przydziale wybiera nową parę w ograniczonej pętli; sam test nie jest ponawiany. `integration.ncdc_ports` sprawdza deterministyczne kolizje TCP/UDP i sprzątanie.

Workflow `Check status` działa na runnerach GitHub-hosted `ubuntu-22.04`, `ubuntu-24.04` i `ubuntu-26.04`. Wywołują go push do `main` lub `feature/stage4-webadmin-dedicated-service`, PR do `main` oraz uruchomienie ręczne. Workflow ma uprawnienia tylko do odczytu i nie zachowuje poświadczeń po pobraniu kodu. [Definicja workflow](../../.github/workflows/build.yml)

Zadanie aplikacji kompiluje C++ Release, uruchamia pełny CTest wraz z testami systemd, buduje i testuje WebAdmin na Node.js 24 oraz uruchamia Chromium/Playwright i audyt npm na każdym obrazie Ubuntu. ASan i UBSan działają na wszystkich trzech obrazach i warunkują check `CI (C++ and WebAdmin)`. Osobne workflow obejmują CodeQL, TSAN i fuzzing, testy offline i loopback Etapu III oraz testy pamięci masowej i integracji Etapu IV. Publiczne testy Etapu III działają na Ubuntu 22.04 po ręcznym uruchomieniu przez właściciela. [Zależności](DEPENDENCIES.md) · [Linia beta](BETA.md)

Workflow bezpieczeństwa i testów end-to-end Etapu V obejmuje izolowane testy C++, WebAdminu, Chromium i publicznej bramy z MariaDB na wszystkich trzech obrazach Ubuntu. PR-y zmieniające kod, konfigurację lub testy sprawdzają commit gałęzi PR. Push do `main` sprawdza wynikowy commit tej gałęzi. Raporty zawierają SHA źródła oraz manifest SHA-256. [Workflow Etapu V](../../.github/workflows/stage5-security-e2e.yml)

Jedna grupa współbieżności na workflow i ref ma `cancel-in-progress: false`. Zadanie aplikacji ma limit 120 minut, a sanitizery 90 minut. Każdy przebieg potwierdza zapisany w nim SHA źródła. [Bieżące wyniki GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions)
