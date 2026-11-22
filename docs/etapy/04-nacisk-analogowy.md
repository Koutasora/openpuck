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
- [ ] 1. Firmware (`config.h`, `config.cpp`, `triton.h` albo nowy `pressure.h`, `mode_ps3.cpp`, `mode_xbox_og.cpp`, `webusb_config.cpp`): ustawienie nacisku dla PS3 i OG Xbox na końcu Cfg (jak `g_padStickCfg`), pole zapisu i bajty w statusie, wyliczanie nacisku z analogowej wartości spustu (bez bitu kliknięcia), protokół 28.
- [ ] 2. Test host g++ (`tests/pressure`): wyliczanie nacisku, próg bitu cyfrowego, spust wyłączony jako L2/R2, fizyczny przycisk 255, ustawienie wyłączone = bez zmian.
- [ ] 3. `docs/index.html`: przełącznik i wybory w opcjach trybów PS3 i OG Xbox, kopia zapasowa ustawień; podgląd kliknięcia spustu w Live (do pomiaru), jeśli go nie ma; `tests/sounds-web/run.js`.
- [ ] 4. `docs/PROTOCOL.md`, README, CHECKLISTA.
- [ ] 5. Test użytkownika: pomiar spustu (Live), nacisk w PS3 i OG Xbox; poprawka 255 przy kliknięciu według pomiaru.
- Sprawdzenie: testy automatyczne, build firmware (RAM i flash przed i po), przegląd managera, test na sprzęcie.

## Decyzje
- Wszystkie kroki w jednym przebiegu (użytkownik 2026-11-23); pomiar spustu w teście użytkownika na końcu.
- Ustawienie osobno dla trybu PS3 i OG Xbox (nie dla rodzin DS4/Xbox), bo tylko te tryby mają bajty nacisku; włączane i wyłączane w opcjach (użytkownik).
- Spust ustawiony jako źródło nacisku przestaje działać jako L2/R2, żeby lekkie celowanie nie wciskało L2/R2.

## Podsumowanie
(wypełnia /etap-zamknij)
