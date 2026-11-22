# Etap 03: midi-na-zywo

## Cel
Zakładka Sounds ma edytor piano roll: przeciągnięty plik MIDI można edytować, odsłuchać na żywo na padzie przez WebHID (raport 0x83) i zapisać do jednego z 8 slotów melodii pucka (do 32 pozycji, jeden głos). Bez zmian w firmware.

## Kryteria zamknięcia
- [x] Sprawdzone na sprzęcie i zapisane w Decyzjach: znaczenie `side` 0-4, czy `dur` kończy ton, sposób zatrzymania tonu, LFO, słyszalny zakres nut.
- [x] Piano roll edytuje melodię bieżącej zakładki (dodawanie, przesuwanie, długość, usuwanie, przyciąganie do siatki, zaznaczanie), zsynchronizowany z tabelą nut.
- [x] Przeciągnięty plik MIDI (i przycisk importu) wczytuje się bez limitu 32 nut i bez automatycznego przesuwania o oktawy.
- [x] Odsłuch na padzie przez WebHID: całość albo zaznaczenie, Stop, gra w karcie w tle, wyciszenie przy stopie, rozłączeniu i zamknięciu strony.
- [x] Zapis do slotu tylko w limitach pucka (licznik N/32, Crop to selection); format bloba bez zmian.
- [x] Zakładka Player usunięta, połączenie WebHID w Sounds, panel testowy w Status > Debug.
- [x] Test Playwright (`tests/sounds-web/`) przechodzi.
- [x] Test użytkownika na padzie (główny PC).
- [x] `docs/PROTOCOL.md`, README i CHECKLISTA zaktualizowane.

## Plan
- [x] 1. `docs/index.html`: połączenie WebHID i panel testowy tonu 0x83; test na sprzęcie, wyniki w Decyzjach.
- [x] 2. `docs/index.html`: zakładka Player usunięta; połączenie (Connect controller, wybór interfejsu) w karcie Sounds; panel testowy w Status > Debug.
- [x] 3. `docs/index.html`: piano roll w Sounds zamiast osi czasu (canvas: siatka nut z nazwami, oś ms z powiększaniem i przewijaniem, dodawanie, przeciąganie, zmiana długości, Delete, siatka wył./10/50/100 ms, zaznaczanie zakresu, nuty bez nakładania), tabela nut zostaje i jest zsynchronizowana.
- [x] 4. `docs/index.html`: import MIDI przeciągnięciem na piano roll i przyciskiem, wybór ścieżki, bez cięcia do 32 i bez automatycznego przesuwania o oktawy.
- [x] 5. `docs/index.html`: odsłuch na padzie (Play/Stop całości albo zaznaczenia, kreska pozycji, side 0 i 1, głośność z Settings, nuta = 0x83 z dur = długość, pauza = off tone, zegar w Workerze); podgląd w przeglądarce zostaje.
- [x] 6. `docs/index.html`: licznik N/32, Crop to selection, Save to device zablokowany poza limitami.
- [x] 7. `tests/sounds-web/`: test Playwright w Chromium z udawanym `navigator.hid`.
- [x] 8. `docs/PROTOCOL.md`, `README.md`, `CHECKLISTA.md`.
- [x] 9. Poprawki po teście użytkownika (`docs/index.html`, `tests/sounds-web/run.js`): przyciski odsłuchu w pasku nad piano rollem; przełącznik Fold (tylko wiersze z nutami); rysowanie przeciągnięciem (klik = długość ostatniej nuty), prawy/podwójny klik usuwa, kursory przesuwania i zmiany długości, szerszy uchwyt, wyższe wiersze; klik w klawisz gra nutę na padzie (bez połączenia w przeglądarce); "Crop to 32" sam przycina do 32 pozycji (od początku zaznaczenia albo melodii), znacznik końca 32. pozycji na zaznaczeniu; pole BPM (z MIDI przy imporcie, inaczej 120): zmiana rozciąga melodię proporcjonalnie, siatka w taktach (1/4-1/32) przy tym BPM, ms zostają jako opcja.
- BPM przechowywane per zakładka tylko w przeglądarce (jak nazwy melodii), poza blobem pucka: format zapisu bez zmian, puck i tak gra milisekundy.
- [x] 10. Firmware (`OpenPuck/sounds.h`, `sounds.cpp`, `sounds_fw.cpp`, `sounds_fw.h`, `webusb_config.cpp`, `serial_console.cpp`): melodia = 4 głosy (side 0, 1, 3, 4) po 32 pozycje, blob `sounds.bin` wersja 2 (v1 wczytywany: melodia do obu gładzików), 4 sekwencery ze wspólnym startem, pauza głosu = off tone na jego side, koniec = zerowy 0x80, protokół statusu 27 (26 już zajęty); 0x31 bez zmian (jeden głos).
- [x] 11. `docs/index.html`: przełącznik toru LT/RT/LG/RG, nuty do aktywnego toru, inne przygaszone, tabela i licznik N/32 aktywnego toru, import MIDI do aktywnego toru, Crop to 32 tnie zakres we wszystkich torach, odsłuch 4 torów naraz, zapis bloba v2 (protokół >= 27), przy protokole 25-26 zapis tylko toru LT z ostrzeżeniem; "Play on puck" usunięty.
- [x] 12. Testy: `tests/sounds` (v2, wczytanie v1, 4 głosy), build firmware z RAM/flash przed i po, clang-format 18 i lint, `tests/sounds-web` (tory).
- [x] 13. `docs/PROTOCOL.md`, README, CHECKLISTA; test użytkownika: wgranie UF2, zapis i granie 4 głosów z pucka.
- [x] 14. `docs/index.html`, `tests/sounds-web/run.js`, CHECKLISTA: import MIDI do 4 torów naraz: lista źródła przy każdym torze (ścieżka, kanał ścieżki z kilkoma kanałami, Keep current, Empty), domyślnie źródła bez perkusji od najwyższej średniej wysokości na LT, RT, LG, RG (jedno źródło: tylko LT), liczba pozycji przy każdym, jeden Import, BPM z pliku.
- [ ] 15. `docs/index.html`, `tests/sounds-web/run.js`, README, CHECKLISTA: jedna tabela w Mapping > Shortcuts: na górze Confirm with i głośność, wiersz na każdy skrót (back4 + A/B/X/Y/D-pad: tryb i melodia rodziny trybu z dopiskiem, gdy dzielona; back4 + RB/LB: melodie profili 1-3; połączenie pada: melodia), stałe skróty pod spodem, Save as PNG zostaje; karty Back4 chords, Confirmations i Events usunięte (bez powtórzeń); melodie przygaszone z podpowiedzią przy Confirm with = Vibration; firmware bez zmian.
- Sprawdzenie: test Playwright z kroku 7, przegląd managera, test użytkownika na padzie.

## Decyzje
- Odtwarzanie przez WebHID i przekazywanie 0x80-0x86 w trybie Steam, bez zmian w firmware: przekaźnik (~1 wpis na 4 ms, kolejka 32) wystarcza na 4 głosy na żywo.
- Zmiana zakresu 2026-11-22 (użytkownik): zamiast osobnego odtwarzacza (Player, 4 głosy) piano roll w Sounds z odsłuchem na padzie i zapisem do slotu; zakładka Player usunięta, połączenie WebHID w Sounds, panel testowy w Status > Debug.
- Zapis do slotu w limitach firmware (32 pozycje, jeden głos, 30-5000 ms, do 4000 Hz), bez zmian w firmware; sloty = istniejące 8 zakładek melodii.
- Piano roll ma jeden głos, tak jak melodia w pucku, a odsłuch gra na side 0 i 1 z głośnością z Settings: słychać to, co zostanie zapisane.
- Format raportu i strojenie wg SteamHapticsSinger (BSD-3) i SDL; kod własny, bez kopiowania programu.
- Panel testowy: Stop all wycisza side 0-4 (side 2 nieznany, traktowany jako osobny kanał) i wysyła zerowy 0x80; pole dur ograniczone do 0..32767, bo bit 15 (0x8000) używa wariant stopu z SteamHapticsSinger i jego znaczenie jest nieznane; "Max (32767 ms)" zamiast "do zatrzymania".
- Test 2026-11-22 (główny PC, OpenPuck w trybie Steam, Steam włączony): WebHID łączy się, interfejs slotu z padem wybrany sam (interface 4), deskryptor OpenPucka podaje 63 B dla 0x83 i 0x80; side 0-4 wszystkie grają (słychać i czuć); ton z `dur` 1000 ms kończy się sam po 1 s. Side: 0 lewy gładzik, 1 prawy gładzik, 2 oba gładziki, 3 lewy uchwyt, 4 prawy uchwyt (etykiety panelu potwierdzone). Grają tylko przez interfejs slotu z padem (inne interfejsy pucka: cisza, zgodnie z bramką slotu w przekaźniku). Sweep na side 0 i 3 przy -24 dB: słyszalne nuty w całym zakresie MIDI 24-48 i 84-108, więc progu 130 Hz z etapu 02 odtwarzacz nie stosuje (bez automatycznego przesuwania o oktawy, przesunięcie tylko ręczne). Wszystkie trzy warianty stopu (off tone, zero tone, rumble off) ucinają ton puszczony z dur 32767; odtwarzacz kończy nutę off tonem na danym side (nie rusza innych głosów). LFO (freq 5, depth 128) słychać dopiero przy 0 dB; przy 50/500 i depth 255 gra, efekt nieoceniony na ucho, więc vibrato w odtwarzaczu opcjonalne, domyślnie wyłączone, wartości surowe.
- Strojenie: bez korekty per aktuator (tabele SteamHapticsSinger dają uchwytom ok. +2%), bo nie da się tego sprawdzić na ucho; częstotliwość = nuta MIDI wprost.
- Piano roll zapisuje melodię jako listę pozycji: przerwy to pauzy, sąsiednie pauzy łączone, przerwa krótsza niż 30 ms dołączana do poprzedniej nuty (albo następnej, gdy poprzednia przekroczyłaby 5000 ms), pauzy dłuższe niż 5000 ms dzielone; pauza na końcu zostaje tylko taka, jaką melodia miała przed edycją (od końca ostatniej nuty, jeśli co najmniej 30 ms), więc piano roll może skrócić melodię, a usunięcie ostatniej nuty daje pustą melodię; dotyczy tylko melodii edytowanej w piano rollu.
- Tabela nut i piano roll od C1 (MIDI 24, `NOTE_LO` 36 -> 24), bo test pokazał słyszalne niskie nuty.
- Odsłuch na padzie z głośnością z Settings (ta sama, z którą gra puck); nuta = 0x83 na side 0 i 1 z dur = długość nuty, więc po zerwaniu połączenia nuta i tak kończy się sama (po odłączeniu urządzenia HID nie da się wysłać wyciszenia).
- Połączenie WebHID dostępne tylko przy pucku OpenPuck połączonym przez WebUSB (Sounds i Debug są w panelu po połączeniu); wystarcza, bo zapis do slotu i tak wymaga OpenPucka. Puck Valve i pad na kablu bez OpenPucka nie są obsługiwane w tym etapie.
- Raport 0x83 dopełniany zerami do długości z deskryptora urządzenia (domyślnie 9 B): deskryptor pucka Valve i pada na kablu może mieć inną długość niż OpenPuck.

- Uwagi użytkownika po pierwszym teście 2026-11-22: Play za daleko od piano rolla, brak zawijania zakresu nut (wybrany Fold), rysowanie nieintuicyjne (wybrane: rysowanie przeciągnięciem, prawy klik usuwa, czytelne kursory i uchwyty, klik w klawisz gra nutę).

- Zmiana zakresu 2026-11-22 (użytkownik): nuty osobno dla 4 aktuatorów, łącznie z firmware w tym etapie (puck zapisuje i gra 4 głosy); edycja przez przełącznik aktywnego toru.
- Blob v2: payload = confirm, gainDb, event[8], 8 melodii x 4 głosy (kolejność side 0, 1, 3, 4) x {count, 32 x {freq u16, ms u16}}; ok. 4,1 KB zamiast 1 KB; v1 wczytywany do obu gładzików, żeby stare melodie brzmiały jak dotąd.
- Op 0x31 (niezapisana melodia) zostaje jednogłosowy: bufor komend 160 B nie zmieści 4 x 32 nut; strona zamiast niego ma odsłuch na żywo przez WebHID.
- Protokół statusu 27 dla bloba v2: 26 już oznacza "kombinacja back4 może być 0xFE = None"; strona przy 25-26 zapisuje tylko tor LT (bloba v1) z ostrzeżeniem i odsłuchuje LT na obu gładzikach, jak puck.
- Przycisk "Play on puck" (op 0x31) usunięty ze strony: odsłuch na żywo przez WebHID go zastępuje, a 0x31 mieści tylko jeden głos; firmware dalej obsługuje 0x31 (zgodność).
- Firmware: bez kopii SoundsCfg na stosie (4,2 KB przekraczało stos pętli 4 KB); blob sprawdzany w miejscu, potem dekodowany; bufor s_io usunięty (fragmenty serializowane w locie); zapis przyjmuje też blob v1 (starsza strona w pamięci przeglądarki).

- "Zawijanie zakresu nut" znaczyło zwijanie tabeli nut (źle odczytane jako Fold): tabela nut w Sounds zwinięta domyślnie pod nagłówkiem "Note list (N)"; Fold zostaje.

- Import MIDI: źródło bez grywalnych nut zostawia tor bez zmian (komunikat); na pucku z jednym głosem domyślnie wypełniany tylko LT.

- Test 2026-11-22 (główny PC, firmware z protokołem 27): po wgraniu UF2 piano roll, import MIDI do torów, odsłuch na padzie i melodie z pucka grają tak, jak użytkownik chciał ("gra idealnie").
- Porządki przed zamknięciem: `readVoice` nie przesuwa wskaźnika poza krótki bufor podglądu (0x31); 5 długich komentarzy na końcu linii w `webusb_config.cpp` (sprzed etapu) przeniesionych nad instrukcje, lint komentarzy czysty.

- Porządek na stronie (użytkownik 2026-11-22): skróty, tryby, melodie i dźwięk/wibracja w jednej tabeli w Mapping > Shortcuts zamiast trzech kart; melodie dalej per rodzina trybów (firmware bez zmian), wiersz pokazuje, gdy melodia jest wspólna z innym skrótem.

## Podsumowanie
(wypełnia /etap-zamknij)
