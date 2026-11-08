# Webasto: Heizungsseite und VanPi-Daten (RV3D.93)

## Erkennung und Bedienung

Der Heizungstyp wird in den Display- oder Websettings gewählt, als `heater_type`
im NVS gespeichert und beim Start geladen. `HEATER_TYPE_WEBASTO = 4` aktiviert
die Webasto-Bedienung; CamperUI erkennt das Gerät nicht automatisch am HTTP-Payload.
Start/Stopp bleiben unverändert über `PUT /heater/true[/<temp>]` bzw.
`PUT /heater/false` angebunden. Ein erfolgreicher Start bestätigt keine Telemetrie.

Die Stethoskop-Taste und das Diagnosefenster sind ausschließlich bei Autoterm
verfügbar. Bei einem Typwechsel wird ein geöffnetes Autoterm-Fenster geschlossen.

## Belegter Datenweg

Im mitgelieferten `docs/flows.json`, Tab **Heater Webasto** (`b8917b97d5ee0c66`):

- Funktion **temp** (`634cf50f64721a70`): `msg.payload.temp - 50` → global `heattemp`.
- Funktion **voltage** (`94ea8fc746d56e91`): `msg.payload.volts / 1000` → global `heatvolt`.
- Funktion **get heater stats** (`ef5df576.d9abc8`), `GET /heater`
  (`f43c42b0.3680f`), gibt diese Globals auf oberster JSON-Ebene als
  `heattemp` und `heatvolt` aus. CamperUI skaliert sie nicht erneut.
- `parse_heater_json()` → `parse_heater_telemetry()` →
  `state.heater_diagnostics` → Webasto-Karte in `ui_update_climate_tab()`.
  `heattemp` entspricht „Heizer“, `heatvolt` entspricht „Spannung“.

Webasto verwendet keine Felder aus `autoterm1`. Autoterm nutzt weiterhin sein
Unterobjekt bzw. die bisher unterstützte flache Antwort. Die separate Anzeige
der Innen-/Außentemperaturen bleibt unverändert.

## Fehlende Daten und Grenzen

Der VanPi-Flow initialisiert `heattemp` mit `"wait"` und `heatvolt` mit `0`.
Auch der HTTP-Builder liefert bei fehlenden Globals diese Ersatzwerte. Deshalb
bleibt Webasto-Spannung ≤ 5 V wie bisher ohne Messanzeige. Fehlende, ungültige,
veraltete (> 45 s ohne gültige JSON-Antwort) oder nicht mehr verbundene Werte
erscheinen als `--`, niemals als erfundene 0 V/0 °C. Eine tatsächlich numerisch
gelieferte Temperatur 0 °C ist darstellbar. Der vorhandene VanPi-Builder macht
allerdings selbst aus einer globalen Temperatur 0 durch `|| "wait"` ein `"wait"`.

Pro Antwort werden die verfügbaren Felder neu bestimmt. Ein fehlendes Feld darf
keinen früheren Messwert weiter anzeigen. Cache-Daten sind zusätzlich nach
LIVE/DEMO und konfiguriertem Heizungstyp getrennt.

Die 45 s beziehen sich auf den HTTP-Empfang, nicht auf das Alter der Messung im
VanPi. Der Flow liefert hier keine Messzeitstempel oder eindeutige Herkunftskennung.
Veraltete oder von einer anderen Integration beschriebene VanPi-Globals sind
damit nicht zuverlässig erkennbar. Es gibt keinen Rückgriff auf Batteriewerte,
Cerbo-Daten oder erfundene Webasto-Feldnamen.

## Für den betroffenen Nutzer noch benötigt

Im selben WLAN im Browser öffnen:

`http://<VanPi-IP>:1880/heater`

Benötigt werden die vollständigen JSON-Antworten einmal bei ausgeschalteter
Webasto und einmal während sie läuft (nach kurzer Wartezeit), dazu VanPi-Version,
Webasto-Modell/Anbindung und ein Screenshot der zugehörigen VanPi-WebUI-Anzeige.
Relevant sind besonders `heattemp`, `heatvolt`, `heatstatus`, `heatertoggle` und
eventuelle Unterobjekte. Zugangsdaten und persönliche Sensorbezeichnungen können
geschwärzt werden, Struktur, Feldnamen, Werte und Datentypen bitte erhalten.

Der mitgelieferte Flow belegt eine mögliche Datenlieferung, nicht deren Funktion
bei diesem Nutzer. Ob sein VanPi diese Webasto tatsächlich ausliest, ist offen.

## Prüfung

`tools/test_heater_http.py` kompiliert die echten Parser-Hilfsfunktionen und den
Telemetrie-Teil von `parse_heater_json()` mit ArduinoJson. Der Test führt zuvor die
echten Webasto-Funktionen und den HTTP-Builder aus `docs/flows.json` lokal aus.
Geprüft werden unter anderem Skalierung, Null-/Fehlwerte, Teilantworten,
Dezimalzeichen, Aktualität, Quellen-/Typwechsel und bestehende Autoterm-Diagnose.
Die Fixtures sind erzeugte Testdaten, keine Aufnahme vom betroffenen Gerät.
