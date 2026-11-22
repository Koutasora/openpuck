# Checklista forka OpenPuck

## Etap 01: plan i szkielet
- [x] Klon repo autora, gałąź `etap-01-plan-i-szkielet`
- [x] Plan etapów w `docs/etapy/`
- [x] Build firmware na LAB (`sh tools/build-lab.sh`), plik `build/openpuck/OpenPuck.uf2`
- [ ] Wgrany .uf2 z LAB działa na głównym PC jak firmware autora

## Etap 02: przypisania i strona (dawne etapy 02-05)
Gotowe i zbudowane na LAB, testy bez sprzętu przechodzą:
- [x] Logika przypisań, makr, profili i klawiatury/myszy w module `binds`, test `sh tests/binds/run.sh` (także rozpoznawanie hosta)
- [x] Build wpisuje wersję i commit (`gen_version.sh` w `tools/build-lab.sh`)
- [x] Panel autora (`docs/index.html`) z rozszerzeniami: profile, akcje Button/Combo/Macro/Key/Mouse, rysunki Valve, Macros, Live view, Shortcuts, bateria, host; sprawdzony na atrapie pucka w Chromium

### 1. Wgranie i strona
- [x] Wgraj `build/openpuck/OpenPuck.uf2` przez zakładkę Firmware update panelu (tak jak w etapie 01); pad łączy się bez ponownego parowania
- [x] Uruchom stronę lokalnie: w folderze repo `python -m http.server 8000 -d docs`, otwórz `http://localhost:8000` w Chrome, Connect
- [x] Pasek u góry pokazuje wersję z commitem (nie "unknown"), baterię pada i host
- [x] Stare ustawienia (tryby, kombinacje back4, przypisania łopatek) zostały po wgraniu

### 2. Przypisania pada (tryb PS5 i tryb Xbox, tester pada: joy.cpl albo gamepad-tester.com)
- [x] Button: łopatka daje wybrany przycisk
- [x] Combo: łopatka daje kilka przycisków naraz (np. L1 + R1)
- [x] Macro: łopatka odtwarza makro raz do końca; puszczenie w trakcie nie przerywa
- [x] Profile: przełączenie na stronie i na padzie (4 łopatki + RB następny, + LB poprzedni), liczba wibracji = numer profilu; LB/RB w tym czasie nie dochodzą do gry
- [x] Ustawienia zostają po odłączeniu zasilania pucka
- [x] Live view: lewa strona reaguje na pad (przyciski, drążki, triggery), prawa pokazuje to, co dostaje komputer

### 3. Klawiatura i mysz na PC (tryb PS5 z panelem, potem DS4-gyro)
Testować przy komputerze, do którego puck jest podłączony fizycznie: przez pulpit zdalny (RDP) Windows kieruje fizyczną mysz i klawiaturę do sesji konsoli, nie do sesji zdalnej.
Działają tylko w tych dwóch trybach i tylko gdy puck uzna, że jest na PC. Po wykryciu PC puck raz się ponownie wylicza (pad znika na chwilę).
- [x] Po ok. 10 s Menedżer urządzeń pokazuje pad, "Klawiatura HID" i "Mysz zgodna z HID"; na stronie "Host: PC"
- [x] Key: łopatka wpisuje literę w Notatniku, przytrzymana powtarza; Ctrl+Shift+C działa (np. narzędzia w przeglądarce); przycisk Record na stronie ustawia skrót naciśnięty na klawiaturze
- [x] Mouse: klik lewy/prawy, kółko jeden ząbek na wciśnięcie, kółko powtarzane przy trzymaniu i stop po puszczeniu
- [x] Puszczona łopatka puszcza klawisz (nic się nie "przykleja"), także po wyłączeniu pada z wciśniętą łopatką
- [x] Kombinacje back4 nadal działają i niczego nie wpisują
- [x] Discord push-to-talk: łopatka z klawiszem F13, w Discordzie skrót "naciśnij i mów" = ta łopatka; w grze (np. The Blood of Dawnwalker) wciśnięcie łopatki nie przełącza gry na klawiaturę/mysz i pad dalej działa. Dla porównania łopatka z Mouse middle: sprawdzić, czy gra się przełącza
- [x] Wybudzanie PC z uśpienia padem działa jak przed zmianą
- Tryb Xbox (XInput) na PC: klawiatury i myszy tu celowo nie ma, firmware w tym trybie bez zmian (powody w `docs/PROTOCOL.md` §10.3: deskryptor statyczny, klasa urządzenia 0xFF, Windows nie wczytałby dodatkowego interfejsu HID, brak pewnego sygnału PC). Push-to-talk na F13 na razie przez tryb PS5 albo DS4-gyro (`BA 1 3 0 K68`)
- [x] Ustawienie "Keyboard/mouse host" na stronie: Always console usuwa klawiaturę, Always PC dodaje ją od razu po starcie, Auto wraca do rozpoznawania

### 4. Konsola
- [x] Tryby konsolowe (PS5 game, PS5 przez adapter, DS4 game, PS3, Switch, Xbox OG, Xbox 360) działają jak przed zmianą
- [x] Xbox 360 (XInput, auth XSM3) na konsoli i na PC: deskryptor i działanie bez zmian względem poprzedniego firmware (w tym trybie nic nie dodano)
- [x] Na konsoli łopatka z klawiszem/myszą nic nie robi; przypisania Button/Combo/Macro i profile działają

### 5. Strona
- [x] Shortcuts: ściągawka pokazuje tryby przypisane do kombinacji back4 w tym pucku; Save as PNG zapisuje obrazek (jasny i ciemny)
- [x] Macros: utworzenie makra, Test macro, zapis na padzie, przypisanie do łopatki
- [x] Strona działa przy wąskim oknie (telefon / wąska przeglądarka)

### 6. Wydanie
- [ ] Wydanie `.uf2` na GitHubie repo (po teście)
- [ ] Strona dostępna z GitHuba (GitHub Pages albo inna droga; prywatne repo bez płatnego planu nie ma Pages)
