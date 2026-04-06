# Nucleo Sensorless Testplan

Diese Anleitung beschreibt die sinnvolle Testreihenfolge fuer den aktuellen sensorlosen Stand der Loetkolben-Firmware auf dem Nucleo-H755ZI-Q.

Ziel ist, moeglichst alle Betriebs-, UI-, Fault-, Storage- und Kalibrierpfade bereits ohne reale Sensorik zu pruefen, damit beim spaeteren Umstieg auf die Zielhardware vor allem noch Hardwareintegration und Fehlersuche uebrig bleiben.

## Vorbereitung

1. Iron-Firmware bauen und flashen.
2. Seriellen Port von USART3 oeffnen.
3. Nach dem Boot auf den Prompt `>` warten.

Erwartung:

- Boot-Logs erscheinen sauber.
- Die Firmware laeuft ohne Reset-Schleife.
- Die UART-Konsole ist benutzbar.

## Test 1: Konsole und Grunddiagnose

### Schritte

1. `help`
2. `status`
3. `ui`
4. `screen`
5. `fan`
6. `dock`
7. `cal`

### Was wir testen

- Ob die Konsole vollstaendig aktiv ist
- Ob alle zentralen Diagnosepfade antworten
- Ob Grundzustand, UI, Display, Fan, Dock und Kalibrierung lesbar sind

### Erwartung

- Alle Befehle liefern sofort eine Ausgabe.
- Es gibt keine Haenger oder leeren Antworten.
- Kein Fault ist aktiv.

## Test 2: Simulations-Kaltstart

### Schritte

1. `sim reset`
2. `status`
3. `screen overview`
4. `screen`

### Was wir testen

- Definierter Startzustand der Simulation
- Korrekte Initialwerte fuer Temperatur, Leistung und Anzeige

### Erwartung

- Spitze und Umgebung liegen nahe Startwert.
- PWM und Leistung sind klein oder null.
- Die Hauptansicht zeigt sinnvolle Werte.

## Test 3: Normales Aufheizen

### Schritte

1. `target 320`
2. `stream 500`
3. 10 bis 20 Sekunden beobachten
4. `stream off`
5. `status`

### Was wir testen

- Uebernahme eines normalen Sollwerts
- Heizverhalten der Simulation
- Zustandswechsel zwischen Idle, Heatup und Hold

### Erwartung

- `set=320.0C` wird angezeigt.
- `tip` steigt.
- `mode` geht erst in `HEATUP` und spaeter in `HOLD`.

## Test 4: Abkuehlen und Lastverhalten

### Schritte

1. `sim load 150`
2. `status`
3. `sim tip 380`
4. `status`
5. `fan`
6. `sim load 100`

### Was wir testen

- Einfluss einer groesseren thermischen Last
- Verhalten bei Ueberschwingen
- Kuehlpfad und Lueftermodell

### Erwartung

- Hoehere Last verlangsamt das Aufheizen.
- Bei `sim tip 380` geht der Regler in Kuehlen oder Idle.
- Der Fan laeuft mit hoeherer PWM und RPM an.

## Test 5: Umgebungseinfluss

### Schritte

1. `sim ambient 35`
2. `status`
3. `sim ambient 25`
4. `status`

### Was wir testen

- Einfluss einer hoeheren Umgebungsbasis auf die Simulationsstrecke

### Erwartung

- `amb` und `base` steigen auf etwa `35.0C`.
- Das Temperaturverhalten reagiert sichtbar traeger.
- Nach Rueckstellen auf `25C` normalisiert sich der Verlauf.

## Test 6: Dock und Standby

### Schritte

1. `dock on`
2. `status`
3. `stream 500`
4. Einige Sekunden beobachten
5. `dock off`
6. `status`
7. `stream off`

### Was wir testen

- Simuliertes Einsetzen in die Docking-Ablage
- Standby-Zieltemperatur
- Unterschied zwischen eingestelltem Sollwert und effektivem Regelsoll

### Erwartung

- `dock=on` und `standby=1` erscheinen.
- `eff` sinkt auf den Standby-Zielwert.
- Beim Undocken springt `eff` wieder auf das normale Arbeitssoll.

## Test 7: Display-Seiten

### Schritte

1. `screen overview`
2. `screen`
3. `screen thermal`
4. `screen`
5. `screen control`
6. `screen`
7. `screen cal`
8. `screen`
9. `screen next`
10. `screen`

### Was wir testen

- Mehrseitige Display-Ansicht
- Plausible Darstellung von Overview, Thermal, Control und Calibration

### Erwartung

- Jede Seite hat eigene, sinnvolle Inhalte.
- `screen` zeigt immer die aktuell aktive Seite.
- Die Seiten lassen sich zyklisch wechseln.

## Test 8: UI-Grundlogik

### Schritte

1. `ui`
2. Falls ein Encoder angeschlossen ist: drehen und kurz druecken
3. Danach erneut `ui`

### Was wir testen

- UI-Zustand
- Sollwertveraenderung ueber Encoder
- Menuewechsel und Seitenwechsel ueber UI

### Erwartung

- `ui` zeigt `screen`, `menu`, `page`, `save_pending`, `target` und `stream`.
- Mit Encoder aendert sich der Sollwert in 5-C-Schritten.
- Menueaktionen werden ueber UART-Logs sichtbar.

Hinweis:

- Falls kein Encoder angeschlossen ist, diesen Testteil einfach ueberspringen.

## Test 9: Fault-Pfad

### Schritte

1. `fault`
2. `fault inject`
3. `status`
4. `fault`
5. `fault ack`
6. `fault`
7. `status`

### Was wir testen

- Fault-Latching
- Zustandswechsel in `FAULT`
- Quittierungslogik

### Erwartung

- Nach `fault inject` geht die Station in den Fault-Zustand.
- Der Fehler bleibt gelatcht.
- `fault ack` loescht den injizierten Fehler, wenn keine aktive Ursache mehr anliegt.

## Test 10: Kalibrierpfad ohne reale Sensoren

### Schritte

1. `cal`
2. `cal start`
3. `screen cal`
4. `stream 500`
5. Warten, bis `cal` oder `status` `cap=1` zeigt
6. `cal point`
7. Schritte 5 und 6 wiederholen, bis mindestens 3 Punkte gespeichert sind
8. `cal finish`
9. `stream off`
10. `cal`
11. `storage`

### Was wir testen

- Bring-up-Kalibrierung
- Stabilitaets- und Capture-Logik
- Speichern der Kalibrierdaten

### Erwartung

- Eine Session startet sauber.
- Punkte lassen sich erst nach stabiler Phase uebernehmen.
- Nach `cal finish` ist die Kalibriertabelle gueltig und gespeichert.

## Test 11: Persistenz

### Schritte

1. `target 280`
2. `storage`
3. Board neu starten
4. `status`
5. `storage`
6. `cal`

### Was wir testen

- Persistenz von Sollwert und Kalibrierdaten ueber Reset

### Erwartung

- Nach dem Neustart sind Solltemperatur und Kalibrierstatus wieder vorhanden.
- Storage meldet weiterhin gueltige Daten.

## Test 12: Abschlusscheck

### Schritte

1. `sim reset`
2. `dock off`
3. `stream off`
4. `fault`
5. `status`
6. `screen overview`
7. `screen`

### Was wir testen

- Rueckkehr in einen sauberen Grundzustand nach allen Tests

### Erwartung

- Kein Fault aktiv
- Sinnvolle Ziel- und Temperaturwerte
- Hauptanzeige wieder plausibel

## Wichtige Befehle im Ueberblick

### Allgemein

- `help`
- `status`
- `stream`
- `stream on`
- `stream off`
- `stream <ms>`

### Temperatur und Simulation

- `target`
- `target <degC>`
- `sim`
- `sim ambient <degC>`
- `sim load <percent>`
- `sim tip <degC>`
- `sim reset`

### Dock, Fan, Display

- `dock`
- `dock on`
- `dock off`
- `dock toggle`
- `fan`
- `screen`
- `screen next`
- `screen overview`
- `screen thermal`
- `screen control`
- `screen cal`

### Fault und Kalibrierung

- `fault`
- `fault inject`
- `fault ack`
- `cal`
- `cal start`
- `cal point`
- `cal finish`
- `cal cancel`

### Persistenz und UI

- `storage`
- `ui`

## Ziel nach Abschluss dieser Tests

Wenn alle Tests erfolgreich durchlaufen, ist der sensorlose Nucleo-Stand so weit abgesichert, dass beim Wechsel auf echte Hardware im Wesentlichen noch diese Themen offen sind:

- reale Sensorpfade verifizieren
- reale Skalierungsfaktoren und Polaritaeten pruefen
- echte Hardwarefehler finden und beseitigen

Die Zustandsmaschine, UI-Grundlogik, Fault-Logik, Storage, Kalibrierfluss, Display-Darstellung und der diagnostische Bedienpfad sollten dann bereits weitgehend stehen.