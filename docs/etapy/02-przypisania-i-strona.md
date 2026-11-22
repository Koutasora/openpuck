# Etap 02: przypisania-i-strona

## Cel
Cały zakres forka w jednym etapie: tylne przyciski L4/R4/L5/R5 dają przycisk, kombinację albo makro, 3 profile
przełączane na padzie, klawiatura i mysz w trybach PC, własna strona konfiguracji. Na końcu jeden build, test na
sprzęcie u użytkownika i wydanie .uf2 na GitHubie.

## Kryteria zamknięcia
Firmware: przypisania i profile
- [x] Tylny przycisk w każdym trybie emulacji (Xbox, Switch, DS4/PS3, PS5): jeden przycisk, kombinacja albo makro (kroki z czasami)
- [x] 3 profile, back4 + RB następny / back4 + LB poprzedni, liczba krótkich wibracji = numer profilu
- [x] Profile i makra zostają po odłączeniu zasilania; stary `cfg.bin` dalej się wczytuje, obecne przypisania stają się profilem 1
- [x] Polecenia WebUSB: odczyt i zapis przypisań i profili, stan na żywo (przyciski, sticki, triggery, profil, bateria i stan ładowania)
- [x] Logika makr i kombinacji w osobnym module bez zależności od sprzętu, z testem uruchamianym na LAB
- [x] Build wpisuje wersję i commit (panel nie pokazuje "unknown")

Firmware: klawiatura i mysz na PC
- [x] Na PC pad oraz klawiatura i mysz jako osobne urządzenia; tylny przycisk może dać klawisz (z modyfikatorami) albo przycisk/kółko myszy
- [x] Puck rozpoznaje, czy jest podłączony do PC czy do konsoli; klawiatura i mysz tylko na PC
- [x] Tryby konsolowe (PS5_GAME, PS5_AUTH, PS3, Switch, Xbox OG) bez zmian w deskryptorach USB

Strona konfiguracji
- [x] Makieta zaakceptowana przez użytkownika przed budową strony
- [x] Rysunek Steam Controllera z nazwami przycisków wg typu pada, tabela przypisań, edytor makr, wybór profilu
- [x] Podgląd na żywo: co wciskasz na padzie i co dostaje komputer (Gamepad API, klawiatura/mysz w przeglądarce)
- [x] Przycisk "test makra" bez wychodzenia ze strony; edytor makr zrozumiały bez instrukcji
- [x] Karta ze skrótami pada (kombinacje back4, profile, wyłączanie) jako obrazek do zapisania
- [x] Procent baterii i stan ładowania pada
- [ ] Strona na GitHub Pages repo

Test i wydanie
- [x] Użytkownik wgrał jeden końcowy .uf2 i przeszedł test z `CHECKLISTA.md` na PC (tester pada, Notatnik) i na konsoli
- [ ] Wydanie .uf2 na GitHubie repo
- [x] `CHECKLISTA.md` zaktualizowana

## Plan
- [x] `tools/build-lab.sh`: wersja i commit przez `gen_version.sh`
- [x] `OpenPuck/binds.cpp/.h`: typy akcji, 3 profile × 4 typy padów × 4 przyciski, silnik makr; test `tests/binds` (w64devkit)
- [x] Integracja w firmware: plik `binds.bin`, hook w `rf_link.cpp`, back4 + LB/RB z wibracją, polecenia WebUSB (z baterią), polecenia konsoli
- [x] Klawiatura i mysz: osobne interfejsy HID w trybach PC, rozpoznawanie PC/konsola, akcje klawisz/mysz w `binds`
- [x] Makieta strony, akceptacja użytkownika (panel autora z rozszerzeniami, zrzuty z atrapą)
- [x] Strona: panel autora w płaskim stylu, rysunki Valve, profile, Combo/Macro, Macros, Live view, bateria
- [x] Strona: akcje Key/Mouse, karta skrótów jako obrazek, usunięcie zielonych resztek
- [x] Makra prostsze w obsłudze (na końcu)
- [x] Dźwięki z haptyki (HAPTIC_LFO_TONE, `docs/PROTOCOL.md`): melodie w pucku, przypisanie do zdarzeń (zmiana profilu, zmiana trybu osobno dla rodzin PlayStation/Xbox/Switch, połączenie pada), przełącznik wibracja/dźwięk/oba/wyłączone i głośność w Settings; na stronie edytor tonów z odsłuchem i import MIDI (jedna linia melodii); domyślnie własne, oryginalne motywy
- [x] Rzadsze wysyłanie stanu klawiatury/myszy (dokończyć i sprawdzić `d47ac39`)
- [x] Strona nie przebudowuje kart przy każdym odczycie (zaznaczenie tekstu), przycisk Copy w Version
- [x] Nazwy profili, kopiowanie i reset profilu, eksport/import samych przypisań i makr, nowe nazwy myszy w Lizard mapping
- [x] GitHub Pages (konfiguracja w repo; widoczność repo zmienia użytkownik)
- [ ] Końcowy build, lista testów w `CHECKLISTA.md`, test użytkownika, wydanie
- Sprawdzenie: test `tests/binds` i build na LAB po każdym kroku; test strony w Chromium (Playwright) na LAB bez sprzętu; jeden test na sprzęcie u użytkownika na końcu

## Decyzje
- Akcje tylnych przycisków stosowane w jednym miejscu przed trybami: tryby dalej tłumaczą tylko kody przycisków, mniej konfliktów przy merge z `upstream`.
- Przypisania w osobnym pliku `binds.bin`: układ `cfg.bin` autora bez zmian, stary zapis się wczytuje.
- Profile: back4 + LB/RB (do tej pory wolne), potwierdzenie wibracją.
- Test logiki na LAB przez przenośny w64devkit 2.11.0 (GCC 16.2, bez instalatora, jak arduino-cli).
- Gałąź autora `copilot/170-add-controller-mapping` pominięta: jej kombinacje back4 + D-pad są już w `main` (`g_chordDpad`).
- Dźwięki: najkrótsza nuta i pauza to 30 ms (firmware, strona, import MIDI łączy krótsze segmenty z poprzednią nutą): nuta to 2 raporty 0x83 na pada, a kolejka przekaźnika (32 wpisy, ~1 wpis na cykl) przy krótszych zalewała i wypychała inne ramki.
- Dźwięki: kolejna nuta czeka, gdy kolejka slotu ma więcej niż 4 wpisy (`relayDepth`); po 1 s bez opróżnienia melodia się kończy. Wolimy opóźnić nutę niż wypchnąć cudze ramki.
- Flaga zmiany trybu (`/sndmode.bin`) usuwana zaraz po odczycie przy starcie (żyje tylko w RAM do pierwszego połączenia); przy odtwarzaniu ponownie sprawdzane `confirm & SC_SOUND`.
- Confirmations = Both: impulsy wibracji najpierw, melodia po ostatnim impulsie (zerowe 0x80 melodii ucinały impuls).
- Melodie NIE respektują `g_rumble` (wyłączona wibracja dla typu pada): dźwięk to osobne ustawienie Confirmations.
- Strona: zmiana głośności/potwierdzeń zapisuje tylko te pola na bazie ostatnio odczytanego/zapisanego bloku (bez niezapisanych edycji melodii); zapis w trakcie zapisu jest kolejkowany; import przypisań czeka na trwający zapis.
- Dawne etapy 02-05 połączone w jeden: użytkownik chce w openpuck jednej gałęzi bez pośrednich PR i jednego testu na sprzęcie na końcu.
- Dźwięki: import MIDI przesuwa melodię o pełne oktawy w górę (do 130 Hz najniższej nuty, w granicach 4000 Hz), edytor ma przyciski Octave up/down i ostrzeżenie; powód: aktuator nie gra niskich nut, próg `SND_AUDIBLE_MIN` 130 Hz jest tymczasowy do testu na sprzęcie.
- Bateria w poleceniu stanu na żywo: firmware już czyta `g_battery`/`g_batteryState`, strona tylko je pokazuje.
- Plik `binds.bin`: magic 0xB1, suma kontrolna, 1079 B; zły plik = wartości domyślne. Wersja 2 dodaje akcje Key/Mouse w tym samym układzie; wersja 1 nadal się wczytuje.
- WebUSB: polecenia 0x26-0x2A, odpowiedzi 0xAC (stan na żywo), 0xAE (fragment bloku), 0xAF (potwierdzenie); wersja bloku statusu 23 (opis w `docs/PROTOCOL.md` §10.2). Kody odpowiedzi zastąpione, patrz niżej (0xB0/0xB2/0xB3).
- Makro gra raz do końca, puszczenie go nie przerywa, ponowne wciśnięcie w trakcie ignorowane.
- `g_type[].back` przepisywane z aktywnego profilu (0 dla kombinacji/makra): stary panel autora i `cfg.bin` zostają spójne.
- Wynik akcji wpisywany też do surowego raportu (rep[2..5]), żeby trafiał do trybów push (Xbox, Xbox OG).
- Przy trzymanych 4 łopatkach `g_back[]` zerowane, a LB/RB maskowane: łopatki i LB/RB nie przeciekają do hosta podczas kombinacji back4 (drobna zmiana względem autora).
- Profil aktywny jeden dla wszystkich padów (nie osobno na slot).
- Strona = panel autora `docs/index.html` po angielsku, jego układ i styl; dokładamy tylko nasze elementy (profile, akcje, rysunek, makra, podgląd, bateria). Pierwsza makieta (osobna strona, rysunek odręczny) odrzucona przez użytkownika.
- Rysunek pada: rzuty z oficjalnego rysunku technicznego Valve (`steam_controller_reference_20260429.pdf` z repo SteamHardware/SteamController), CC BY-NC-SA 4.0, z podpisem autorstwa; pliki CAD poza gitem.
- Panel autora ma tylko ciemny motyw: zostaje bez przełącznika jasny/ciemny, bo strona ma trzymać się jego wyglądu.
- Kolejność po akceptacji strony (użytkownik): najpierw pad + klawiatura/mysz i rozpoznawanie PC/konsola, potem strona (Key/Mouse, karta skrótów), makra prostsze na końcu.
- Odpowiedzi WebUSB przeniesione na 0xB0 (stan na żywo), 0xB2 (fragment bloku), 0xB3 (potwierdzenie): 0xAC/0xAD zajmuje przystawka ReversePuck.
- Klawiatura i mysz tylko w trybach PS5 (z panelem) i DS4-gyro: są już złożone i nie robią auth konsoli. Tryb Xbox pominięty: statyczny, jednointerfejsowy, konsola Xbox 360 robi auth XSM3 i bez testu na sprzęcie ryzyko zepsucia; do decyzji użytkownika.
- Mysz = istniejący interfejs "wake mouse" (5 przycisków + kółko, deskryptor bez zmian); klawiatura = nowy interfejs niebootowy, tylko po wykryciu PC, nie przy 4 padach naraz (limit endpointów).
- Rozpoznawanie hosta: zapytanie Windows o MS OS 2.0 albo połączenie panelu WebUSB = PC; auth Sony 0xF0-0xF3 = konsola; Linux/macOS: odczyt tożsamości pada i 5 s bez auth = PC. Start bez klawiatury (chyba że poprzednio PC), po wykryciu jedna przeenumeracja bez restartu. Ręcznie: auto / zawsze PC / zawsze konsola w `host.bin`.
- Kroki makr z klawiszem pominięte: zmieniałyby układ kroków, a akcje Key/Mouse na łopatce pokrywają główny przypadek.
- Makra na stronie: kroki opisane zdaniami (naciśnij/czekaj), szablony, nagrywanie z pada przez stan na żywo (~30 Hz), oś czasu, nazwy w przeglądarce (firmware nazw nie trzyma).
- Strona do testu uruchamiana lokalnie (`python -m http.server -d docs`): repo prywatne, GitHub Pages wymaga płatnego planu; sposób publikacji do ustalenia przy wydaniu.
- Podgląd na żywo i nasze odczyty WebUSB wstrzymane w trakcie aktualizacji firmware (uwaga managera: równoległy odczyt gubił potwierdzenia aktualizacji).
- clang-format 18 z pip w venv (przenośnie), uruchamiany tylko na plikach zmienionych w gałęzi: CI autora blokuje PR bez formatowania.
- Użytkownik (po nocy): klawiatura/mysz także w trybie Xbox na PC (gra w trybie Xbox, push-to-talk na F13), o ile konsola Xbox 360 zostaje bez zmian; kopia zapasowa obejmuje przypisania, makra, host i nazwy makr; publikacja strony po teście.
- F13-F24 na stronie z notką: gry przełączają się w tryb klawiatura/mysz na każdy klawisz lub klik (problem użytkownika z Mouse 3 na pucku Valve), F13-F24 zwykle ignorują.
- Tryby PS5 DualSense i HID gyro (DS4): bcdUSB 2.10 zamiast 2.00 (test użytkownika: Windows przy 2.00 nie czyta MS OS 2.0, brak WinUSB, panel się nie łączy, host nierozpoznany); nierozpoznany host w tych trybach = PC, bo konsola bez adaptera ich nie przyjmuje. Tryby clean bez zmian.
- Test 2026-11-22 (główny PC, PS5 DualSense): klik lewy/prawy i klawisz z łopatek działają. Na LAB przez RDP nie działały: fizyczna mysz/klawiatura trafia do sesji konsoli, nie RDP (diagnostyka 0xB4 pokazała wysłane raporty). Diagnostyka (Copy diagnostics, 0x2C/0xB4) zostaje w panelu.
- Test w grach (użytkownik): push-to-talk z łopatki (F13, mysz) działa w Wiedźminie i emulatorze bez przycinania; w jednej grze każde zdarzenie klawiatura/mysz przełącza grę z pada na klawiaturę/mysz. Uznane za zachowanie gry, bez zmian w pucku (odrzucone: sztuczny przycisk pada po akcji, klawisze multimedialne).
- Test na sprzęcie 2026-11-22 (główny PC): użytkownik sprawdził całą listę z `CHECKLISTA.md` (przypisania, profile z wibracją, makra, klawiatura/mysz, strona) — działa.
- Do następnego czatu: rzadsze wysyłanie stanu klawiatury/myszy (częściowo w `d47ac39`, niewgrane), strona nie przebudowuje kart przy każdym odczycie (zaznaczenie tekstu znika), potem końcowy build, wydanie .uf2 i publikacja strony.
- Dźwięki: oryginalne dźwięki PlayStation/Xbox/Switch nie są przepisywane ani dołączane (utwory i znaki dźwiękowe tych firm); użytkownik może wgrać własne melodie przez import MIDI. Domyślne motywy własne.
- Do decyzji użytkownika w nowym czacie: nazwy profili, kopiowanie profilu i reset przypisań, eksport/import samych przypisań i makr, nowe nazwy myszy w Lizard mapping.
- Użytkownik (2026-11-22): pozostałe kroki jednym ciągiem bez przystanków; dźwięki w pełnym zakresie, edytor tonów bez makiety (pokazany przy teście końcowym); dołączone nazwy profili, kopiowanie/reset profilu, eksport/import przypisań, nazwy myszy w Lizard; repo `Koutasora/openpuck` będzie publiczne dla GitHub Pages (zmienia użytkownik).
- `d47ac39` (sendState najwyżej raz na ms) sprawdzony: ograniczenie tylko odracza wywołanie, `kbmFwTask` biegnie w każdej iteracji loop(), więc ostatni stan (np. puszczenie klawisza) wychodzi najpóźniej w następnej milisekundzie; bez zmian.
- Dźwięki: plik `sounds.bin` (magic 0xB8, wersja 1, suma kontrolna, 1048 B), 8 slotów melodii po 32 nuty {freq u16, ms u16}: prosty stały układ, bez kompresji, mieści się w jednym bloku flash jak `binds.bin`.
- Dźwięki: jedna tabela 8 zdarzeń -> numer melodii (profil 1/2/3 osobno, tryb PlayStation/Xbox/Switch/inne, połączenie), domyślnie melodia i dla zdarzenia i: prościej niż liczba nut i pozwala użytkownikowi dowolnie przypisać.
- Dźwięki: ustawienie potwierdzeń (wył./wibracja/dźwięk/oba) dotyczy zmiany profilu (wibracja = N impulsów jak dotąd); melodie trybu i połączenia grają tylko gdy ustawienie zawiera dźwięk. Domyślnie wibracja, więc zachowanie sprzed zmiany bez zmian, dopóki 0x83 nie sprawdzony na sprzęcie.
- Dźwięki: nuta = raport 0x83 (side 0 i 1, gain_db = głośność, bez LFO) przez relayEnqueue jak wibracja; znaczenie `side` i słyszalność nie potwierdzone na sprzęcie, więc oba boki; każda pauza i koniec melodii wysyłają zerowy raport 0x80 (haptyka się zatrzaskuje, nie wiadomo czy `dur` kończy ton).
- Dźwięki: zmiana trybu to reboot (modeSwitchReboot), więc flaga w pliku `/sndmode.bin` (RAM nie przeżywa); melodia trybu gra przy pierwszym połączeniu po starcie i zastępuje melodię połączenia. Melodia połączenia najwyżej raz na 20 s na pada (łącze potrafi migotać).
- Dźwięki: WebUSB 0x2D-0x31 (odczyt/zapis bloku we fragmentach po 96 B, odsłuch slotu, odsłuch podanej melodii), odpowiedzi 0xB5/0xB6; wersja bloku statusu 25 (opis w `docs/PROTOCOL.md` §10.5).
- Zakres częstotliwości nut 1-4000 Hz i czasu 30-5000 ms, głośność -60..0 dB, domyślnie -6: granice tylko po to, by zły blok z przeglądarki nie wysłał śmieci do pada.
- Strona nie przebudowuje DOM bez zmiany: jeden strażnik na `textContent`/`innerHTML` pomija zapis tej samej wartości, a zakładki kontrolerów mają podpis; łatwiej niż poprawiać każde miejsce osobno i chroni nowe karty.
- Dźwięki na stronie: melodie i zdarzenia zapisuje przycisk Save to device (edycja nut nie powinna pisać we flash przy każdym kliknięciu), a tryb potwierdzeń i głośność zapisują się same po 400 ms jak inne ustawienia (zapisują cały blok, także rozpoczęte zmiany melodii).
- Nazwy profili i melodii w przeglądarce (localStorage) jak nazwy makr; wchodzą do kopii zapasowej (`panel.profileNames`, `panel.sounds`).
- Import MIDI: jedna linia (najwyższa nuta akordu), pauzy jako nuty o freq 0 (liczą się do limitu 32), tempo z meta 0x51, ścieżka do wyboru (perkusja oznaczona); czasy i częstotliwości poza zakresem obcinane z informacją, SMPTE i format 2 odrzucane.
- Plik przypisań to czytelny JSON (nie hex bloku), walidowany przed jakąkolwiek zmianą stanu; akcje Key/Mouse wymagają statusu >= 24, jak w firmware.
- Reset profilu = L4 LB, R4 RB, L5 L3, R5 R3 dla każdego typu pada (jak `bindsInitDefaults`); kopiowanie i reset idą zwykłym zapisem bloku przypisań.
- Copy w karcie Version kopiuje build firmware (wersja albo hash, zależnie od tego, co wpisał `gen_version.sh`) i protokół statusu.
- Test 2026-11-22 (główny PC): Play on puck gra melodię z aktuatora; raport 0x83 HAPTIC_LFO_TONE (side 0 i 1, bez LFO) jest słyszalny.
- Limit melodii zostaje 32 pozycje (nuty i pauzy) po 30-5000 ms, 8 melodii: użytkownik nie potrzebuje dłuższych utworów.
- Import melodii tylko z MIDI (bez RTTTL, MusicXML, audio): użytkownikowi wystarcza.
- Kombinacje back4 + B/X/Y i D-pad mogą mieć wartość None (`CHORD_NONE` = 0xFE, bajt spoza trybów i inny niż 0xFF z `cfg.bin`): nic nie robią; opcja na stronie od wersji bloku 26.

## Podsumowanie
(wypełnia /etap-zamknij)
