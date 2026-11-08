# Display-Akku: Laufzeit und Diagnose (RV3D.93)

Die vorhandene Home-Kachel enthält jetzt Spannung, Testzeit und Test-Minimum.
Sie bleibt über **Home-Einstellungen → Display-Akku** und die Websettings
schaltbar. Ein Antippen öffnet **Display-Akku**, mit festen Tasten **Zurück**,
**Test starten** und **Test beenden**. Die Diagnoseinformationen dazwischen sind
vertikal scrollbar. Es gibt keine zweite Akku-Kachel.

## Testablauf

1. Akku wie gewohnt laden. Vor dem Test die externe Versorgung manuell trennen.
2. Kachel antippen und **Test starten** drücken. Der Start wird erst übernommen,
   wenn der Anfangsdatensatz erfolgreich gespeichert wurde.
3. Home zeigt `Test läuft / 2 h 34 min`. Spannung und Test-Minimum stammen aus
   echten Board-Messungen, auch im Demo-Modus. Nach dem Start wird ein frisches
   ADC-Fenster begonnen, damit vorangegangene Ladesamples nicht in das Minimum
   dieses Tests eingehen.
4. **Test beenden** speichert die Messdauer als **beendeten Teiltest**. Diese
   Bezeichnung bedeutet nicht, dass der Akku vollständig entladen wurde.

Ein gestarteter Test läuft bei ausgeblendeter Home-Kachel, auf anderen Seiten
und bei ausgeschaltetem Display weiter. Ausblenden ist **kein Teststopp**.
Ohne laufenden Test beendet das Ausblenden weiterhin die Messabfrage.

## Diagnose und Grenzen

Neben aktuellem und letztem Test bleiben die bisherigen Messwerte verfügbar:
ADC-Rohwert, ADC-Eingangsspannung, Min/Max seit Messaktivierung, gültige Samples,
Lesefehler und Messalter. Fehlerhafte oder veraltete aktuelle Werte erscheinen
als `--`; fehlendes Test-Minimum als `unbekannt`. Ein fehlendes ADC-Fenster oder
eine Unterbrechung des laufenden Zeitgebers über zehn Sekunden markiert Messlücken.
Das Minimum ist das kleinste tatsächlich erfasste Spannungssample des Tests.

Die Diagnose liest außerdem **nur lesend** die vorhandenen CH32-Register
`REG_INPUT (0x04)` und `REG_DIRECTION (0x02)`. EXIO0/STAT, Eingangsbyte und
Richtungsbyte werden roh und ohne Bewertung angezeigt. Fehlerhafte oder über
zehn Sekunden alte Registerwerte bleiben unbekannt. Es werden keine Pinrichtungen,
Ladeparameter oder Power-Ausgänge umgeschaltet.

Im [offiziellen V4-Schaltplan](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-4/blob/main/hardware/schematics/ESP32-S3-Touch-LCD-4%20V4.0.pdf)
führt ETA6098-STAT über R12 an EXIO0. Dieser Ladehinweis ist **kein eindeutiger
Nachweis für externe Versorgung**. Auch Firmware/Richtung des CH32-Eingangs müssen
noch am Board geprüft werden. Deshalb gibt es weder einen automatischen Teststart
noch ein automatisches Testende, keinen geschätzten SOC, keine Strommessung und
keine behauptete Erkennung „lädt“, „voll“ oder „Akku leer“.

Die Spannungsmessung folgt weiterhin dem [offiziellen Waveshare-Akkubeispiel](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-4/blob/main/examples/arduino/14_LVGL_BatteryVoltage/14_LVGL_BatteryVoltage.ino):
`raw × 3,3 V / 1023 × 3`. Nominale Referenz-/Teilerwerte sind keine Kalibrierung.
Die Nennkapazität eines Akkus wird nicht in einen Prozentstand umgerechnet.

## Neustart und Datenhaltung

Die Dauer verwendet den monotonen 64-Bit-Zeitgeber; sie benötigt weder WLAN
noch eine gültige Uhrzeit. Gespeichert werden nur ein kleiner Testdatensatz
(68 Bytes, Formatversion und Prüfsumme), nicht die gesamten Systemeinstellungen.
Eigener NVS-Namespace: `display_bat`, Schlüssel: `runtime_v1`.

- Schreiben beim Start, beim Beenden und alle fünf Minuten während des Tests.
- Ein fehlgeschlagener Schreibversuch wird frühestens nach fünf Minuten erneut
  versucht; kein Schreibzugriff je Messung oder je UI-Aktualisierung.
- Bei Speicherfehlern wird ausdrücklich gewarnt. Ein nicht sicher speicherbarer
  Teststart wird abgelehnt; ein Stopp beendet die Zeitmessung im RAM auch dann,
  wenn seine Speicherung scheitert.
- Nach Flashzugriffen wird die bestehende RGB-Resynchronisierung angefordert.
- Nach einem Neustart gilt ein gespeicherter offener Test als **unterbrochen**.
  Resetgrund und Unterbrechung werden erfasst und gespeichert. Der Test startet
  nicht von selbst wieder; Ausfallzeit wird nicht zur Laufzeit addiert.
- Die wiederhergestellte Dauer trägt **mind.** als Untergrenze. Seit dem letzten
  erfolgreichen Checkpoint können typischerweise bis zu fünf Minuten fehlen;
  bei Schreibfehlern oder längerer Programmunterbrechung auch mehr.
- Watchdog, Software-Neustart, Brownout und Power-on beweisen keine vollständige
  Akkuentladung. Auch das Wiederanschließen der Versorgung ist kein solcher Beweis.
- Beschädigte oder unbekannte Datensatzversionen werden nicht als gültige
  Messungen übernommen. Die Diagnose weist darauf hin; ein neuer Test kann
  einen frischen Datensatz anlegen.

Es gibt bewusst keine Rangliste „längste vollständige Entladung“ und zunächst
keinen Spannungsverlauf. Der letzte Test bleibt einschließlich Kennzeichnung
**Teiltest** oder **unterbrochen** gespeichert.

## Prüfung und noch nötige Hardwaretests

`tools/test_display_battery.py --compiler <nativer C++-Compiler oder zig.exe>`
prüft das echte Polling und die echten Speicher-/Testfunktionen mit I2C-/NVS-Mocks:
Zeitbasis, Minimum, fehlende Samples, ADC-Grenzen, Prüfsumme, Datensatzversion,
Fünf-Minuten-Abstand, Start-/Stoppfehler, Wiederherstellung nach Stromverlust,
kein automatischer Wiederanlauf sowie Weiterlaufen bei versteckter Kachel.

Noch am V4-Display prüfen: Start/Stopp, Verhalten bei ausgeblendeter Kachel und
Display-Timeout, absichtlicher Neustart, vollständiges Abklemmen nach einem
Sicherungspunkt sowie Bedienbarkeit und Scrollen der Diagnose. EXIO0/STAT bei
USB-Laden, vollem Akku am USB und reinem Akkubetrieb vergleichen. Diese Rohwerte
dürfen bis zur eindeutigen Bewertung keine automatische Steuerung auslösen.
