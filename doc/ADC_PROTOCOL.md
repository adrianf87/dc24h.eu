# Protokół ADC

Status alpha-0.1.2: parser i automat protokołu nie są zaimplementowane.
Nie deklarujemy zgodności działającego huba z ADC.

Następny etap protokołu obejmie oddzielny parser i serializer, typy wiadomości,
separację komend i parametrów, escaping oraz limity długości wejścia.
Każdy element należy zweryfikować względem oficjalnej specyfikacji ADC,
a następnie dodać testy poprawnych i błędnych ramek. Handshake, identyfikatory,
sesje, chat, search i połączenia klientów będą wprowadzane osobno.

Warstwa ADC nie powinna bezpośrednio zarządzać deskryptorami TCP ani bazą.
