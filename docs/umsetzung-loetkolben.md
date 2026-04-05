# Umsetzung Loetkolben-Firmware

## Ausgangslage

Das vorhandene Repository ist im Moment ein Dual-Core-STM32CubeMX-Projekt mit einem kleinen Testprogramm auf CM7 und einem weitgehend leeren CM4-Projekt.

Das Konzept in `docs/anforderung loetkolben.txt` ist fuer eine deutlich andere Zielhardware geschrieben als das aktuelle Nucleo-Testsetup in `nucleotest.ioc`.

Wichtig fuer die weitere Umsetzung:

- V1 soll bewusst einfach bleiben
- V1 soll nur auf einem Core laufen
- das bestehende Testprojekt soll erhalten bleiben
- es soll moeglich sein, gezielt entweder das Test-Image oder die Loetkolben-Firmware zu bauen und zu flashen

## Bewertung des Konzepts

Das Konzept ist fuer eine V1 gut geeignet. Es ist funktional klar, sicherheitsorientiert und bereits ausreichend konkret fuer eine saubere Firmware-Struktur.

Besonders gut fuer eine einfache erste Umsetzung:

- nur ein Core in V1
- feste Messrate von 50 Hz
- feste Heiz-PWM mit 20 kHz
- klar definierte Off-Fenster-Sequenz fuer die Thermoelementmessung
- klare Trennung zwischen Normalbetrieb und Kalibriermodus
- klare harte Fehlerfaelle

## Wichtige technische Schlussfolgerungen

### 1. V1 nur auf CM7

Fuer V1 sollte ausschliesslich der CM7 benutzt werden.

Begruendung:

- genug Rechenleistung auf einem Core
- weniger Komplexitaet beim Debuggen
- keine Inter-Core-Kommunikation
- deutlich einfachere Inbetriebnahme

Der CM4 bleibt im Projekt erhalten, wird fuer die Loetkolben-Firmware aber zunaechst nicht benoetigt.

### 2. Kein RTOS in V1

Fuer dieses Technikerprojekt ist ein bare-metal Ansatz mit wenigen Interrupts und einer klaren Main-Loop die einfachste und robusteste Loesung.

Empfohlene Aufteilung:

- Main-Loop fuer UI, Menue, Logging und unkritische Hintergrundarbeit
- Timer-Interrupt fuer 50-Hz-Regelzyklus
- Timer-basierte kurze Messsequenz im Mikrosekundenbereich
- ADC mit DMA fuer kurze Sample-Bursts
- EXTI/Input-Capture nur fuer Ereignisse wie Faults, Tacho und Encoder

### 3. Die aktuelle IOC-Datei sollte nicht fuer beide Hardwarewelten verbogen werden

Das aktuelle `nucleotest.ioc` ist noch auf Nucleo-Testfunktionen und deren Pinbelegung ausgelegt. Das kollidiert direkt mit der neuen Zielhardware.

Beispiele fuer Konflikte:

- aktuelle Nucleo-/ETH-/USB-Belegung belegt mehrere Pins, die laut Loetkolben-Konzept andere Funktionen bekommen sollen
- das bestehende Testprogramm auf `CM7/Core/Src/main.c` nutzt BSP-LEDs, Button und USART-Testlogik
- die Loetkolben-Hardware braucht dagegen ADC, SPI, I2C, Timer, DAC und Fault-Eingaenge auf anderen Pins

Deshalb sollte die Loetkolben-Firmware nicht als grosse `#ifdef`-Schicht in die bestehende Test-IOC-Datei gepresst werden.

## Empfohlene Projektstrategie

## Ziel

Ein Repository, zwei klar getrennte Firmware-Varianten:

1. Test-Firmware
2. Loetkolben-Firmware

Beide sollen separat buildbar und flashbar bleiben.

## Empfohlene einfache Loesung

### Variante A: zwei CM7-Apps im selben Repo

Das ist die fuer dein Ziel einfachste und sauberste Loesung.

Vorschlag:

- bestehendes `CM7` bleibt die Test-Firmware
- bestehendes `CM4` bleibt unberuehrt oder leer
- neue separate CM7-App fuer die Loetkolben-Firmware anlegen, zum Beispiel in einem neuen Unterordner
- fuer beide Apps eigene Build- und Launch-Eintraege anlegen

Vorteile:

- Testprojekt bleibt unveraendert erhalten
- keine unuebersichtlichen `#ifdef`-Konstrukte in einem einzigen `main.c`
- CubeMX-Regeneration fuer die Loetkolben-Hardware bleibt sauber
- Flash-Auswahl ist spaeter nur eine Frage des gewaehlten Launch- oder Build-Profils

### Variante B: ein CM7-Projekt mit App-Schalter

Das ist moeglich, aber hier nicht meine Empfehlung.

Nachteil:

- die Hardwareinitialisierung unterscheidet sich zu stark
- CubeMX-Code wird schnell unuebersichtlich
- das Risiko steigt, dass Test- und Zielhardware sich gegenseitig stoeren

## Konkrete Empfehlung

Ich empfehle fuer dieses Repo:

- `CM7` als bestehende Test-Firmware belassen
- eine zweite Firmware-Variante fuer den Loetkolben als eigenes CM7-Projekt im selben Repo anlegen
- am Root-CMake eine einfache Auswahl ergaenzen, welches Projekt gebaut werden soll
- in `.vscode/launch.json` zwei Debug-/Flash-Konfigurationen pflegen

Damit hast du effektiv den gewuenschten Schalter, aber als Build-/Flash-Auswahl statt als komplizierten Quellcode-Schalter.

## Vorschlag fuer die Repo-Struktur

Ein moeglicher spaeterer Aufbau:

```text
CM7/                    -> bestehende Test-Firmware
CM4/                    -> bleibt fuer jetzt unangetastet
iron_cm7/               -> neue Loetkolben-Firmware
docs/
  anforderung loetkolben.txt
  umsetzung-loetkolben.md
```

Alternativ kann der neue Ordner auch anders benannt werden. Wichtig ist die saubere Trennung.

## Aktueller Stand im Repository

Die Trennung ist jetzt bereits als Grundgeruest im Repo angelegt:

- `CM7/` bleibt die bestehende Test-Firmware
- `iron_cm7/` ist als neue, getrennte CM7-Firmware-Variante angelegt
- im Root-CMake gibt es jetzt die Cache-Variable `FIRMWARE_VARIANT` mit den Werten `TEST` und `IRON`
- in `CMakePresets.json` gibt es dafuer die Presets `Debug`, `Release`, `Debug-Iron` und `Release-Iron`
- in `.vscode/launch.json` gibt es zwei Flash-/Debug-Ziele: `Test CM7` und `Loetkolben CM7`
- in `.vscode/tasks.json` gibt es jetzt getrennte Build-Tasks fuer beide Varianten, damit F5 nicht versehentlich das falsche Preset baut

Wichtig:

- `iron_cm7/` ist im Moment bewusst nur ein Scaffold auf Basis des bisherigen CM7-Testprojekts
- die eigentliche Loetkolben-Pinbelegung und Peripheriekonfiguration kommt im naechsten Schritt

Danach wurde das Scaffold bereits weiter in Richtung Zielhardware umgebaut:

- keine Abhaengigkeit mehr von der Nucleo-BSP im `iron_cm7`-Target
- zentrale Pin-Defines fuer die Loetkolben-Hardware in `iron_cm7/Core/Inc/main.h`
- minimale Zustandsmaschine in `iron_cm7/Core/Src/app.c`
- sichere Grundinitialisierung fuer kritische Ausgaenge wie `HEATER_EN` und `TIP_CLAMP`
- eigener Peripherie-Layer in `iron_cm7/Core/Src/peripherals.c`
- MSP-seitige AF-Konfiguration fuer die ersten Kernperipherien in `iron_cm7/Core/Src/stm32h7xx_hal_msp.c`

Wichtig dabei:

- die derzeit hinterlegten sicheren Ausgangspegel sind Annahmen und muessen gegen die reale Hardwarepolaritaet geprueft werden, bevor der Heizpfad aktiv getestet wird

Aktuell bereits als Grundperipherie vorbereitet:

- ADC1 fuer internes Thermoelement
- ADC2 fuer Strom und Buck-Boost-Spannung
- DAC1 fuer Buck-Boost-Referenz
- I2C1 fuer INA238
- I2C3 fuer MCP9808
- SPI2 fuer Display
- TIM1 fuer Heizer-PWM
- TIM3 fuer Backlight-PWM
- TIM5 fuer den Drehencoder
- TIM7 als Mikrosekunden-Zeitbasis fuer das Messfenster

Zusätzlich ist jetzt bereits ein erstes Messfenster-Grundgeruest vorhanden:

- 50-Hz-Messausloesung im Software-Tick
- Off-Fenster mit Mikrosekunden-Wartezeiten
- Clamp-Umschaltung fuer das Messfenster
- 8-fach-ADC-Abtastung des internen Thermoelements
- robuste Mittelung mit Verwerfen von Minimum und Maximum

Wichtig:

- der PWM-Timer fuer den Heizer laeuft zwar bereits als Grundstruktur
- der Leistungspfad bleibt im aktuellen Stand aber bewusst gesperrt
- es wird also noch keine echte Heizfreigabe vorgenommen
- die ADC-/DAC-/SPI-HAL-Module sind im aktuellen Workspace noch nicht lokal vorhanden und daher fuer einen stabilen Build vorlaeufig wieder aus dem Iron-Target herausgenommen
- der Messpfad liefert bis zur Nachruestung dieser Treiber deshalb aktuell Dummy-Rohwerte statt echter ADC-Daten

Das ist Absicht, damit zuerst die Mess- und Sicherheitsbasis ohne Risiko steht.

Bewusst noch nicht im ersten Schritt verdrahtet:

- SPI1 fuer MAX31856
- I2C2 fuer HUSB238A
- TIM15 fuer Fan-PWM
- TIM17 fuer Fan-Tacho
- USB-CDC

Diese Punkte sind nicht vergessen, sondern werden erst nach dem stabilen Kernpfad ergänzt.

## Pin- und Hardwarebewertung

Die Pinliste im Konzept ist insgesamt plausibel und gut gruppierbar.

### Sinnvolle Funktionsgruppen

#### Heizpfad

- PE9: Heiz-PWM ueber TIM1 CH1
- PE2: Heater Enable
- PF10: Tip Clamp
- PF9: Tip Check

Das passt gut zu deiner Messfenster-Idee.

#### Temperaturmessung

- PF11: internes Thermoelementsignal auf ADC1 INP2
- PG9/PG10/PG11/PG12/PG13/PD7: MAX31856 ueber SPI1 mit DRDY und Fault
- PA8/PC9: MCP9808 ueber I2C3

Das ist fuer V1 gut handhabbar.

#### Leistungs- und Spannungsmessung

- PA6: Stromsignal auf ADC2 INP3
- PC4: Buck-Boost-Ausgangsspannung auf ADC2 INP4
- PB6/PB7/PB5: INA238 auf I2C1 plus Alert
- PF6: INA300 Latch
- PE3: Overcurrent-Eingang

Auch das ist fuer V1 passend.

#### Buck-Boost

- PA4: DAC1 OUT1 als Sollwert
- PA5: Buck-Boost Fault

Fuer V1 reicht eine feste 24-V-Vorgabe voellig aus.

#### Benutzerinterface

- PB13/PB15/PB14/PD8/PD9: Display auf SPI2 plus DC, Reset, CS
- PA7: Display-Backlight PWM
- PA0/PA1: Encoder ueber TIM5 Encoder Mode
- PA2: Encoder-Taster

Das ist einfach und gut fuer V1.

#### Luefter

- PE5: Fan PWM auf TIM15 CH1
- PB9: Fan Tacho auf TIM17 CH1 Input Capture
- eventuell zusaetzlicher Fan Enable, falls Hardwareseitig vorhanden

Hier fehlt nur die finale Bestaetigung, ob ein separater Fan-Enable-Pin existiert.

#### USB / Diagnose

- PA9/PA11/PA12: USB FS

USB-CDC fuer Diagnose ist spaeter sinnvoll, aber nicht als erster Inbetriebnahmeschritt.

## Firmware-Architektur fuer V1

## Einfacher Modulzuschnitt

Fuer die Loetkolben-Firmware wuerde ich folgende Module vorsehen:

- `app_main`: Initialisierung und Hauptzustandsmaschine
- `heater`: PWM, Enable, Tip-Clamp, sichere Abschaltung
- `temp_internal`: ADC-Messsequenz des internen Thermoelements
- `temp_external`: MAX31856 im Kalibriermodus
- `ambient`: MCP9808 fuer Cold-Junction und Gehaeuseschutz
- `power_monitor`: Strom, Spannung, Leistung, INA238
- `buckboost`: DAC-Sollwert und Fault-Ueberwachung
- `fan`: Ein/Aus, PWM, Tacho
- `encoder_ui`: Encoder, Taster, Menue
- `display`: ST7789 mit partiellen Updates
- `calibration`: LUT, Plausibilitaet, Interpolation
- `control`: PID-Regler und Betriebszustaende
- `fault`: zentrale Fehler- und Warnungslogik
- `storage`: Flash/EEPROM-Emulation fuer Sollwert und Kalibrierdaten
- `diag`: serielle oder spaetere USB-Diagnoseausgabe

## Empfohlene Betriebszustaende

Die Gesamtlogik sollte als einfache Zustandsmaschine aufgebaut werden:

- `BOOT`
- `IDLE`
- `NO_CALIBRATION`
- `HEATING`
- `READY`
- `CALIBRATION`
- `FAULT`

Das ist fuer Inbetriebnahme und Fehlersuche deutlich uebersichtlicher als verstreute Flags.

## Mess- und Regelkonzept fuer V1

### Temperaturmessung

Empfehlung fuer eine einfache erste robuste Umsetzung:

1. TIM1 erzeugt dauerhaft 20-kHz-PWM fuer den Heizer.
2. Ein separater 50-Hz-Timer startet alle 20 ms ein Messfenster.
3. Im Messfenster wird `HEATER_EN` abgeschaltet.
4. Ein Mikrosekunden-Timer steuert die Sequenz 15 us, Clamp umschalten, 20 us warten, ADC-Burst ausloesen.
5. ADC1 erfasst 8 Samples per DMA.
6. Danach Clamp wieder aktivieren und `HEATER_EN` wieder freigeben.
7. Die Auswertung erfolgt direkt danach im Interrupt oder kurz darauf im Main-Kontext.

Das ist fuer V1 deutlich einfacher als eine hochoptimierte Komplett-DMA-Loesung, aber immer noch sauber genug.

### Auswertung des internen Signals

So wie im Konzept beschrieben:

- 8 Werte aufnehmen
- Minimum und Maximum verwerfen
- restliche 6 mitteln
- Mittelwert ueber LUT in Temperatur umrechnen
- Cold-Junction-Kompensation mit MCP9808 anwenden

### Regelung

Der PID-Regler laeuft mit 50 Hz und nutzt immer den neuesten Temperaturwert.

Wichtig fuer V1:

- Anti-Windup vorsehen
- Ausgang sauber auf 0 bis 100 Prozent begrenzen
- bei Fehler oder fehlender Kalibrierung sofort Ausgang auf 0

## Sicherheitskonzept fuer V1

Hier sollte bewusst zentral und einfach gearbeitet werden.

Empfehlung:

- eine zentrale Fault-Verwaltung mit Fehlerbits und Klartexten
- eine einzige Funktion `heater_shutdown()` als harte Sicherheitsreaktion
- alle harten Fehler gehen ueber denselben Abschaltpfad

Harte Fehler fuer V1 direkt umsetzen:

- Spitzentemperatur ueber Grenzwert
- Overcurrent
- Buck-Boost Fault
- OPV-PGOOD fehlt
- ungueltige oder fehlende Kalibrierung im Normalbetrieb
- Umgebungstemperatur zu hoch
- dauerhafter Sensorausfall

## Anzeige und Bedienung

Fuer V1 sollte die UI bewusst knapp bleiben.

Hauptseite:

- Ist-Temperatur gross
- Soll-Temperatur
- Heizleistung in Watt
- Statuszeile
- Warn- oder Fehlertext

Menue zunaechst nur mit:

- Kalibrierung starten
- Kalibrierstatus anzeigen
- eventuell Solltemperatur-Grenzen

## Persistenz

Fuer V1 reichen drei persistente Datenbereiche:

- letzte Solltemperatur
- Kalibrierdaten-LUT
- Flag `kalibrierung_gueltig`

Wichtig ist dabei weniger Eleganz als Robustheit:

- Datenstruktur versionieren
- CRC mit abspeichern
- nur gueltige Datensaetze akzeptieren

## Konkreter Umsetzungsplan

## Phase 1: Projektstruktur vorbereiten

1. Bestehende Test-Firmware unveraendert lassen.
2. Zweite Firmware-Variante fuer den Loetkolben anlegen.
3. Root-CMake so erweitern, dass Test oder Loetkolben gezielt gebaut werden koennen.
4. Zwei Launch-Konfigurationen fuer Flash/Debug anlegen.

## Phase 2: CubeMX fuer Loetkolben-Grundhardware

Zuerst nur die Basis aktivieren:

- System Clock
- GPIO fuer sichere Default-Zustaende
- TIM1 fuer Heater-PWM
- GPIO fuer Heater Enable und Tip Clamp
- ADC1 fuer internes Thermoelement
- ADC2 fuer Strom und Spannung
- TIM5 Encoder
- SPI2 Display-Grundlage
- I2C3 MCP9808
- I2C1 INA238
- DAC1 fuer Buck-Boost

Noch nicht zuerst machen:

- USB-CDC
- MAX31856
- komplexe Display-Grafik
- Luefter-Tacho-Feinlogik

## Phase 3: Minimal lauffaehige Firmware

Ziel:

- Display lebt
- Encoder funktioniert
- Umgebungstemperatur wird gelesen
- Heizpfad bleibt sicher aus
- Fault-Eingaenge koennen angezeigt werden

## Phase 4: Heizpfad und interne Temperaturmessung

Ziel:

- 20-kHz-PWM laeuft
- Enable/Clamp-Sequenz funktioniert zeitlich korrekt
- 8er-ADC-Burst liefert stabile Rohwerte
- Rohwerte werden ueber Diagnose ausgegeben

Erst wenn dieser Schritt sauber steht, sollte die eigentliche Temperaturregelung aktiviert werden.

## Phase 5: Regelung ohne Kalibrierbetrieb freischalten

Zuerst:

- Dummy- oder Testbetrieb nur mit begrenzter Leistung
- Schutzgrenzen konservativ setzen
- Logging aktiv halten

## Phase 6: Kalibriermodus

Dann erst:

- MAX31856 anbinden
- stabile externe Referenztemperatur erkennen
- LUT abspeichern
- Normalbetrieb nur bei gueltiger LUT erlauben

## Phase 7: Restfunktionen

- Heizleistungsanzeige
- INA238-Diagnose
- Luefterlogik
- USB-CDC-Diagnose

## Flash- und Umschaltstrategie

Der gewuenschte Schalter sollte als Build-/Flash-Auswahl umgesetzt werden, nicht als Hardware-Umschalter und nicht als ein gemeinsames Binary mit vielen Sonderfaellen.

Einfacher Weg:

- eine Launch-Konfiguration `Test CM7`
- eine Launch-Konfiguration `Loetkolben CM7`
- optional zusaetzlich getrennte Build-Presets oder Tasks

Dann entscheidest du beim Flashen schlicht, welches ELF geflasht wird.

## Was ich als naechstes konkret empfehlen wuerde

1. Die Loetkolben-Firmware als zweite, getrennte CM7-Variante im selben Repo anlegen.
2. Fuer diese Variante eine eigene CubeMX-Konfiguration auf Basis deiner Pinliste erstellen.
3. In der ersten Ausbaustufe nur sichere Grundhardware und Diagnose aufbauen.
4. Den Heizpfad erst danach schrittweise aktivieren.

## Offene Punkte vor der Umsetzung

Diese Punkte sollten wir vor dem eigentlichen Einbau kurz verifizieren:

- existiert ein separater Fan-Enable-Pin oder nicht
- ist `TIP-Check` rein Diagnose oder funktional sicherheitsrelevant
- wo und wie sollen persistente Daten im Flash liegen
- soll USB-CDC direkt in V1 enthalten sein oder erst spaeter folgen
- soll die Loetkolben-Firmware ebenfalls auf CM7 allein debuggt und geflasht werden

## Fazit

Das Konzept ist fuer eine erste robuste Firmware gut geeignet.

Die wichtigste Architekturentscheidung ist: nicht das bestehende Testprojekt umbauen, bis Test- und Zielhardware ununterscheidbar ineinanderlaufen, sondern im selben Repo zwei getrennte Firmware-Varianten pflegen.

Damit bleibt dein Testprojekt erhalten, und du bekommst trotzdem genau die gewuenschte Moeglichkeit, spaeter per Auswahl das eine oder das andere Image auf den Chip zu flashen.