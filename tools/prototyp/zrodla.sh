# SPDX-License-Identifier: GPL-3.0-or-later
# Wspólna lista plików rdzenia (logika bez Arduino) i flag kompilacji na komputerze.
# Wczytują ją tools/prototyp/testy.sh i tools/jakosc/sprawdz.sh (`source`), żeby testy, sanitizery,
# clang-tidy i pokrycie widziały te same pliki. Wymaga zmiennej FW (katalog firmware/prototyp).
ZRODLA=("$FW/src/wspolne.cpp" "$FW/src/noc.cpp" "$FW/src/czynniki.cpp" "$FW/src/kokpit.cpp" "$FW/src/humor.cpp"
        "$FW/src/nawigacja.cpp" "$FW/src/ekrany.cpp" "$FW/src/dzien.cpp" "$FW/src/cykl.cpp" "$FW/src/dobranoc.cpp"
        "$FW/src/stany.cpp" "$FW/src/sypialnia.cpp" "$FW/src/airbridge.cpp" "$FW/src/otoczenie.cpp" "$FW/src/bateria.cpp"
        "$FW/src/wybudzenie.cpp" "$FW/src/start.cpp" "$FW/src/dyrygent.cpp" "$FW/src/plan.cpp"
        "$FW/src/magazyn.cpp" "$FW/src/reguly.cpp"
        "$FW/src/reguly_parser.cpp" "$FW/src/reguly_rodzaje.cpp" "$FW/src/reguly_pamiec.cpp"
        "$FW/src/sprawy.cpp" "$FW/src/kolejka.cpp" "$FW/src/rodzaje_e.cpp")
# STR.edf z AirBridge: czytnik EDF (src/edf.cpp) i historia nocy ze STR (src/str.cpp), ADR-0035
ZRODLA+=("$FW/src/edf.cpp" "$FW/src/str.cpp")
# noc liczona na panelu z plików sesji PLD, EVE, CSL (src/noc_pliki.cpp), ADR-0036
ZRODLA+=("$FW/src/noc_pliki.cpp")
# Portret nocy: odcinki 5 min ostatniej nocy i plik /dane/portret.csv (src/portret.cpp), ADR-0040
ZRODLA+=("$FW/src/portret.cpp" "$FW/src/portret_ekran.cpp")
# strona kokpitu „Sypialnia · 30 nocy” (src/sypialnia_ekran.cpp), ADR-0044
ZRODLA+=("$FW/src/sypialnia_ekran.cpp")
# układ ekranów z telefonu: kolejność stron kokpitu i ekranów dziennych (src/uklad_ekranow.cpp), ADR-0045
ZRODLA+=("$FW/src/uklad_ekranow.cpp")
# porównanie nocy z plików z raportem JSON (src/porownanie.cpp), tryb `noc_zrodlo=porownanie`, ADR-0036
ZRODLA+=("$FW/src/porownanie.cpp")
# ekrany zdarzeń E w tonie „problem” (S3, src/ekrany_zdarzen.cpp)
ZRODLA+=("$FW/src/ekrany_zdarzen.cpp")
# slot dzienny, obrazek dnia, ekrany CO2 S-K-01, S-K-02, S-K-13
# stany wzorców L (src/wzorce.cpp) i rodzaje warunków L i H (src/rodzaje_l.cpp)
ZRODLA+=("$FW/src/wzorce.cpp" "$FW/src/rodzaje_l.cpp")
# stany nocy 5.1.4 (src/stan_nocy.cpp)
ZRODLA+=("$FW/src/stan_nocy.cpp" "$FW/src/slot_wieczorny.cpp")
# odkrycia O-03…O-12, S7, S8, „Co znaczą te liczby”
ZRODLA+=("$FW/src/odkrycia.cpp" "$FW/src/podsumowanie.cpp")
ZRODLA+=("$FW/src/slot_dzienny.cpp" "$FW/src/obrazki.cpp" "$FW/src/kontekst.cpp" "$FW/src/ekrany_kontekst.cpp")
# raport A4 dla lekarza (src/raport*.cpp), kod QR (src/qr.cpp), serwer raportu (rdzeń)
ZRODLA+=("$FW/src/raport.cpp" "$FW/src/raport_pdf.cpp" "$FW/src/raport_ekran.cpp" "$FW/src/qr.cpp" "$FW/src/serwer_raportu.cpp")
# moduły SPRZĘT (src/sprzet.cpp), INFEKCJA (src/infekcja.cpp), pozostałe S-K (src/kontekst_sk.cpp)
ZRODLA+=("$FW/src/sprzet.cpp" "$FW/src/infekcja.cpp" "$FW/src/kontekst_sk.cpp")
# Konfiguracja z telefonu: sekrety z NVS (src/sekrety.cpp)
ZRODLA+=("$FW/src/sekrety.cpp" "$FW/src/formularz.cpp" "$FW/src/ekrany_konfiguracji.cpp")
# Tabela miast do pogody (src/miasta.cpp)
ZRODLA+=("$FW/src/miasta.cpp")
# Limit ładowania baterii na kablu: decyzja o zapisie do ładowarki SY6974B (src/ladowarka.cpp), ADR-0046
ZRODLA+=("$FW/src/ladowarka.cpp")
# Aktualizacja firmware z wydań GitHub (ADR-0032, ADR-0038): rdzeń decyzji, wersji, opisu wydania i SHA-256
ZRODLA+=("$FW/src/aktualizacja.cpp")
# Rodzaj odświeżenia e-ink: pełne albo częściowe (src/odswiezanie.cpp), R-NF-25
ZRODLA+=("$FW/src/odswiezanie.cpp")
# Szybkość ścieżki „przycisk → rysowanie” (T-WYM-99): przełącznik pamięci podręcznej do testów i haczyk faz pomiaru
ZRODLA+=("$FW/src/szybkosc.cpp")
# Czekanie na zasilaniu zewnętrznym zamiast głębokiego snu i pamięć plików między przebiegami (ADR-0047)
ZRODLA+=("$FW/src/czekanie.cpp" "$FW/src/pliki_pamiec.cpp")
# Próbkowanie przycisków i kolejka naciśnięć: rdzeń wspólny dla adaptera ESP32 i atrapy (R-PRZ-20)
ZRODLA+=("$FW/src/przyciski_probki.cpp")
# Program „Dobry start”: porady, stan programu, wybór porady dnia, ankieta (src/dobry_start.cpp), ADR-0049
ZRODLA+=("$FW/src/dobry_start.cpp")
# Wysyłka obrazu do sterownika e-ink kawałkami z odwróceniem bitów (src/wysylka_obrazu.cpp, T-KOMP-17, 22.11.2026)
ZRODLA+=("$FW/src/wysylka_obrazu.cpp")
# Trwale nieczytelny plik danych: licznik wybudzeń w RTC, sprawa w NVS, rodzaj danych słowem (src/nieczytelne.cpp)
ZRODLA+=("$FW/src/nieczytelne.cpp")
# Limity czasu operacji sieciowych wobec strażnika zadania i znacznik „w sieci” (src/siec_limity.cpp, T-WYM-81, R-AW-39)
ZRODLA+=("$FW/src/siec_limity.cpp")
# Programy testów rdzenia: każdy linkowany z obiektami ZRODLA, uruchamiany w testy.sh, sanitizerach i pokryciu
# (testy wybudzenia: test/testy_wybudzenie.cpp i scenariusze dyrygenta test/testy_dyrygenta.cpp; plan wybudzenia
# src/plan.cpp w ZRODLA i test/testy_plan_wybudzen.cpp; test/testy_slot_dzienny_w_planie.cpp, slot dzienny w planie wybudzeń;
# magazyn danych src/magazyn.cpp w ZRODLA i test/testy_magazyn_danych.cpp; przypadki brzegowe magazynu:
# test/testy_magazyn_brzegowe.cpp i test/testy_magazyn_brzegowe_2.cpp; silnik reguł src/reguly*.cpp w ZRODLA,
# test/testy_silnik_regul.cpp i test/testy_silnik_regul_brzegowe.cpp; karty z budzikiem: test/testy_karty_z_budzikiem.cpp
# i test/testy_karty_z_budzikiem_brzegowe.cpp; kolejka linii: test/testy_kolejka_linii.cpp i test/testy_kolejka_linii_brzegowe.cpp).
TESTY=("$FW/test/testy.cpp" "$FW/test/testy_wybudzenie.cpp" "$FW/test/testy_dyrygenta.cpp" "$FW/test/testy_plan_wybudzen.cpp"
       "$FW/test/testy_slot_dzienny_w_planie.cpp" "$FW/test/testy_magazyn_danych.cpp" "$FW/test/testy_magazyn_brzegowe.cpp"
       "$FW/test/testy_magazyn_brzegowe_2.cpp" "$FW/test/testy_rekord_nocy.cpp" "$FW/test/testy_silnik_regul.cpp" "$FW/test/testy_silnik_regul_brzegowe.cpp"
       "$FW/test/testy_kolejka_linii.cpp" "$FW/test/testy_kolejka_linii_brzegowe.cpp"
       "$FW/test/testy_karty_z_budzikiem.cpp" "$FW/test/testy_karty_z_budzikiem_brzegowe.cpp")
TESTY+=("$FW/test/testy_slot_dzienny.cpp" "$FW/test/testy_slot_dzienny_dyrygent.cpp") # slot dzienny, obrazki dnia, S-K-01
TESTY+=("$FW/test/testy_wygaszacz_brzegowe.cpp") # testy brzegowe wygaszacza ekranu (R-KON-73…R-KON-77)
TESTY+=("$FW/test/testy_wygaszacz_brzegowe_2.cpp") # testy brzegowe wygaszacza, wariant W3 (R-KON-75)
TESTY+=("$FW/test/testy_raport_a4.cpp") # raport A4, kod QR, serwer raportu, ściąga
TESTY+=("$FW/test/testy_stany_nocy_sloty_wieczorne.cpp") # stany nocy 5.1.4, sloty wieczór i tydzień, rytm ekranu L, linie kolejki
TESTY+=("$FW/test/testy_sprzet_infekcja_sk.cpp") # SPRZĘT, INFEKCJA, pozostałe S-K
TESTY+=("$FW/test/testy_sprzet_infekcja_sk_brzegowe.cpp") # przypadki brzegowe: SPRZĘT, INFEKCJA, pozostałe S-K
TESTY+=("$FW/test/testy_alerty_odchylen.cpp") # alerty odchyleń od normy S-K-40…S-K-47, linia „dobra zmiana” (ADR-0039)
TESTY+=("$FW/test/testy_ekrany_zdarzen_e.cpp") # pełne ekrany zdarzeń E (S3), R-KON-14, linia „Wciąż”, koniec slotu dziennego
TESTY+=("$FW/test/testy_ekrany_zdarzen_e_brzegowe.cpp") # przypadki brzegowe: ekrany zdarzeń E
TESTY+=("$FW/test/testy_czynniki_odkrycia.cpp" "$FW/test/testy_czynniki_odkrycia_brzegowe.cpp")  # czynniki nocy, odkrycia i przypadki brzegowe
TESTY+=("$FW/test/testy_profil_publiczny.cpp" "$FW/test/testy_profil_publiczny_brzegowe.cpp") # profil publiczny (5.4.14, R-EKR-43, R-EKR-51)
TESTY+=("$FW/test/testy_aparat_autoset_brzegowe.cpp") # przypadki brzegowe: aparat AirSense AutoSet
TESTY+=("$FW/test/testy_poranek_szybciej.cpp") # szybszy wynik nocy rano: gęste okno, powtórka przy przetwarzaniu, pobranie po przycisku
TESTY+=("$FW/test/testy_poranek_brzegowe.cpp") # przypadki brzegowe szybszego wyniku nocy rano
TESTY+=("$FW/test/testy_zasilanie_rytm.cpp") # częstsze sprawdzanie rano na zasilaniu zewnętrznym (R-DANE-63…65)
TESTY+=("$FW/test/testy_ladowarka.cpp") # limit ładowania na kablu: rejestry SY6974B, nigdy wyżej niż zastane, formularz (ADR-0046)
TESTY+=("$FW/test/testy_ladowarka_brzegowe.cpp") # testy brzegowe limitu ładowania: własny model układu, błędy na każdym kroku, wpisy nieaktualne
TESTY+=("$FW/test/testy_ladowarka_brzegowe_2.cpp") # testy brzegowe limitu ładowania, część 2: własności zapisu, przegląd bajtów REG04 × wpis, sekwencje 3 wybudzeń z błędem
TESTY+=("$FW/test/testy_belka.cpp") # belka informacyjna: pola, ustępowanie środka, bateria, ekrany bez belki (ADR-0043)
TESTY+=("$FW/test/testy_belka_bez_kabla_brzegowe.cpp") # testy brzegowe belki: na kablu sama ikona (R-EKR-66, 16.11.2026)
TESTY+=("$FW/test/testy_zegar_ntp.cpp") # zegar nowego panelu, NTP 3 próby i czas z AirBridge (R-AW-18, T-TEST-327)
TESTY+=("$FW/test/testy_str_edf.cpp") # STR.edf: czytnik EDF, noce 'S', pierwszeństwo, STR w dyrygencie (ADR-0035)
TESTY+=("$FW/test/testy_str_edf_brzegowe.cpp") # przypadki brzegowe STR.edf
TESTY+=("$FW/test/testy_noc_pliki.cpp") # noc z plików sesji: zgodność z noce.py, brzegowe, dyrygent (ADR-0036)
TESTY+=("$FW/test/testy_noc_pliki_brzegowe.cpp") # przypadki brzegowe nocy z plików sesji
TESTY+=("$FW/test/testy_portret.cpp") # Portret nocy: odcinki wobec tools/portret, plik T-DANE-38, rysunek (ADR-0040)
TESTY+=("$FW/test/testy_porownanie.cpp") # tryb porównawczy: noc z plików wobec raportu JSON, U-05, /dane/porownanie.csv
TESTY+=("$FW/test/testy_porownanie_brzegowe.cpp") # przypadki brzegowe trybu porównawczego
TESTY+=("$FW/test/testy_aktualizacja.cpp" "$FW/test/testy_aktualizacja_dyrygent.cpp" "$FW/test/testy_aktualizacja_brzegowe.cpp") # aktualizacja z wydań GitHub (ADR-0032, ADR-0038) i przypadki brzegowe
TESTY+=("$FW/test/testy_kanal_wczesny.cpp") # kanał wczesny aktualizacji: lista wydań z pre-release (R-KONF-62, ADR-0032 zmiana 2026-11-23)
TESTY+=("$FW/test/testy_kanal_wczesny_brzegowe.cpp") # testy brzegowe kanału wczesnego: SemVer, nigdy w dół, przełączenie kanału
TESTY+=("$FW/test/testy_szybciej_przycisk.cpp") # szybsza zmiana ekranu po przycisku: liczniki i równoważność (T-WYM-99)
TESTY+=("$FW/test/testy_szybciej_przycisk_brzegowe.cpp") # testy brzegowe T-WYM-99: mutacje kluczy, testy różnicowe z pamięcią i bez
TESTY+=("$FW/test/testy_kabel_bez_snu.cpp") # czekanie na kablu zamiast głębokiego snu: równoważność, liczniki, awarie (ADR-0047)
TESTY+=("$FW/test/testy_kabel_bez_snu_brzegowe.cpp") # testy brzegowe ADR-0047: usterka odczytu a pamięć plików, losowe sekwencje
TESTY+=("$FW/test/testy_kabel_bez_snu_brzegowe_2.cpp") # testy brzegowe ADR-0047, część 2: lista katalogu i nowy rok, usterki nośnika na obu ścieżkach
TESTY+=("$FW/test/testy_otoczenie_zegar_kabel.cpp") # źródło otoczenia po czekaniu na kablu: dwie podstawy czasu, budżet odczytu ciała względny (T-TEST-519)
TESTY+=("$FW/test/testy_otoczenie_zegar_kabel_brzegowe.cpp") # testy brzegowe: otoczenie po czekaniu przy przepełnieniu millis() (T-TEST-519)
TESTY+=("$FW/test/testy_co2_cisza.cpp") # R-AW-41: linia E-08 przy ciszy samego źródła otoczenia (T-TEST-523)
TESTY+=("$FW/test/testy_odczyt_nieudany.cpp") # T-WYM-19: odczyt nieudany ≠ plik pusty; nieudane dopisanie w logu
TESTY+=("$FW/test/testy_odczyt_nieudany_brzegowe_2.cpp") # testy brzegowe odczytu nieudanego, część 2: legalne zapisy bez odmów, dwa lata, przełom roku
TESTY+=("$FW/test/testy_odczyt_nieudany_brzegowe.cpp") # testy brzegowe odczytu nieudanego: semantyka panelu (LittleFS.exists() = open())
TESTY+=("$FW/test/testy_plik_nieczytelny.cpp") # R-AW-40: trwale nieczytelny plik danych na U-05 i w „Otwartych sprawach” (T-TEST-520)
TESTY+=("$FW/test/testy_plik_nieczytelny_brzegowe_2.cpp") # testy brzegowe trwale nieczytelnego pliku, część 2 (w testy.sh blokujące)
TESTY+=("$FW/test/testy_plik_nieczytelny_brzegowe.cpp") # testy brzegowe trwale nieczytelnego pliku (w testy.sh blokujące)
TESTY+=("$FW/test/testy_kabel_ladowanie_brzegowe.cpp") # testy brzegowe: limit ładowania × czekanie na kablu, losowo na dwóch ścieżkach
TESTY+=("$FW/test/testy_kolejnosc_ekranow.cpp") # kolejność i widoczność ekranów z telefonu (ADR-0045), zgodność z bazą sprzed zmiany
TESTY+=("$FW/test/testy_norma_tymczasowa.cpp") # norma tymczasowa „sprzed zmiany” po zmianie ustawień: liczenie, K1, K2, reguły bez zmian (ADR-0048)
TESTY+=("$FW/test/testy_norma_tymczasowa_naiwna.cpp") # naiwna implementacja R-NOR-27 na losowych historiach (ADR-0048)
TESTY+=("$FW/test/testy_k1_ahi_niepewne.cpp") # K1: AHI `niepewne` / `duży wyciek` zamiast `uczę się` przy nocach niepewnych (R-KOK-37)
TESTY+=("$FW/test/testy_k1_ahi_niepewne_scenariusze.cpp") # scenariusze normy tymczasowej i R-KOK-37
TESTY+=("$FW/test/testy_k1_ahi_niepewne_naiwna.cpp") # naiwna R-KOK-37 na losowych historiach, wejścia jak w dyrygencie
TESTY+=("$FW/test/testy_odswiezanie_czesciowe.cpp") # rodzaj odświeżenia e-ink: pełne i częściowe (R-NF-25)
TESTY+=("$FW/test/testy_odswiezanie_czesciowe_brzegowe.cpp") # testy brzegowe R-NF-25: bez nowych odświeżeń wobec N = 0, licznik, kolejka, parser
TESTY+=("$FW/test/testy_kolejnosc_portret.cpp" "$FW/test/testy_kolejnosc_slot_dyrygent.cpp") # ADR-0045 z dyrygentem: Portret ▶ na pierwszą stronę, dzien.ukryte w slocie dziennym
INCLUDY=(-I"$FW/src" -I"$FW/include" -I"$FW/test/atrapy")
# Flagi ostrzeżeń. Pełny zestaw dla rdzenia i podglądu.
OSTRZEZENIA=(-Wall -Wextra -Werror -Wshadow -Wconversion -Wformat=2 -Wduplicated-cond -Wlogical-op -Wnull-dereference)
# test/testy.cpp: bez -Wshadow i -Wconversion: 29 ostrzeżeń w kodzie
# testów (zmienne `r`, `h` w zagnieżdżonych blokach scenariuszy, liczby całkowite w wektorach float);
# zmiana nazw w 15 zasłonięciach łatwo przestawia, do której zmiennej odwołuje się test, a zmiana testów
# wymaga uzasadnienia w opisie zmiany; ostrzeżenia nie dotyczą kodu panelu.
OSTRZEZENIA_TESTOW=(-Wall -Wextra -Werror -Wformat=2 -Wduplicated-cond -Wlogical-op -Wnull-dereference)
# src/main.cpp i src/adaptery_esp32.cpp (tylko składnia z atrapami nagłówków Arduino i ESP-IDF): pełny zestaw
# OSTRZEZENIA, z -Wconversion, (dyrygent jest w src/dyrygent.cpp).
TESTY+=("$FW/test/testy_przerywniki_s2d_brzegowe.cpp") # przypadki brzegowe: przerywniki S2d
TESTY+=("$FW/test/testy_parsowanie_liczb.cpp") # liczbaCalk/Dluga/Rzecz równoważne atoi/atol/atof (clang-tidy cert-err34-c)
TESTY+=("$FW/test/testy_sprzet_maska_filtr.cpp") # SPRZĘT tylko maska i filtr, migracja dat, filtr w tygodniach, formularz bez rury i komory
TESTY+=("$FW/test/testy_nawigacja.cpp") # nowa nawigacja (▶ dalej, ◀ wstecz, ↻ ekran dnia, przytrzymanie = zapis, strony n / 3, ADR-0041)
TESTY+=("$FW/test/testy_kolejka_szybkie.cpp") # szybkie naciśnięcia: kolejka z czasu liczenia i rysowania na modelu próbkowania (R-PRZ-20, T-TEST-516)
TESTY+=("$FW/test/testy_kolejka_szybkie_brzegowe.cpp") # testy brzegowe szybkich naciśnięć: serie losowe, pytanie tak/nie z kolejki i z czasu rysowania
TESTY+=("$FW/test/testy_nawigacja_mapa_brzegowe.cpp") # przypadki brzegowe: mapa nawigacji 5.6.2 tabelarycznie
TESTY+=("$FW/test/testy_sypialnia_30.cpp") # strona kokpitu „Sypialnia · 30 nocy” (R-KOK-29…R-KOK-33, ADR-0044)
TESTY+=("$FW/test/testy_formularz_u03.cpp") # formularz U-03 uzupełniony (miasto, CO2, humor, dźwięk, cisza, podpowiedź, czynniki)
TESTY+=("$FW/test/testy_alarm_baterii.cpp") # E-05: M-25 z napięcia, linia ≤ 15%, S5 ≤ 5%, R-PIE-07 (decyzja 05.11.2026)
TESTY+=("$FW/test/testy_konfiguracja_z_telefonu.cpp") # sekrety z NVS, formularz konfiguracji, pierwsza konfiguracja
TESTY+=("$FW/test/testy_konfiguracja_z_telefonu_brzegowe.cpp") # przypadki brzegowe: konfiguracja z telefonu
TESTY+=("$FW/test/testy_kokpit_legenda_u01_sprzet.cpp") # K1 legenda `↔ stabilnie`, U-01 do odczytu, „Sprzęt” z terminem, slot dzienny przy linii
TESTY+=("$FW/test/testy_scalenie_u01_stopka.cpp") # U-01: wiersz aktualizacji z dyrygenta, szerokość, odesłania do nazw widocznych na ekranie
TESTY+=("$FW/test/testy_k2_zwijanie_s7_liczby.cpp") # K2 z wierszami bez danych w jednej linii, S7, „Co znaczą te liczby”, stopka karty
TESTY+=("$FW/test/testy_slot_ustawienia_sprzet.cpp") # wykres slotu dziennego przy linii uwagi, U-01 i „Sprzęt” na wartościach granicznych
TESTY+=("$FW/test/testy_zaproszenie_linia_uwagi.cpp") # linia zaproszenia według ekranu dnia, droga do „Sprzętu”, linia uwagi
TESTY+=("$FW/test/testy_kokpit_karty_przekroj.cpp") # K2 we wszystkich trybach i brakach, krok S-K-35, stopka karty, pusty ekran spraw, E-23
TESTY+=("$FW/test/testy_siec_panelu_u02_brzegowe.cpp") # przypadki brzegowe: sieć panelu U-02
TESTY+=("$FW/test/testy_dobry_start.cpp" "$FW/test/testy_dobry_start_brzegowe.cpp") # program „Dobry start” (ADR-0049): silnik, dyrygent, ekran S-K-48, formularz
TESTY+=("$FW/test/testy_dobry_start_brzegowe_2.cpp" "$FW/test/testy_dobry_start_brzegowe_dyrygent.cpp") # testy brzegowe „Dobry start”, część 2 i z dyrygentem
TESTY+=("$FW/test/testy_dobry_start_v2.cpp") # program „Dobry start” w wersji 2 (R-DS-54…R-DS-71)
TESTY+=("$FW/test/testy_dobry_start_v2_brzegowe.cpp") # testy brzegowe „Dobry start” wersja 2
TESTY+=("$FW/test/testy_dobry_start_cisza_otoczenia.cpp" "$FW/test/testy_dobry_start_ocena_niepelna.cpp") # „Dobry start” z ciszą źródła otoczenia (R-AW-41) i oceną niepełną (R-AW-40, R-DS-60)
TESTY+=("$FW/test/testy_wysylka_obrazu.cpp") # obraz do sterownika e-ink blokiem: bajt w bajt jak pętla GxEPD2 1.6.9, kawałki
TESTY+=("$FW/test/testy_wysylka_obrazu_brzegowe.cpp") # testy brzegowe: długości brzegowe, wzorce, wyrównanie, strażnik bufora
# Programy testów, które do 22.11.2026 były tylko w testy.sh, bez sanitizerów i pokrycia:
# od 22.11.2026 każdy test/testy*.cpp jest w TESTY, a sprawdz_liste_testow() przerywa testy.sh, sanitizery i pokrycie,
# gdy pojawi się program spoza listy.
TESTY+=("$FW/test/testy_belka_znaki_brzegowe.cpp") # testy brzegowe znaków belki
TESTY+=("$FW/test/testy_ekrany_co2.cpp" "$FW/test/testy_ekrany_co2_brzegowe.cpp") # ekrany CO2, przerywniki S2d G01–G18
TESTY+=("$FW/test/testy_ekrany_serwisowe_brzegowe.cpp") # U-05, U-07, „Co nowego”, U-01 (czyta src/dyrygent.cpp: katalog $FW)
TESTY+=("$FW/test/testy_karta_po_aktualizacji.cpp" "$FW/test/testy_karta_po_aktualizacji_brzegowe.cpp") # R-KONF-61
TESTY+=("$FW/test/testy_karta_po_aktualizacji_portret.cpp" "$FW/test/testy_karta_po_aktualizacji_pytanie.cpp")
TESTY+=("$FW/test/testy_karta_po_aktualizacji_slot.cpp" "$FW/test/testy_karta_slot_brzegowe.cpp")
TESTY+=("$FW/test/testy_kokpit_k1_k2.cpp" "$FW/test/testy_kokpit_k1_k2_brzegowe.cpp") # K1 i K2 według trybu (R-KOK-16)
TESTY+=("$FW/test/testy_kolejnosc_przyciskow_stopki.cpp") # kolejność podpisów ◀ ▶ ↻ w stopkach
TESTY+=("$FW/test/testy_przerywniki_s2d.cpp") # przerywniki S2d (argument: katalog repozytorium)
TESTY+=("$FW/test/testy_raport_a4_brzegowe.cpp") # raport A4, serwer raportu, kod QR (argument: katalog na pliki)
TESTY+=("$FW/test/testy_rekord_nocy_brzegowe.cpp") # rekord nocy, wersja 4 pliku nocy
TESTY+=("$FW/test/testy_slot_dzienny_brzegowe.cpp") # slot dzienny i obrazki dnia
TESTY+=("$FW/test/testy_stany_nocy_sloty_wieczorne_brzegowe.cpp") # stany nocy, sloty wieczorny i tygodniowy
TESTY+=("$FW/test/testy_styki_modulow_brzegowe.cpp") # styki modułów (linie OCZEKUJE nie blokują)
TESTY+=("$FW/test/testy_trend_ps_podpisy_osi.cpp" "$FW/test/testy_trend_ps_podpisy_osi_brzegowe.cpp") # podpisy osi S3, trend PS
TESTY+=("$FW/test/testy_wieczor_bez_wifi_obrazki.cpp" "$FW/test/testy_wieczor_bez_wifi_obrazki_brzegowe.cpp") # ocena wieczorna bez Wi-Fi
TESTY+=("$FW/test/testy_wzorce_l_hipotezy_h.cpp" "$FW/test/testy_wzorce_l_hipotezy_h_brzegowe.cpp") # katalog L/H wobec spec
TESTY+=("$FW/test/testy_zdarzenia_e.cpp" "$FW/test/testy_zdarzenia_e_brzegowe.cpp") # zdarzenia E (brzegowe nie blokują, jak w testy.sh)
TESTY+=("$FW/test/testy_zrzut_konfiguracja_brzegowe.cpp") # konfiguracja w zrzucie Z
TESTY+=("$FW/test/testy_awarie_sieci.cpp") # awarie sieci: R-AW-35 rano, strażnik w sieci (T-WYM-81, R-AW-39), druga sesja nocy (R-DANE-24, R-DANE-69)
TESTY+=("$FW/test/testy_awarie_sieci_brzegowe.cpp") # przypadki brzegowe awarii sieci: R-KON-57 i R-DANE-69, R-AW-35

# Testy, których wynik ≠ 0 tylko wypisuje otwarte przypadki (tak samo w testy.sh); zgłoszenia sanitizera i tak
# przerywają bramkę (grep wyniku w tools/jakosc/sprawdz.sh).
TESTY_NIEBLOKUJACE=(testy_zdarzenia_e_brzegowe)

# Uruchomienie programu testu rdzenia z jego argumentami i katalogiem roboczym: JEDNO źródło argumentów dla
# tools/prototyp/testy.sh i tools/jakosc/sprawdz.sh (sanitizery, pokrycie; od 22.11.2026).
# $1 program, $2 przedrostek katalogów na pliki wyjściowe testów (testy.sh: katalog podglądu bez ukośnika na końcu;
# sprawdz.sh: katalog roboczy bramki). Wymaga REPO i FW. Zwraca kod wyjścia programu (0 dla TESTY_NIEBLOKUJACE).
# Programy z danymi z narzędzi Pythona (testy_str_edf, testy_noc_pliki, testy_portret) testy.sh uruchamia sam
# z katalogiem danych; tu (sanitizery, pokrycie) biegną bez niego i części z danymi pomijają, jak przed tą zmianą.
uruchom_test_rdzenia() {
  local prog="$1" wyj="$2" n kod=0 b
  n="$(basename "$prog")"
  case "$n" in
    testy_wzorce_l_hipotezy_h|testy_przerywniki_s2d|testy_trend_ps_podpisy_osi_brzegowe|testy_aparat_autoset_brzegowe)
      "$prog" "$REPO" || kod=$? ;;
    testy_raport_a4)
      "$prog" "$wyj-raport" || kod=$? ;;
    testy_raport_a4_brzegowe)
      mkdir -p "$wyj-raport-a4"
      "$prog" "$wyj-raport-a4" || kod=$? ;;
    testy_ekrany_zdarzen_e_brzegowe)
      mkdir -p "$wyj-ekrany-zdarzen-e"
      "$prog" "$wyj-ekrany-zdarzen-e" || kod=$? ;;
    testy_ekrany_serwisowe_brzegowe)
      (cd "$FW" && "$prog") || kod=$? ;;  # czyta src/dyrygent.cpp (przypadek [i])
    testy_norma_tymczasowa_naiwna)
      "$prog" 30000 || kod=$? ;;
    testy_k1_ahi_niepewne_naiwna)
      "$prog" 3000 0 || kod=$?   # wejścia spójne
      [ "$kod" = 0 ] && { "$prog" 3000 1 || kod=$?; } ;;  # wejścia jak w dyrygencie (okresy infekcji)
    *)
      "$prog" || kod=$? ;;
  esac
  for b in "${TESTY_NIEBLOKUJACE[@]}"; do
    if [ "$b" = "$n" ]; then
      [ "$kod" = 0 ] || echo " są otwarte przypadki (wyżej)"
      return 0
    fi
  done
  return "$kod"
}

# Każdy program test/testy*.cpp jest w TESTY, żeby nowy test nie wypadł z sanitizerów i pokrycia (do 22.11.2026 28
# programów było tylko w testy.sh). Woła testy.sh na starcie i sprawdz.sh przed sanitizerami i pokryciem.
sprawdz_liste_testow() {
  local t x jest braki=()
  for t in "$FW"/test/testy*.cpp; do
    jest=0
    for x in "${TESTY[@]}"; do [ "$x" = "$t" ] && jest=1 && break; done
    [ "$jest" = 1 ] || braki+=("${t#"$FW"/}")
  done
  if [ "${#braki[@]}" -gt 0 ]; then
    echo "BŁĄD: programy testów spoza listy TESTY w tools/prototyp/zrodla.sh (bez sanitizerów i pokrycia): ${braki[*]}" >&2
    return 1
  fi
  return 0
}
