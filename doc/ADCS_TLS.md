# ADCS i TLS

Status alpha-0.1.3: nie ma TLS, certyfikatów ani listenera ADCS.
Program nie linkuje obecnie biblioteki kryptograficznej.

Plan: TLS w module Network/ADCS z utrzymywaną biblioteką, walidacją konfiguracji
certyfikatu i klucza oraz testami błędnego handshake i zamknięcia połączenia.
Nie tworzyć własnej kryptografii ani nie wyłączać walidacji w celu obejścia
błędów. Docelowe wersje TLS i zasady odnowienia certyfikatów zostaną zapisane
podczas implementacji. Klucze prywatne pozostają poza repozytorium.
