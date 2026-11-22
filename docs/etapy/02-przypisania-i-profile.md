# Etap 02: przypisania-i-profile

## Cel
Tylne przyciski L4/R4/L5/R5 we wszystkich trybach emulacji dają przycisk, kombinację albo makro; 3 profile
przełączane na padzie z potwierdzeniem wibracją. Firmware wysyła stan przycisków na żywo (pod podgląd w etapie 04).

## Kryteria zamknięcia
- [ ] Tylny przycisk w każdym trybie emulacji (Xbox, Switch, DS4/PS3, PS5): jeden przycisk, kombinacja albo makro (kroki z czasami)
- [ ] 3 profile, back4 + RB następny / back4 + LB poprzedni, liczba krótkich wibracji = numer profilu
- [ ] Profile i makra zostają po odłączeniu zasilania; stary `cfg.bin` dalej się wczytuje, obecne przypisania stają się profilem 1
- [ ] Polecenia WebUSB: odczyt i zapis przypisań i profili, stan przycisków na żywo
- [ ] Logika makr i kombinacji w osobnym module bez zależności od sprzętu, z testem uruchamianym na LAB
- [ ] Build wpisuje wersję i commit (panel nie pokazuje "unknown")
- [ ] Użytkownik na głównym PC przetestował tryby PS5 i Xbox: przycisk, kombinacja, makro, przełączanie profili
- [ ] `CHECKLISTA.md` zaktualizowana

## Plan
- [ ] `OpenPuck/binds.cpp/.h`: typy akcji, 3 profile × 4 typy padów × 4 przyciski, silnik makr na `millis()`; działa przed trybami (dokłada przyciski do stanu pada), tłumaczenie kodów w trybach bez zmian
- [ ] Zapis w osobnym pliku `binds.bin` (jak ustawienie Switch Pro), układ `Cfg` bez zmian; profil 1 tworzony z `g_type[].back`
- [ ] Przełączanie profili back4 + LB/RB obok istniejących kombinacji back4, wibracja przez `haptics.cpp`
- [ ] `webusb_config.cpp`: polecenia przypisań, profilu i stanu na żywo; stare pole `back[]` = akcja "przycisk" w aktywnym profilu
- [ ] `serial_console.cpp`: polecenia do ustawiania kombinacji i makr do testów
- [ ] Przenośny kompilator (w64devkit w `%LOCALAPPDATA%\Programs`) i test modułu `binds` na LAB
- [ ] `tools/build-lab.sh`: wersja i commit przez `gen_version.sh`
- [ ] `CHECKLISTA.md`: punkty etapu 02
- Sprawdzenie: `sh tools/build-lab.sh` daje .uf2, test modułu `binds` przechodzi, build w Actions repo; użytkownik wgrywa .uf2 na głównym PC i sprawdza w testerze pada (PS5, Xbox)

## Decyzje
- Akcje tylnych przycisków stosowane w jednym miejscu przed trybami: tryby dalej tłumaczą tylko kody przycisków, mniej konfliktów przy merge z `upstream`.
- Przypisania w osobnym pliku `binds.bin`: układ `cfg.bin` autora bez zmian, stary zapis się wczytuje.
- Profile: back4 + LB/RB (do tej pory wolne), potwierdzenie wibracją.
- Test logiki na LAB przez przenośny w64devkit (bez instalatora, jak arduino-cli).
- Gałąź autora `copilot/170-add-controller-mapping` pominięta: jej kombinacje back4 + D-pad są już w `main` (`g_chordDpad`).

## Podsumowanie
(wypełnia /etap-zamknij)
