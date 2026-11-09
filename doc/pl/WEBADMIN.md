# WebAdmin

Status alpha-0.1.9: katalog planowanego frontendu, bez aplikacji i pakietów npm.
Obecne etapy realizują Core, TCP, parser ADC i TLS.

Stos docelowy: React + TypeScript + Vite. Apache2 będzie serwował zbudowane
zasoby i przekazywał żądania do zabezpieczonego API C++. Frontend nie będzie
łączył się bezpośrednio z MariaDB. Ekrany, sesje, role i akcje administracyjne
powstaną po zdefiniowaniu rzeczywistego API; nie będą symulowały stanu huba.

Lokalne API tylko do odczytu jest zaimplementowane: [API.md](API.md). Wymaga prywatnego katalogu i tego samego efektywnego UID, dlatego worker Apache innego UID nie może się połączyć. Brama Apache, logowanie WWW, role i frontend pozostają planowane. Nie udostępniaj wspólnego tokena w JavaScript przeglądarki.
