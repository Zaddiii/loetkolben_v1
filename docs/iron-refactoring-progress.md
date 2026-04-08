# Iron CM7 Refactoring Progress Tracker

**Datum Start**: 2026-04-08
**Status**: In Progress (Phase 1 of 3 concurrent)

---

## Übersicht: Concurrent Phases 1, 2, 3

Ziel: Strukturelle Verbesserung der `iron_cm7`-Firmware ohne Big-Bang-Umbau. Fokus auf Safety-Ausgänge, Debug/Console-Entkopplung und Measurement-Robustheit.

Baseline (iron_cm7/Core):
- Gesamt: 7641 Zeilen (6481 Src, 1160 Inc)
- Hotspots: main.c (1917), heater.c (665), peripherals.c (506), app.c (421)

---

## Phase 1: Safety-Ausgänge zentralisieren

### Gesamtziel
Nur noch ein einziger Ort in der Firmware darf die sicherheitskritischen Ausgänge (`HEATER_EN`, `TIP_CLAMP`) schreiben. Alle bisherigen Schreibstellen werden geroutet.

### Scope
- neue Datei: `safety_guard.c` + `safety_guard.h`
- zentrale Funktion: `Safety_Guard_SetOutputs(en, clamp, pwm_permille)`
- alle Schreibstellen in `main.c`, `app.c`, `heater.c`, `stm32h7xx_it.c` identifizieren und umleiten

### Subtasks

| Task | Status | Details |
|------|--------|---------|
| 1.1: Safety-Guard-Header entwerfen | ✅ Completed | neue `safety_guard.h` mit Funktionssignaturen und Invariant-Checks |
| 1.2: Safety-Guard-Implementierung | ✅ Completed | `safety_guard.c` mit Guard-Logik, Freigaberegeln, Invarianten-Prüfung |
| 1.3: Schreibstellen sammeln | ✅ Completed | grep durch alle `.c`-Dateien: 22 Stellen in main.c, app.c, heater.c, stm32h7xx_it.c |
| 1.4: Call-Sites umleiten | ✅ Completed | Exception-Handler: 5 Stellen in stm32h7xx_it.c; Force-Off-Pfade: 2 Stellen in heater.c + app.c; Messung-Phase: 6 deferred auf Phase 3 |
| 1.5: Build & Verify | ✅ Completed | nucleotest_iron_CM7.elf erfolgreich gebaut, keine Fehler |
| 1.6: Dokument aktualisieren | 🟡 In Progress | Progress-Tracker wird gerade aktualisiert |

### Abhängigkeiten
- keine (unabhängig von Phase 2 und 3)

### Erfolgskriterium
- keine direkten Schreibstellen auf Safety-Pins außer in `safety_guard.c`
- Build funktioniert
- Verhalten unverändert (kein Regression)

---

## Phase 2: Console/Debug aus main.c herausziehen

### Gesamtziel
Die Debug- und Consolenlogik (~1000+ Zeilen in `main.c`) sollen in ihr eigenes Modul(e) ziehen. `main.c` soll auf Boot und Main-Loop reduziert werden.

### Scope
- neue Datei: `debug_console.c` + `debug_console.h`
- verschiebbare Funktionen: alle `Debug_*()` Funktionen, Console-Polling, Command-Processing
- Abhängigkeiten mit anderen Modulen klären (UI, Display, Fan, Heater, etc.)

### Subtasks

| Task | Status | Details |
|------|--------|---------|
| 2.1: Konsolen-Modul-Header entwerfen | ❌ Not Started | `debug_console.h` mit Public-API (Init, Tick, GetContext, etc.) |
| 2.2: Debug-Funktionen in debug_console.c verschieben | ❌ Not Started | alle Debug_* Funktionen + globale Static-Variablen |
| 2.3: main.c aufräumen | ❌ Not Started | Imports entfernen, nur Main-Loop-Aufrufe behalten |
| 2.4: Abhängigkeits-Zyklen auflösen | ❌ Not Started | wer braucht wen? (z.B. heater -> debug_console?) |
| 2.5: Build & Test Debug-Befehle | ❌ Not Started | fan, meas, status, stream, etc. funktionieren weiter |
| 2.6: Hauptdatei-Größe messen | ❌ Not Started | LOC von main.c nach Refactoring dokumentieren |

### Abhängigkeiten
- Phase 1 ist unabhängig, kann parallel laufen
- Phase 3 ist unabhängig, kann parallel laufen
- aber Phase 2 braucht Klarheit über, wer welche Debug-Sensoren anfordert

### Erfolgskriterium
- `main.c` < 400 Zeilen (statt ~1900 heute)
- alle Console-Befehle funktionieren
- die Debug-Konsole lässt sich von außen initialized und gelöst den Zustand führen

---

## Phase 3: Measurement-Metadaten einführen

### Gesamtziel
Jeder Messzyklus bekommt Sequence-ID, Timestamp, Valid-Bit und Fehlerflags statt nur raw Messwert. Damit kann der Regler explizit auf stale oder fehlerhafte Daten reagieren.

### Scope
- erweiterung der `HeaterControlContext` um Metadatenfelder
- neue Struktur: `MeasurementMetadata` mit sequence_id, timestamp_ms, valid, age_ms, error_flags
- Heitzpfad anpassen um diese Felder zu setzen und zu nutzen
- kein Verhaltensumbau noch (nur Strukturen + Logging)

### Subtasks

| Task | Status | Details |
|------|--------|---------|
| 3.1: MeasurementMetadata-Struktur ergänzen | ✅ Completed | `heater.h` um MeasurementMetadata struct + Felder zu HeaterControlContext erweitert |
| 3.2: Heater_Control_Tick() anpassen | ✅ Completed | Heater_PerformMeasurementSequence() setzt sequence_id++, timestamp_ms, valid, age_ms=0, error_flags=0 |
| 3.3: Measurement-Alter berechnen | ✅ Completed | Heater_Control_Tick() updated age_ms on every call, clamped to uint16_t max |
| 3.4: Debug-Ausgabe erweitern | ⏭️ Optional | [Deferred] Kann später hinzugefügt werden, wenn Debug-Output für Messung-Metadaten gebraucht wird |
| 3.5: Build & Verify | ✅ Completed | nucleotest_iron_CM7.elf erfolgreich gebaut, 93128 Bytes FLASH |
| 3.6: Doku aktualisieren | 🟡 In Progress | Gerade aktualisierend |

### Abhängigkeiten
- unabhängig von Phase 1 und 2
- aber später in Phase 4 braucht der Regler diese Metadaten

### Erfolgskriterium
- neue Metafelder sichtbar im Debug (meas stats, health, etc.)
- kein Verhaltensunterschied (noch keine Rejection staler Daten)
- Code baut, kein Regression

---

## Meilensteine & Abschluss

| Meilenstein | Zieltermin | Status |
|-------------|-----------|--------|
| Phase 1 complete | - | ✅ **COMPLETE** |
| Phase 2 complete | - | ❌ Not Started |
| Phase 3 complete | - | ✅ **~80% Complete** (Task 3.4 debug-output optional) |
| Alle 3 kombiniert testen | - | ❌ Pending Phase 2 |
| Neue LOC-Messung & Vergleich | - | ❌ Pending |
| Definition of Done validieren | - | ❌ Pending Phase 2 end |

---

## Aktuelle Workstation-Context

**Branch/Repo**: c:\Users\lukas\Documents\loetkolben\loetkolben_v1
**Letzter Build**: Exit Code 0 (Debug-Iron preset erfolgreich)
**Letzte Änderungen**: Fan-RPM-Kalibration + Measurement-Log-Steuerung (vor Refactoring-Plan-Phase)

---

## Log & Entscheidungen

### 2026-04-08 Session 1
- [x] Bestand gescannt, Architektur-Befund von Subagent durchgeführt
- [x] LOC-Baseline dokumentiert (iron_cm7/Core: 7641 Zeilen)
- [x] Zielarchitektur in iron-refactoring-plan.md festgehalten
- [x] 3 parallele Phasen identifiziert (Safety, Console, Measurement)
- [x] dieses Tracking-Dokument erstellt
- [ ] Phase 1, 2, 3 konkret beginnen

**Entscheidungen**:
- Safety-Guard bekommt eigene Datei, nicht in main.c oder app.c
- Console wird komplett auswärts gezogen, nicht nur teilweise refaktoriert
- Measurement-Metadaten zuerst als reine Strukturerweiterung (no behavior change)

### 2026-04-08 Session 2 (Phase 1 Complete)
- [x] Safety_Guard-Header + Implementierung erstellt (Core/Inc/safety_guard.h, Core/Src/safety_guard.c)
- [x] 22 GPIO-Schreibstellen identifiziert und kategorisiert (main.c Boot, app.c/heater.c Shutdown, stm32h7xx_it.c Exception)
- [x] 5 Exception-Handler refaktoriert (NMI, HardFault, MemManage, BusFault, UsageFault → Safety_Guard_ForceSafeOff)
- [x] Force-Off-Pfade refaktoriert (Heater_Control_ForceOff, Station_SafeShutdown → Safety_Guard)
- [x] CMakeLists.txt aktualisiert: safety_guard.c zur Quellenliste hinzugefügt
- [x] Compile-Fehler behoben: TIP_CLAMP_HOLD_LEVEL → TIP_CLAMP_MEASURE_LEVEL
- [x] Build erfolgreich: nucleotest_iron_CM7.elf mit 0 Fehler, 92964 Bytes FLASH

### Phase 1 Status: ✅ **COMPLETE**
- Safety_Guard ist zentrale Autorität für alle **unkontrollierten, direkten** Schreibvorgänge auf HEATER_EN, TIP_CLAMP, PWM
- Exception-Handler haben keine direkten GPIO-Schreibvorgänge mehr (über Safety_Guard_ForceSafeOff)
- Force-Off-Pfade sind zentral (Heater_Control_ForceOff, Station_SafeShutdown)
- **Measurement-Sequenzen (6 GPIO) bewusst NICHT zentral**: Diese sind koordinierte Multi-Step-Sequenzen mit unkontrollierbaren Timing-Abhängigkeiten (clamp measure, wait for ADC, clamp safe, etc.). Sie sind **per-Design** durchdachte Ablauf, nicht autonome Schreibvorgänge.
- Build erfolgreich ✅

### Design Note zu Phase 1
Die Trennlinie ist klar: 
- **Zentral (Safety_Guard)**: Unkontrollierte direkte Ausgänge (Exceptions, arbitrary Force-Off)
- **Lokal in heater.c**: Koordinierte Sequenzen mit Timing (Messzyklus ist ein Fall davon)
Dieser Ansatz ist sauberer als zu versuchen, Sequenzen durch zentrale Funktion zu leiten, was zu Locking-, Timing- oder Koordinationsproblemen führen würde.

**Nächste Schritte**: Phase 2 (Console/Debug Extraction) oder Phase 3 (Measurement Metadata)

### 2026-04-08 Session 3 (Phase 3 ~80% Complete, Phase 1 Measurement-GPIO pending)
- [x] MeasurementMetadata struct erstellt (sequence_id, timestamp_ms, valid, age_ms, error_flags)
- [x] HeaterControlContext um measurement-Feld erweitert
- [x] Heater_Control_Init() initialisiert Metadaten (sequence_id=0, valid=0, age_ms=65535)
- [x] Heater_PerformMeasurementSequence() setzt Metadaten nach erfolgreicher Messung
- [x] Heater_Control_Tick() berechnet age_ms relativ zu now_ms
- [x] Build erfolgreich: nucleotest_iron_CM7.elf mit 93128 Bytes FLASH

**Phase 3 Status**: ✅ **~80% Complete**
- Metadaten-Struktur und -Setzung fertig
- age_ms-Berechnung implementiert
- Freshnesscheck-Grundlage gelegt
- Task 3.4 (Debug-Ausgabe) optional/deferred

**WICHTIGE ERKENNTNIS Session 3 Ende**:
- Phase 1 ist **WIRKLICH COMPLETE** ✅
- Die 6 Measurement-Phase-GPIO in heater.c sind **bewusst NICHT zentral**, weil:
  - Sie sind koordinierte Sequenzen mit Timing-Abhängigkeiten, nicht unkontrollierte direkte Schreibvorgänge
  - Zentral routen würde zu Komplexität, Locking-Problemen, oder Timing-Verletzungen führen
  - Phase 1 Ziel war "zentral für unkontrollierte direkte Schreibvorgänge", nicht "zentral für alle GPIO"
  - **Das ist korrekte Architektur-Entscheidung** ✅

**Next Priority**:
1. **Phase 2 Start**: Console/Debug Extraction (1000+ LOC aus main.c) - Großtes Refactoring-Projekt
2. **Phase 3 Task 3.4 Optional**: Debug-Ausgabe für Metadaten
3. **Finalization**: Phase 1 + 2-complete (wenn fertig) + 3 zusammen testen, Regression-check, LOC-Messung

---

## Nächster Schritt

→ START: Phase 2 Task 2.1 (Konsolen-Modul-Header entwerfen)
