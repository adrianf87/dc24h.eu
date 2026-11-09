# WebAdmin

Status alpha-0.1.4: katalog planowanego frontendu, bez aplikacji i pakietów npm.
Obecne etapy realizują Core, TCP, parser ADC i TLS.

Stos docelowy: React + TypeScript + Vite. Apache2 będzie serwował zbudowane
zasoby i przekazywał żądania do zabezpieczonego API C++. Frontend nie będzie
łączył się bezpośrednio z MariaDB. Ekrany, sesje, role i akcje administracyjne
powstaną po zdefiniowaniu rzeczywistego API; nie będą symulowały stanu huba.
