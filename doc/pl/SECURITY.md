# Granice bezpieczeństwa

To infrastruktura transportu i parsera w wersji alpha, bez uwierzytelniania użytkowników. Domyślny nasłuch jest lokalny; publiczny adres wymaga jawnej opcji. Używaj nieuprzywilejowanego użytkownika. TLS chroni wyłącznie nasłuchy ADCS; zwykły TCP pozostaje nieszyfrowany.

Parser ogranicza ramki, parametry i pamięć, sprawdza UTF-8/NFC, nagłówki oraz escape. Nie weryfikuje tożsamości SID/IP, uprawnień ani semantyki sesji i nie wykonuje komend. Limity klientów, bezczynności i negocjacji nie są ochroną DDoS ani limitami per-IP/przepustowości. Treść ramek, PID i PAS nie trafiają do logów.

TLS >=1.2 wymaga wczytywalnego certyfikatu i zgodnego klucza; klient sprawdza zaufanie, nazwę i ważność. Nie ma certyfikatów klientów ani kont. Zaszyfrowane klucze PEM są odrzucane bez interakcji. Sekrety pozostają poza Git, z ograniczonymi uprawnieniami plików. Szczegóły: [ADCS_TLS.md](ADCS_TLS.md).

Sygnały stopu są odbierane synchronicznie, SIGPIPE jest ignorowany przez program. Deskryptory mają close-on-exec. SIGHUP zachowuje semantykę systemową. Błędy klienta zamykają jego gniazdo; krytyczny błąd pętli może zatrzymać proces.

Plan obejmuje kolejki wyjściowe, backpressure, limity wiadomości, walidację sesji, role, Argon2id, parametryzowane SQL i zabezpieczone API. Te funkcje nie są jeszcze zaimplementowane.
