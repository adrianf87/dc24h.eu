# Implementacja publicznego WebAdmin, etap IV

Aplikacja korzysta z istniejących tabel C++ przechowujących konta i
weryfikatory haseł. Wersjonowane migracje etapu IV dodają powiązania e-mail,
wyzwania zabezpieczone HMAC, liczniki prób, sesje, oczekujące rejestracje i
zdarzenia audytu. Potwierdzenie skrzynki tworzy konto `registered`.
Pierwsze poprawne hasło i kod e-mail aktywują zastrzeżone konto
Administrator. Każde następne logowanie wymaga nowego kodu. Przy
autoryzacji aplikacja sprawdza aktualną rolę, rewizję poświadczeń i stan konta.

[Instrukcja wdrożenia i wycofania](STAGE_IV_DEDICATED_SERVICE.md) obejmuje
prywatną konfigurację, rozdzielenie baz testowej i produkcyjnej, instalację
na Debianie 13 oraz kryteria odbioru.
