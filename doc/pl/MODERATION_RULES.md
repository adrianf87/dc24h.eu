# Reguły moderacji — alpha-0.8.8

Kick, mute, unmute, bany konta, CID i adresu IP oraz cofanie bana korzystają
teraz z jednej decyzji bez alokacji. Aktor musi mieć uprawnienie danej operacji.
Cel musi być rodzajem podmiotu przyjętym przez tę operację i spełniać jej regułę
rangi. Ten etap nie dodaje polecenia czatu, migracji SQL, trwałego wyciszenia,
żywego `+ban` ani przycisku w przeglądarce, który wykonuje moderację.

Schemat v8 i pliki migracji 1–8 pozostają bez zmian. Kody wyników na łączu i w
serwisie banów pozostają tymi, które już zwracają kick, mute i Ban Core.
Szczegółowa odmowa nie trafia do klienta i nie jest wpisem audytu.

## Decyzja

`security::moderate(operation, actor, target, policy)` zwraca jedną z wartości:

| Odmowa | Znaczenie |
| --- | --- |
| `none` | Uprawnienie i cel są poprawne |
| `missing_permission` | Aktor nie ma uprawnienia operacji, VIP nie dostał delegacji albo ban nie ma `blocks.manage` |
| `self_target` | Operacja zabrania działania wobec aktora |
| `not_lower` | Porównywana ranga nie istnieje, jest równa albo wyższa |
| `wrong_subject` | Rodzaj celu nie pasuje do operacji |

Nieznana operacja, nieznana rola i maska VIP spoza `users.kick`, `users.mute` i
`audit.read` kończą się odmową. Funkcja nie alokuje, nie czyta zegara, nie
otwiera SQL i nie zachowuje nicka, CID, adresu, powodu ani hasła. Status konta,
dowód hasła, blokady rewizji i gotowość sesji nadal sprawdza wywołujący.
Wyłączone konto nadal nie może serwisować banów.

## Operacje

| Operacja | Uprawnienie | Cel |
| --- | --- | --- |
| Kick | `users.kick` | Gotowa sesja o ściśle niższej roli, również gość. Siebie nie można wyrzucić. VIP tylko z delegacją `users.kick` |
| Mute i unmute | `users.mute` | Ta sama reguła sesji co kick. Operacje nie zastępują kicka. VIP tylko z delegacją `users.mute` |
| Ban konta | `users.ban` i `blocks.manage` | Trwałe konto o ściśle niższej roli. Siebie nie można zbanować. Gość nie jest kontem |
| Ban CID | `users.ban` i `blocks.manage` | CID powiązany z kontem używa reguły konta. Niezmapowany CID nie ma roli celu |
| Ban adresu IP | `users.ban` i `blocks.manage` | Tylko dokładny adres. Nie ma roli celu, więc blokada może objąć adres personelu |
| Cofnięcie bana | `users.ban` i `blocks.manage` | Aktor może cofnąć własny zapis. W przeciwnym razie konto autora musi mieć ściśle niższą rolę. Konto albo zmapowany CID nadal muszą być ściśle niższe |

Moderator i wyższe role mają kick oraz mute. Operator i wyższe role dostają oba
uprawnienia banów naraz. Owner podlega regule celu. Globalne przełączniki funkcji,
limity flood, polityka ADCS i limity połączeń nadal obowiązują.

Kolejność sprawdzeń sesji pozostaje dotychczasowa. Kick zgłasza brak albo
niegotowy cel jako niedostępny przed wynikiem uprawnienia, z wyjątkiem braku
aktora i celu własnego: te pozostają odmową. Mute nadal odmawia braku uprawnienia,
zanim zgłosi niedostępny cel. Serwis banów nadal zgłasza brak konta jako nie
znalezione, brak autora jako uszkodzony zapis, a złą rangę jako odmowę, zanim
przyjmie dowód hasła.

## Miejsca wywołania

- `KickCore` i prywatne polecenie `+kick`
- `MuteCore` oraz `+mute` / `+unmute`
- `LoginHub::revoke_auth_session_as`
- Tworzenie i cofanie bana, w tym zmapowany CID i ranga autora

`can_revoke` i `can_mute` używają tej samej decyzji bez osobnej tożsamości, więc
odmowę wobec siebie nadal wykonuje wywołujący.

## API i WebAdmin

Uwierzytelniony status oraz `--show-configuration` dodają opcjonalną tablicę
`configuration.moderation`. Każdy wpis podaje operację, uprawnienie, rodzaj celu,
regułę celu oraz to, czy sesja gościa jest dozwolona, czy własny cel jest
zabroniony, czy delegacja VIP jest możliwa i czy wymagane jest także
`blocks.manage`. Tablica opisuje stałe reguły. Skonfigurowana lista VIP pozostaje
w `configuration.roles`. Nie ma w niej konta, nicka, adresu ani sekretu.

Starsze klienty ignorują pole. Zniekształcona tablica czyści zrzut WebAdmin
zamiast pokazać słabszą regułę. Dwujęzyczna strona Moderacja jest tylko do
odczytu. Nie wybiera aktora i nie wysyła kicka, mute ani bana.

## Poza tym etapem

Trwałe albo czasowe wyciszenie, żywe polecenie `+ban`, bany podsieci i nazw
hostów, przeglądarka audytu oraz zmiana tego, kto już może wykonać kick, mute
albo ban, nie są zaimplementowane. Publikacja tabeli nie tworzy tych silników.

## Weryfikacja

Testy natywne obejmują macierz siedmiu ról dla kick, mute, unmute, banów kont,
zmapowanych i niezmapowanych CID, banów adresów i cofania, w tym siebie, gościa,
delegację VIP, zły rodzaj celu i nieprawidłową maskę VIP. Decyzja nie alokuje.
Dotychczasowe zestawy kick, mute, ról i banów zachowują wyniki. Lokalne polecenia
używają ośmiu wątków CPU. Runnery GitHub Ubuntu 24.04 mają cztery logiczne CPU,
sprawdzane przez `nproc`, a równoległość wynika z `nproc`. Zakończenie zdalnego
CI nie jest oczekiwane.
