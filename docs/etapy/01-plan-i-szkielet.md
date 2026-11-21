# Etap 01: plan-i-szkielet

## Cel
Fork OpenPuck z własnymi przypisaniami tylnych przycisków (kombinacje, makra, profile, na PC też klawiatura
i mysz) oraz własną stroną konfiguracji z rysunkiem pada i tabelą. W tym etapie: zakres, plan etapów, działający build.

Robi:
- przypisania L4/R4/L5/R5 we wszystkich trybach emulacji: przycisk, kombinacja, makro,
- 3 profile przełączane back-4 + LB/RB, potwierdzenie wibracją,
- klawiatura i mysz w trybach PC (PS5 z panelem, DS4-gyro),
- strona WebUSB: SVG Steam Controllera z nazwami zależnymi od typu pada, tabela, edytor makr; hosting na GitHub Pages forka.

Nie robi:
- klawiatury i myszy w trybach konsolowych (PS5_GAME, PS5_AUTH, PS3, Switch, Xbox OG),
- zmian w trybach Steam i Lizard (tam przypisuje Steam),
- oficjalnych grafik padów Sony/Microsoft/Nintendo,
- klawiatury w trybie Xbox (ewentualnie później jako opcja "tryb PC").

## Kryteria zamknięcia
- [x] cel i zakres opisane w pliku etapu
- [x] technologie i powody w sekcji "Decyzje"
- [x] etapy 02-05 w roadmapie jako PLANOWANY
- [x] build firmware działa (lokalnie albo w Actions forka) i daje .uf2
- [ ] użytkownik wgrał .uf2 z LAB na głównym PC i pad działa jak na firmware autora
- [x] `CHECKLISTA.md` po polsku w repo

## Plan
- [x] Plik etapu i roadmapa w `docs/etapy/`
- [x] Build: przenośny `arduino-cli` + rdzeń Adafruit nRF52, `make build`; dodatkowo Actions forka
- [ ] Test ręczny na głównym PC: wgranie `build/openpuck/OpenPuck.uf2`, parowanie, tryb PS5 i Xbox, tylne przyciski
- [x] `CHECKLISTA.md` w katalogu głównym
- Sprawdzenie: wynik `make build` i plik .uf2, wynik testu, Actions forka

## Decyzje
- Technologie jak u autora (Arduino + TinyUSB na nRF52840, panel w czystym HTML/JS): łatwe dociąganie zmian z upstream.
- Zmiany autora dociągane przez merge z `upstream`, bez rebase: brak force-push, historia forka zostaje.
- Nowe pola konfiguracji tylko na końcu zapisu we flashu: stary cfg.bin dalej się wczytuje.
- Klawiatura/mysz tylko w trybach z interfejsami złożonymi (PS5, DS4-gyro): tryby konsolowe muszą zostać pojedynczym HID.
- Build na LAB: `tools/build-lab.sh` (arduino-cli 1.6.0 przenośne w %LOCALAPPDATA%/Programs/arduino-cli, rdzeń adafruit:nrf52 1.7.0): na LAB nie ma make ani python3.
- Testy na sprzęcie robi użytkownik na głównym PC z własnym padem; `tests/xinput` pominięty (na LAB brak kompilatora C/C++).
- Na razie bez forka i bez GitHuba: praca tylko lokalnie, repo autora jako `upstream` do dociągania zmian; fork na GitHubie zrobi użytkownik, gdy będzie miał fizyczny dostęp do komputera.
- Strona: jeden rysunek Steam Controllera z nazwami zależnymi od typu pada zamiast osobnych padów: to fizyczny pad w ręku, bez cudzych grafik.

## Podsumowanie
(wypełnia /etap-zamknij)
