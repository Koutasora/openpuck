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
- [x] użytkownik wgrał .uf2 z LAB na głównym PC i pad działa jak na firmware autora
- [x] `CHECKLISTA.md` po polsku w repo

## Plan
- [x] Plik etapu i roadmapa w `docs/etapy/`
- [x] Build: przenośny `arduino-cli` + rdzeń Adafruit nRF52, `make build`; dodatkowo Actions forka
- [x] Test ręczny na głównym PC: wgranie `build/openpuck/OpenPuck.uf2`, parowanie, tryb PS5 i Xbox, tylne przyciski
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

Decyzje:
- Technologie jak u autora (Arduino + TinyUSB, nRF52840, panel HTML/JS), żeby łatwo dociągać jego zmiany (merge z `upstream`).
- Własne prywatne repo `Koutasora/openpuck` z importu GitHuba zamiast forka: niewidoczne dla autora; import ominął brak uprawnienia Workflows w tokenie LAB.
- Klawiatura i mysz tylko w trybach PC z interfejsami złożonymi (PS5, DS4-gyro); tryby konsolowe zostają pojedynczym HID.
- Profile przełączane back-4 + LB/RB z potwierdzeniem wibracją; strona z jednym rysunkiem Steam Controllera i tabelą.
- Test `tests/xinput` pominięty (brak kompilatora C/C++ na LAB); sprawdzenie na sprzęcie u użytkownika.

Zrobione:
- Klon kodu autora (commit `512a406`), roadmapa z etapami 02-05, `CHECKLISTA.md`.
- `tools/build-lab.sh`: build na LAB bez make i python3 (arduino-cli 1.6.0 przenośne, rdzeń adafruit:nrf52 1.7.0), wynik `build/openpuck/OpenPuck.uf2` (32% flash, 22% RAM).
- Wgranie .uf2 na puck użytkownika przez zakładkę Firmware update panelu autora: panel pokazuje build "unknown / clean", pad połączył się bez ponownego parowania.
- Usunięte gałęzie autora skopiowane importem (zostało `main`).

Otwarte sprawy:
- Build ma wpisywać wersję i commit (`gen_version.sh` w `tools/build-lab.sh`), żeby panel nie pokazywał "unknown".
- Osobny test trybów PS5 i Xbox z tylnymi przyciskami na obecnym firmware nie był zrobiony; zrobić przy pierwszym buildzie etapu 02.
- Przed etapem 02 przejrzeć gałąź autora `copilot/170-add-controller-mapping` (może dublować nasze mapowanie).
- `adafruit-nrfutil` zainstalowany pip do systemowego Pythona, build go nie wymaga: do usunięcia albo przeniesienia do venv.

Ryzyka:
- Autor często zmienia kod i układ `cfg.bin`; merge z `upstream` może dawać konflikty w `config.*` i `webusb_config.cpp`.
- Puck bez przycisku RST: wgrywanie tylko przez panel WebUSB; nieudany firmware bez działającego WebUSB wymaga zwarcia RST-GND.
- Brak testów automatycznych na LAB; logikę makr warto wydzielić tak, żeby dało się ją testować bez sprzętu.
