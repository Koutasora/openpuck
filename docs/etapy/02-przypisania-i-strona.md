# Etap 02: przypisania-i-strona

## Cel
Cały zakres forka w jednym etapie: tylne przyciski L4/R4/L5/R5 dają przycisk, kombinację albo makro, 3 profile
przełączane na padzie, klawiatura i mysz w trybach PC, własna strona konfiguracji. Na końcu jeden build, test na
sprzęcie u użytkownika i wydanie .uf2 na GitHubie.

## Kryteria zamknięcia
Firmware: przypisania i profile
- [ ] Tylny przycisk w każdym trybie emulacji (Xbox, Switch, DS4/PS3, PS5): jeden przycisk, kombinacja albo makro (kroki z czasami)
- [ ] 3 profile, back4 + RB następny / back4 + LB poprzedni, liczba krótkich wibracji = numer profilu
- [ ] Profile i makra zostają po odłączeniu zasilania; stary `cfg.bin` dalej się wczytuje, obecne przypisania stają się profilem 1
- [ ] Polecenia WebUSB: odczyt i zapis przypisań i profili, stan na żywo (przyciski, sticki, triggery, profil, bateria i stan ładowania)
- [ ] Logika makr i kombinacji w osobnym module bez zależności od sprzętu, z testem uruchamianym na LAB
- [ ] Build wpisuje wersję i commit (panel nie pokazuje "unknown")

Firmware: klawiatura i mysz na PC
- [ ] Tryb PS5 (z panelem) i DS4-gyro: tylny przycisk może dać klawisz (z modyfikatorami) albo przycisk/ruch myszy
- [ ] Tryby konsolowe (PS5_GAME, PS5_AUTH, PS3, Switch, Xbox OG) bez zmian w deskryptorach USB

Strona konfiguracji
- [ ] Makieta zaakceptowana przez użytkownika przed budową strony
- [ ] Rysunek Steam Controllera z nazwami przycisków wg typu pada, tabela przypisań, edytor makr, wybór profilu
- [ ] Podgląd na żywo: co wciskasz na padzie i co dostaje komputer (Gamepad API, klawiatura/mysz w przeglądarce)
- [ ] Przycisk "test makra" bez wychodzenia ze strony
- [ ] Procent baterii i stan ładowania pada
- [ ] Strona na GitHub Pages repo

Test i wydanie
- [ ] Użytkownik wgrał jeden końcowy .uf2 i przeszedł test z `CHECKLISTA.md` na PC (tester pada, Notatnik) i na konsoli
- [ ] Wydanie .uf2 na GitHubie repo
- [ ] `CHECKLISTA.md` zaktualizowana

## Plan
- [x] `tools/build-lab.sh`: wersja i commit przez `gen_version.sh`
- [x] `OpenPuck/binds.cpp/.h`: typy akcji, 3 profile × 4 typy padów × 4 przyciski, silnik makr; test `tests/binds` (w64devkit)
- [ ] Integracja w firmware: plik `binds.bin`, hook w `rf_link.cpp`, back4 + LB/RB z wibracją, polecenia WebUSB (z baterią), polecenia konsoli
- [ ] Klawiatura i mysz: interfejsy HID w trybach PS5 i DS4-gyro, akcje klawisz/mysz w `binds`
- [ ] Makieta strony (lokalny HTML), akceptacja użytkownika
- [ ] Strona: nowy plik w `docs/` obok panelu autora, WebUSB do nowych poleceń, GitHub Pages
- [ ] Końcowy build, lista testów w `CHECKLISTA.md`, test użytkownika, wydanie
- Sprawdzenie: test `tests/binds` i build na LAB po każdym kroku; test strony w Chromium (Playwright) na LAB bez sprzętu; jeden test na sprzęcie u użytkownika na końcu

## Decyzje
- Akcje tylnych przycisków stosowane w jednym miejscu przed trybami: tryby dalej tłumaczą tylko kody przycisków, mniej konfliktów przy merge z `upstream`.
- Przypisania w osobnym pliku `binds.bin`: układ `cfg.bin` autora bez zmian, stary zapis się wczytuje.
- Profile: back4 + LB/RB (do tej pory wolne), potwierdzenie wibracją.
- Test logiki na LAB przez przenośny w64devkit 2.11.0 (GCC 16.2, bez instalatora, jak arduino-cli).
- Gałąź autora `copilot/170-add-controller-mapping` pominięta: jej kombinacje back4 + D-pad są już w `main` (`g_chordDpad`).
- Dawne etapy 02-05 połączone w jeden: użytkownik chce w openpuck jednej gałęzi bez pośrednich PR i jednego testu na sprzęcie na końcu.
- Bateria w poleceniu stanu na żywo: firmware już czyta `g_battery`/`g_batteryState`, strona tylko je pokazuje.

## Podsumowanie
(wypełnia /etap-zamknij)
