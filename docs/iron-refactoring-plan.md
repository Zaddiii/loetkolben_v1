# Iron CM7 Refactoring Plan

## Zweck

Dieses Dokument definiert den Zielzustand fuer die `iron_cm7`-Firmware auf dem aktuellen Testboard sowie den Weg dorthin. Es ist bewusst keine Sammlung von Einzelpatches, sondern ein Arbeitsdokument fuer eine systematische Bereinigung der Codebasis.

Wichtig dabei:

- Die aktuelle Plattform ist ein Testaufbau, nicht die finale Hardware.
- Der Code soll fuer Bring-up und Fehlersuche stark beobachtbar bleiben.
- Die Firmware soll nicht durch immer neue Einzelreparaturen zu einer schwer wartbaren Sammlung von Spezialfaellen werden.
- Safety, Messpfad, Fehlerrueckmeldung und Konsole werden als zusammenhaengende Architekturaufgabe behandelt.

## Ziele

- Klare Modulgrenzen und eindeutige Verantwortlichkeiten.
- Keine versteckten Doppelzustaendigkeiten fuer sicherheitskritische Ausgaenge.
- Messung, Regelung, UI und Logging sauber entkoppeln.
- Fehler nicht nur tolerieren, sondern systematisch erkennen, protokollieren und sicher behandeln.
- Schrittweise Refaktorierung ohne Big-Bang-Umbau.
- Messbare Vorher-Nachher-Basis fuer Codeumfang und Hotspots.

## Nicht-Ziele

- Kein sofortiger Umbau auf finale Hardwarearchitektur.
- Kein RTOS-Einfuehren als Selbstzweck.
- Kein radikales Redesign aller Treiber auf einmal.
- Keine Vermischung von Test-Firmware (`CM7`) und Iron-Firmware (`iron_cm7`).

## Ist-Zustand in einem Satz

Die Firmware ist funktional schon deutlich weiter als ein reines Scaffold, aber die Struktur ist noch stark von Bring-up-Schritten, direkter Debug-Logik in `main.c` und enger Kopplung zwischen Messung, Safety und Diagnose gepraegt.

## Operative LOC-Baseline

Stand: 2026-04-08

### Gesamtrepo fuer C/H-Dateien

- `.c`: 63 Dateien, 67544 Zeilen
- `.h`: 105 Dateien, 148894 Zeilen
- Gesamt: 216438 Zeilen

Diese Zahl ist fuer Architekturvergleiche nur eingeschraenkt brauchbar, weil sie stark von STM32-/HAL-/CMSIS-Code und generierten Headern dominiert wird.

### Relevante Baseline fuer den eigentlichen Umbau

- `iron_cm7/Core`: 33 Dateien, 7641 Zeilen
- `iron_cm7/Core/Src`: 17 Dateien, 6481 Zeilen
- `iron_cm7/Core/Inc`: 16 Dateien, 1160 Zeilen
- `CM7/Core`: 9 Dateien, 1398 Zeilen
- `CM4/Core`: 9 Dateien, 1229 Zeilen
- `Common`: 1 Datei, 451 Zeilen

Fuer den Vorher-Nachher-Vergleich des Refactorings ist `iron_cm7/Core` die primaere Kennzahl.

### Groesste aktuelle Hotspots in `iron_cm7/Core`

- `main.c`: 1917 Zeilen
- `heater.c`: 665 Zeilen
- `peripherals.c`: 506 Zeilen
- `st7789.c`: 438 Zeilen
- `stm32h7xx_hal_conf.h`: 430 Zeilen
- `app.c`: 421 Zeilen
- `calibration.c`: 332 Zeilen
- `fan.c`: 261 Zeilen
- `stm32h7xx_hal_msp.c`: 246 Zeilen
- `display.c`: 245 Zeilen
- `ui.c`: 231 Zeilen
- `stm32h7xx_it.c`: 216 Zeilen

## Hauptprobleme heute

### 1. `main.c` ist ueberlastet

`main.c` enthaelt nicht nur Boot und Main-Loop, sondern auch eine grosse Menge Debug-, Konsolen- und Reporting-Logik. Dadurch entstehen drei Probleme:

- zu hohe Kopplung zwischen Anwendung und Diagnose
- schlechtere Testbarkeit
- groessere Gefahr, dass Debug-Features Timing und Bedienbarkeit beeinflussen

### 2. Messung und Safety sind noch nicht hart genug getrennt

Der Heizpfad braucht eine Architektur, in der Messung, Regelung und Ausgangsfreigabe unterschiedliche Rollen haben. Aktuell liegt noch zu viel Logik in einer zusammenhaengenden Kette.

Risiken:

- stale measurement wird zu spaet erkannt
- Timingfehler und Messaussetzer bleiben weiche Symptome statt harter Safety-Events
- Ausgaenge koennen an mehreren Stellen indirekt beeinflusst werden

### 3. Konsole und Logging sind funktional, aber noch kein sauberes System

Die UART-Konsole ist wertvoll fuer Bring-up und Diagnose. Gleichzeitig ist sie heute noch eng in den Main-Loop eingebettet.

Ziel ist daher nicht, die Konsole zu entfernen, sondern sie als sauberes Modul zu behandeln:

- Eingabe robust
- Ausgabe steuerbar und standardmaessig ruhig
- keine blockierende oder unkontrollierte Log-Flut in kritischen Pfaden

### 4. Fehlerbehandlung ist noch nicht als durchgaengiges Konzept formuliert

Es gibt bereits wichtige Grundlagen wie Fault-Flags, Reset-Cause-Logs und Safe-State-Reaktionen. Was noch fehlt, ist ein konsistentes Gesamtmodell:

- Was ist recoverable?
- Was ist safety-kritisch und muss latched werden?
- Welche Fehler fuehren zu Retry, welche zu Safe-Off, welche zu Reset?
- Welche Minimalinformationen muessen vor einem Reset veroeffentlicht oder gespeichert werden?

### 5. Testboard und finale Hardware muessen architektonisch getrennt gedacht werden

Die Testhardware ist kein Problem, solange ihre Sonderfaelle an den richtigen Stellen isoliert werden. Problematisch wird es erst, wenn Testboard-Annahmen quer durch die gesamte Anwendung wachsen.

Deshalb gilt:

- Hardwareunterschiede in Profile, Board-Layer oder Konfiguration kapseln
- Safety- und Application-Logik moeglichst hardwareagnostisch halten

## Zielarchitektur

Die Zielarchitektur soll klein bleiben, aber hart getrennte Verantwortungen haben.

### Schicht 1: Board/HAL

Verantwortung:

- GPIO, Timer, ADC, SPI, I2C, UART, DAC
- konkrete STM32-Initialisierung
- hardwareabhaengige Hilfsfunktionen

Dateien heute:

- `peripherals.c`
- `stm32h7xx_hal_msp.c`
- `stm32h7xx_it.c`

Ziel:

- keine fachliche Stationslogik in dieser Schicht

### Schicht 2: Device Drivers

Verantwortung:

- konkrete Sensor- und Displaytreiber
- keine Stationszustandslogik

Dateien heute:

- `ina238.c`
- `mcp9808.c`
- `max31856.c`
- `st7789.c`

Ziel:

- wohldefinierte Poll-/Read-/Status-Schnittstellen

### Schicht 3: Measurement Service

Neue Verantwortung:

- exakter Messablauf als eigener Sequencer/FSM
- Messmetadaten und Freshness
- explizite Fehlerklassifikation des Messpfads

Geplante Zustandsidee:

- `IDLE`
- `PREPARE_OFF`
- `CLAMP_MEASURE`
- `SAMPLE_INTERNAL`
- `SAMPLE_AUX`
- `CLAMP_SAFE`
- `PUBLISH`
- `FAULT`

Wichtige Ausgaben:

- `measurement_valid`
- `measurement_sequence_id`
- `measurement_timestamp_ms`
- `measurement_age_ms`
- `measurement_error_flags`

### Schicht 4: Control Service

Verantwortung:

- Soll-/Ist-Vergleich
- Reglerlogik
- Buck-Boost-Referenz
- nur mit frischen und gueltigen Messdaten arbeiten

Wichtig:

- keine direkte Herrschaft ueber `HEATER_EN` und `TIP_CLAMP`

### Schicht 5: Safety Guard

Neue Kernregel:

Nur dieses Modul darf die sicherheitskritischen Ausgaenge wirklich setzen.

Verantwortung:

- Freigabe oder Sperre von `HEATER_EN`
- `TIP_CLAMP`-Sicherheitszustand
- Erzwingung von Invarianten
- Reaktion auf Safety-Fehler und Timeout

Invarianten:

- kein Heizpfad ohne gueltige frische Messung
- Messfenster und Heizerfreigabe duerfen nicht kollidieren
- bei Safety-Fehlern sofort Safe-Off
- bei unklaren Zustaenden immer konservativer Ausgangszustand

### Schicht 6: Application FSM

Verantwortung:

- Stationszustand
- Moduslogik
- Dock/Standby/Ready/Fault
- Fault-Latching und User-Acknowledge

Datei heute:

- `app.c`

Ziel:

- App-FSM entscheidet fachlich, aber nicht direkt ueber elektrische Detailsequenzen

### Schicht 7: UI and Diagnostics

Verantwortung:

- Encoder/UI
- Display
- Console/Debug/Statusberichte

Wichtig:

- Diagnosesystem darf das Verhalten des Mess- und Safety-Pfads nicht verschlechtern
- Logging soll steuerbar, gepuffert und priorisiert sein

## Zielbild fuer die wichtigsten Dateien

### `main.c`

Soll nur noch enthalten:

- Systemstart
- Peripherie-Init
- Modul-Init
- schlanke kooperative Main-Loop
- keine grosse fachliche Diagnose- oder Parserlogik

### `heater.c`

Soll mittelfristig aufgeteilt werden in:

- `heater_measurement.c`
- `heater_control.c`
- optional `heater_safety.c` oder separates `safety_guard.c`

### `debug`/`console`

Soll aus `main.c` herausgezogen werden in eigene Dateien, zum Beispiel:

- `debug_console.c`
- `debug_report.c`
- spaeter optional `debug_log.c`

## Refactoring-Prinzipien

### 1. Kein Big Bang

Umbauten erfolgen in kleinen Schritten mit laufendem Build und laufender Diagnosefaehigkeit.

### 2. Erst Struktur, dann Verhalten

Wenn moeglich zuerst Schnittstellen und Modulgrenzen einfuehren, bevor Regelverhalten oder Safety-Policy veraendert wird.

### 3. Safety vor Komfort

Neue UI- oder Debug-Features sind nachrangig gegenueber sauberem Messpfad, sicherem Ausgangsverhalten und klarer Fault-Strategie.

### 4. Testboard-Sonderfaelle kapseln

Keine quer verteilten Testboard-Ausnahmen in App- oder Safety-Logik.

### 5. Jede Diagnosefunktion braucht einen Eigentumsbereich

Statusmeldungen, Measurement-Logs, Crash-Infos und Reset-Cause-Daten sollen jeweils klaren Orten und Zustandsregeln folgen.

## Migrationsplan

### Phase 0: Baseline einfrieren

Ziel:

- bestehendes Verhalten dokumentieren
- LOC-Baseline als Vergleichspunkt festhalten
- Hotspots benennen

Artefakte:

- dieses Dokument

### Phase 1: Safety-Ausgaenge zentralisieren

Ziel:

- eine zentrale Funktion oder ein Modul fuer alle sicherheitskritischen Ausgangsschreibzugriffe

Ergebnis:

- keine verstreuten direkten Schreibstellen mehr fuer `HEATER_EN` und `TIP_CLAMP`, ausser in klar begruendeten Low-Level-Notfaellen

### Phase 2: Messmetadaten einfuehren

Ziel:

- jeder Messzyklus hat Sequence-ID, Timestamp, Valid-Bit und Fehlerflags

Ergebnis:

- stale oder fehlende Daten koennen explizit behandelt werden

### Phase 3: Messung als FSM abspalten

Ziel:

- der Messablauf wird als lesbarer Sequencer aufgebaut

Ergebnis:

- klarere Safety-Regeln
- besseres Timing-Verstaendnis
- leichter analysierbare Messfehler

### Phase 4: Regler und Measurement trennen

Ziel:

- Regler arbeitet nur auf publizierter Messstruktur

Ergebnis:

- Control-Pfad wird unabhaengiger vom Messdetail

### Phase 5: Diagnose aus `main.c` herausziehen

Ziel:

- Console, Reporting und Logik in eigene Module

Ergebnis:

- `main.c` wird klein und stabil
- Diagnose wird test- und wartbarer

### Phase 6: Fault- und Reset-Strategie vereinheitlichen

Ziel:

- klarer Umgang mit HardFault, BusFault, Measurement-Timeout, Sensorfehlern und Warning-Zustaenden

Ergebnis:

- sauberes Safe-Off-Verhalten
- reproduzierbare Neustarts
- klarere Fehlerberichte

### Phase 7: Validierung und Vergleich

Ziel:

- Vorher-Nachher-Vergleich auf Codeumfang, Modulgroessen und beobachtbarem Verhalten

Messgroessen:

- Gesamt-LOC in `iron_cm7/Core`
- Groesse von `main.c`
- Groesse von `heater.c` bzw. Nachfolgemodulen
- Anzahl direkter Schreibstellen auf Safety-Ausgaenge
- Anzahl Diagnosefunktionen in `main.c`
- Fehlermeldungsqualitaet nach absichtlicher Fault-Injektion

## Definition of Done fuer den strukturellen Umbau

Der Umbau gilt nicht schon dann als erfolgreich, wenn der Code nur anders aussieht. Erfolgreich ist er erst, wenn die folgenden Punkte gleichzeitig erfuellt sind:

- `main.c` ist deutlich kleiner und auf Boot/Main-Loop reduziert
- Messung und Regelung sind getrennte Verantwortungen
- Safety-Ausgaenge haben einen einzigen fachlichen Eigentumsbereich
- UART-Konsole ist standardmaessig ruhig und fuer Bring-up robust benutzbar
- Faults fuehren reproduzierbar zu Safe-Off und nachvollziehbarer Diagnose
- die Testhardware ist ueber Profile oder Board-Layer gekapselt

## Empfohlene erste konkrete Schritte

1. Safety-Guard-Schnittstelle einfuehren und direkte Ausgangsschreibstellen sammeln.
2. Measurement-Metadatenstruktur in `heater.c` einfuehren, noch ohne grossen Verhaltensumbau.
3. Debug-/Console-Code aus `main.c` in ein erstes eigenes Modul ziehen.
4. Fault- und Reset-Pfad einmal geschlossen definieren und dokumentieren.

## Umgang mit LOC als Metrik

LOC ist hier nur ein Hilfsmittel. Weniger Zeilen sind nicht automatisch besser. Relevant ist die Kombination aus:

- kleinerer Groesse der Hotspots
- klareren Modulgrenzen
- weniger Doppelzustaendigkeiten
- besser beobachtbarem und sichererem Laufzeitverhalten

Deshalb wird die LOC-Baseline nur zusammen mit Architektur- und Verhaltenskriterien verwendet.