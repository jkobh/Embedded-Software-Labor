# Versuch 1: Satellitentechnik – GPS/CDMA-Decoder (C++)

Ein C++-Programm, das aus einem empfangenen CDMA-Summensignal (1023 Chips)
bestimmt, welche GPS-Satelliten gesendet haben, welches Bit sie gesendet
haben und an welcher Position (`delta`) die Chipsequenz beginnt.

## Aufbau

```
Aufgabe1/
├── main.cpp              # kompletter Decoder (eine Datei)
├── gps_sequence_7.txt    # Summensignal von der Labor-Webseite
├── LAB_UB_1.pdf          # Aufgabenstellung
└── Makefile
```

## Bauen und Ausfuehren

```bash
make            # baut ./satellite_decoder
make run        # Decoder auf gps_sequence_7.txt laufen lassen
make clean      # Binaries entfernen
```

### Debug-Ausgabe

Mit `make debug` wird eine Debug-Version gebaut (`-DDEBUG`, mit `-g`), die
zusaetzlich zu den Ergebnissen auch Zwischenwerte pro Satellit auf
`stderr` ausgibt: Peak-Wert und delta, Mittelwert, Standardabweichung und
den Z-Score der Korrelation. Das hilft beim Nachvollziehen, warum ein
Satellit erkannt wird (Z-Score >= 5) oder nicht.

```bash
make debug
```

Der Dateiname kann auch direkt als Kommandozeilenparameter uebergeben werden
(Schritt 1 der Aufgabe):

```bash
./satellite_decoder <summensignal.txt>
```

## Ergebnis auf `gps_sequence_7.txt`

```
Satellite  6 has sent bit 1 (delta = 21)
Satellite 10 has sent bit 1 (delta = 91)
Satellite 12 has sent bit 0 (delta = 864)
Satellite 21 has sent bit 1 (delta = 189)
```

`delta` ist die Position im Summensignal, an der die Chipsequenz des
Satelliten beginnt.

## Funktionsweise

1. **Gold-Codes**: Zwei 10-stufige Schieberegister (LFSR) `G1` und `G2` –
   beide starten mit `1111111111` – erzeugen pro Satellit eine 1023-Chip-
   Sequenz. Die Tap-Paare pro Satellit (z. B. Satellit 6: `2,10`) stammen aus
   Abb. 9 des Skripts. Die Chipfolge ist `G1(10) XOR G2(tapA) XOR G2(tapB)`,
   gespeichert als +1/−1.
2. **Kreuzkorrelation**: Das Summensignal wird zyklisch mit jedem Gold-Code
   korreliert (alle 1023 Verschiebungen). Zyklisch, weil die Satelliten
   asynchron empfangen werden – die Sequenz beginnt irgendwo mitten im Signal
   („Anfangsstueck hinten anhaengen“).
3. **Peak-Erkennung**: Ein echter Treffer hebt sich deutlich vom Rauschen ab
   (Z-Score `(max|Korr.| − Mittelwert) / Standardabweichung` >= 5). Das
   Vorzeichen des Peaks ergibt das Bit (Bit 1 = Sequenz unveraendert,
   Bit 0 = invertiert), die Position des Maximums das `delta`.

## Hinweise

- Das Summensignal enthaelt Werte von −4 bis +4, auch 0 (Ausloeschung sich
  ueberlagernder Chips). Die Rohwerte werden unveraendert eingelesen, nicht
  auf ±1 quantisiert.
- Genutzt werden nur STL-Klassen (`std::array`, `std::vector`, …) statt
  nativer C-Arrays, wie in der Aufgabe gefordert.
