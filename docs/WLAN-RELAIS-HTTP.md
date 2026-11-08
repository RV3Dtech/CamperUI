# WLAN-Relais: Ausschalten und HTTP-Body (RV3D.93)

## Befund

CamperUI sendete alle PUT-Befehle mit `Content-Type: application/json` und dem
Body `{}`. Im mitgelieferten `docs/flows.json` verarbeitet Function-Node
**check input and switch state**, ID `766532c5bcc4ca2d`, den Endpunkt
`PUT /wrelay/:input/:value` so:

```js
if (msg.req.params.value === "true" || msg.payload.includes("state: true")) {
    // Einschalten
} else if (msg.req.params.value === "false") {
    // Ausschalten
}
```

Node-RED parst den JSON-Body als Objekt. Beim Einschalten wird `.includes()` durch
die Kurzschlussauswertung übersprungen. Beim Ausschalten wird die Methode auf dem
Objekt aufgerufen: `msg.payload.includes is not a function`. Damit fehlen sowohl
Weiterleitung zum Relaisschalter als auch HTTP-Antwort; CamperUI läuft in seinen
Read-Timeout (ESP32 HTTPClient `-11`). Nicht jeder `-11` hat diese Ursache.

Die VanPi-WebUI umgeht in diesem Referenz-Flow die HTTP-Eingangsfunktion und
steuert den Relaisschalter direkt. Ihr funktionierendes Ausschalten widerspricht
dem Befund deshalb nicht. `GET /wrelay` liest gespeicherte VanPi-Globals; diese
sind keine unabhängige Bestätigung des tatsächlichen WLAN-Gerätezustands.

Der Bericht nennt VanPi 2.1.0. Ob dort exakt dieser Function-Code und welche
Node-RED-Version installiert sind, ist noch nicht bestätigt.

## Begrenzter CamperUI-Workaround

Nur PUT-Befehle, deren Pfad mit `/wrelay/` beginnt, verwenden jetzt:

```http
Content-Type: text/plain; charset=utf-8

{}
```

Der Body bleibt absichtlich nicht leer. Node-RED liefert hier eine Zeichenkette
`"{}"`; `.includes("state: true")` ist damit sicher ausführbar und ergibt false.
Die gewünschte Schaltstellung bleibt wie bisher im URL-Parameter `true`/`false`.
Alle anderen PUT-Endpunkte behalten JSON und `{}`. Verbindungs-Timeout 400 ms,
Antwort-Timeout 1500 ms, Befehlsqueue, Touchfilter und Zustandslogik sind unverändert.
Es wurden keine automatischen Wiederholungen hinzugefügt.

Ein bloß leerer Body mit weiter gesetztem `application/json` ist keine Lösung:
im geprüften Node-RED 4.1.0 entstand auch daraus ein Objekt und derselbe Fehler.

## Saubere spätere Korrektur im VanPi-Flow

In der genannten Funktion nur den ersten Bedingungsausdruck absichern:

```js
if (msg.req.params.value === "true" ||
    (typeof msg.payload === "string" && msg.payload.includes("state: true"))) {
    // Bestehenden Einschaltzweig unverändert lassen.
}
else if (msg.req.params.value === "false") {
    // Bestehenden Ausschaltzweig unverändert lassen.
}
```

So bleiben URL-Befehle mit Objekt-/Leer-Payload und bisher unterstützte Text-Bodys
verträglich. Dies ist nur dokumentiert: **Weder der Referenz-Flow noch ein laufender
VanPi wurden verändert.** Der CamperUI-Workaround funktioniert auch mit dieser
Typprüfung und kann alte, noch nicht korrigierte VanPi-Flows weiter bedienen.

## Reproduzierbarer lokaler HTTP-Test

Getestet mit **Node-RED 4.1.0 / Node.js 24.22.1**, nur auf `127.0.0.1` mit zufälligem
Port. Die Test-Runtime enthält echte HTTP-In-/Function-/HTTP-Response-Nodes und
führt den unveränderten Referenz-Code sowie dessen unveränderten **set answer**-
Node aus. Der erste Ausgang endet in einer speicherbasierten Befehlssenke; es
gibt keine Geräteverbindung, MQTT-Nodes oder HTTP-Ausgänge.

`tools/test_wrelay_http.py` kompiliert zuerst die echte `http_put()`-Funktion mit
einem HTTPClient-Mock. Die so aufgezeichneten tatsächlichen Header/Body-Paare
werden anschließend als echte HTTP-Anfragen an Node-RED gesendet. Damit wird
auch das Body-Parsing geprüft, nicht nur eine Function mit künstlichem Payload.

Bestanden:

- Bisheriges JSON `{}`: Payload ist Objekt; Ein wird weitergeleitet und beantwortet,
  Aus wirft `.includes`, erzeugt keinen Befehl und keine HTTP-Antwort.
- Leeres JSON: tatsächlichen Payload-Typ und ausbleibende Antwort erfasst.
- Text-Workaround: alle Kanäle 1–8 jeweils Ein und Aus, Payload als String `"{}"`,
  genau ein weitergeleiteter boolescher Befehl und HTTP 200 pro Anfrage.
- Andere PUT-Endpunkte behalten JSON `{}`; auch ähnlich benannte Pfade wie
  `/wrelayiot/` erhalten die Sonderbehandlung nicht.
- Demo-/WLAN-/Zieladressprüfungen und beide CamperUI-Timeouts bleiben erhalten.

Zum Wiederholen Node-RED separat in einen Testordner installieren (z. B.
`npm install --prefix <Testordner> --ignore-scripts node-red@4.1.0`), dann:

```text
python tools/test_wrelay_http.py --compiler <nativer C++-Compiler oder zig.exe> --node-red <Testordner>/node_modules/node-red
```

Der Test startet keinen vorhandenen Node-RED und beendet seine eigene Runtime.
Hintergrund zur echten Body-Verarbeitung: [Node-RED HTTP-In-Implementierung](https://github.com/node-red/node-red/blob/4.1.0/packages/node_modules/%40node-red/nodes/core/network/21-httpin.js).

## Noch am Gerät prüfen

Mit der korrigierten CamperUI beide Schaltstellungen an abcklaus' VanPi 2.1.0
testen und den tatsächlichen Verbraucher bzw. das Relais beobachten. Eine
HTTP-200-Antwort oder ein gespeicherter GET-Wert beweist noch keine physische
Schaltung. Falls Aus weiterhin scheitert: vollständiges CamperUI-PUT-Log,
Node-RED-Fehlerlog, dessen Versionsnummer und den installierten Function-Code
vergleichen. Kein blindes Wiederholen bei ausbleibender Antwort.
