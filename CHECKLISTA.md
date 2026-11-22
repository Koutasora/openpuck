# Checklista forka OpenPuck

## Etap 01: plan i szkielet
- [x] Klon repo autora, gałąź `etap-01-plan-i-szkielet`
- [x] Plan etapów w `docs/etapy/`
- [x] Build firmware na LAB (`sh tools/build-lab.sh`), plik `build/openpuck/OpenPuck.uf2`
- [ ] Wgrany .uf2 z LAB działa na głównym PC jak firmware autora

## Etap 02: przypisania i profile
- [x] Logika akcji i makr w osobnym module `binds`, test na LAB (`sh tests/binds/run.sh`) przechodzi
- [x] Build wpisuje wersję i commit (`gen_version.sh` w `tools/build-lab.sh`)
- [ ] Firmware wysyła do strony stan przycisków na żywo (pod podgląd w etapie 04) — polecenie WebUSB `0x27` gotowe i zbudowane, test na sprzęcie czeka
- [ ] Tylny przycisk: przycisk pada, kombinacja, makro — gotowe i zbudowane (konsola `BA`/`BM`), test na sprzęcie czeka (PS5, Xbox)
- [ ] 3 profile, przełączanie back-4 + LB/RB, wibracja — gotowe i zbudowane, test na sprzęcie czeka
- [ ] Stary zapis ustawień dalej się wczytuje — `cfg.bin` bez zmian, przypisania w `binds.bin`; test na sprzęcie czeka

## Etap 03: klawiatura i mysz na PC
Gotowe i zbudowane (`sh tools/build-lab.sh`, plik `build/openpuck/OpenPuck.uf2`), test na sprzęcie czeka. Klawiatura i mysz
działają tylko w trybach PS5 (z panelem) i DS4-gyro, tylko gdy puck uzna, że jest na PC. Mysz to istniejący interfejs
"wake mouse", klawiatura to nowy interfejs dodawany po wykryciu PC (jedno ponowne wyliczenie USB, pad znika na chwilę).

Przypisania ustawiasz w konsoli szeregowej (rozruch debug CDC w trybie Steam, jak dotąd dla `BA`/`BM`); zapisują się w
`binds.bin`, więc działają po przełączeniu w tryb PS5. Typ pada: `3` = PS5, `2` = DS4-gyro; profil `1`; łopatki 0..3 = L4 R4 L5 R5:
- `BA 1 3 0 K04` — L4 = klawisz A
- `BA 1 3 1 K06:03` — R4 = Ctrl+Shift+C (kod klawisza hex : modyfikatory hex: 01 LCtrl, 02 LShift, 04 LAlt, 08 LWin)
- `BA 1 3 2 MoL` — L5 = lewy przycisk myszy (`MoR` prawy, `MoM` środkowy, `MoB` wstecz, `MoF` dalej)
- `BA 1 3 3 MoD+` — R5 = kółko w dół, powtarzane co 100 ms przy trzymaniu (`MoU` / `MoD` = jeden ząbek na wciśnięcie)
- `BL` — lista przypisań; `HOST` — co puck myśli o hoście

Test na PC (Windows):
- [ ] Wgraj `OpenPuck.uf2`, przełącz na tryb PS5 (z panelem), podłącz puck do PC i włącz pada
- [ ] Po ok. 10 s (puck raz ponownie się wylicza, pad na moment znika) Menedżer urządzeń pokazuje trzy rzeczy: pad (DualSense), "Klawiatura HID" w Klawiaturach, "Mysz zgodna z HID" w Myszach
- [ ] Tester pada (joy.cpl albo gamepad-tester.com): pad działa jak dotąd, wszystkie przyciski i drążki
- [ ] Notatnik: łopatka z `K04` wpisuje literę, przytrzymana powtarza ją; `K06:03` daje Ctrl+Shift+C (np. w przeglądarce otwiera narzędzia)
- [ ] Mysz: łopatka z `MoL` klika, `MoD+` przewija długą stronę w dół i przestaje po puszczeniu; `MoU` przewija o ząbek na wciśnięcie
- [ ] Puszczona łopatka puszcza klawisz (żadnych "przyklejonych" liter); wyłączenie pada przy wciśniętej łopatce też
- [ ] Kombinacje back4 (zmiana trybu, profile back4 + LB/RB) działają i nie wpisują niczego
- [ ] Tryb DS4-gyro: to samo (menedżer urządzeń, Notatnik, mysz); w konsoli typ pada `2`
- [ ] Kontrola wykrycia: puck w trybie Steam z konsolą (rozruch debug CDC) na PC, polecenie `HOST` pisze `host PC (...)`
- [ ] Linux / macOS (jeśli masz pod ręką): klawiatura pojawia się po ok. 10 s od włączenia pada; `HOST` pokazuje powód "identity reports read"

Test na konsoli:
- [ ] Tryby konsolowe (PS5 console via auth adapter, PS5 game, DS4 game, PS3, Switch, Xbox OG, Xbox 360) działają jak przed zmianą, bez klawiatury w Menedżerze urządzeń na PC
- [ ] Konsola z trybem PS5 albo DS4-gyro (PS4 / PS5, jeśli akceptuje pada): pad działa jak dotąd, brak klawiatury i brak kliknięć myszy od łopatek; po przepięciu na PC `HOST` pokazuje "PlayStation auth reports" albo zapamiętany wynik
- [ ] Łopatka z `K..` albo `Mo..` na konsoli nic nie robi (nie wciska niczego w grze)

Ustawienie ręczne hosta (gdy automat się pomyli), w konsoli szeregowej:
- [ ] `HOST` pokazuje wykryty host, powód, ustawienie i czy interfejs klawiatury jest włączony
- [ ] `HOST P` = zawsze PC: po restarcie w trybie PS5 klawiatura jest w Menedżerze urządzeń od razu, także bez wykrycia
- [ ] `HOST C` = zawsze konsola: klawiatura znika, łopatki z klawiszem / myszą nic nie robią
- [ ] `HOST A` = automat (domyślnie); ustawienie zostaje po odłączeniu zasilania
- [ ] Stary `binds.bin` (wersja 1) dalej się wczytuje po wgraniu nowego firmware (przypisania z etapu 02 bez zmian)

## Etap 04: strona konfiguracji
- [ ] Makieta zaakceptowana
- [ ] Rysunek Steam Controllera z nazwami wg typu pada
- [ ] Tabela przypisań i edytor makr
- [ ] Podgląd na żywo: co wciskasz na padzie (nowy kanał WebUSB z firmware)
- [ ] Procent baterii i stan ładowania pada
- [ ] Podgląd na żywo: co dostaje komputer (Gamepad API + klawiatura/mysz w przeglądarce)
- [ ] Przycisk "test makra" bez wychodzenia ze strony
- [ ] Strona na GitHub Pages forka

## Etap 05: testy i wydanie
- [ ] Test na PC (tester pada, Notatnik)
- [ ] Test na konsoli
- [ ] Wydanie .uf2 na GitHubie forka
