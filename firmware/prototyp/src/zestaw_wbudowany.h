// SPDX-License-Identifier: GPL-3.0-or-later
// WYGENEROWANE przez tools/zestaw/buduj_zestaw.py ze spec funkcjonalnej, docs/spec/progi.csv
// i docs/spec/katalog/ (spec techniczna 9.3, ADR-0023). Nie edytuj ręcznie: zmień źródła i uruchom skrypt.
// Zestaw wbudowany w firmware; parsuje go reguly::zestawWbudowany() przy pierwszym wywołaniu.
#pragma once

namespace reguly {
namespace wbudowany {
static const char PROGI[] = R"zestaw(#cpap-eink progi 1 zestaw=z-4c871f19ae16 pochodzenie=wbudowany firmware=1.0.0..1.99.99 tryby=CPAP,AutoSet,ASV,ASVAuto zamrozenie=brak zrodlo=67c59237a7f3 skrot=sha256:4580060d3e188fdf62119e14e3a0d0228ef9d8b7715d28aa1bf4ceca90821cb1
id_progu;wartosc;jednostka;tryb
R-DANE-13.m01_min;4;h;wszystkie
R-DANE-27.start_nocy;18:00;godzina;wszystkie
R-DANE-27.wczesny_sen_min;4;h;wszystkie
R-DANE-23.przeciek_nosowa;24;L/min;wszystkie
E-23.przeciek_p95;27;L/min;wszystkie
R-SKI-32.spo2_min_proba;50;%;wszystkie
R-SKI-32.spadek_max;4;pkt proc./s;wszystkie
R-SKI-32.odcinek_min;10;s;wszystkie
R-SKI-20.pokrycie_spo2;70;%;wszystkie
R-SKI-27.noc_z_terapia_m01;15;min;wszystkie
R-SKI-24.noce_nieczytelne;4/7;noce (k z n);wszystkie
R-DANE-29.tryb_obslugiwany;CPAP,AutoSet,ASV,ASVAuto;tag MOP;wszystkie
M-21.pola_ustawien;EAI,EAX,ANS,AXS;tagi;ASVAuto
M-21.pola_ustawien;IPC,EPR;tagi;CPAP
M-21.pola_ustawien;MPI,MPA,EPR;tagi;AutoSet
M-21.pola_ustawien;EEP,MNS,MXS;tagi;ASV
M-21.maksimum_cisnienia;EAX;cmH2O (wzór);ASVAuto
M-21.maksimum_cisnienia;MPA;cmH2O (wzór);AutoSet
H-01.minimum_cisnienia;EAI;tag;ASVAuto
H-01.minimum_cisnienia;MPI;tag;AutoSet
H-01.cisnienie;M-06;metryka;ASVAuto
E-36.sygnal_cisnienia;M-06p;metryka;ASV,ASVAuto
E-36.sygnal_cisnienia;M-05p;metryka;CPAP,AutoSet
E-36.slowo_cisnienia;EPAP;słowo;ASV,ASVAuto
E-36.slowo_cisnienia;Ciśnienie;słowo;CPAP,AutoSet
R-NOR-07.zmiana_ustawien;MOP,EAI,EAX,ANS,AXS;tagi;ASVAuto
R-NOR-07.zmiana_ustawien;MOP,IPC,EPR;tagi;CPAP
R-NOR-07.zmiana_ustawien;MOP,MPI,MPA,IPC,EPR;tagi;AutoSet
R-NOR-07.zmiana_ustawien;MOP,EEP,MNS,MXS;tagi;ASV
L-04.wariant;EPAP (wariant 2);wariant;ASVAuto
WY16.data_zamrozenia;brak;data;wszystkie
R-NOR-12.delta.M-01;-1.5;h;wszystkie
R-NOR-12.delta.M-02;3;/h;wszystkie
R-NOR-12.delta.M-03;1;/h;wszystkie
R-NOR-12.delta.M-05p;2;cmH2O;wszystkie
R-NOR-12.delta.M-06p;2;cmH2O;ASV,ASVAuto
R-NOR-12.delta.M-07;10;L/min;wszystkie
R-NOR-12.delta.M-11;0.3;GI;wszystkie
R-NOR-12.delta.M-13;2;/min;wszystkie
R-NOR-12.delta.M-17;2;przerwy;wszystkie
R-NOR-12.delta.M-14;60;mL;wszystkie
R-NOR-12.delta.M-15;1.0;L/min;wszystkie
R-NOR-03.wejscie;3/5;noce (k z n);wszystkie
R-NOR-04.wyjscie_noce;3;noce;wszystkie
R-NOR-04.wyjscie_ulamek_delta;0.5;× Δ;wszystkie
R-NOR-06.nowy_poziom;21;noce;wszystkie
R-NOR-20.metryka_zdrowotna;21;noce;wszystkie
R-NOR-21.linia_od_nocy;21;noce;wszystkie
R-NOR-21.linia_co_dni;7;dni;wszystkie
R-EKR-36.p90_ahi;5;/h;wszystkie
R-EKR-36.p90_ca;2;/h;CPAP,AutoSet
E-05.bateria_linia;15;%;wszystkie
E-05.bateria_krytyczna;5;%;wszystkie
E-05.bateria_koniec;20;%;wszystkie
E-07.noce_bez_czujnika;3;noce;wszystkie
E-10.noc_bez_terapii_m01;15;min;wszystkie
E-10.noce_problem;2;noce;wszystkie
E-11.terapia_min;15;min;wszystkie
R-EKR-55.terapia_min;15;min;wszystkie
E-11.krotka_noc_max;4;h;wszystkie
E-12.noc_min;4;h;wszystkie
E-12.odsetek_min;70;%;wszystkie
E-12.historia_min;14;dni;wszystkie
E-12.okno_po_przerwie;30;dni;wszystkie
E-12.wyjscie_dni;3;dni;wszystkie
E-20.odsetek_czasu;10;%;wszystkie
E-20.minuty;30;min;wszystkie
E-21.wilgotnosc_linia;35;%;wszystkie
E-22.wejscie;3/5;noce (k z n);wszystkie
E-22.wyjscie;3;noce;wszystkie
E-30.ahi;5;/h;wszystkie
E-30.okno;7;noce;wszystkie
E-31.krok_lekarz;21;noce;wszystkie
E-33.krok_lekarz;21;noce;wszystkie
E-33.mediana_ca;3;/h;wszystkie
E-33.okno;5;noce;wszystkie
E-36.noce;5/7;noce (k z n);wszystkie
E-36.krok_lekarz;21;noce;wszystkie
E-37.bilans_roznica;20;%;wszystkie
E-37.bilans_minuty;30;min;wszystkie
E-37.bilans_noce;3/5;noce (k z n);wszystkie
L-01.srednia_ca;5;/h;wszystkie
L-01.noce_ca;3/7;noce (k z n);wszystkie
L-01.wyjscie_max;3;/h;wszystkie
L-01.wyjscie_noce;7;noce;wszystkie
L-01.ponowne_wejscie;28;noce;wszystkie
L-01.karencja_okno;56;dni;CPAP,AutoSet
L-01.karencja_noc;14;noc trwania;CPAP,AutoSet
L-01.karencja_ca;10;/h;CPAP,AutoSet
L-01.asv_noce_po_zmianie;7;noce;ASV,ASVAuto
L-02.a_noce;3/14;noce (k z n);wszystkie
L-02.a_minuty_noc;20;min;wszystkie
L-02.a_wyjscie;2/14;noce (k z n);wszystkie
L-02.b_mnoznik;2;× norma;wszystkie
L-02.b_roznica;1;pkt proc.;wszystkie
L-02.b_wyjscie_mnoznik;1.25;× norma;wszystkie
L-02.b_wyjscie_noce;7;noce;wszystkie
L-03.okno;14;noce;wszystkie
L-03.ahi_wysokie;10;/h;wszystkie
L-03.min_noce;10;noce;wszystkie
L-03.wyjscie_wysokie;7;/h;wszystkie
L-03.okno_srednie;28;noce;wszystkie
L-03.ahi_srednie_dol;5;/h;wszystkie
L-03.ahi_srednie_gora;10;/h;wszystkie
L-03.sennosc;2/4;tygodnie (k z n);wszystkie
L-04.margines_max;0.5;cmH2O;AutoSet
L-04.margines_max;0.5;cmH2O;ASVAuto
L-04.noce;5/7;noce (k z n);AutoSet
L-04.noce;5/7;noce (k z n);ASVAuto
L-04.zdarzenia_obturacyjne;5;/h;AutoSet
L-04.zdarzenia_obturacyjne;5;/h;ASVAuto
L-04.wyjscie_noce;3/7;noce (k z n);AutoSet
L-04.wyjscie_noce;3/7;noce (k z n);ASVAuto
L-04.wyjscie_zdarzenia;3;/h;AutoSet
L-04.wyjscie_zdarzenia;3;/h;ASVAuto
L-05.wzrost;3;/min;wszystkie
L-05.noce;3;noce;wszystkie
L-05.wyjscie_wzrost;1.5;/min;wszystkie
L-05.wyjscie_noce;3;noce;wszystkie
L-06.terapia_min;15;min;wszystkie
L-06.spo2_88;88;%;wszystkie
L-06.spo2_90;90;%;wszystkie
L-06.a_t88;5;min;wszystkie
L-06.a_noce;3/7;noce (k z n);wszystkie
L-06.b_t88;30;min;wszystkie
L-06.b_noce;2/3;noce (k z n);wszystkie
L-06.c_mnoznik;2;× mediana;wszystkie
L-06.c_roznica;10;min;wszystkie
L-06.c_noce;3/7;noce (k z n);wszystkie
L-06.wyjscie;2/7;noce (k z n);wszystkie
L-07.wzrost_m05p;2;cmH2O;wszystkie
L-07.czas;14;noce;wszystkie
L-08.e12_dni;28;dni;wszystkie
L-08.szybki_m19;50;%;wszystkie
L-08.szybki_m19_wczesniej;90;%;wszystkie
L-08.wyjscie_m19;70;%;wszystkie
L-08.wyjscie_dni;14;dni;wszystkie
L-09.e22_noce;21;noce;wszystkie
L-09.sk10_noce;10/14;noce (k z n);wszystkie
L-09.wyjscie_noce;7;noce;wszystkie
L-10.tygodnie;2;tygodnie;wszystkie
L-10.wyjscie_tygodnie;2;tygodnie;wszystkie
L-10.pilnosc_b_tygodnie;4;tygodnie;wszystkie
L-11.sennosc;3/4;tygodnie (k z n);wszystkie
L-11.okno_ahi;28;noce;wszystkie
L-11.ahi_max;5;/h;wszystkie
L-11.m19_min;70;%;wszystkie
L-11.wariant_2tyg;2;tygodnie;wszystkie
L-11.wariant_2tyg_okno;4;tygodnie;wszystkie
L-11.wyjscie;1/4;tygodnie (k z n);wszystkie
L-12.okno;14;noce;wszystkie
L-12.ahi;3;/h;wszystkie
L-12.m01;-1;h;wszystkie
L-12.m07_p95;10;L/min;wszystkie
R-LEK-10.delta_ahi;3;/h;wszystkie
R-LEK-10.delta_ca;2;/h;wszystkie
R-LEK-10.delta_t88;10;min;wszystkie
R-LEK-10.delta_csr;1;pkt proc.;wszystkie
H-01.margines_min;1;cmH2O;wszystkie
H-01.mnoznik;2;× częstość;wszystkie
H-01.noce;8/14;noce (k z n);wszystkie
H-02.maksimum_tryb;15;cmH2O;wszystkie
H-03.odsetek;40;%;wszystkie
H-03.start_min;20;min;wszystkie
H-03.noce;6/14;noce (k z n);wszystkie
H-08.okno;56;noce;wszystkie
H-06.noce;4/7;noce (k z n);wszystkie
H-09.m01;6;h;wszystkie
R-EKR-26.grupa_min;5;noce;wszystkie
O-03.historia;30;dni;wszystkie
O-05.odsetek;60;%;wszystkie
O-05.noce;5/7;noce (k z n);wszystkie
O-06.co2;1200;ppm;wszystkie
O-07.wilgotnosc;35;%;wszystkie
O-09.start;00:30;godzina;wszystkie
O-10.okno;30;min;wszystkie
O-10.noce;5;noce;wszystkie
O-12.grupa_min;5;noce;wszystkie
O-12.efektywna_min;5;noce;wszystkie
R-CZY-24.waga_kontrolna;1/7;prawdopodobieństwo;wszystkie
R-CZY-24.okno_odpowiedzi;90;dni;wszystkie
R-CZY-21.mala_grupa;10;noce;wszystkie
S-K-01.co2;1000;ppm;wszystkie
S-K-01.czas;2;h;wszystkie
S-K-01.co2_biezacy;1000;ppm;wszystkie
R-EKR-54.co2_mediana;1000;ppm;wszystkie
S-K-02.pm25;15;µg/m³;wszystkie
S-K-03.temperatura;16;°C;wszystkie
S-K-04.wilgotnosc;35;%;wszystkie
S-K-05.prognoza_max;29;°C;wszystkie
S-K-05.sypialnia;26;°C;wszystkie
S-K-06.pylek;10;% górnych wartości sezonu;wszystkie
R-KON-18.grupa_min;5;noce;wszystkie
S-K-07.sd_startu;60;min;wszystkie
S-K-08.m01;30;min;wszystkie
S-K-09.weekendy;4;weekendy;wszystkie
S-K-09.roznica;30;min;wszystkie
S-K-09.grupa_min;5;noce;wszystkie
S-K-10.przeciek_nosowa;24;L/min;wszystkie
S-K-10.problem_p95;15;L/min;wszystkie
S-K-10.uwaga_o_masce;10;L/min;wszystkie
R-KONF-42.zakres_min;10;L/min;wszystkie
R-KONF-42.zakres_max;15;L/min;wszystkie
R-KON-13.kilka_nocy;3/7;noce (k z n);wszystkie
S-K-11.epizody;5;epizody;wszystkie
S-K-11.wzrost;20;% tła;wszystkie
S-K-12.mediana_przecieku;30;L/min;wszystkie
S-K-12.terapia_min;15;min;wszystkie
S-K-12.terapia_max;2;h;wszystkie
S-K-13.co2;1000;ppm;wszystkie
S-K-13.czas;4;h;wszystkie
S-K-13.co2_max;2000;ppm;wszystkie
S-K-14.wilgotnosc;35;%;wszystkie
S-K-15.sypialnia;26;°C;wszystkie
S-K-15.za_oknem;29;°C;wszystkie
S-K-15.m01;4;h;wszystkie
S-K-16.m17;3;przerwy;wszystkie
S-K-17.sygnaly;2/5;sygnały (k z n);wszystkie
S-K-17.noce;2;noce;wszystkie
S-K-18.rekordy;5/7;noce (k z n);wszystkie
S-K-19.gi_mnoznik;1.5;× mediana;wszystkie
S-K-19.gi_baza;14;noce;wszystkie
S-K-19.gi_okno;30;noce;wszystkie
S-K-19.tymczasowy_dodatek;0.1;FlowLim;wszystkie
R-KON-30.po_zmianie;14;noce;wszystkie
S-K-21.okno;60;noce;wszystkie
S-K-21.grupa_min;5;noce;wszystkie
S-K-21.roznica;30;min;wszystkie
S-K-27.odsetek;70;%;wszystkie
S-K-27.okres;90;dni;wszystkie
S-K-28.kamienie;7,30,100,180,365;noce;wszystkie
S-K-29.noce_bez;3;noce;wszystkie
S-K-30.wzrost;2;/min;wszystkie
S-K-30.noce;2;noce;wszystkie
S-K-30.m05p;2;cmH2O;wszystkie
S-K-30.puls;5;/min;wszystkie
R-INF-06.koniec_wzrost;1;/min;wszystkie
R-INF-06.koniec_noce;2;noce;wszystkie
R-INF-04.maks_noce;14;noce;wszystkie
R-INF-04.karencja;14;noce;wszystkie
R-INF-07.wzrost;3;/min;wszystkie
S-K-31.delta;10;L/min;wszystkie
S-K-31.noce_wzrostu;14;noce;wszystkie
R-MAS-03.pokazania;4;pokazania;wszystkie
R-MAS-08.po_wymianie;14;noce;wszystkie
R-MAS-12.noce;3;noce;wszystkie
R-MAS-12.spadek;8;L/min;wszystkie
S-K-32.delta;10;L/min;wszystkie
WY16.zrodlo_nocy;do zadeklarowania;źródło;wszystkie
WY16.dlugosc_okresu_nocy;30;noce;wszystkie
WY16.skrypt_commit;do zadeklarowania;commit;wszystkie
R-NOR-01.okno_nocy;30;noce;wszystkie
R-NOR-09.okno_dni;90;dni;wszystkie
R-NOR-10.min_noce;14;noce;wszystkie
R-NOR-07.rozruch_po_zmianie;14;noce;wszystkie
R-NOR-05.niepewna_odsetek;10;%;wszystkie
R-NOR-05.niepewna_minuty;30;min;wszystkie
R-NOR-05.niepewna_mediana;30;L/min;wszystkie
R-NOR-05.niepewna_p95;15;L/min;wszystkie
R-CZY-19.przerwa;2;doby;wszystkie
R-CZY-19.okno_ostatniej_nocy;23:30;godzina;wszystkie
R-CZY-02.grupa_kontrolna;1/7;prawdopodobieństwo;wszystkie
R-KON-23.przeciek_noce;3/7;noce (k z n);wszystkie
R-KON-23.przerwy_noce;3/7;noce (k z n);wszystkie
R-KON-23.m01;6;h;wszystkie
R-KON-23.n4;5/7;noce (k z n);wszystkie
R-MAS-09.grupa_min;5;noce;wszystkie
R-MAS-12.odstep;30;dni;wszystkie
R-MAS-10.pokazania;4;pokazania;wszystkie
R-SKI-34.noce_bez;3;noce;wszystkie
R-SKI-34.czekanie_dni;7;dni;wszystkie
R-SKI-39.pytania;4;pytania;wszystkie
R-INF-07.od_nocy;3;noc okresu;wszystkie
R-INF-07.co_dni;3;dni;wszystkie
R-SKI-18.gorzej_odstep;3;dni;wszystkie
R-KON-14.pokazania;2/7;pokazania w dniach;wszystkie
R-KON-18.limit_dni;7;dni;wszystkie
R-KON-19.limit_dni;7;dni;wszystkie
R-KON-20.limit_dni;7;dni;wszystkie
R-KON-21.okres;90;dni;wszystkie
R-KON-29.sezon;1 października–30 kwietnia;daty;wszystkie
R-KON-29.limit_dni;7;dni;wszystkie
R-KON-29.pokazania;4;pokazania;wszystkie
R-KON-29.grupa_min;5;noce;wszystkie
R-SKI-09.b_dni;28;dni;wszystkie
R-SKI-10.a_co_dni;3;dni;wszystkie
R-SKI-10.po_omowione;14;noce;wszystkie
R-SKI-11.po_omowione;56;noce;wszystkie
R-LEK-09.wyciszenie;56;noce;wszystkie
R-LEK-25.drugie;28;noce;wszystkie
R-LEK-27.pelny_ekran;14;noce;wszystkie
R-EKR-25.odkrycia;7;dni;wszystkie
R-EKR-27.powrot;30;dni;wszystkie
R-EKR-18.noce_miesiaca;20;noce;wszystkie
R-EKR-20.noce_miesiaca;20;noce;wszystkie
R-EKR-33.noce;30;noce;wszystkie
R-KOK-12.margines;0.5;cmH2O;wszystkie
R-KOK-12.margines;0.5;cmH2O;ASVAuto
R-KOK-12.noce;3/7;noce (k z n);wszystkie
R-KOK-16.trend_delta;0.5;× Δ;wszystkie
R-KOK-16.noce_7;4;noce;wszystkie
R-KOK-16.noce_23;14;noce;wszystkie
R-KOK-16.delta_ps;2;cmH2O;ASV,ASVAuto
R-AW-04.pokrycie_przeplywu;80;%;wszystkie
H-01.gi_okno;30;min;wszystkie
S-K-20.wzrost_m05p;2;cmH2O;wszystkie
S-K-20.czas;14;noce;wszystkie
S-K-21.co2;1000;ppm;wszystkie
S-K-21.temperatura;26;°C;wszystkie
S-K-21.pylek;10;% górnych wartości sezonu;wszystkie
R-NOR-23.krok_lekarz;21;noce;wszystkie
R-LEK-24.linia_co_dni;3;dni;wszystkie
R-SKI-37.linia_co_dni;3;dni;wszystkie
R-SKI-09.b_co_dni;7;dni;wszystkie
R-SKI-36.linia_dni;3;dni;wszystkie
R-INF-10.linia_co_dni;3;dni;wszystkie
S-K-31.limit_dni;7;dni;wszystkie
R-PRZ-03.wiem_problem;7;dni;wszystkie
R-PRZ-03.wiem_a;3;dni;wszystkie
R-PIE-10.prosby_dziennie;1;prośby;wszystkie
R-PIE-12.wieczory;4;wieczory;wszystkie
R-PIE-12.okno;7;dni;wszystkie
R-HUM-02.karty_dobe;2;karty na dobę;wszystkie
R-HUM-23.udzial_kart;0.5;× wyświetlenia;wszystkie
R-HUM-23.min_wyswietlen;7;wyświetlenia (minimum w max);wszystkie
R-HUM-27.pula_min;3;zdania;wszystkie
R-HUM-20.zdanie_powrot;30;dni;wszystkie
R-HUM-09.dowod_spokoju;co trzecie;wyświetlenie;wszystkie
R-HUM-07.karta_1_na_2;1/2;wyświetlenia (k z n);wszystkie
R-HUM-10.sceny_dni;6;dni;wszystkie
R-HUM-14.przerywnik_dni;7;dni;wszystkie
R-DANE-09.odczyt;15;min;wszystkie
R-DANE-10.odczyt;15;min;wszystkie
WY3.falszywe_max;1;ekrany;wszystkie
WY3.okno;14;noce;wszystkie
WY3.tryb_strojenia;ASVAuto;tag MOP;ASVAuto
WY4.odkrycia_nieuzyteczne;1;odkrycia;wszystkie
WY4.odrzucone_uzyteczne;1;kandydaci;wszystkie
WY5.przypadki_grupa;5;przypadki;wszystkie
WY6.wzorce_ab;2;miesiące;wszystkie
WY9.noce;10;noce;wszystkie
WY9.tol.M-02;0.5;/h;wszystkie
WY9.tol.cisnienia;0.5;cmH2O;wszystkie
WY9.tol.M-07;2;L/min;wszystkie
WY9.tol.M-08_M-09;2;min;wszystkie
WY9.tol.M-01;1;min;wszystkie
WY9.tol.M-10;0.02;FlowLim;wszystkie
WY9.tol.M-13;0.5;/min;wszystkie
WY9.tol.M-14;20;mL;wszystkie
WY9.tol.M-15;0.3;L/min;wszystkie
WY9.tol.M-16;10;%;wszystkie
WY9.tol.M-18;5;min;wszystkie
WY9.tol.M-24;1;pkt proc.;wszystkie
WY9.tol.T88_T90;2;min;wszystkie
WY11.tryb_probny;2–3;miesiące;wszystkie
WY15.pula_min;3;zdania;wszystkie
R-DANE-08.tolerancja_gi;0.05;GI;wszystkie
R-GI-01.tolerancja_cech;0.05;GI;wszystkie
R-EKR-16.seria_min;7;noce;wszystkie
S-K-33.pm25_czysta;35;µg/m3;wszystkie
S-K-33.zima_roznica;8;°C;wszystkie
S-K-33.zima_szczyt;2;°C;wszystkie
S-K-33.lato_dom;24;°C;wszystkie
S-K-33.lato_roznica;2;°C;wszystkie
R-KON-43.horyzont;23:00;godzina;wszystkie
S-K-34.okno_od;06:00;godzina;wszystkie
S-K-34.okno_do;09:00;godzina;wszystkie
S-K-34.slisko_opad;50;%;wszystkie
S-K-34.deszcz_opad;60;%;wszystkie
S-K-34.wiatr;60;km/h;wszystkie
S-K-35.przed_terminem;7;dni;wszystkie
S-K-35.co_ile;2;dni;wszystkie
S-K-36.pylek;10;% górnych wartości sezonu;wszystkie
R-KON-46.od_godziny;16:00;godzina;wszystkie
R-KON-46.okno_wkrotce;60;min;wszystkie
R-KON-50.blok_obrazka;120;min;wszystkie
R-KON-60.zimno;5;°C;wszystkie
R-KON-60.brzydka_zachmurzenie;90;%;wszystkie
R-KON-60.brzydka_udzial;50;% godzin horyzontu;wszystkie
R-LEK-35.ca_bezwzgledne;5;/h;wszystkie
R-LEK-35.ahi_bezwzgledne;10;/h;wszystkie
L-01.okno;7;noce;wszystkie
L-02.b_okno;7;noce;wszystkie
L-04.okno;7;noce;AutoSet
L-04.okno;7;noce;ASVAuto
L-07.okno;7;noce;wszystkie
S-K-19.tymczasowy_okno;7;noce;wszystkie
L-02.a_okno;14;noce;wszystkie
L-11.wyjscie_odpowiedzi;3;tygodnie z odpowiedzią;wszystkie
O-03.noce;7;noce;wszystkie
O-04.tydzien;7;noce;wszystkie
O-04.miesiac;30;noce;wszystkie
R-EKR-26.okno;90;noce;wszystkie
R-CZY-24.warstwa_min;1;noce;wszystkie
R-NOR-14.oczekiwanie;14;dni;wszystkie
R-HUM-28.ahi_bardzo_niskie;1;AHI/h;wszystkie
R-HUM-28.ahi_duzo_zdarzen;10;AHI/h;wszystkie
R-HUM-28.terapia_dluga;7;h;wszystkie
R-HUM-28.terapia_w_celu;6;h;wszystkie
R-HUM-28.tydzien_noce;5;noce;wszystkie
R-HUM-28.wyciek_niewielki;10;L/min;wszystkie
R-HUM-28.poprawa_m01;1;h;wszystkie
R-HUM-28.poprawa_noce;5;noce;wszystkie
M-31.okno_po_bezdechu;10;s;ASV,ASVAuto
M-31.margines_ps;0.5;cmH2O;ASV,ASVAuto
M-32.margines_epap;0.5;cmH2O;ASVAuto
M-33.cel_wentylacji;90;%;ASV,ASVAuto
R-NOR-12.delta.M-31;1;/h;ASV,ASVAuto
R-DANE-52.ua_dolna_granica;1;/h;CPAP,AutoSet
L-01.asv_bezdechy;5;/h;ASV,ASVAuto
L-01.asv_noce;3/7;noce (k z n);ASV,ASVAuto
L-01.asv_udzial_pelne;50;%;ASV,ASVAuto
L-01.asv_wyjscie;3;/h;ASV,ASVAuto
L-04.bezdechy_max_epap;5;/h;ASVAuto
L-04.wyjscie_bezdechy_max_epap;3;/h;ASVAuto
E-33.asv_mediana;3;/h;ASV,ASVAuto
R-LEK-40.bezdechy_bezwzgledne;5;/h;ASV,ASVAuto
M-31.pmax_aparatu;25;cmH2O;ASV,ASVAuto
R-NOR-12.delta.M-30;1;/h;ASV,ASVAuto
L-01.asv_min_noce;3;noce;ASV,ASVAuto
)zestaw";
static const char KATALOG[] = R"zestaw(#cpap-eink katalog 1 zestaw=z-4c871f19ae16 pochodzenie=wbudowany firmware=1.0.0..1.99.99 tryby=CPAP,AutoSet,ASV,ASVAuto zamrozenie=brak zrodlo=67c59237a7f3 skrot=sha256:8f5699e078953d621d446bc4fff26b88a80a699a5779fe59f830a0e60b64281a
id_reguly;wariant;rodzaj;parametry;liczone_noce;okno;poziom;grupa;slot;tryby;wymaga_normy;w_rozruchu;wyklucza;wymaga;bramki;limit;teksty;stan;pilnosc
E-01;;sygnal_komponentu;sygnal=brak_danych;;;brak_danych;R1;rano;wszystkie;nie;tak;;;;;;zablokowana;
E-03;;sygnal_komponentu;sygnal=nieczytelny_raport;;;brak_danych;R1;rano;wszystkie;nie;tak;;;;;;zablokowana;
E-05;krytyczne;prog_biezacy_wyjscie;metryka=M-25,prog=E-05.bateria_krytyczna,wyjscie=E-05.bateria_krytyczna;;;brak_danych;R1;rano;wszystkie;nie;tak;E-05/linia;;;;E-05.tytul,E-05.powod,E-05.krok,E-05.linia_krytyczna;aktywna;
E-05;linia;prog_biezacy_wyjscie;metryka=M-25,prog=E-05.bateria_linia,wyjscie=E-05.bateria_koniec;;;linia;awarie;rano;wszystkie;nie;tak;;;;;E-05.linia;aktywna;
E-04;;noc_czesciowa;;z_rekordem;;linia;dane;rano;wszystkie;nie;tak;;;;;E-04.linia;aktywna;
E-07;;sygnal_komponentu;sygnal=czujnik_sypialni_milczy;;;linia;dane;rano;wszystkie;nie;tak;;;;raz;E-07.linia;aktywna;
E-08;;sygnal_komponentu;sygnal=co2_milczy;;;linia;dane;rano;wszystkie;nie;tak;;;;raz_na_epizod;E-08.linia,E-08.linia_bez_czasu;aktywna;
E-30;;statystyka_okna;metryka=M-02,stat=mediana,prog=E-30.ahi,por=ge,n=E-30.okno;wazne_bez_niepewnych;E-30.okno;problem;R5b;rano;wszystkie;nie;tak;E-31;;;;E-30.tytul,E-30.powod,E-30.krok,E-30.podpis_lewo,E-30.podpis_prawo;aktywna;
E-31;lekarz;stan_odchylenia;metryka=M-02,min=E-31.krok_lekarz;wazne;;problem;R5b;rano;wszystkie;tak;nie;E-31/obserwuj;;;;E-31.tytul,E-31.powod,E-31.krok_lekarz,E-31.podpis_lewo,E-31.podpis_prawo;aktywna;
E-31;obserwuj;stan_odchylenia;metryka=M-02;wazne;;problem;R5b;rano;wszystkie;tak;nie;;;;;E-31.tytul,E-31.powod,E-31.krok,E-31.podpis_lewo,E-31.podpis_prawo;aktywna;
E-33;mediana;statystyka_okna;metryka=M-03ca,stat=mediana,prog=E-33.mediana_ca,por=ge,n=E-33.okno;wazne_bez_niepewnych;E-33.okno;;;rano;wszystkie;nie;nie;;;;;;aktywna;
E-33;;stan_odchylenia;metryka=M-03ca;wazne;;problem;R5b;rano;wszystkie;tak;nie;;E-33/mediana;;;E-33.tytul,E-33.powod,E-33.krok,E-33.podpis_lewo,E-33.podpis_prawo;aktywna;
E-36;lekarz;odchylenie_nocy_normy;metryka=E-36.sygnal_cisnienia,kzn=E-36.noce,min=E-36.krok_lekarz;wazne;E-36.noce;problem;R5b;rano;wszystkie;tak;nie;E-36/obserwuj;;;;E-36.tytul,E-36.powod,E-36.krok_lekarz,E-36.podpis_lewo,E-36.podpis_prawo;aktywna;
E-36;obserwuj;odchylenie_nocy_normy;metryka=E-36.sygnal_cisnienia,kzn=E-36.noce;wazne;E-36.noce;problem;R5b;rano;wszystkie;tak;nie;;;;;E-36.tytul,E-36.powod,E-36.krok,E-36.podpis_lewo,E-36.podpis_prawo;aktywna;
E-37;gi;stan_odchylenia;metryka=M-11;wazne;;problem;R5b;rano;ASV,ASVAuto;tak;nie;;;;;;zablokowana;
E-37;bilans;k_z_n_nocy;;wazne;;problem;R5b;rano;ASV,ASVAuto;nie;nie;;;;;;zablokowana;
E-23;;prog_nocy;metryka=M-07,prog=E-23.przeciek_p95,por=ge;z_terapia;;problem;R5b;rano;wszystkie;nie;tak;S-K-10,S-K-14,E-20;;;;E-23.tytul,E-23.powod,E-23.krok,E-23.podpis_lewo,E-23.podpis_prawo;aktywna;
E-23;bez_sk10_kilka;nie_reguly;regula=S-K-10/kilka;;;;;rano;wszystkie;nie;tak;;;;;;aktywna;
E-22;;duzy_wyciek;prog=E-20.minuty,odsetek=E-20.odsetek_czasu,kzn=E-22.wejscie,wyjscie=E-22.wyjscie;z_terapia;E-22.wejscie;problem;R5c;rano;wszystkie;nie;tak;E-21;;;;E-22.tytul,E-22.powod,E-22.krok,E-22.podpis_lewo,E-22.podpis_prawo;aktywna;
E-21;;stan_odchylenia_n;metryka=M-07;wazne;;problem;R5c;rano;wszystkie;tak;nie;S-K-14;;;;E-21.tytul,E-21.powod,E-21.krok,E-21.podpis_lewo,E-21.podpis_prawo;aktywna;
E-21;wilgotnosc;prog_biezacy_wyjscie;metryka=wilgotnosc_min_nocy,prog=E-21.wilgotnosc_linia,wyjscie=E-21.wilgotnosc_linia,por=lt;;;linia;noc;rano;wszystkie;tak;nie;;E-21;;;E-21.linia;aktywna;
E-10;problem;noce_bez_terapii;prog=E-10.noc_bez_terapii_m01,n=E-10.noce_problem,wyjscie=R-DANE-13.m01_min;kompletne;E-10.noce_problem;problem;R5d;rano;wszystkie;nie;tak;E-10/linia,E-13,E-14;;;;E-10.tytul,E-10.powod,E-10.krok,E-10.podpis_lewo,E-10.podpis_prawo;aktywna;
E-12;;regularnosc_okna;prog=E-12.noc_min,odsetek=E-12.odsetek_min,min=E-12.historia_min,przerwa=E-12.okno_po_przerwie,wyjscie=E-12.wyjscie_dni;kompletne;;problem;R5d;rano;wszystkie;nie;tak;;;;;E-12.tytul,E-12.powod,E-12.krok,E-12.podpis_lewo,E-12.podpis_prawo;aktywna;
E-13;;stan_odchylenia;metryka=M-01;wazne;;problem;R5d;rano;wszystkie;tak;nie;;;;;E-13.tytul,E-13.powod,E-13.krok,E-13.podpis_lewo,E-13.podpis_prawo;aktywna;
E-14;;stan_odchylenia;metryka=M-17;wazne;;problem;R5d;rano;wszystkie;tak;nie;S-K-16;;;;E-14.tytul,E-14.powod,E-14.krok,E-14.podpis_lewo,E-14.podpis_prawo;aktywna;
E-10;linia;noce_bez_terapii;prog=E-10.noc_bez_terapii_m01;kompletne;;linia;noc;rano;wszystkie;nie;tak;;;;;E-10.linia;aktywna;
E-11;;przedzial_nocy;metryka=M-01,min=E-11.terapia_min,max=E-11.krotka_noc_max;kompletne;;linia;noc;rano;wszystkie;nie;tak;;;;;E-11.linia;aktywna;
E-20;;duzy_wyciek;prog=E-20.minuty,odsetek=E-20.odsetek_czasu;z_terapia;;linia;noc;rano;wszystkie;nie;tak;;;;;E-20.linia;aktywna;
L-01;.srednia;statystyka_okna;metryka=M-03ca,stat=srednia,prog=L-01.srednia_ca,por=ge;wazne_bez_niepewnych;L-01.okno;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;tylko_raport;
L-01;.noce;k_z_n_nocy;metryka=M-03ca,prog=L-01.srednia_ca,por=ge,kzn=L-01.noce_ca;wazne_bez_niepewnych;L-01.okno;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;tylko_raport;
L-01;.po_zmianie;od_zdarzenia;zdarzenie=zmiana_ustawien,max=L-01.asv_noce_po_zmianie,jednostka=noce;wazne_bez_niepewnych;;;;rano;ASV,ASVAuto;nie;tak;;;;;;tylko_raport;
L-01;.bez_zmiany;zlozenie;op=nie,skladniki=L-01/.po_zmianie;;;;;rano;ASV,ASVAuto;nie;tak;;;;;;tylko_raport;
L-01;cpap.okno_poczatek;od_zdarzenia;zdarzenie=poczatek_leczenia,max=L-01.karencja_okno,jednostka=dni;;;;;rano;CPAP,AutoSet;nie;tak;;;;;;tylko_raport;
L-01;cpap.okno;zlozenie;op=i,skladniki=L-01/cpap.okno_poczatek;;;;;rano;CPAP,AutoSet;nie;tak;;;;;;tylko_raport;
L-01;cpap.karencja_ca;statystyka_okna;metryka=M-03ca,stat=srednia,prog=L-01.karencja_ca,por=ge;wazne_bez_niepewnych;L-01.okno;;;rano;CPAP,AutoSet;nie;tak;;;;;;tylko_raport;
L-01;cpap.bez_ca10;zlozenie;op=nie,skladniki=L-01/cpap.karencja_ca;;;;;rano;CPAP,AutoSet;nie;tak;;;;;;tylko_raport;
L-01;cpap.karencja;zlozenie;op=i,skladniki=L-01/cpap.okno+L-01/cpap.bez_ca10;;;;;rano;CPAP,AutoSet;nie;tak;;;;;;tylko_raport;
L-01;;zlozenie_l;op=i,skladniki=L-01/.srednia+L-01/.noce+L-01/.bez_zmiany,zmienne=x:L-01/.srednia.wartosc|norma:norma.M-03ca;wazne_bez_niepewnych;L-01.okno;lekarz;R3;rano;ASV,ASVAuto;nie;tak;;;;;L-01.tytul,L-01.powod,L-01.krok;tylko_raport;
L-01;cpap;zlozenie_l;op=i,skladniki=L-01/.srednia+L-01/.noce,zmienne=x:L-01/.srednia.wartosc|norma:norma.M-03ca;wazne_bez_niepewnych;L-01.okno;lekarz;R3;rano;CPAP,AutoSet;nie;tak;;;;;L-01.tytul,L-01.powod,L-01.krok;tylko_raport;
L-01;.wyjscie_srednia;statystyka_okna;metryka=M-03ca,stat=srednia,prog=L-01.wyjscie_max,por=lt;wazne_bez_niepewnych;L-01.okno;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;tylko_raport;
L-01;.wyjscie;trwanie;regula=L-01/.wyjscie_srednia,min=L-01.wyjscie_noce,jednostka=noce;wazne_bez_niepewnych;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;tylko_raport;
L-02;a.kzn;k_z_n_nocy;metryka=M-09,prog=L-02.a_minuty_noc,por=ge,kzn=L-02.a_noce;wazne_bez_niepewnych;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;tylko_raport;
L-02;a.suma;statystyka_okna;metryka=M-09,stat=suma,prog=L-02.a_minuty_noc,por=ge,n=L-02.a_okno;wazne_bez_niepewnych;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;tylko_raport;
L-02;a;zlozenie_l;op=i,skladniki=L-02/a.kzn,zmienne=n:L-02/a.kzn.n|min:L-02/a.suma.wartosc;wazne_bez_niepewnych;;lekarz;R3;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;L-02.tytul,L-02.powod_a,L-02.krok;tylko_raport;
L-02;a.wyjscie_k;k_z_n_nocy;metryka=M-09,prog=L-02.a_minuty_noc,por=ge,kzn=L-02.a_wyjscie;wazne_bez_niepewnych;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;tylko_raport;
L-02;a.wyjscie;zlozenie;op=nie,skladniki=L-02/a.wyjscie_k;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;tylko_raport;
L-02;b.razy;okno_wobec_normy;metryka=M-09p,stat=mediana,wzgledem=mediana,razy=L-02.b_mnoznik,por=ge;wazne_bez_niepewnych;L-02.b_okno;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-02;b.delta;okno_wobec_normy;metryka=M-09p,stat=mediana,wzgledem=mediana,delta=L-02.b_roznica,por=ge;wazne_bez_niepewnych;L-02.b_okno;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-02;b;zlozenie_l;op=i,skladniki=L-02/b.razy+L-02/b.delta,zmienne=x:L-02/b.razy.wartosc|norma:L-02/b.razy.norma;wazne_bez_niepewnych;L-02.b_okno;lekarz;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;L-02.tytul,L-02.powod_b,L-02.krok,L-02.linia_b;tylko_raport;
L-02;b.wyjscie_mediana;okno_wobec_normy;metryka=M-09p,stat=mediana,wzgledem=mediana,razy=L-02.b_wyjscie_mnoznik,por=le;wazne_bez_niepewnych;L-02.b_okno;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-02;b.wyjscie;trwanie;regula=L-02/b.wyjscie_mediana,min=L-02.b_wyjscie_noce,jednostka=noce;wazne_bez_niepewnych;;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-03;ge10.mediana;statystyka_okna_min;metryka=M-02,stat=mediana,prog=L-03.ahi_wysokie,por=ge,n=L-03.okno,min=L-03.min_noce;wazne_bez_niepewnych;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;aktywna;
L-03;ge10.bez_e12;zlozenie_l;op=nie,skladniki=E-12,nieznane=falsz;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;aktywna;
L-03;ge10;zlozenie_l;op=i,skladniki=L-03/ge10.mediana+L-03/ge10.bez_e12,zmienne=x:L-03/ge10.mediana.wartosc;wazne_bez_niepewnych;L-03.okno;lekarz;R3;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;okres_infekcji;;L-03.tytul,L-03.powod,L-03.krok;aktywna;B
L-03;ge10.wyjscie;statystyka_okna_min;metryka=M-02,stat=mediana,prog=L-03.wyjscie_wysokie,por=lt,n=L-03.okno,min=L-03.min_noce;wazne_bez_niepewnych;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;aktywna;
L-03;5_10.dol;statystyka_okna;metryka=M-02,stat=mediana,prog=L-03.ahi_srednie_dol,por=ge;wazne_bez_niepewnych;L-03.okno_srednie;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;tylko_raport;
L-03;5_10.gora;statystyka_okna;metryka=M-02,stat=mediana,prog=L-03.ahi_srednie_gora,por=lt;wazne_bez_niepewnych;L-03.okno_srednie;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;tylko_raport;
L-03;5_10.sennosc;odpowiedzi_tygodni;pytanie=sennosc,odpowiedz=tak,kzn=L-03.sennosc;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;tylko_raport;
L-03;5_10;zlozenie_l;op=i,skladniki=L-03/5_10.dol+L-03/5_10.gora,zmienne=x:L-03/5_10.dol.wartosc;wazne_bez_niepewnych;L-03.okno_srednie;lekarz;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;okres_infekcji;;L-03.tytul,L-03.powod,L-03.krok,L-03.linia_5_10;tylko_raport;
L-03;5_10.wyjscie;statystyka_okna;metryka=M-02,stat=mediana,prog=L-03.ahi_srednie_dol,por=lt;wazne_bez_niepewnych;L-03.okno_srednie;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;tylko_raport;
L-04;.epap;blisko_ustawienia;metryka=M-06p,ustawienie=EAX,margines=L-04.margines_max,kzn=L-04.noce,z_rodzajem=tak;wazne_bez_niepewnych;L-04.okno;;;rano;ASVAuto;nie;tak;;;;;;tylko_raport;
L-04;.oa;statystyka_okna;metryka=M-03oa,stat=mediana,prog=L-04.zdarzenia_obturacyjne,por=ge;wazne_bez_niepewnych;L-04.okno;;;rano;ASVAuto;nie;tak;;;;;;tylko_raport;
L-04;;zlozenie_l;op=i,skladniki=L-04/.epap+L-04/.oa,zmienne=max:L-04/.epap.max|y:L-04/.oa.wartosc;wazne_bez_niepewnych;L-04.okno;lekarz;R3;rano;ASVAuto;nie;tak;;;;;L-04.tytul_asvauto,L-04.powod_asvauto,L-04.krok;tylko_raport;
L-04;.wyjscie_epap;blisko_ustawienia;metryka=M-06p,ustawienie=EAX,margines=L-04.margines_max,kzn=L-04.wyjscie_noce,z_rodzajem=tak;wazne_bez_niepewnych;L-04.okno;;;rano;ASVAuto;nie;tak;;;;;;tylko_raport;
L-04;.wyjscie_epap_nie;zlozenie;op=nie,skladniki=L-04/.wyjscie_epap;;;;;rano;ASVAuto;nie;tak;;;;;;tylko_raport;
L-04;.wyjscie_oa;kolejne_noce;metryka=M-03oa,prog=L-04.wyjscie_zdarzenia,por=lt,n=L-04.okno;wazne_bez_niepewnych;L-04.okno;;;rano;ASVAuto;nie;tak;;;;;;tylko_raport;
L-04;autoset.cisnienie;blisko_ustawienia;metryka=M-05p,ustawienie=MPA,margines=L-04.margines_max,kzn=L-04.noce;wazne_bez_niepewnych;L-04.okno;;;rano;AutoSet;nie;tak;;;;;;tylko_raport;
L-04;autoset.obt;statystyka_okna;metryka=M-03oa,stat=mediana,prog=L-04.zdarzenia_obturacyjne,por=ge;wazne_bez_niepewnych;L-04.okno;;;rano;AutoSet;nie;tak;;;;;;tylko_raport;
L-04;autoset;zlozenie_l;op=i,skladniki=L-04/autoset.cisnienie+L-04/autoset.obt,zmienne=max:L-04/autoset.cisnienie.max|y:L-04/autoset.obt.wartosc;wazne_bez_niepewnych;L-04.okno;lekarz;R3;rano;AutoSet;nie;tak;;;;;L-04.tytul,L-04.powod,L-04.krok;tylko_raport;
L-04;autoset.wyjscie_max;blisko_ustawienia;metryka=M-05p,ustawienie=MPA,margines=L-04.margines_max,kzn=L-04.wyjscie_noce;wazne_bez_niepewnych;L-04.okno;;;rano;AutoSet;nie;tak;;;;;;tylko_raport;
L-04;autoset.wyjscie_max_nie;zlozenie;op=nie,skladniki=L-04/autoset.wyjscie_max;;;;;rano;AutoSet;nie;tak;;;;;;tylko_raport;
L-04;autoset.wyjscie_obt;kolejne_noce;metryka=M-03oa,prog=L-04.wyjscie_zdarzenia,por=lt,n=L-04.okno;wazne_bez_niepewnych;L-04.okno;;;rano;AutoSet;nie;tak;;;;;;tylko_raport;
L-04;.wyjscie;zlozenie;op=lub,skladniki=L-04/.wyjscie_epap_nie+L-04/.wyjscie_oa+L-04/autoset.wyjscie_max_nie+L-04/autoset.wyjscie_obt;;;;;rano;AutoSet,ASVAuto;nie;tak;;;;;;tylko_raport;
L-05;.m13;kolejne_noce;metryka=M-13,wzgledem=mediana,delta=L-05.wzrost,por=ge,n=L-05.noce;wazne_bez_niepewnych;L-05.noce;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-05;.gorzej;odpowiedzi_tygodni;pytanie=gorzej,odpowiedz=tak;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;tylko_raport;
L-05;.towarzyszacy;zlozenie;op=lub,skladniki=L-01+L-01/cpap+L-02/b+L-05/.gorzej;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;tylko_raport;
L-05;;zlozenie_l;op=i,skladniki=L-05/.m13+L-05/.towarzyszacy,zmienne=x:L-05/.m13.x|norma:norma.M-13|n:L-05/.m13.z;wazne_bez_niepewnych;L-05.noce;lekarz;R3;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;L-05.tytul,L-05.powod,L-05.krok;tylko_raport;
L-05;infekcja.m13;kolejne_noce;metryka=M-13,wzgledem=mediana,delta=L-05.wzrost,por=ge,n=L-05.noce;z_terapia_bez_niepewnych;L-05.noce;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;tylko_okres_infekcji;;;tylko_raport;
L-05;infekcja;zlozenie_l;op=i,skladniki=L-05/infekcja.m13+L-05/.gorzej,zmienne=x:L-05/infekcja.m13.x|norma:norma.M-13|n:L-05/infekcja.m13.z;z_terapia_bez_niepewnych;L-05.noce;lekarz;R3;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;tylko_okres_infekcji;;L-05.tytul,L-05.powod,L-05.krok;tylko_raport;
L-05;.wyjscie;kolejne_noce;metryka=M-13,wzgledem=mediana,delta=L-05.wyjscie_wzrost,por=lt,n=L-05.wyjscie_noce;wazne_bez_niepewnych;L-05.wyjscie_noce;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-06;;zawsze;;;;lekarz;R2;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;zablokowana;
L-07;.wzrost;okno_wobec_normy;metryka=M-06p,stat=mediana,wzgledem=mediana,delta=L-07.wzrost_m05p,por=ge;wazne_bez_niepewnych;L-07.okno;;;rano;ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-07;.wzrost_cpap;okno_wobec_normy;metryka=M-05p,stat=mediana,wzgledem=mediana,delta=L-07.wzrost_m05p,por=ge;wazne_bez_niepewnych;L-07.okno;;;rano;CPAP,AutoSet;tak;nie;;;;;;tylko_raport;
L-07;.wzrost_dowolny;zlozenie;op=lub,skladniki=L-07/.wzrost+L-07/.wzrost_cpap;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-07;.czas;trwanie;regula=L-07/.wzrost_dowolny,min=L-07.czas,jednostka=noce;wazne_bez_niepewnych;;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-07;.zmiana;od_zdarzenia;zdarzenie=zmiana_ustawien,max=L-07.czas,jednostka=noce;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-07;.bez_zmiany;zlozenie;op=nie,skladniki=L-07/.zmiana;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-07;;zlozenie_l;op=i,skladniki=L-07/.czas+L-07/.bez_zmiany,zmienne=x:L-07/.wzrost.wartosc;wazne_bez_niepewnych;L-07.okno;lekarz;;rano;ASV,ASVAuto;tak;nie;;;;;L-07.tytul_asv,L-07.linia_asv;tylko_raport;
L-07;cpap;zlozenie_l;op=i,skladniki=L-07/.czas+L-07/.bez_zmiany,zmienne=x:L-07/.wzrost_cpap.wartosc;wazne_bez_niepewnych;L-07.okno;lekarz;;rano;CPAP,AutoSet;tak;nie;;;;;L-07.tytul,L-07.linia;tylko_raport;
L-07;.wyjscie;zlozenie;op=nie,skladniki=E-36;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-08;.e12;trwanie;regula=E-12,min=L-08.e12_dni,jednostka=dni;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;aktywna;
L-08;.bez_e22_dzis;zlozenie;op=nie,skladniki=E-22;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;aktywna;
L-08;.bez_e22;trwanie;regula=L-08/.bez_e22_dzis,min=L-08.e12_dni,jednostka=dni;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;aktywna;
L-08;;zlozenie;op=i,skladniki=L-08/.e12+L-08/.bez_e22;z_terapia;;lekarz;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;L-08.linia;aktywna;C
L-08;szybki.teraz;regularnosc;noc=E-12.noc_min,prog=L-08.szybki_m19,por=lt,bez_przerwy=tak;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;aktywna;
L-08;szybki.wczesniej;regularnosc;noc=E-12.noc_min,prog=L-08.szybki_m19_wczesniej,por=ge,odniesienie=miesiac_wczesniej,bez_przerwy=tak;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;aktywna;
L-08;szybki;zlozenie_l;op=i,skladniki=L-08/szybki.teraz+L-08/szybki.wczesniej,zmienne=n:L-08/szybki.teraz.n|m:L-08/szybki.wczesniej.m;z_terapia;;lekarz;R3;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;L-08.tytul_szybki,L-08.powod_szybki,L-08.krok_szybki;aktywna;B
L-08;.wyjscie_m19;regularnosc;noc=E-12.noc_min,prog=L-08.wyjscie_m19,por=ge;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;aktywna;
L-08;.wyjscie;trwanie;regula=L-08/.wyjscie_m19,min=L-08.wyjscie_dni,jednostka=dni;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;aktywna;
L-09;.e22;trwanie;regula=E-22,min=L-09.e22_noce,jednostka=noce;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;aktywna;
L-09;.sk10;k_z_n_nocy;metryka=M-07,prog=R-NOR-05.niepewna_p95,por=ge,kzn=L-09.sk10_noce,brak=niespelnia;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;aktywna;
L-09;;zlozenie_l;op=lub,skladniki=L-09/.e22+L-09/.sk10,zmienne=n:L-09/.sk10.n|x:L-09/.sk10.x;z_terapia;;problem;R5c;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;okres_infekcji;;L-09.tytul,L-09.powod,L-09.krok,L-09.linia;aktywna;
L-09;.wyjscie_e20;zlozenie;op=nie,skladniki=E-20;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;aktywna;
L-09;.wyjscie_p95;prog_nocy;metryka=M-07,prog=R-NOR-05.niepewna_p95,por=lt,brak=spelnia;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;aktywna;
L-09;.wyjscie_noc;zlozenie;op=i,skladniki=L-09/.wyjscie_e20+L-09/.wyjscie_p95;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;aktywna;
L-09;.wyjscie;trwanie;regula=L-09/.wyjscie_noc,min=L-09.wyjscie_noce,jednostka=noce;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;aktywna;
L-10;.sk19_p90;okno_wobec_normy;metryka=M-10p,stat=mediana,wzgledem=p90,por=ge;wazne_bez_niepewnych;S-K-19.tymczasowy_okno;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-10;.sk19_delta;okno_wobec_normy;metryka=M-10p,stat=mediana,wzgledem=mediana,delta=S-K-19.tymczasowy_dodatek,por=ge;wazne_bez_niepewnych;S-K-19.tymczasowy_okno;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-10;.po_zmianie;od_zdarzenia;zdarzenie=zmiana_ustawien,max=R-KON-30.po_zmianie,jednostka=noce;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-10;.bez_zmiany;zlozenie;op=nie,skladniki=L-10/.po_zmianie;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-10;.sk19;zlozenie;op=i,skladniki=L-10/.sk19_p90+L-10/.sk19_delta+L-10/.bez_zmiany;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-10;.sk19_tygodnie;trwanie_tygodni;regula=L-10/.sk19,min=L-10.tygodnie;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-10;.sennosc;odpowiedzi_kolejne;pytanie=sennosc,odpowiedz=tak,n=L-10.tygodnie,w_ostatnich=L-10.tygodnie;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-10;;zlozenie_l;op=i,skladniki=L-10/.sk19_tygodnie+L-10/.sennosc,zmienne=n:L-10/.sk19_tygodnie.n;wazne_bez_niepewnych;S-K-19.tymczasowy_okno;lekarz;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;L-10.tytul,L-10.powod,L-10.krok,L-10.linia;tylko_raport;
L-10;.bez_sk19;zlozenie;op=nie,skladniki=L-10/.sk19;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-10;.bez_sk19_tygodnie;trwanie_tygodni;regula=L-10/.bez_sk19,min=L-10.wyjscie_tygodnie;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-10;.sennosc_nie;odpowiedzi_kolejne;pytanie=sennosc,odpowiedz=nie,n=L-10.wyjscie_tygodnie,w_ostatnich=L-10.wyjscie_tygodnie;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-10;.wyjscie;zlozenie;op=lub,skladniki=L-10/.bez_sk19_tygodnie+L-10/.sennosc_nie;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;tak;nie;;;;;;tylko_raport;
L-11;.sennosc;odpowiedzi_tygodni;pytanie=sennosc,odpowiedz=tak,kzn=L-11.sennosc;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;aktywna;
L-11;.m19;regularnosc;noc=E-12.noc_min,prog=L-11.m19_min,por=ge;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;aktywna;
L-11;.ahi;statystyka_okna;metryka=M-02,stat=mediana,prog=L-11.ahi_max,por=lt;z_terapia;L-11.okno_ahi;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;aktywna;
L-11;;zlozenie_l;op=i,skladniki=L-11/.sennosc+L-11/.m19+L-11/.ahi,zmienne=n:L-11/.sennosc.n|x:L-11/.ahi.wartosc;z_terapia;L-11.okno_ahi;lekarz;R3;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;L-11.tytul,L-11.powod,L-11.krok;aktywna;B
L-11;2tyg.odp;odpowiedzi_kolejne;pytanie=sennosc,odpowiedz=tak,n=L-11.wariant_2tyg,w_ostatnich=L-11.wariant_2tyg_okno;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;;aktywna;
L-11;2tyg;zlozenie_l;op=i,skladniki=L-11/2tyg.odp,zmienne=n:L-11/2tyg.odp.n|x:L-11/.ahi.wartosc;;;lekarz;R3;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;;;;L-11.tytul,L-11.powod,L-11.krok;aktywna;B
L-11;.wyjscie;odpowiedzi_najwyzej;pytanie=sennosc,odpowiedz=tak,kzn=L-11.wyjscie,min_odpowiedzi=L-11.wyjscie_odpowiedzi;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;aktywna;
L-12;.po_zmianie;od_zdarzenia;zdarzenie=zmiana_ustawien,min=L-12.okno,jednostka=noce;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;tylko_raport;
L-12;.ahi;porownanie_przed_po;metryka=M-02,stat=mediana,n=L-12.okno,delta=L-12.ahi,por=ge;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;tylko_raport;
L-12;.m01;porownanie_przed_po;metryka=M-01,stat=mediana,n=L-12.okno,delta=L-12.m01,por=le;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;tylko_raport;
L-12;.m07;porownanie_przed_po;metryka=M-07,stat=mediana,n=L-12.okno,delta=L-12.m07_p95,por=ge;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;tylko_raport;
L-12;.warunek;zlozenie;op=lub,skladniki=L-12/.ahi+L-12/.m01+L-12/.m07;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;tylko_raport;
L-12;;zlozenie_l;op=i,skladniki=L-12/.po_zmianie+L-12/.warunek,kopiuj=L-12/.ahi+L-12/.m01+L-12/.m07;z_terapia;L-12.okno;lekarz;R3;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;L-12.tytul,L-12.powod,L-12.krok;tylko_raport;
L-12;.bez_warunku;zlozenie;op=nie,skladniki=L-12/.warunek;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;tylko_raport;
L-12;.bez_warunku_14;trwanie;regula=L-12/.bez_warunku,min=L-12.okno,jednostka=noce;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;tylko_raport;
L-12;.nowa_zmiana;od_zdarzenia;zdarzenie=zmiana_ustawien,max=L-12.okno,jednostka=noce;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;tylko_raport;
L-12;.wyjscie;zlozenie;op=lub,skladniki=L-12/.bez_warunku_14+L-12/.nowa_zmiana;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;;;;;tylko_raport;
H-01;L-03;zawsze;;;;;;rano;ASV,ASVAuto;nie;tak;;L-03;;;;zablokowana;
H-01;L-03~autoset;zawsze;;;;;;rano;AutoSet;nie;tak;;L-03;;;;zablokowana;
H-01;L-10;zawsze;;;;;;rano;ASV,ASVAuto;nie;tak;;L-10;;;;zablokowana;
H-01;L-10~autoset;zawsze;;;;;;rano;AutoSet;nie;tak;;L-10;;;;zablokowana;
H-02;L-04;zlozenie_l;op=i,skladniki=L-04,zmienne=max:L-04/.epap.max|y:L-04/.oa.wartosc;wazne_bez_niepewnych;L-04.okno;;;rano;ASVAuto;nie;tak;;L-04;;;H-02.linia_asvauto,H-02.zdanie,H-02.pytanie_asvauto;tylko_raport;
H-02;L-04~autoset;zawsze;;;;;;rano;AutoSet;nie;tak;;L-04/autoset;;;;zablokowana;
H-03;L-03;zawsze;;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;L-03;;;H-03.linia,H-03.pytanie;zablokowana;
H-05;L-01;zlozenie_l;op=i,skladniki=L-12,zmienne=data:L-12/.po_zmianie.data;;;;;rano;ASV,ASVAuto;nie;nie;;L-01;;;H-05.linia,H-05.pytanie;tylko_raport;
H-05;L-01~cpap;zlozenie_l;op=i,skladniki=L-12,zmienne=data:L-12/.po_zmianie.data;;;;;rano;CPAP,AutoSet;nie;nie;;L-01/cpap;;;H-05.linia,H-05.pytanie;tylko_raport;
H-04;L-01;zawsze;;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;L-01;;;;zablokowana;
H-05;L-12;zlozenie_l;op=i,skladniki=L-12,zmienne=data:L-12/.po_zmianie.data;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;L-12;;;H-05.linia,H-05.pytanie;tylko_raport;
H-06;L-09;zawsze;;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;L-09;;;H-06.linia,H-06.pytanie;zablokowana;
H-07;L-09;zawsze;;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;L-09;;;H-07.linia;zablokowana;
H-09;L-11;statystyka_okna_min;metryka=M-01,stat=mediana,prog=H-09.m01,por=lt,n=L-11.okno_ahi;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;L-11;;;H-09.linia,H-09.pytanie;aktywna;
H-09;L-11~2tyg;statystyka_okna_min;metryka=M-01,stat=mediana,prog=H-09.m01,por=lt,n=L-11.okno_ahi;z_terapia;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;L-11/2tyg;;;H-09.linia,H-09.pytanie;aktywna;
H-08;L-11;zawsze;;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;L-11;;;H-08.linia;aktywna;
H-08;L-11~2tyg;zawsze;;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;L-11/2tyg;;;H-08.linia;aktywna;
H-08;L-02;zawsze;;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;L-02;;;H-08.linia;tylko_raport;
H-08;L-05;zawsze;;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;nie;;L-05;;;H-08.linia;tylko_raport;
H-08;L-06;zawsze;;;;;;rano;CPAP,AutoSet,ASV,ASVAuto;nie;tak;;L-06;;;H-08.linia;zablokowana;
H-08;L-01;od_zdarzenia;zdarzenie=zmiana_ustawien,max=H-08.okno,jednostka=noce;;;;;rano;ASV,ASVAuto;nie;tak;;L-01;;;H-08.linia;tylko_raport;
H-08;L-01~cpap;od_zdarzenia;zdarzenie=zmiana_ustawien,max=H-08.okno,jednostka=noce;;;;;rano;CPAP,AutoSet;nie;tak;;L-01/cpap;;;H-08.linia;tylko_raport;
H-08;L-04~autoset;zawsze;;;;;;rano;AutoSet;nie;tak;;L-04/autoset;;;H-08.linia;tylko_raport;
O-03;M-02;rekord_tygodnia;metryka=M-02,n=O-03.noce,min=O-03.historia,delta=R-NOR-12.delta.M-02;wazne;;;R8;rano;wszystkie;nie;nie;;;;;O-03.tytul,O-03.ahi.powod,O-03.krok;aktywna;
O-03;M-07;rekord_tygodnia;metryka=M-07,n=O-03.noce,min=O-03.historia,delta=R-NOR-12.delta.M-07;wazne;;;R8;rano;wszystkie;nie;nie;;;;;O-03.tytul,O-03.wyciek.powod,O-03.krok;aktywna;
O-04;M-01;tydzien_miesiac;metryka=M-01,okno_a=O-04.tydzien,okno_b=O-04.miesiac,delta=R-NOR-12.delta.M-01,por=ge;wazne;;;R8;rano;wszystkie;nie;nie;;;;;O-04.tytul,O-04.powod,O-04.krok;aktywna;
O-04;M-02;tydzien_miesiac;metryka=M-02,okno_a=O-04.tydzien,okno_b=O-04.miesiac,delta=R-NOR-12.delta.M-02,por=le;wazne;;;R8;rano;wszystkie;nie;nie;;;;;O-04.tytul,O-04.powod,O-04.krok;aktywna;
O-04;M-07;tydzien_miesiac;metryka=M-07,okno_a=O-04.tydzien,okno_b=O-04.miesiac,delta=R-NOR-12.delta.M-07,por=le;wazne;;;R8;rano;wszystkie;nie;nie;;;;;O-04.tytul,O-04.powod,O-04.krok;aktywna;
O-05;;rozklad_w_nocy;prog=O-05.odsetek,kzn=O-05.noce;wazne;;;R8;rano;wszystkie;nie;nie;;;;;O-05.tytul,O-05.powod,O-05.krok;zablokowana;
O-06;M-17;grupy_nocy;metryka=M-17,cecha=co2,granica=O-06.co2,por_cechy=gt,n=R-EKR-26.okno,min=R-EKR-26.grupa_min,delta=R-NOR-12.delta.M-17;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-06.tytul,O-06.powod,O-06.krok;zablokowana;
O-06;M-02;grupy_nocy;metryka=M-02,cecha=co2,granica=O-06.co2,por_cechy=gt,n=R-EKR-26.okno,min=R-EKR-26.grupa_min,delta=R-NOR-12.delta.M-02;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-06.tytul,O-06.powod,O-06.krok;zablokowana;
O-07;M-07;grupy_nocy;metryka=M-07,cecha=wilgotnosc,granica=O-07.wilgotnosc,por_cechy=lt,n=R-EKR-26.okno,min=R-EKR-26.grupa_min,delta=R-NOR-12.delta.M-07,por=ge;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-07.tytul,O-07.powod,O-07.krok;aktywna;
O-08;M-02;grupy_nocy;metryka=M-02,cecha=weekend,n=R-EKR-26.okno,min=R-EKR-26.grupa_min,delta=R-NOR-12.delta.M-02;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-08.tytul,O-08.powod,O-08.krok;aktywna;
O-09;M-01;grupy_nocy;metryka=M-01,cecha=start,granica=O-09.start,por_cechy=gt,n=R-EKR-26.okno,min=R-EKR-26.grupa_min,delta=R-NOR-12.delta.M-01;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-09.tytul,O-09.powod,O-09.krok;aktywna;
O-09;M-02;grupy_nocy;metryka=M-02,cecha=start,granica=O-09.start,por_cechy=gt,n=R-EKR-26.okno,min=R-EKR-26.grupa_min,delta=R-NOR-12.delta.M-02;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-09.tytul,O-09.powod,O-09.krok;aktywna;
O-10;;grupy_nocy;metryka=M-11,cecha=gi,n=R-EKR-26.okno,min=O-10.noce,delta=R-NOR-12.delta.M-11;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-10.tytul,O-10.powod,O-10.krok;zablokowana;
O-11;;zmiana_cechy_gi;;wazne;;;R8;rano;wszystkie;nie;nie;;;;;O-11.tytul,O-11.powod,O-11.krok;zablokowana;
O-12;A.M-02;czynnik_nocy;czynnik=A,metryka=M-02,delta=R-NOR-12.delta.M-02,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;A.M-07;czynnik_nocy;czynnik=A,metryka=M-07,delta=R-NOR-12.delta.M-07,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;A.M-01;czynnik_nocy;czynnik=A,metryka=M-01,delta=R-NOR-12.delta.M-01,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;A.M-03ca;czynnik_nocy;czynnik=A,metryka=M-03ca,delta=R-NOR-12.delta.M-03,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;K.M-02;czynnik_nocy;czynnik=K,metryka=M-02,delta=R-NOR-12.delta.M-02,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;K.M-07;czynnik_nocy;czynnik=K,metryka=M-07,delta=R-NOR-12.delta.M-07,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;K.M-01;czynnik_nocy;czynnik=K,metryka=M-01,delta=R-NOR-12.delta.M-01,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;K.M-03ca;czynnik_nocy;czynnik=K,metryka=M-03ca,delta=R-NOR-12.delta.M-03,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;S.M-02;czynnik_nocy;czynnik=S,metryka=M-02,delta=R-NOR-12.delta.M-02,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;S.M-07;czynnik_nocy;czynnik=S,metryka=M-07,delta=R-NOR-12.delta.M-07,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;S.M-01;czynnik_nocy;czynnik=S,metryka=M-01,delta=R-NOR-12.delta.M-01,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;S.M-03ca;czynnik_nocy;czynnik=S,metryka=M-03ca,delta=R-NOR-12.delta.M-03,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;J.M-02;czynnik_nocy;czynnik=J,metryka=M-02,delta=R-NOR-12.delta.M-02,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;J.M-07;czynnik_nocy;czynnik=J,metryka=M-07,delta=R-NOR-12.delta.M-07,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;J.M-01;czynnik_nocy;czynnik=J,metryka=M-01,delta=R-NOR-12.delta.M-01,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;J.M-03ca;czynnik_nocy;czynnik=J,metryka=M-03ca,delta=R-NOR-12.delta.M-03,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;L.M-02;czynnik_nocy;czynnik=L,metryka=M-02,delta=R-NOR-12.delta.M-02,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;L.M-07;czynnik_nocy;czynnik=L,metryka=M-07,delta=R-NOR-12.delta.M-07,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;L.M-01;czynnik_nocy;czynnik=L,metryka=M-01,delta=R-NOR-12.delta.M-01,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;L.M-03ca;czynnik_nocy;czynnik=L,metryka=M-03ca,delta=R-NOR-12.delta.M-03,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;T.M-02;czynnik_nocy;czynnik=T,metryka=M-02,delta=R-NOR-12.delta.M-02,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;T.M-07;czynnik_nocy;czynnik=T,metryka=M-07,delta=R-NOR-12.delta.M-07,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;T.M-01;czynnik_nocy;czynnik=T,metryka=M-01,delta=R-NOR-12.delta.M-01,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;T.M-03ca;czynnik_nocy;czynnik=T,metryka=M-03ca,delta=R-NOR-12.delta.M-03,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;P.M-02;czynnik_nocy;czynnik=P,metryka=M-02,delta=R-NOR-12.delta.M-02,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;P.M-07;czynnik_nocy;czynnik=P,metryka=M-07,delta=R-NOR-12.delta.M-07,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;P.M-01;czynnik_nocy;czynnik=P,metryka=M-01,delta=R-NOR-12.delta.M-01,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
O-12;P.M-03ca;czynnik_nocy;czynnik=P,metryka=M-03ca,delta=R-NOR-12.delta.M-03,min=O-12.grupa_min,efektywna=O-12.efektywna_min,warstwa_min=R-CZY-24.warstwa_min,mala=R-CZY-21.mala_grupa,okno=R-CZY-24.okno_odpowiedzi,waga=R-CZY-24.waga_kontrolna,n=R-EKR-26.okno;wazne;;;R8;tydzien;wszystkie;nie;nie;;;;;O-12.tytul,O-12.tytul_skrot,O-12.tytul_ukryty,O-12.powod,O-12.powod_ukryty,O-12.przypadek,O-12.krok,O-12.krok_leki;aktywna;
)zestaw";
static const char TEKSTY[] = R"zestaw(#cpap-eink teksty 1 zestaw=z-4c871f19ae16 pochodzenie=wbudowany firmware=1.0.0..1.99.99 tryby=CPAP,AutoSet,ASV,ASVAuto zamrozenie=brak zrodlo=67c59237a7f3 skrot=sha256:3e12c3f942171efcb20ec41e40acf1e87962ad3da616e4707c3596f3b5a5b1ad
id_tekstu;rola;limit;tekst
E-04.linia;linia;60;Noc {data} zapisana częściowo, liczby mogą być niepełne.
E-05.krok;krok;60;→ Podłącz kabel USB-C.
E-05.linia;linia;60;Ekran: bateria {x}%, naładuj w tym tygodniu.
E-05.linia_krytyczna;linia;60;Naładuj ekran: bateria {x}%.
E-05.powod;powod;70;Bateria {x}%.
E-05.tytul;tytul;30;Naładuj ekran
E-07.linia;linia;60;Czujnik sypialni nie odpowiada od {prog:E-07.noce_bez_czujnika} nocy.
E-08.linia;linia;60;Brak odczytu CO2 od {czas}.
E-08.linia_bez_czasu;linia;60;Brak odczytu CO2.
E-10.krok;krok;60;→ Wróć do terapii dziś w nocy.
E-10.linia;linia;60;Noc {data} bez terapii.
E-10.podpis_lewo;podpis_lewo;40;czas z maską · 30 nocy
E-10.podpis_prawo;podpis_prawo;40;linia przerywana = {prog:E-10.noc_bez_terapii_m01} min
E-10.powod;powod;70;Ostatnia terapia: {data}.
E-10.tytul;tytul;30;Noce bez terapii: {n}
E-11.linia;linia;60;Krótka noc: {czas}. Liczby mniej pewne.
E-12.krok;krok;60;→ Najwięcej daje każda pełna noc.
E-12.podpis_lewo;podpis_lewo;40;czas z maską · 30 nocy
E-12.podpis_prawo;podpis_prawo;40;linia przerywana = {prog:E-12.noc_min} h
E-12.powod;powod;70;Ponad {prog:E-12.noc_min} h tylko w {n} z {d} nocy, cel: {ceil(0,7·d)}.
E-12.tytul;tytul;30;Terapia nieregularna
E-13.krok;krok;60;→ Jeśli budzi cię maska, sprawdź jej dopasowanie.
E-13.podpis_lewo;podpis_lewo;40;czas z maską · 30 nocy
E-13.podpis_prawo;podpis_prawo;40;linia przerywana = zwykle {norma}
E-13.powod;powod;70;Średnio {x}, zwykle {norma} (ostatnie 5 nocy).
E-13.tytul;tytul;30;Noce krótsze niż zwykle
E-14.krok;krok;60;→ Sprawdź, co budzi: toaleta, wyciek, ucisk, suchość.
E-14.podpis_lewo;podpis_lewo;40;przerwy w nocy · 30 nocy
E-14.podpis_prawo;podpis_prawo;40;linia przerywana = zwykle {norma}
E-14.powod;powod;70;Przerwy w nocy: {x}, zwykle {norma}.
E-14.tytul;tytul;30;Częściej zdejmujesz maskę
E-20.linia;linia;60;Wczoraj duży wyciek: {min} min. Bezdechy mniej pewne.
E-21.krok;krok;60;→ Umyj lub wymień poduszkę, sprawdź paski.
E-21.linia;linia;60;Powietrze w sypialni suche: {w}%.
E-21.podpis_lewo;podpis_lewo;40;wyciek l/min · 30 nocy
E-21.podpis_prawo;podpis_prawo;40;linia przerywana = zwykle {norma}
E-21.powod;powod;70;Wyciek rośnie od {n} nocy: {x} L/min, zwykle {norma}.
E-21.tytul;tytul;30;Sprawdź maskę
E-22.krok;krok;60;→ Sprawdź dopasowanie lub zapytaj o inny rozmiar.
E-22.podpis_lewo;podpis_lewo;40;duży wyciek, min na noc · 30 nocy
E-22.podpis_prawo;podpis_prawo;40;linia przerywana = {prog:E-20.minuty} min
E-22.powod;powod;70;Duży wyciek w {n} z 5 nocy, średnio {min} min.
E-22.tytul;tytul;30;Maska nieszczelna
E-23.krok;krok;60;→ Sprawdź maskę. Gdy wraca, pokaż lekarzowi.
E-23.podpis_lewo;podpis_lewo;40;wyciek l/min · 30 nocy
E-23.podpis_prawo;podpis_prawo;40;linia przerywana = {prog:E-23.przeciek_p95} l/min
E-23.powod;powod;70;{x} l/min lub więcej przez 5% czasu terapii, próg {prog:E-23.przeciek_p95}.
E-23.tytul;tytul;30;Wyciek w nocy ponad próg
E-30.krok;krok;60;→ Sprawdź wyciek i obserwuj kolejne noce.
E-30.podpis_lewo;podpis_lewo;40;AHI · 30 nocy
E-30.podpis_prawo;podpis_prawo;40;linia przerywana = {prog:E-30.ahi} / h
E-30.powod;powod;70;AHI powyżej {prog:E-30.ahi} w {n} z {prog:E-30.okno} nocy, ostatnio {x}.
E-30.tytul;tytul;30;Noce z AHI powyżej {prog:E-30.ahi}
E-31.krok;krok;60;→ Sprawdź wyciek i pozycję snu, obserwuj kolejne noce.
E-31.krok_lekarz;krok;60;→ Obserwuj kolejne noce.
E-31.podpis_lewo;podpis_lewo;40;AHI · 30 nocy
E-31.podpis_prawo;podpis_prawo;40;linia przerywana = zwykle {norma}
E-31.powod;powod;70;Średnio {x} / h, zwykle {norma}.
E-31.tytul;tytul;30;Bezdechów więcej niż zwykle
E-33.krok;krok;60;→ Obserwuj kolejne noce.
E-33.podpis_lewo;podpis_lewo;40;bezdechy centralne / h · 30 nocy
E-33.podpis_lewo_asv;podpis_lewo;40;przy pełnym wsparciu / h · 30 nocy
E-33.podpis_prawo;podpis_prawo;40;linia przerywana = zwykle {norma}
E-33.powod;powod;70;{x} / h, zwykle {norma}.
E-33.tytul;tytul;30;Więcej bezdechów centralnych
E-33.tytul_asv;tytul;30;Bezdechy przy pełnym wsparciu
E-36.krok;krok;60;→ Obserwuj kolejne noce.
E-36.krok_lekarz;krok;60;→ Obserwuj kolejne noce.
E-36.podpis_lewo;podpis_lewo;40;{prog:E-36.slowo_cisnienia} w szczytach nocy · 30 nocy
E-36.podpis_prawo;podpis_prawo;40;linia przerywana = zwykle {norma}
E-36.powod;powod;70;{prog:E-36.slowo_cisnienia} w szczytach nocy {x}, zwykle {norma}.
E-36.tytul;tytul;30;{prog:E-36.slowo_cisnienia} wyższe niż zwykle
H-01.linia;ramka;60;Najpewniej do poprawy: minimalne ciśnienie
H-01.pytanie;pytanie;200;Zdarzenia skupiają się przy niskim ciśnieniu ({a} / h wobec {b} / h): czy minimalne ciśnienie może mieć z tym związek, czy szukać innej przyczyny?
H-01.zdanie;ramka;60;Obszar do omówienia z lekarzem.
H-02.linia;ramka;60;Najpewniej do poprawy: maksymalne ciśnienie
H-02.linia_asvauto;ramka;60;Najpewniej do poprawy: zakres EPAP
H-02.linia_tryb;ramka;60;Najpewniej do poprawy: tryb terapii
H-02.pytanie;pytanie;200;Ciśnienie przy maksimum {max} ze zdarzeniami {y} / h: czy zakres ciśnienia albo tryb terapii mogą mieć z tym związek, czy szukać innej przyczyny?
H-02.pytanie_asvauto;pytanie;200;EPAP przy maksimum {max} ze zdarzeniami {y} / h: czy zakres EPAP może mieć z tym związek, czy szukać innej przyczyny?
H-02.pytanie_asvauto_m32;pytanie;200;EPAP przy maksimum {max}, bezdechy przy nim {y} / h: czy zakres EPAP może mieć z tym związek, czy szukać innej przyczyny?
H-02.zdanie;ramka;60;Obszar do omówienia z lekarzem.
H-03.linia;ramka;60;Najpewniej do poprawy: start terapii (rampa)
H-03.pytanie;pytanie;200;Zdarzenia skupione w pierwszych {prog:H-03.start_min} min sesji: czy start terapii (rampa) może mieć z tym związek, czy szukać innej przyczyny?
H-04.linia;ramka;60;Najpewniej do poprawy: ustawienia ciśnienia lub EPR
H-04.pytanie;pytanie;200;Centralne {x} / h od zmiany ustawień {data}: czy ustawienia ciśnienia lub EPR mogą mieć z tym związek, czy szukać innej przyczyny?
H-05.linia;ramka;60;Przyczynę oceni lekarz.
H-05.pytanie;pytanie;200;Wyniki gorsze od zmiany ustawień {data}: czy ustawienia mogą mieć z tym związek, czy szukać innej przyczyny?
H-06.linia;ramka;60;Najpewniej do poprawy: szczelność przy ustach (typ maski)
H-06.pytanie;pytanie;200;Wyciek falami: czy typ maski albo drożność nosa mogą mieć z tym związek?
H-07.linia;ramka;60;Do sprawdzenia z dostawcą: typ maski w aparacie
H-08.linia;ramka;60;Przyczynę oceni lekarz.
H-09.linia;ramka;60;Najpewniej do poprawy: czas z maską
H-09.pytanie;pytanie;200;Senność przy medianie {czas} z maską na noc: czy czas z maską może mieć z tym związek, czy szukać innej przyczyny?
L-01.krok;krok;60;→ Omów to z lekarzem w najbliższych tygodniach.
L-01.powod;powod;70;Centralne średnio {x} / h z {prog:L-01.okno} nocy, zwykle {norma}.
L-01.powod_asv;powod;70;Bezdechy średnio {x} / h z {prog:L-01.okno} nocy, przy pełnym wsparciu {p}%.
L-01.tytul;tytul;30;Bezdechy centralne utrwalone
L-01.tytul_asv;tytul;30;Bezdechy mimo pełnego wsparcia
L-01.zdanie_po_zmianie;raport;120;Początek wzorca {n} nocy po zmianie ustawień {data}.
L-02.krok;krok;60;→ Omów wynik z lekarzem w najbliższych tygodniach.
L-02.linia_asv;raport;120;Oddychanie okresowe: aparat ASV go nie oznacza.
L-02.linia_b;linia;60;Oddychanie okresowe częściej. Na kontroli: raport 2 / 6.
L-02.powod_a;powod;70;Oddychanie okresowe w {n} z {prog:L-02.a_okno} nocy, razem {min} min.
L-02.powod_b;powod;70;Oddychanie okresowe {x}% czasu nocy, zwykle {norma}%.
L-02.tytul;tytul;30;Urządzenie oznaczyło wzorzec
L-03.krok;krok;60;→ Omów z lekarzem, pokaż raport z 2 tygodni.
L-03.linia_5_10;linia;60;Bezdechy {x} / h od 4 tygodni. Na kontroli: raport 2 / 6.
L-03.powod;powod;70;Mediana z {prog:L-03.okno} nocy {x} / h, wyciek mały.
L-03.tytul;tytul;30;Bezdechy utrzymują się wyżej
L-04.krok;krok;60;→ Omów z lekarzem, pokaż raport z 2 tygodni.
L-04.powod;powod;70;Ciśnienie blisko maksimum {max}, bezdechy obturacyjne {y} / h.
L-04.powod_asvauto;powod;70;EPAP blisko maksimum {max}, bezdechy obturacyjne {y} / h.
L-04.powod_asvauto_m32;powod;70;EPAP blisko maksimum {max}, bezdechy przy nim {y} / h.
L-04.tytul;tytul;30;Ciśnienie przy górnej granicy
L-04.tytul_asvauto;tytul;30;EPAP przy górnej granicy
L-05.krok;krok;60;→ Pokaż to lekarzowi w najbliższych tygodniach.
L-05.powod;powod;70;Oddechy {x} / min, zwykle {norma}, od {n} nocy.
L-05.tytul;tytul;30;Zmiana oddechu od kilku nocy
L-07.linia;linia;60;Ciśnienie rośnie od 2 tygodni. Na kontroli: raport 2 / 6.
L-07.linia_asv;linia;60;EPAP rośnie od 2 tygodni. Na kontroli: raport 2 / 6.
L-07.tytul;tytul;30;Ciśnienie rośnie od tygodni
L-07.tytul_asv;tytul;30;EPAP rośnie
L-08.krok_szybki;krok;60;→ Omów to z lekarzem w najbliższych tygodniach.
L-08.linia;linia;60;Regularność niższa od 4 tygodni. Na kontroli: raport 2 / 6.
L-08.powod_szybki;powod;70;Ponad 4 h w {n} z 30 nocy, miesiąc temu {m} z 30.
L-08.tytul_szybki;tytul;30;Dużo mniej nocy z maską
L-09.krok;krok;60;→ Zapytaj dostawcę sprzętu o inną maskę.
L-09.linia;linia;60;Wyciek nie ustępuje od {n} nocy. Pokaż na kontroli.
L-09.powod;powod;70;Duży wyciek w {n} z 14 nocy. Bezdechy mniej pewne.
L-09.tytul;tytul;30;Wyciek nie ustępuje
L-10.krok;krok;60;→ Pokaż to lekarzowi w najbliższych tygodniach.
L-10.linia;linia;60;Spłaszczone wdechy i senność. Na kontroli: raport 2 / 6.
L-10.powod;powod;70;Od {n} tygodni więcej spłaszczonych wdechów i senność.
L-10.tytul;tytul;30;Spłaszczone wdechy i senność
L-11.krok;krok;60;→ Pokaż to lekarzowi, sama liczba nie wystarczy.
L-11.powod;powod;70;Senność w {n} z {prog:L-11.wariant_2tyg_okno} tygodni, urządzenie liczy {x} / h.
L-11.tytul;tytul;30;Senność mimo terapii
L-12.krok;krok;60;→ Omów to z lekarzem w najbliższych tygodniach.
L-12.powod;powod;70;{metryka} {x}, przed zmianą {y} ({data}).
L-12.tytul;tytul;30;Gorzej od zmiany ustawień
L.etykieta_a;etykieta;60;Skontaktuj się w ciągu kilku dni
L.etykieta_b;etykieta;60;Warto skonsultować · w najbliższych tygodniach
L.pytanie_ogolne;pytanie;200;{tytuł}: czy to wymaga korekty leczenia albo dalszej diagnostyki?
L.trwa_po_wizycie;linia;60;{nazwa}: trwa 8 tyg. po wizycie.
L.wciaz;linia;60;Wciąż: {tytuł}.
L.wciaz_kontrola;linia;60;Wciąż: {tytuł}. Pokaż na kontroli.
L.wciaz_omowione;linia;60;Wciąż: {tytuł}. Omówione {data}.
O-03.ahi.powod;powod;70;AHI {x} / h, najniżej od {miesiąc}.
O-03.krok;krok;60;→ Nic do zmiany. Tak trzymaj.
O-03.tytul;tytul;30;Rekordowy tydzień
O-03.wyciek.powod;powod;70;Wyciek {x} l/min, najniżej od {miesiąc}.
O-04.krok;krok;60;→ Trzymaj obecny rytm.
O-04.powod;powod;70;{Metryka} {x}, w ostatnim miesiącu {y}.
O-04.tytul;tytul;30;Lepszy tydzień niż miesiąc
O-05.krok;krok;60;→ Pokaż to na kontroli.
O-05.powod;powod;70;{p}% zdarzeń w ostatniej części nocy.
O-05.tytul;tytul;30;Zdarzenia nad ranem
O-06.krok;krok;60;→ Wietrz przed snem.
O-06.powod;powod;70;{Metryka} {x} przy CO2 ponad {prog:O-06.co2}, {y} niżej. Z {n} nocy.
O-06.tytul;tytul;30;W duszne noce {zmiana}
O-07.krok;krok;60;→ Nawilżacz pokojowy na noc.
O-07.powod;powod;70;Wyciek wyższy o {dx} l/min przy wilgotności poniżej {prog:O-07.wilgotnosc}%.
O-07.tytul;tytul;30;Suche noce, większy wyciek
O-08.krok;krok;60;→ Weekend o zwykłej porze.
O-08.powod;powod;70;W weekendy {metryka} {różnica}. Z {n} nocy.
O-08.tytul;tytul;30;Weekendy inne niż tydzień
O-09.krok;krok;60;→ Maska przed 00:30, gdy możesz.
O-09.powod;powod;70;Po 00:30 {metryka} {różnica}. Z {n} nocy.
O-09.tytul;tytul;30;Pora startu a twoje noce
O-10.krok;krok;60;→ Pokaż to lekarzowi.
O-10.powod;powod;70;Opór oddechu rośnie, gdy ciśnienie w nocy jest niskie.
O-10.tytul;tytul;30;Opór oddechu a ciśnienie
O-11.krok;krok;60;→ Senność? Pokaż to lekarzowi.
O-11.powod;powod;70;Najczęściej: {cecha}. Wcześniej: inny.
O-11.tytul;tytul;30;Zmienił się kształt wdechów
O-12.krok;krok;60;→ Sam zdecyduj, co z tym zrobisz.
O-12.krok_leki;krok;60;→ Powiedz o tym lekarzowi.
O-12.powod;powod;80;{Metryka} {x} {odmiana}, {y} bez. Z {n}/{m} nocy.
O-12.powod_ukryty;powod;70;Wynik z twoich odpowiedzi rano. Szczegóły: ▶ kokpit.
O-12.przypadek;powod;70;Może to przypadek.
O-12.tytul;tytul;40;{Odmiana}: {zmiana}
O-12.tytul_skrot;tytul;30;Odkrycie: {czynnik}
O-12.tytul_ukryty;tytul;30;Odkrycie o czynniku nocy
R-DANE-29.linia_nieznany;linia;60;Nie znam trybu aparatu: tej nocy nie oceniam.
R-DANE-29.linia_tryb;linia;60;Tryb aparatu {tryb}: ta wersja go nie ocenia.
R-INF-10.linia;linia;60;Gorączka? Skontaktuj się z lekarzem. Duszność: wezwij pomoc.
R-MAS-01.filtr;powod;70;Filtr: wymieniony dziś.
R-MAS-01.maska;powod;70;Maska: wymieniona dziś.
R-MAS-01.tytul;tytul;30;Wymiana zapisana.
R-MAS-11.linia;linia;60;Nowy model maski. Uczę się normy.
R-MAS-12.etykieta;etykieta;30;Sprzęt
R-MAS-12.opis;powod;70;Wyciek spadł z {y} do {x} l/min od {data}.
R-MAS-12.stopka;linia;60;Bez odpowiedzi do 11:00 = nie zapisuję wymiany.
R-MAS-12.tytul;tytul;30;Wymieniona maska?
R-MAS-14.powod;powod;70;Uczę się normy wycieku od nowa: 14 nocy.
R-MAS-14.tytul;tytul;30;Nowy model maski zapisany.
R-NOR-14.linia;linia;60;Nowe ustawienia aparatu. Uczę się normy.
S-K-03.etykieta;etykieta;30;Przed snem
S-K-03.krok;krok;60;→ Owiń rurę otuliną.
S-K-03.powod;powod;70;W sypialni {t} °C. W zimnej rurze para skrapla się w wodę.
S-K-03.tytul;tytul;30;Chłodna sypialnia
S-K-04.etykieta;etykieta;30;Przed snem
S-K-04.krok;krok;60;→ Włącz nawilżacz pokojowy na noc.
S-K-04.powod;powod;70;Wilgotność {w}%. Suche powietrze wysusza nos i usta.
S-K-04.tytul;tytul;30;Suche powietrze w sypialni
S-K-05.etykieta;etykieta;30;Przed snem
S-K-05.krok;krok;60;→ Schłodź sypialnię przed snem.
S-K-05.powod;powod;70;Dziś {t} °C. W upały noce z maską bywają krótsze.
S-K-05.powod_sypialnia;powod;70;W sypialni {t} °C. W upały noce z maską bywają krótsze.
S-K-05.tytul;tytul;30;Gorąca noc przed tobą
S-K-06.etykieta;etykieta;30;Przed snem
S-K-06.krok;krok;60;→ Zatkany nos? Powiedz lekarzowi.
S-K-06.powod;powod;70;Dziś dużo pyłku. Zatkany nos utrudnia oddychanie przez maskę.
S-K-06.tytul;tytul;30;Dużo pyłku {rodzaj}
S-K-07.etykieta;etykieta;30;Przed snem
S-K-07.krok;krok;60;→ Maska dziś około {hh:mm}.
S-K-07.powod;powod;70;Przez 7 dni maska o różnych porach, rozrzut {min} min.
S-K-07.tytul;tytul;30;Stała pora sprzyja masce
S-K-08.etykieta;etykieta;30;Przed snem
S-K-08.krok;krok;60;→ Przygotuj maskę już teraz.
S-K-08.powod;powod;70;Wczoraj noc bez maski. Każda noc z terapią się liczy.
S-K-08.tytul;tytul;30;Dziś znowu z maską
S-K-09.etykieta;etykieta;30;Przed weekendem
S-K-09.krok;krok;60;→ W sobotę maska o zwykłej porze.
S-K-09.powod;powod;70;W weekendy masz maskę średnio o {min} min krócej.
S-K-09.tytul;tytul;30;Weekend przed tobą
S-K-10.etykieta;etykieta;30;Ostatnia noc
S-K-10.kilka.etykieta;etykieta;30;Maska · od kilku nocy
S-K-10.kilka.krok;krok;60;→ Dopasuj maskę, sprawdź poduszkę.
S-K-10.kilka.powod;powod;70;Wyciek ponad {próg} l/min w {n} z 7 nocy.
S-K-10.kilka.powod_od;powod;70;Wyciek powyżej {próg} l/min w {n} z 7 nocy.
S-K-10.kilka.tytul;tytul;30;Maska przecieka od {n} nocy
S-K-10.krok;krok;60;→ Sprawdź poduszkę i paski.
S-K-10.opisowy.etykieta;etykieta;30;Ostatnia noc
S-K-10.opisowy.krok;krok;60;→ Obserwuj kolejne noce.
S-K-10.opisowy.powod;powod;70;W części nocy wyciek {x} l/min, bez dużego wycieku.
S-K-10.opisowy.tytul;tytul;30;Wyciek widoczny
S-K-10.opisowy_kilka.etykieta;etykieta;30;Ostatnie noce
S-K-10.opisowy_kilka.krok;krok;60;→ Obserwuj kolejne noce.
S-K-10.opisowy_kilka.powod;powod;70;Wyciek od {próg} l/min w {n} z 7 nocy.
S-K-10.opisowy_kilka.tytul;tytul;30;Wyciek widoczny od {n} nocy
S-K-10.powod;powod;70;Wyciek ponad {próg} l/min przez {min} min. AHI mniej pewne.
S-K-10.powod_p95;powod;70;W części nocy wyciek {x} l/min. AHI mniej pewne.
S-K-10.tytul;tytul;30;Maska przeciekała
S-K-11.etykieta;etykieta;30;Ostatnia noc
S-K-11.krok;krok;60;→ Zapytaj dostawcę o inną maskę lub pasek pod brodę.
S-K-11.powod;powod;70;Taki wzór pasuje do otwierania ust we śnie. To częsta sprawa.
S-K-11.tytul;tytul;30;Wyciek pojawiał się falami
S-K-12.etykieta;etykieta;30;Ostatnia noc
S-K-12.krok;krok;60;→ Patrz na trend z całego tygodnia, nie na tę noc.
S-K-12.powod;powod;70;Duży wyciek albo krótka noc. Liczby z tej nocy są mniej pewne.
S-K-12.tytul;tytul;30;Ta noc jest mało czytelna
S-K-14.etykieta;etykieta;30;Ostatnia noc
S-K-14.krok;krok;60;→ Suchość co rano? Powiedz o tym dostawcy sprzętu.
S-K-14.powod;powod;70;W nocy wilgotność {w}%, wyciek {x} l/min. Suche usta?
S-K-14.powod_norma;powod;70;W nocy wilgotność {w}%, wyciek wyższy niż zwykle. Suche usta?
S-K-14.tytul;tytul;30;Sucha noc i wyższy wyciek
S-K-15.etykieta;etykieta;30;Ostatnia noc
S-K-15.krok;krok;60;→ Dziś schłodź pokój, zanim założysz maskę.
S-K-15.powod;powod;70;W sypialni {t}°C, maska tylko {czas}. W upał to częste.
S-K-15.powod_norma;powod;70;W sypialni {t}°C, maska {ile} krócej niż zwykle. W upał to częste.
S-K-15.tytul;tytul;30;Gorąca noc, krótsza maska
S-K-16.etykieta;etykieta;30;Ostatnia noc
S-K-16.krok;krok;60;→ Sprawdź, co budziło w nocy.
S-K-16.powod;powod;70;Coś mogło przeszkadzać w nocy.
S-K-16.powod_norma;powod;70;Zwykle {m}. Coś mogło przeszkadzać w nocy.
S-K-16.tytul;tytul;30;Maska zdejmowana {n} razy
S-K-17.cisnienie;powod;70;ciśnienie wyższe niż zwykle
S-K-17.epap;powod;70;EPAP wyższe niż zwykle
S-K-17.etykieta;etykieta;30;Ostatnie 2 noce
S-K-17.krok;krok;60;→ Czujesz się gorzej? Skonsultuj się z lekarzem.
S-K-17.oddech;powod;70;Oddech szybszy o {x} na minutę
S-K-17.tytul;tytul;30;Noc inna niż zwykle
S-K-17.wyciek;powod;70;wyciek wyższy niż zwykle
S-K-18.etykieta;etykieta;30;Twój tydzień
S-K-18.krok_czas;krok;60;→ Każda godzina z maską się liczy.
S-K-18.krok_dane;krok;60;→ Część ocen jeszcze nie działa: za mało nocy.
S-K-18.krok_dane_pliki;krok;60;→ Bezdechów nie oceniam: brak plików nocy.
S-K-18.krok_dane_wyciek;krok;60;→ Bezdechów nie oceniam: noce z wyciekiem.
S-K-18.krok_linia;krok;60;→ Sprawa z linii wyżej wciąż otwarta.
S-K-18.krok_nic;krok;60;→ Nic do zrobienia. Tak trzymaj.
S-K-18.krok_oceny;krok;60;→ Część ocen wyłączona: zobacz ▶.
S-K-18.krok_pora;krok;60;→ Stała pora snu, też w weekend.
S-K-18.krok_przerwy;krok;60;→ Sprawdź, co budzi w nocy.
S-K-18.krok_reguly;krok;60;→ Część reguł nie działa: zobacz Test sprzętu.
S-K-18.krok_sennosc;krok;60;→ Za kierownicą nie ryzykuj.
S-K-18.krok_sprawy;krok;60;→ Otwarte sprawy: strona 3 / 6.
S-K-18.krok_wyciek;krok;60;→ Sprawdź szczelność maski.
S-K-18.powod;powod;70;Noc z maską: średnio {czas}.
S-K-18.powod_bez;powod;70;W tym tygodniu bez nocy z maską.
S-K-18.tytul;tytul;30;{n} z 7 nocy ponad 4 h
S-K-19.etykieta;etykieta;30;Twój tydzień
S-K-19.krok;krok;60;→ Senność w dzień? Pokaż te dane lekarzowi.
S-K-19.powod;powod;70;W tym tygodniu więcej wdechów o płaskim kształcie niż zwykle u ciebie.
S-K-19.tytul;tytul;30;Więcej spłaszczonych wdechów
S-K-20.asv.powod;powod;70;Aparat dobiera EPAP średnio o {x} cmH2O wyżej niż zwykle.
S-K-20.asv.tytul;tytul;30;EPAP rośnie od tygodni
S-K-20.etykieta;etykieta;30;Twój trend
S-K-20.krok;krok;60;→ Przy najbliższej wizycie pokaż ten trend lekarzowi.
S-K-20.powod;powod;70;Urządzenie dobiera średnio o {x} cmH2O więcej niż zwykle.
S-K-20.tytul;tytul;30;Ciśnienie rośnie od tygodni
S-K-21.co2.krok;krok;60;→ Spróbuj z uchylonym oknem.
S-K-21.co2.powod;powod;70;Przy nocach z dusznym powietrzem maska krócej o {min} min.
S-K-21.co2.tytul;tytul;30;Odkrycie: twoje noce i CO2
S-K-21.etykieta;etykieta;30;Odkrycie
S-K-21.pylek.krok;krok;60;→ Zatkany nos? Powiedz lekarzowi.
S-K-21.pylek.powod;powod;70;Przy dużym pyłku maska krócej o {min} min.
S-K-21.pylek.tytul;tytul;30;Odkrycie: twoje noce i pyłek
S-K-21.upal.krok;krok;60;→ Chłodź sypialnię przed snem.
S-K-21.upal.powod;powod;70;Przy upalnych nocach maska krócej o {min} min.
S-K-21.upal.tytul;tytul;30;Odkrycie: twoje noce i upał
S-K-21.weekend.krok;krok;60;→ Weekend o zwykłej porze.
S-K-21.weekend.powod;powod;70;W weekendy maska krócej o {min} min.
S-K-21.weekend.tytul;tytul;30;Odkrycie: twoje noce i weekend
S-K-27.etykieta;etykieta;30;Kamień milowy
S-K-27.krok;krok;60;→ Kolejny cel: 90 dni.
S-K-27.powod;powod;70;Maska 4 h lub dłużej na {n} z 30 nocy.
S-K-27.tytul;tytul;30;30 dni: próg osiągnięty
S-K-29.etykieta;etykieta;30;Kamień milowy
S-K-29.krok;krok;60;→ Dziś wieczorem znowu z maską.
S-K-29.powod;powod;70;Po {n} nocach przerwy dziś {czas} z maską.
S-K-29.tytul;tytul;30;Wracasz do rytmu
S-K-30.etykieta;etykieta;30;Rano · noc inna niż zwykle
S-K-30.powod;powod;70;Oddechy {x} / min, zwykle {norma}. Organizm mógł być obciążony.
S-K-30.pytanie;pytanie;60;Katar albo infekcja?
S-K-30.tytul;tytul;30;Oddychasz szybciej niż zwykle
S-K-31.etykieta;etykieta;30;Rano · sprzęt
S-K-31.krok;krok;60;→ Wymień i przytrzymaj ◀.
S-K-31.powod;powod;70;Ma {m} mies., wyciek rośnie od {t} tyg.
S-K-31.tytul;tytul;30;Maska się zużywa
S-K-32.etykieta;etykieta;30;Sprzęt
S-K-32.krok;krok;60;→ Wymiana co {m} mies. się opłaca.
S-K-32.powod;powod;70;Wyciek {x} l/min, przed wymianą {y}.
S-K-32.tytul;tytul;30;Nowa maska działa
S2D.G01.neutralna.powod;powod;100;Podobno zyskaliśmy godzinę. Księgowość snu nie potwierdza.
S2D.G01.neutralna.tytul;tytul;90;Czas zmienił się w nocy.
S2D.G01.neutralna_marzec.tytul;tytul;90;Dzisiaj śpisz godzinę krócej.
S2D.G01.warunek.powod;powod;100;Maska też zmieniła pozycję, niestety bez konsultacji. Efekty widać w raporcie.
S2D.G01.warunek.tytul;tytul;90;Zegar przestawił się sam.
S2D.G02.neutralna.powod;powod;100;Dzień, w którym nawet budzik zastanawia się nad zmianą zawodu.
S2D.G02.neutralna.tytul;tytul;90;Poniedziałek.
S2D.G02.warunek.powod;powod;100;Na szczęście twoje wyniki postanowiły nie pogarszać sytuacji.
S2D.G02.warunek.tytul;tytul;90;Poniedziałek i tak ma fatalną reputację.
S2D.G03.neutralna.powod;powod;100;Oficjalnie można już udawać, że wszystkie problemy poczekają do poniedziałku.
S2D.G03.neutralna.tytul;tytul;90;Piątek.
S2D.G03.warunek.powod;powod;100;Przynajmniej jeden projekt obył się bez spotkania kryzysowego.
S2D.G03.warunek.tytul;tytul;90;Tydzień zamknięty, terapia pod kontrolą.
S2D.G04.neutralna.powod;powod;100;Większość z nich tradycyjnie odłożymy na przyszły poniedziałek.
S2D.G04.neutralna.tytul;tytul;90;Nowy rok, nowe możliwości.
S2D.G04.warunek.powod;powod;100;Na szczęście aparat nadal skutecznie studzi ich ambicje.
S2D.G04.warunek.tytul;tytul;90;Nowy rok, stare bezdechy.
S2D.G05.neutralna.tytul;tytul;90;Święta to czas spokoju, refleksji i udawania, że trzeci kawałek sernika się nie liczy.
S2D.G05.warunek.powod;powod;100;Mikołaj najwyraźniej uznał, że w tym roku zasługujesz na coś praktycznego.
S2D.G05.warunek.tytul;tytul;90;AHI w celu.
S2D.G06.neutralna.powod;powod;100;Jedyny okres w roku, kiedy poszukiwanie jajek uchodzi za poważną aktywność dorosłych.
S2D.G06.neutralna.tytul;tytul;90;Wielkanoc.
S2D.G06.warunek.powod;powod;100;Nie nazywajmy tego cudem, ale okoliczności są sprzyjające.
S2D.G06.warunek.tytul;tytul;90;Maska została na twarzy do rana.
S2D.G07.neutralna.powod;powod;100;Przyroda budzi się do życia. Ja, jako profesjonalny budzik, czuję się nieco zagrożony.
S2D.G07.neutralna.tytul;tytul;90;Wiosna.
S2D.G07.warunek.powod;powod;100;Sypialnia ma już wystarczającą wentylację.
S2D.G07.warunek.tytul;tytul;90;Wiosenne porządki warto zacząć od maski.
S2D.G08.neutralna.powod;powod;100;Dni coraz krótsze, wieczory coraz dłuższe. Natura najwyraźniej też nie przepada za poniedziałkami.
S2D.G08.neutralna.tytul;tytul;90;Jesień.
S2D.G08.warunek.powod;powod;100;Wreszcie odrobina stabilności w tym świecie.
S2D.G08.warunek.tytul;tytul;90;Liście spadają, temperatura spada, AHI pozostaje niskie.
S2D.G09.neutralna.powod;powod;100;Nie narzeka, nie spóźnia się i świetnie znosi chłód. Idealny współpracownik.
S2D.G09.neutralna.tytul;tytul;90;Ulepiłem bałwana.
S2D.G09.warunek.powod;powod;100;Zachowuję zimną krew, choć przyznam, że liczyłem na lepszy raport.
S2D.G09.warunek.tytul;tytul;90;AHI powyżej celu.
S2D.G10.neutralna.powod;powod;100;Owce odmówiły dalszej współpracy bez dodatku za pracę nocną.
S2D.G10.neutralna.tytul;tytul;90;Liczenie owiec przeszło audyt.
S2D.G10.warunek.powod;powod;100;Zdarzenia oddechowe również. Przyznam, że ta druga statystyka mniej mnie uspokaja.
S2D.G10.warunek.tytul;tytul;90;Owce policzone.
S2D.G11.neutralna.powod;powod;100;Najbardziej optymistyczna jednostka czasu znana ludzkości.
S2D.G11.neutralna.tytul;tytul;90;Pięć minut drzemki.
S2D.G11.warunek.powod;powod;100;Bezdechy z pewnością doceniają tak uprzejme pozostawienie im wolnego wieczoru.
S2D.G11.warunek.tytul;tytul;90;Maska zdjęta przed czasem.
S2D.G12.neutralna.powod;powod;100;Kusząca perspektywa, zwłaszcza przed pierwszą kawą.
S2D.G12.neutralna.tytul;tytul;90;Pod wodą podobno panuje cisza.
S2D.G12.warunek.powod;powod;100;Proponuję wrócić do jej pierwotnej funkcji.
S2D.G12.warunek.tytul;tytul;90;Przy takich wyciekach maska kwalifikuje się raczej do wentylacji pomieszczeń.
S2D.G13.neutralna.powod;powod;100;Szczególnie kiedy przeciwnik zastanawia się nad pionkiem przez dwadzieścia minut.
S2D.G13.neutralna.tytul;tytul;90;Szachy uczą cierpliwości.
S2D.G13.warunek.powod;powod;100;Sądząc po AHI, nie zdołały nawet wyprowadzić pionków.
S2D.G13.warunek.tytul;tytul;90;Bezdechy próbowały dziś rozegrać partię.
S2D.G14.neutralna.powod;powod;100;Cała godność dorosłego człowieka kończy się na zimnej podłodze.
S2D.G14.neutralna.tytul;tytul;90;Kapcie to niedoceniany wynalazek.
S2D.G14.warunek.powod;powod;100;Szkoda tylko, że bezdechy mają na ten temat dokładnie takie samo zdanie.
S2D.G14.warunek.tytul;tytul;90;Komfort przede wszystkim.
S2D.G15.neutralna.powod;powod;100;Zwłaszcza gdy publiczność uparcie oczekuje muzyki.
S2D.G15.neutralna.tytul;tytul;90;Dyrygowanie ciszą wymaga szczególnego talentu.
S2D.G15.warunek.powod;powod;100;Rzadki przypadek, gdy wszyscy grają z jednej partytury.
S2D.G15.warunek.tytul;tytul;90;AHI niskie, wycieki niewielkie, czas terapii odpowiedni.
S2D.G16.neutralna.powod;powod;100;Nie mam mięśni, ale przynajmniej nie muszę martwić się zakwasami.
S2D.G16.neutralna.tytul;tytul;90;Trening zaliczony.
S2D.G16.warunek.tytul;tytul;90;To była dobra noc.
S2D.G17.neutralna.powod;powod;100;Próba przekonania organizmu, że rozpoczęcie dnia było świadomą decyzją.
S2D.G17.neutralna.tytul;tytul;90;Poranny prysznic.
S2D.G17.warunek.powod;powod;100;Niestety nie poprawi statystyk nocnych wycieków. Sprawdzałem.
S2D.G17.warunek.tytul;tytul;90;Poranny prysznic odświeży samopoczucie.
S2D.G18.neutralna.powod;powod;100;Dopóki ktoś nie przypomni, kto będzie zmieniał pościel.
S2D.G18.neutralna.tytul;tytul;90;Śniadanie do łóżka brzmi luksusowo.
S2D.G18.warunek.powod;powod;100;Teraz pozostaje tylko nie zepsuć dnia. Zacząłbym od porządnego śniadania.
S2D.G18.warunek.tytul;tytul;90;Wyniki terapii bez zarzutu.
)zestaw";
}  // namespace wbudowany
}  // namespace reguly
