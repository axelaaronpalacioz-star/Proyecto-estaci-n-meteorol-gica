#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <math.h>

// ============ WIFI ============
// Sustituye estos datos por los de tu red Wi-Fi.
const char* ssid = "Mega_2.4G_FF44";
const char* password = "GQNyHd5P";

// ============ SERVIDOR WEB ============
WebServer server(80);
// ============ PINES ESP32 ============
const int pinPIR = 27;
const int pinLM35 = 34;
const int pinLuz = 35;
const int pinSuelo = 32;

const int trigPin = 5;
const int echoPin = 18;

#define DHTPIN 16
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// ============ CALIBRACION SUELO ============
// Valores de ejemplo: calibrar con tu sensor.
const int sueloSeco = 3500;
const int sueloHumedo = 1400;

// ============ VARIABLES DE MEDICION ============
int movimiento = 0;
int lecturaLuz = 0;
int lecturaSuelo = 0;
int humedadSuelo = 0;

float temperatura = NAN;
float humedadAmbiental = NAN;
float distancia = -1;

unsigned long ultimaLectura = 0;
const unsigned long intervaloLectura = 2000;

unsigned long ultimoIntentoWiFi = 0;

// ============ PAGINA HTML ============
const char paginaHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport"
        content="width=device-width, initial-scale=1">
  <title>Monitor ambiental ESP32</title>

  <style>
    * { box-sizing: border-box; }

    body {
      margin: 0;
      padding: 20px;
      font-family: Arial, sans-serif;
      background: #edf2f7;
      color: #1e293b;
    }

    header {
      text-align: center;
      margin: 10px auto 25px;
    }

    h1 { font-size: 27px; }
    header p { color: #64748b; }

    .estado {
      text-align: center;
      padding: 12px;
      margin-bottom: 18px;
      background: white;
      border-radius: 12px;
    }

    .grid {
      max-width: 900px;
      margin: auto;
      display: grid;
      grid-template-columns:
        repeat(auto-fit, minmax(220px, 1fr));
      gap: 15px;
    }

    .tarjeta {
      background: white;
      padding: 22px;
      border-radius: 14px;
      box-shadow: 0 3px 12px #00000010;
    }

    .tarjeta h2 {
      font-size: 17px;
      color: #475569;
      margin-top: 0;
    }

    .valor {
      font-size: 30px;
      font-weight: bold;
      margin: 12px 0;
      overflow-wrap: anywhere;
    }

    .detalle {
      color: #64748b;
      font-size: 13px;
    }

    footer {
      text-align: center;
      margin-top: 25px;
      color: #64748b;
      font-size: 13px;
    }
  </style>
</head>

<body>
  <header>
    <h1>Monitor ambiental</h1>
    <p>Lecturas de sensores mediante ESP32</p>
  </header>

  <div class="estado">
    Estado: <strong id="conexion">Conectando...</strong>
    <br><small id="actualizacion">Esperando datos</small>
  </div>

  <main class="grid">

    <section class="tarjeta">
      <h2>Movimiento PIR</h2>
      <div class="valor" id="movimiento">--</div>
      <div class="detalle">Presencia de movimiento</div>
    </section>

    <section class="tarjeta">
      <h2>Temperatura LM35</h2>
      <div class="valor" id="temperatura">--</div>
      <div class="detalle">Temperatura en grados Celsius</div>
    </section>

    <section class="tarjeta">
      <h2>Nivel de luz</h2>
      <div class="valor" id="luz">--</div>
      <div class="detalle">Lectura ADC (0 a 4095)</div>
    </section>

    <section class="tarjeta">
      <h2>Humedad del suelo</h2>
      <div class="valor" id="suelo">--</div>
      <div class="detalle">Porcentaje estimado</div>
    </section>

    <section class="tarjeta">
      <h2>Distancia ultrasónica</h2>
      <div class="valor" id="distancia">--</div>
      <div class="detalle">Distancia en centímetros</div>
    </section>

    <section class="tarjeta">
      <h2>Humedad ambiental DHT11</h2>
      <div class="valor" id="ambiente">--</div>
      <div class="detalle">Humedad relativa</div>
    </section>

  </main>

  <footer>
    Página alojada directamente en el ESP32.
    <br>Los datos se actualizan cada 2 segundos.
  </footer>

  <script>
    function mostrar(id, valor) {
      document.getElementById(id).textContent = valor;
    }

    async function actualizar() {
      try {
        const respuesta = await fetch('/api', {
          cache: 'no-store'
        });

        if (!respuesta.ok) {
          throw new Error('Error HTTP');
        }

        const d = await respuesta.json();

        mostrar('movimiento',
          d.movimiento ? 'Detectado' : 'No detectado');

        mostrar('temperatura',
          d.temperatura === null
            ? 'Error de lectura'
            : d.temperatura.toFixed(1) + ' °C');

        mostrar('luz', d.luz);

        mostrar('suelo', d.humedadSuelo + ' %');

        mostrar('distancia',
          d.distancia < 0
            ? 'Sin eco'
            : d.distancia.toFixed(1) + ' cm');

        mostrar('ambiente',
          d.humedadAmbiental === null
            ? 'Error de lectura'
            : d.humedadAmbiental.toFixed(1) + ' %');

        mostrar('conexion', 'Página conectada');
        mostrar('actualizacion',
          'Última actualización: ' +
          new Date().toLocaleTimeString());

      } catch (error) {
        mostrar('conexion', 'Esperando al ESP32...');
      }
    }

    actualizar();
    setInterval(actualizar, 2000);
  </script>
</body>
</html>
)rawliteral";

// ============ LEER DISTANCIA ============
float leerDistancia() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duracion =
    pulseIn(echoPin, HIGH, 30000UL);

  if (duracion == 0) {
    return -1;
  }

  return duracion * 0.0343f / 2.0f;
}

// ============ LEER SENSORES ============
void leerSensores() {
  movimiento = digitalRead(pinPIR);

  // LM35: aproximadamente 10 mV por grado Celsius.
  uint32_t mv = analogReadMilliVolts(pinLM35);
  temperatura = mv / 10.0f;

  lecturaLuz = analogRead(pinLuz);
  lecturaSuelo = analogRead(pinSuelo);

  humedadSuelo = map(
    lecturaSuelo,
    sueloSeco,
    sueloHumedo,
    0,
    100
  );

  humedadSuelo = constrain(humedadSuelo, 0, 100);

  distancia = leerDistancia();

  // El DHT11 necesita tiempo entre lecturas.
  float h = dht.readHumidity();

  if (isnan(h)) {
    humedadAmbiental = NAN;
  } else {
    humedadAmbiental = h;
  }

  // Monitor serie
  Serial.println("\n----- MEDICIONES -----");

  Serial.print("Movimiento: ");
  Serial.println(movimiento ? "Detectado" : "No detectado");

  Serial.print("Temperatura: ");
  Serial.print(temperatura, 1);
  Serial.println(" C");

  Serial.print("Luz ADC: ");
  Serial.println(lecturaLuz);

  Serial.print("Lectura suelo: ");
  Serial.println(lecturaSuelo);

  Serial.print("Humedad suelo: ");
  Serial.print(humedadSuelo);
  Serial.println("%");

  Serial.print("Distancia: ");
  if (distancia < 0) {
    Serial.println("Sin eco");
  } else {
    Serial.print(distancia, 1);
    Serial.println(" cm");
  }

  Serial.print("Humedad ambiental: ");
  if (isnan(humedadAmbiental)) {
    Serial.println("Error");
  } else {
    Serial.print(humedadAmbiental, 1);
    Serial.println("%");
  }
}

// ============ CONEXION WIFI ============

void conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println();
  Serial.println("Intentando conectar al Wi-Fi...");
  Serial.println("Comprueba que la red sea de 2.4 GHz.");

  unsigned long inicio = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - inicio < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WIFI CONECTADO");
    Serial.print("Direccion IP: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("NO SE PUDO CONECTAR AL WIFI");
    Serial.println("Revisa nombre, contrasena y red de 2.4 GHz.");
  }
}


// ============ CONFIGURAR SERVIDOR ============

void configurarServidor() {

  // Pagina principal
  server.on("/", HTTP_GET, []() {
    server.send_P(
      200,
      "text/html; charset=utf-8",
      paginaHTML
    );
  });

  // Datos de los sensores en formato JSON
  server.on("/api", HTTP_GET, []() {

    String json = "{";

    json += "\"movimiento\":";
    json += String(movimiento);

    json += ",\"temperatura\":";
    if (isnan(temperatura)) {
      json += "null";
    } else {
      json += String(temperatura, 2);
    }

    json += ",\"luz\":";
    json += String(lecturaLuz);

    json += ",\"lecturaSuelo\":";
    json += String(lecturaSuelo);

    json += ",\"humedadSuelo\":";
    json += String(humedadSuelo);

    json += ",\"distancia\":";
    json += String(distancia, 2);

    json += ",\"humedadAmbiental\":";
    if (isnan(humedadAmbiental)) {
      json += "null";
    } else {
      json += String(humedadAmbiental, 2);
    }

    json += "}";

    server.send(
      200,
      "application/json; charset=utf-8",
      json
    );
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "Pagina no encontrada");
  });

  server.begin();
  Serial.println("Servidor web iniciado");
}


// ============ SETUP ============
void setup() {
  Serial.begin(115200);

  pinMode(pinPIR, INPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  analogReadResolution(12);

  analogSetPinAttenuation(pinLM35, ADC_11db);
  analogSetPinAttenuation(pinLuz, ADC_11db);
  analogSetPinAttenuation(pinSuelo, ADC_11db);

  dht.begin();

  conectarWiFi();
  configurarServidor();

  // Obtener las primeras lecturas
  leerSensores();
  ultimaLectura = millis();
}

// ============ LOOP ============
void loop() {
  unsigned long ahora = millis();

  server.handleClient();

  if (ahora - ultimaLectura >= intervaloLectura) {
    ultimaLectura = ahora;
    leerSensores();
  }

  static bool conectadoAntes = false;
  bool conectadoAhora = (WiFi.status() == WL_CONNECTED);

  if (conectadoAhora && !conectadoAntes) {
    Serial.println("Wi-Fi conectado correctamente.");
    Serial.print("Abre en tu celular: http://");
    Serial.println(WiFi.localIP());
  }

  if (!conectadoAhora && conectadoAntes) {
    Serial.println("Se perdio la conexion Wi-Fi.");
  }

  conectadoAntes = conectadoAhora;

  if (!conectadoAhora &&
      ahora - ultimoIntentoWiFi >= 10000) {
    ultimoIntentoWiFi = ahora;
    Serial.println("Intentando reconectar al Wi-Fi...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
  }
}