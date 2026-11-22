# Checklista forka OpenPuck

## Etap 01: plan i szkielet
- [x] Klon repo autora, gałąź `etap-01-plan-i-szkielet`
- [x] Plan etapów w `docs/etapy/`
- [x] Build firmware na LAB (`sh tools/build-lab.sh`), plik `build/openpuck/OpenPuck.uf2`
- [x] Wgrany .uf2 z LAB działa na głównym PC jak firmware autora

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

### 6. Dodatki po pierwszym teście (od 2026-11-22)
- [x] Wgraj nowy `build/openpuck/OpenPuck.uf2` przez Firmware update -> local file; pad łączy się bez ponownego parowania, ustawienia zostają
- [x] Sounds: Play on puck - z aktuatora pada słychać ton (czy w ogóle słychać)
- [x] Sounds: zapis melodii na padzie, po odłączeniu zasilania pucka melodia zostaje
- [x] Zmiana profilu na padzie (back4 + LB/RB): potwierdzenie Vibration / Sound / Both / Off działa zgodnie z ustawieniem; przy Both impulsy i melodia po kolei, impuls nieucięty
- [x] Głośność dźwięków: zmiana suwaka słychać
- [x] Melodia po zmianie trybu (back4 + przycisk) osobno dla rodzin PlayStation, Xbox, Switch i other
- [x] Melodia przy połączeniu pada, najwyżej raz na 20 s (szybkie wyłączenie i włączenie pada nie gra jej drugi raz)
- [x] Import pliku MIDI i odsłuch zaimportowanej melodii
- [x] Octave up/down przesuwa melodię o oktawę (import MIDI sam już nie przesuwa oktaw, patrz sekcja 9)
- [x] Szybka melodia (import MIDI z krótkimi nutami) gra płynnie, pad nie gubi połączenia ani ustawień
- [x] Zaznaczanie tekstu na stronie nie znika (strona nie przebudowuje kart w trakcie)
- [x] Przycisk Copy w polu Version kopiuje wersję
- [x] Profile: zmiana nazwy, kopiowanie i reset profilu
- [x] Eksport i import pliku przypisań (zapis, zmiana, import przywraca)
- [x] Lizard mapping: nazwy przycisków myszy jak w akcjach
- [x] Klawiatura i mysz na PC nadal bez opóźnień (stan wysyłany rzadziej): pisanie, klik, kółko
- [x] Kombinacja back4 ustawiona na None nic nie robi (np. back4+B), ustawienie zostaje po odłączeniu zasilania; Shortcuts jej nie pokazuje

### 7. Wydanie
- [x] Repo publiczne (ustawia użytkownik)
- [x] Po scaleniu PR: GitHub Pages - Settings -> Pages -> Deploy from a branch, `main`, `/docs`; strona działa pod https://koutasora.github.io/openpuck/
- [x] Wydanie 1.0.0: po scaleniu PR tag `1.0.0` na `main`; workflow buduje `.uf2`, tworzy wydanie i gałąź `firmware`, lista wydań na stronie pokazuje wydanie

### 8. Poprawki strony po wydaniu 1.0.0
- [ ] Status -> Status: karta Version ma tylko trzy pola i notkę; przyciski Copy version i Capture 3 s and copy są na górze Status -> Log, nad listą zdarzeń, i działają (schowek); z dongle'em Log też otwiera się bez błędów
- [ ] Status -> Debug: karta Factory erase z opisem, potwierdzenia jak dawniej; górny pasek bez Factory erase i mieści się w jednej linii
- [ ] Mapping -> Shortcuts, tabela: na listach trybów (back4 + B / X / Y i D-pad) sama nazwa trybu (także None), pod listą opis wybranego trybu (host, panel, keyboard/mouse), pełny opis w podpowiedzi
- [ ] Rodzina trybów, na którą nie wskazuje żaden skrót (np. Xbox), ma osobny wiersz "Mode changed from the panel: Xbox" z listą melodii; wiersze Profile 1-3 pokazują nazwy profili użytkownika
- [ ] Mapping -> Live view: drążki w procentach (np. "2%, 3%"), triggery w procentach, surowe wartości w podpowiedzi; Copy diagnostics i Capture mają surowe liczby
- [ ] Notka "First connect uses the Chrome/Edge device picker..." widoczna tylko bez połączenia, po połączeniu znika, po odłączeniu wraca
- [ ] Mapping -> Shortcuts: listy melodii mieszczą się w kolumnie Melody, etykieta i lista w jednym wierszu
- [ ] Rolka myszy zmienia ustawienie w klikniętej (aktywnej) liście, jeden krok na ząbek, bez zawijania; bez kliknięcia lista nie reaguje i rolka przewija stronę
- [ ] Status -> Modes: kafelki trybów USB równej wielkości w równych kolumnach (4 przy pełnej szerokości, mniej w wąskim oknie), bez poziomego przewijania

### 9. Sounds: piano roll i granie na żywo (etap 03)
Punkty §9-11 bez osobnego testu ręcznego odhaczone na podstawie testów automatycznych (`tests/sounds-web`, `tests/sounds`) i testu użytkownika z 2026-11-22.
- [x] Mapping -> Sounds: piano roll pokazuje melodię; klik w puste pole dodaje nutę, przeciągnięcie przesuwa w czasie i wysokości, prawa krawędź zmienia długość, Delete usuwa; tabela pod spodem zmienia się razem z nim i odwrotnie
- [x] Przyciąganie Off / 10 / 50 / 100 ms działa; nuta nałożona na inną przycina ją, żadne nuty się nie nakładają
- [x] Przeciągnięcie po linijce czasu zaznacza zakres; Clear selection czyści; Crop to 32 zostawia zakres od jego początku, najwyżej 32 wpisy (cienka kreska na rolce pokazuje, gdzie kończy się 32. wpis), bez zaznaczenia pierwsze 32 wpisy
- [x] Zoom (przyciski i Ctrl + kółko), przewijanie (kółko, Shift + kółko, suwak pod rolką), Fit
- [x] Przeciągnięcie pliku .mid na piano roll otwiera wybór ścieżki; Import wstawia wszystkie wpisy (także ponad 32), bez przesunięcia o oktawy
- [x] Melodia ponad 32 wpisów: licznik na żółto z podpowiedzią, Save to device nieaktywny; po Crop wraca
- [x] Connect controller (WebHID): OpenPuck w trybie Steam połączony przez WebUSB (puck Valve i pad na kablu bez OpenPucka nie są obsługiwane); interfejs z padem wybierany sam; Play on controller gra melodię na gładzikach, Stop ją kończy, czerwona kreska idzie za dźwiękiem
- [x] Play on controller z zaznaczonym zakresem gra tylko zakres; Disconnect i zamknięcie karty wyciszają pad
- [x] Status -> Debug: Haptic test panel gra ton na każdym side (0 lewy gładzik, 1 prawy, 2 oba, 3 lewy uchwyt, 4 prawy uchwyt), Stop all wycisza
- [x] Zakładki Player już nie ma
- [x] Przyciski Play on controller, Stop i Preview in browser są nad piano rollem; Fold pokazuje tylko używane wiersze (przeciągnięcie pionowo przeskakuje między nimi), wybór zostaje do końca sesji
- [x] Piano roll: przeciągnięcie po pustym miejscu rysuje nutę, zwykły klik dodaje nutę o długości ostatnio użytej, prawy klik i podwójny klik usuwają nutę, kursor zmienia się (krzyżyk, przesuwanie, zmiana długości)
- [x] Klik w klawisz po lewej gra nutę 300 ms (na padzie po połączeniu, inaczej w przeglądarce), nie podczas odtwarzania
- [x] Piano roll: pole BPM (20-400) ustawia się z tempa importowanego pliku MIDI (bez tempa 120), zmiana BPM rozciąga całą melodię proporcjonalnie, a gdy któraś nuta wyszłaby poza 30-5000 ms, zmiana jest odrzucona; BPM jest pamiętany osobno dla każdej melodii tylko w przeglądarce
- [x] Przyciąganie 1/4, 1/8, 1/16 (domyślnie), 1/32 wg BPM; linie taktu i uderzeń na rolce, numery taktów na linijce przy przyciąganiu do uderzeń

### 10. Sounds: cztery głosy (etap 03, wymaga firmware z protokołem statusu 27)
- [x] Nad piano rollem cztery przyciski LT / RT / LG / RG (lewy gładzik, prawy gładzik, lewy uchwyt, prawy uchwyt) z liczbą wpisów; aktywny w kolorze akcentu, nuty pozostałych torów są przyciemnione, obrysowane i nie da się ich chwycić
- [x] Edycja, tabela, licznik "N / 32", Octave up/down i Clear dotyczą aktywnego toru; Fold pokazuje wiersze wszystkich torów
- [x] Crop to 32 tnie wszystkie tory tym samym zakresem, kończy tam, gdzie pierwszy tor przekroczyłby 32 wpisy (kreska na rolce)
- [x] Play on controller gra wszystkie niepuste tory naraz, każdy na swoim aktuatorze (LT side 0, RT side 1, LG side 3, RG side 4); pauza w torze wycisza tylko jego aktuator, Stop wycisza wszystkie cztery
- [x] Klik w klawisz po lewej gra nutę na aktuatorze aktywnego toru
- [x] Clear all voices (obok przycisków torów) po potwierdzeniu czyści wszystkie cztery tory bieżącej melodii; Anuluj nic nie zmienia
- [x] Tabela nut zwinięta pod "Note list (N)", klik rozwija i zwija; Preview in browser miksuje wszystkie tory
- [x] Save to device zapisuje 4 głosy i po ponownym wczytaniu strony wszystkie tory są na miejscu; Play saved melody on puck gra je na pucku; przycisku Play on puck już nie ma
- [x] Puck ze starszym firmware (protokół 25-26): ostrzeżenie "This firmware stores one voice (Left trackpad)", zapis tylko toru LT, melodia gra na obu gładzikach; Play on controller gra tor LT na obu gładzikach (jak puck), przyciski RT / LG / RG są przyciemnione z podpowiedzią "not saved"
- [x] Przywrócenie kopii z czterema głosami na puck ze starszym firmware: komunikat, że głosy RT, LG i RG nie zostały przywrócone
- [x] Kopia zapasowa (Export / Import) z pucka na pucek z innym firmware (v1 i v2) przenosi melodie bez błędu
- [x] Import MIDI do czterech torów: po wczytaniu pliku (przycisk lub przeciągnięcie) cztery listy (LT / RT / LG / RG) ze źródłami "Track N" i "Track N, ch C" (kanał 10 = drums), domyślnie wg średniej wysokości od najwyższej do LT, RT, LG, RG; perkusja nigdy sama; przy jednym źródle tylko LT, reszta Keep current
- [x] Obok każdej listy liczba wpisów toru (powyżej 32 na żółto z podpowiedzią Crop to 32); Import zastępuje wskazane tory po jednym potwierdzeniu z ich nazwami, Keep current zostawia tor, Empty go czyści; komunikat podaje liczbę wpisów na tor

### 11. Shortcuts: jedna tabela (zamiast kart Back4 chords, Confirmations i Events)
- [x] Status -> Modes nie ma już karty Back4 chords, Mapping -> Settings karty Confirmations, Mapping -> Sounds karty Events; wszystko jest w Mapping -> Shortcuts
- [x] Na górze karty Confirm with (Vibration / Sound / Both / Off) i Volume; zmiana zapisuje się od razu; przy Vibration i Off listy melodii są przyciemnione z podpowiedzią (nadal da się je zmieniać)
- [x] Tabela: back4 + A (Steam, stałe), back4 + B / X / Y i D-pad (lista trybów jak dawniej, także None, zapis od razu), back4 + RB / LB (melodie Profile 1-3), Controller connects; kolumna Melody ma listę melodii dla każdego wiersza
- [x] Zmiana trybu skrótu (np. back4 + X na PS5) od razu przenosi jego melodię na rodzinę tego trybu (PlayStation, Xbox, Switch, other); gdy kilka skrótów ma tę samą rodzinę, pod listą jest "shared with ..."; skrót ustawiony na None ma "-" zamiast melodii
- [x] Save to device pod tabelą zapisuje przypisanie melodii (ten sam zapis co z karty Sounds); przy błędnej melodii przycisk jest nieaktywny i podaje powód
- [x] Stałe skróty (Power and slots, Lizard) są pod tabelą jako tekst; bez pucka (strona startowa) karta pokazuje sam arkusz jak dawniej; Save as PNG zawiera kolumnę melodii, gdy puck jest połączony
