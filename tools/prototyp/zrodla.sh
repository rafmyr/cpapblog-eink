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
# Aktualizacja firmware z wydań GitHub (ADR-0032, ADR-0038): rdzeń decyzji, wersji, opisu wydania i SHA-256
ZRODLA+=("$FW/src/aktualizacja.cpp")
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
TESTY+=("$FW/test/testy_raport_a4.cpp") # raport A4, kod QR, serwer raportu, ściąga
TESTY+=("$FW/test/testy_stany_nocy_sloty_wieczorne.cpp") # stany nocy 5.1.4, sloty wieczór i tydzień, rytm ekranu L, linie kolejki
TESTY+=("$FW/test/testy_sprzet_infekcja_sk.cpp") # SPRZĘT, INFEKCJA, pozostałe S-K
TESTY+=("$FW/test/testy_sprzet_infekcja_sk_brzegowe.cpp") # przypadki brzegowe: SPRZĘT, INFEKCJA, pozostałe S-K
TESTY+=("$FW/test/testy_ekrany_zdarzen_e.cpp") # pełne ekrany zdarzeń E (S3), R-KON-14, linia „Wciąż”, koniec slotu dziennego
TESTY+=("$FW/test/testy_ekrany_zdarzen_e_brzegowe.cpp") # przypadki brzegowe: ekrany zdarzeń E
TESTY+=("$FW/test/testy_czynniki_odkrycia.cpp" "$FW/test/testy_czynniki_odkrycia_brzegowe.cpp")  # czynniki nocy, odkrycia i przypadki brzegowe
TESTY+=("$FW/test/testy_profil_publiczny.cpp" "$FW/test/testy_profil_publiczny_brzegowe.cpp") # profil publiczny (5.4.14, R-EKR-43, R-EKR-51)
TESTY+=("$FW/test/testy_aparat_autoset_brzegowe.cpp") # przypadki brzegowe: aparat AirSense AutoSet
TESTY+=("$FW/test/testy_poranek_szybciej.cpp") # szybszy wynik nocy rano: gęste okno, powtórka przy przetwarzaniu, pobranie po przycisku
TESTY+=("$FW/test/testy_poranek_brzegowe.cpp") # przypadki brzegowe szybszego wyniku nocy rano
TESTY+=("$FW/test/testy_zegar_ntp.cpp") # zegar nowego panelu, NTP 3 próby i czas z AirBridge (R-AW-18, T-TEST-327)
TESTY+=("$FW/test/testy_str_edf.cpp") # STR.edf: czytnik EDF, noce 'S', pierwszeństwo, STR w dyrygencie (ADR-0035)
TESTY+=("$FW/test/testy_str_edf_brzegowe.cpp") # przypadki brzegowe STR.edf
TESTY+=("$FW/test/testy_noc_pliki.cpp") # noc z plików sesji: zgodność z noce.py, brzegowe, dyrygent (ADR-0036)
TESTY+=("$FW/test/testy_noc_pliki_brzegowe.cpp") # przypadki brzegowe nocy z plików sesji
TESTY+=("$FW/test/testy_porownanie.cpp") # tryb porównawczy: noc z plików wobec raportu JSON, U-05, /dane/porownanie.csv
TESTY+=("$FW/test/testy_porownanie_brzegowe.cpp") # przypadki brzegowe trybu porównawczego
TESTY+=("$FW/test/testy_aktualizacja.cpp" "$FW/test/testy_aktualizacja_dyrygent.cpp" "$FW/test/testy_aktualizacja_brzegowe.cpp") # aktualizacja z wydań GitHub (ADR-0032, ADR-0038) i przypadki brzegowe
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
TESTY+=("$FW/test/testy_nawigacja.cpp") # nowa nawigacja (▶ dalej, ◀ wstecz, ↻ ekran dnia, przytrzymanie = zapis, strony n / 6)
TESTY+=("$FW/test/testy_nawigacja_mapa_brzegowe.cpp") # przypadki brzegowe: mapa nawigacji 5.6.2 tabelarycznie
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
