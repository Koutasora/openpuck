# Etap 04: nacisk-analogowy

## Cel
W trybach PS3 i OG Xbox wybrany spust steruje siłą nacisku wybranego przycisku (np. □ w MGS2: lekko celowanie, do końca strzał), włączane i wyłączane w opcjach osobno dla każdego z tych dwóch trybów. Kliknięcie spustu nie daje 255 przed końcem ruchu.

## Kryteria zamknięcia
- [ ] Zmierzone na sprzęcie i zapisane w Decyzjach: wartość spustu w pełnym wciśnięciu i w chwili kliknięcia.
- [ ] Ustawienie osobno dla trybu PS3 i OG Xbox: włączone/wyłączone, spust (LT/RT), przycisk docelowy; zapisane w pucku, stary `cfg.bin` dalej się wczytuje (domyślnie wyłączone).
- [ ] Przy włączonym ustawieniu spust daje płynny nacisk 0-255 przycisku docelowego, bit cyfrowy powyżej progu, spust przestaje działać jako L2/R2; fizyczny przycisk dalej daje 255.
- [ ] Kliknięcie spustu nie wymusza 255 przed końcem ruchu (nowe źródło nacisku; OG Xbox L/R według pomiaru).
- [ ] Strona: przełącznik i wybory w opcjach trybów PS3 i OG Xbox; protokół statusu 28.
- [ ] Testy automatyczne przechodzą: test logiki nacisku, `tests/binds`, `tests/sounds`, `tests/sounds-web` (Playwright), build firmware, clang-format i lint.
- [ ] Test użytkownika na sprzęcie (główny PC).
- [ ] `docs/PROTOCOL.md`, README i CHECKLISTA zaktualizowane.

## Plan
- [x] 1. Firmware (`config.h`, `config.cpp`, `triton.h` albo nowy `pressure.h`, `mode_ps3.cpp`, `mode_xbox_og.cpp`, `webusb_config.cpp`): ustawienie nacisku dla PS3 i OG Xbox na końcu Cfg (jak `g_padStickCfg`), pole zapisu i bajty w statusie, wyliczanie nacisku z analogowej wartości spustu (bez bitu kliknięcia), protokół 28.
- [x] 2. Test host g++ (`tests/pressure`): wyliczanie nacisku, próg bitu cyfrowego, spust wyłączony jako L2/R2, fizyczny przycisk 255, ustawienie wyłączone = bez zmian.
- [x] 3. `docs/index.html`: przełącznik i wybory w opcjach trybów PS3 i OG Xbox, kopia zapasowa ustawień; podgląd kliknięcia spustu w Live (do pomiaru), jeśli go nie ma; `tests/sounds-web/run.js`.
- [x] 4. `docs/PROTOCOL.md`, README, CHECKLISTA.
- [ ] 5. Test użytkownika: pomiar spustu (Live), nacisk w PS3 i OG Xbox; poprawka 255 przy kliknięciu według pomiaru.
- Sprawdzenie: testy automatyczne, build firmware (RAM i flash przed i po), przegląd managera, test na sprzęcie.

## Decyzje
- Wszystkie kroki w jednym przebiegu (użytkownik 2026-11-23); pomiar spustu w teście użytkownika na końcu.
- Ustawienie osobno dla trybu PS3 i OG Xbox (nie dla rodzin DS4/Xbox), bo tylko te tryby mają bajty nacisku; włączane i wyłączane w opcjach (użytkownik).
- Spust ustawiony jako źródło nacisku przestaje działać jako L2/R2, żeby lekkie celowanie nie wciskało L2/R2.
- Pola zapisu: 88-90 (PS3: włączone, spust 0 LT / 1 RT, przycisk), 91-93 (OG Xbox, to samo); bajty statusu w firmware p[198..203] (na stronie p[196..201]), długość bloba 202, protokół statusu 28.
- Cele PS3: 0 Square, 1 Triangle, 2 Circle, 3 Cross, 4 L1, 5 R1, 6-9 D-pad Up/Right/Down/Left; cele OG Xbox: 0 A, 1 B, 2 X, 3 Y, 4 White, 5 Black; cel to przycisk wyjściowy trybu (bez zamiany A/B).
- Próg bitu cyfrowego na PS3: nacisk 10 z 255 (`PRESS_DIGITAL_MIN`), niski, żeby lekki nacisk już trzymał przycisk, ale ponad szum spustu w spoczynku; do korekty po pomiarze użytkownika.
- Zapis w Cfg za `ext` (na samym końcu), żeby stary plik zachował położenie `ext`; krótki plik (0xFF) lub wartość spoza zakresu = wyłączone.
- Spust-źródło jest wycinany (bit kliknięcia i wartość) przed przypisaniem tylnych przycisków, więc tylny przycisk na L2/R2 (LT/RT) dalej działa.
- Istniejące wymuszanie 255 przy kliknięciu zwykłego LT/RT w OG Xbox bez zmian (decyzja po pomiarze użytkownika, krok 5).
- Build przed: RAM 66996 B, flash 290644 B; po: RAM 67012 B (+16), flash 291300 B (+656).
- Strona: przy "Triggers L / R" w Live dopisywane "(L click)" / "(R click)" z bitów raw L2/R2 (do pomiaru wartości przy kliknięciu); kopia zapasowa ma klucz `pressure`, przywracana tylko na puck z protokołem 28.
- Wydania (użytkownik 2026-11-23): 1.0.0 zostaje Latest i nietknięte; dodatkowe wydania jako pre-release do przełączania na liście "Update from a release": 1.0.4 (main po etapie 03, utworzone 2026-11-23) i 1.0.5 z naciskiem po scaleniu PR etapu 04 (`gh release create --prerelease --latest=false`).
- Scalenie do main tworzy tylko tag (tag pushowany przez GITHUB_TOKEN nie uruchamia budowania), więc wydanie z .uf2 powstaje dopiero po `gh release create`; 1.0.0 to stan po etapie 02, bez 4 głosów, dlatego punkt odniesienia do testu to 1.0.4.

## Podsumowanie
(wypełnia /etap-zamknij)
