# Etap 03: midi-na-zywo

## Cel
Strona openpuck gra pliki MIDI na żywo na aktuatorach haptyki nowego Steam Controllera przez WebHID (raport OUTPUT 0x83 HAPTIC_LFO_TONE), bez zmian w firmware: przez OpenPuck w trybie Steam, puck Valve albo pad na kablu. Do 4 głosów, velocity na głośność, opcjonalne vibrato (LFO), bez limitu nut.

## Kryteria zamknięcia
- [ ] Sprawdzone na sprzęcie i zapisane w Decyzjach: działanie przy włączonym/wyłączonym Steamie, znaczenie `side` 0-4, czy `dur` kończy ton, sposób zatrzymania tonu, LFO, najniższe słyszalne nuty gładzików i uchwytów.
- [ ] Zakładka Player łączy się przez WebHID z 28DE:1304 (OpenPuck w trybie Steam / puck Valve) i 28DE:1302 (pad na kablu), sama wybiera interfejs slotu z podłączonym padem.
- [ ] 4 aktuatory: każdy ma źródło (ścieżka/kanał), priorytet (najwyższa/najniższa nuta), przesunięcie o oktawy, wł./wył.
- [ ] Odtwarzanie: start, pauza, stop, pozycja; gra też w karcie w tle; stop, zamknięcie strony i rozłączenie wyciszają wszystkie aktuatory.
- [ ] Głośność z velocity i suwak ogólny w zakresie -60..0 dB; vibrato domyślnie wyłączone.
- [ ] Import melodii do pucka (Sounds) działa jak dotąd.
- [ ] Test Playwright (Chromium, udawane `navigator.hid`) przechodzi: bajty raportów, kolejność i czas, wyciszenie przy stopie.
- [ ] Test użytkownika na padzie (główny PC).
- [ ] `docs/PROTOCOL.md` (side 3/4, zatrzymanie tonu, granie na żywo), README i CHECKLISTA zaktualizowane.

## Plan
- [ ] 1. `docs/index.html`: zakładka Player, połączenie WebHID (wybór slotu po raportach wejściowych, jak SteamHapticsSinger), panel testowy: ton na każdy `side`, warianty stopu, `dur`, LFO, przemiatanie częstotliwości. Test użytkownika na sprzęcie, wyniki do Decyzji; dalsze kroki dopasowane do wyników.
- [ ] 2. `docs/index.html`: `midiParse` zachowuje velocity i kanał (import do Sounds bez zmian).
- [ ] 3. `docs/index.html`: przydział głosów, czyli linia na aktuator ze źródła i priorytetu, przesunięcie o oktawy, strojenie per aktuator.
- [ ] 4. `docs/index.html`: odtwarzacz z zegarem w Workerze, kolejka `sendReport(0x83, 9 B)`, start/pauza/stop/pozycja, wyciszanie wszystkich, velocity na `gain_db`, LFO.
- [ ] 5. `docs/index.html`: interfejs Player (plik, 4 głosy, bieżąca nuta na aktuator, głośność).
- [ ] 6. `tests/player/`: test Playwright w Chromium z udawanym `navigator.hid`.
- [ ] 7. `docs/PROTOCOL.md`, `README.md`, `CHECKLISTA.md`.
- Sprawdzenie: test Playwright z kroku 6, ręczny test importu MIDI w Sounds, przegląd managera, test użytkownika na padzie (Steam wł./wył., OpenPuck i puck Valve).

## Decyzje
- Odtwarzanie przez WebHID i przekazywanie 0x80-0x86 w trybie Steam, bez zmian w firmware: przekaźnik (~1 wpis na 4 ms, kolejka 32) wystarcza na 4 głosy na żywo.
- Osobna zakładka Player zamiast rozbudowy Sounds: Sounds to melodie zapisane w pucku, Player gra na żywo (potwierdzone przez użytkownika).
- 4 głosy (gładziki side 0/1, uchwyty 3/4 wg SteamHapticsSinger) z wyborem przy każdym aktuatorze (potwierdzone przez użytkownika); domyślny zestaw po teście z kroku 1.
- Format raportu i strojenie wg SteamHapticsSinger (BSD-3) i SDL; kod własny, bez kopiowania programu.
- Karta Player bez połączenia WebUSB stoi na stronie startowej (jak Shortcuts), z połączeniem w zakładce Mapping > Player: użytkownicy pucka Valve i pada na kablu nigdy nie łączą się przez WebUSB.
- Panel testowy: Stop all wycisza side 0-4 (side 2 nieznany, traktowany jako osobny kanał) i wysyła zerowy 0x80; pole dur ograniczone do 0..32767, bo bit 15 (0x8000) używa wariant stopu z SteamHapticsSinger i jego znaczenie jest nieznane; "Max (32767 ms)" zamiast "do zatrzymania".
- Raport 0x83 dopełniany zerami do długości z deskryptora urządzenia (domyślnie 9 B): deskryptor pucka Valve i pada na kablu może mieć inną długość niż OpenPuck.

## Podsumowanie
(wypełnia /etap-zamknij)
