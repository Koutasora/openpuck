# Checklista forka OpenPuck

## Etap 01: plan i szkielet
- [x] Klon repo autora, gałąź `etap-01-plan-i-szkielet`
- [x] Plan etapów w `docs/etapy/`
- [x] Build firmware na LAB (`sh tools/build-lab.sh`), plik `build/openpuck/OpenPuck.uf2`
- [ ] Wgrany .uf2 z LAB działa na głównym PC jak firmware autora

## Etap 02: przypisania i profile
- [ ] Firmware wysyła do strony stan przycisków na żywo (pod podgląd w etapie 04)
- [ ] Tylny przycisk: przycisk pada, kombinacja, makro
- [ ] 3 profile, przełączanie back-4 + LB/RB, wibracja
- [ ] Stary zapis ustawień dalej się wczytuje

## Etap 03: klawiatura i mysz na PC
- [ ] Tryb PS5 (z panelem): klawisz i mysz z tylnych przycisków
- [ ] Tryb DS4-gyro: to samo
- [ ] Tryby konsolowe bez zmian w USB

## Etap 04: strona konfiguracji
- [ ] Makieta zaakceptowana
- [ ] Rysunek Steam Controllera z nazwami wg typu pada
- [ ] Tabela przypisań i edytor makr
- [ ] Podgląd na żywo: co wciskasz na padzie (nowy kanał WebUSB z firmware)
- [ ] Podgląd na żywo: co dostaje komputer (Gamepad API + klawiatura/mysz w przeglądarce)
- [ ] Przycisk "test makra" bez wychodzenia ze strony
- [ ] Strona na GitHub Pages forka

## Etap 05: testy i wydanie
- [ ] Test na PC (tester pada, Notatnik)
- [ ] Test na konsoli
- [ ] Wydanie .uf2 na GitHubie forka
