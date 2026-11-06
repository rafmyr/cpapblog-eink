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
TESTY+=("$FW/test/testy_zegar_ntp.cpp") # zegar nowego panelu, NTP 3 próby i czas z AirBridge (R-AW-18, T-TEST-327)
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
TESTY+=("$FW/test/testy_siec_panelu_u02_brzegowe.cpp") # przypadki brzegowe: sieć panelu U-02
