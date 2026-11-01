# VanPi HTTP SOC – Regression und Korrektur

## Befund

Nightmans [API-Screenshot in Beitrag 119](https://forum.pekaway.de/t/esp-basiertes-touch-display-fur-vanpi/2732/119) zeigt diese `/batt`-Antwort:

```json
{"VoltB":26.63,"Ampere":-0.8,"battsoc":96.5}
```

[Beitrag 155](https://forum.pekaway.de/t/esp-basiertes-touch-display-fur-vanpi/2732/155) meldet den erneuten Ausfall bei 91,9 %. Eine konkrete `/batt`-Antwort dieses späteren Zeitpunkts liegt nicht vor. Der frühere Screenshot belegt aber bereits einen numerischen Dezimal-SOC.

Der bisherige Parser akzeptiert die JSON-Dezimalzahl mit `validEnergyNumber()`, übergibt sie anschließend jedoch an `jsonInt()`. Dieser Helfer behandelt nur JSON-Ganzzahlen und Strings. Bei einer JSON-Dezimalzahl liefert sein String-Fallback `nullptr` und damit 0. Dieser falsche Wert wird anschließend als gültig und frisch gespeichert. Mit dem tatsächlich verwendeten ArduinoJson reproduziert: Die obige Antwort ergibt vor dem Fix `SOC=0`, `battery_fields=7` und einen aktualisierten SOC-Zeitstempel.

## Änderung

Nur die SOC-Konvertierung in `parse_batt_json()` wird angepasst. `jsonBatterySoc()` validiert zunächst den Zahlenwert und nutzt dann die vorhandene `jsonFloat()`-Konvertierung. Unterstützt werden JSON-Ganzzahlen, JSON-Dezimalzahlen sowie numerische Strings mit Punkt oder Komma. Nur endliche Werte zwischen 0 und 100 werden übernommen. `jsonInt()` und die anderen API-Parser bleiben unverändert.

Die interne Speicherung bleibt bewusst `int`: Home, Statusleiste, Energie-Dashboard und Bildschirmschoner zeigen weiterhin ganze Prozent an. Positive Nachkommastellen werden wie bisher bei SOC-Strings abgeschnitten: `91.9`, `"91.9"` und `"91,9"` ergeben jeweils 91 %. Der Fehler war der falsche Sprung auf 0, nicht das bestehende Ganzzahl-Anzeigeformat. Auch der Energieverlauf übernimmt weiterhin diesen ganzzahligen SOC.

Fehlende, ungültige oder außerhalb des Bereichs liegende SOC-Werte überschreiben weder den letzten gültigen SOC noch dessen Zeitstempel. Echte 0 % und 100 % bleiben gültig. Gültige Spannung/Strom-Werte derselben Antwort werden unabhängig übernommen. Ein HTTP-Fehler ruft den Parser nicht auf. Der bereits vorhandene Wechsel zwischen Demo und Live setzt die Messwerte zurück; im normalen Live-Betrieb gibt es außer dem `/batt`-Parser keinen weiteren SOC-Schreiber. Cerbo-MQTT schreibt seinen SOC separat und ersetzt den VanPi-SOC nicht.

Die Energiehistorie nutzt zusätzlich Zeitstempel und ihre bestehende Frischeprüfung. Die vorhandenen Anzeige-/Verbindungsregeln der übrigen Ansichten werden mit diesem gezielten Fix nicht verändert. Eine vollständig neue Darstellung veralteter Daten ist nicht Teil der Änderung.

TTGO wird nicht verändert: Nightman hat in [Beitrag 154](https://forum.pekaway.de/t/esp-basiertes-touch-display-fur-vanpi/2732/154) den fehlenden Schreibzugriff auf seine Node-RED-Globalvariable als separaten Flowfehler identifiziert.

## Prüfung

`tools/test_battery_soc.py` extrahiert die echten Parser und Zahlenhelfer aus `http_handler.cpp`. Der C++-Test verwendet dieselben ArduinoJson-Header wie die Firmware; nur Zustand, Uhr und Mutex werden ersetzt. Keine Geräteverbindung und keine Schaltbefehle.

```text
python tools/test_battery_soc.py --compiler /path/to/g++ --arduinojson /path/to/ArduinoJson/src
```

Alternativ kann `--compiler` auf eine Zig-Executable zeigen. Der Test benötigt keine zusätzliche Firmware-Abhängigkeit und legt seine temporäre ausführbare Datei außerhalb des Projekts an.

Abgedeckt: Originalantwort aus dem Forum; 91.9 als Zahl, Punkt- und Komma-String; ganze Zahlen und `.0`; Grenzen 0/100; fehlende/null/bool/leere/ungültige/zu große Werte; Bereichsverletzungen; fehlerhaftes JSON; Erhalt des letzten Werts und SOC-Zeitstempels; unabhängige Aktualisierung von Spannung und Strom.

Der Test schlägt vor der Korrektur mit der Originalantwort fehl und besteht nach dem Fix. Ein Live-Test bei Nightman steht noch aus.
