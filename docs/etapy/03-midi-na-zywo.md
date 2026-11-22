# Etap 03: midi-na-zywo

## Cel
Zakładka Sounds ma edytor piano roll: przeciągnięty plik MIDI można edytować, odsłuchać na żywo na padzie przez WebHID (raport 0x83) i zapisać do jednego z 8 slotów melodii pucka (do 32 pozycji, jeden głos). Bez zmian w firmware.

## Kryteria zamknięcia
- [x] Sprawdzone na sprzęcie i zapisane w Decyzjach: znaczenie `side` 0-4, czy `dur` kończy ton, sposób zatrzymania tonu, LFO, słyszalny zakres nut.
- [ ] Piano roll edytuje melodię bieżącej zakładki (dodawanie, przesuwanie, długość, usuwanie, przyciąganie do siatki, zaznaczanie), zsynchronizowany z tabelą nut.
- [ ] Przeciągnięty plik MIDI (i przycisk importu) wczytuje się bez limitu 32 nut i bez automatycznego przesuwania o oktawy.
- [ ] Odsłuch na padzie przez WebHID: całość albo zaznaczenie, Stop, gra w karcie w tle, wyciszenie przy stopie, rozłączeniu i zamknięciu strony.
- [ ] Zapis do slotu tylko w limitach pucka (licznik N/32, Crop to selection); format bloba bez zmian.
- [ ] Zakładka Player usunięta, połączenie WebHID w Sounds, panel testowy w Status > Debug.
- [ ] Test Playwright (`tests/sounds-web/`) przechodzi.
- [ ] Test użytkownika na padzie (główny PC).
- [ ] `docs/PROTOCOL.md`, README i CHECKLISTA zaktualizowane.

## Plan
- [x] 1. `docs/index.html`: połączenie WebHID i panel testowy tonu 0x83; test na sprzęcie, wyniki w Decyzjach.
- [ ] 2. `docs/index.html`: zakładka Player usunięta; połączenie (Connect controller, wybór interfejsu) w karcie Sounds; panel testowy w Status > Debug.
- [ ] 3. `docs/index.html`: piano roll w Sounds zamiast osi czasu (canvas: siatka nut z nazwami, oś ms z powiększaniem i przewijaniem, dodawanie, przeciąganie, zmiana długości, Delete, siatka wył./10/50/100 ms, zaznaczanie zakresu, nuty bez nakładania), tabela nut zostaje i jest zsynchronizowana.
- [ ] 4. `docs/index.html`: import MIDI przeciągnięciem na piano roll i przyciskiem, wybór ścieżki, bez cięcia do 32 i bez automatycznego przesuwania o oktawy.
- [ ] 5. `docs/index.html`: odsłuch na padzie (Play/Stop całości albo zaznaczenia, kreska pozycji, side 0 i 1, głośność z Settings, nuta = 0x83 z dur = długość, pauza = off tone, zegar w Workerze); podgląd w przeglądarce zostaje.
- [ ] 6. `docs/index.html`: licznik N/32, Crop to selection, Save to device zablokowany poza limitami.
- [ ] 7. `tests/sounds-web/`: test Playwright w Chromium z udawanym `navigator.hid`.
- [ ] 8. `docs/PROTOCOL.md`, `README.md`, `CHECKLISTA.md`.
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
- Raport 0x83 dopełniany zerami do długości z deskryptora urządzenia (domyślnie 9 B): deskryptor pucka Valve i pada na kablu może mieć inną długość niż OpenPuck.

## Podsumowanie
(wypełnia /etap-zamknij)
