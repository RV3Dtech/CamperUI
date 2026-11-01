# RV3D.92 – Core-Vergleich 3.3.11 / 3.3.12

Beide Dateien stammen aus derselben Quellkopie von V1.9.1-RV3D.92-dev einschließlich SOC-Fix. Hinzugekommen sind ausschließlich Startmeldungen zu Firmware/Core/ESP-IDF und dem tatsächlich konfigurierten VanPi-Ziel. Board-Einstellungen und externe Bibliotheken sind für beide Builds gleich.

Die Dateien sind Anwendungs-BINs für die vorhandene CamperUI-Web-Updatefunktion, keine zusammengeführten USB-Erstinstallationsdateien. Private WLAN-Vorgaben sind nicht enthalten; beim normalen OTA-Update bleiben die gespeicherten Geräteeinstellungen erhalten.

Für den Vergleich dieselbe Display-Hardware, dasselbe WLAN und dieselbe VanPi-Konfiguration verwenden. Zunächst die Datei mit `Core-3.3.11`, bei Bedarf anschließend die Referenz mit `Core-3.3.12` über die bestehende Update-Seite installieren. Die bereits veröffentlichte Tester-BIN bleibt unverändert.

Nach dem Neustart sollten im Startlog folgende Zeilen stehen (Beispiel, keine erfassten Live-Daten):

```text
[BUILD] Firmware: V1.9.1-RV3D.92-dev | Arduino-ESP32: 3.3.11 | ESP-IDF: ...
[HTTP] VanPi target: http://<gespeicherte-VanPi-Adresse>:1880 | source=LIVE
```

`source=DEMO` bedeutet, dass der Demo-Modus aktiv ist. Für einen Live-HTTP-Vergleich muss hier `LIVE` stehen. Die URL wird nach dem Laden der gespeicherten Einstellungen mit demselben Helfer erzeugt, den auch die HTTP-Abfragen verwenden.

Für die Rückmeldung sind der vollständige Startabschnitt, etwaige `[HTTP] GET ...`-Fehler, die angezeigten Batterie-/SOC-Werte und die Beobachtung über mehrere Neustarts hilfreich. Ein erfolgreicher Build allein bestätigt keine Ursache im Core; das zeigt erst der Vergleich am Gerät.
