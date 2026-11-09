# WebAdmin

Status alpha-0.1.3: katalog planowanego frontendu, bez aplikacji i pakietów npm.
Obecne etapy realizują Core i surowy transport TCP.

Stos docelowy: React + TypeScript + Vite. Apache2 będzie serwował zbudowane
zasoby i przekazywał żądania do zabezpieczonego API C++. Frontend nie będzie
łączył się bezpośrednio z MariaDB. Ekrany, sesje, role i akcje administracyjne
powstaną po zdefiniowaniu rzeczywistego API; nie będą symulowały stanu huba.
