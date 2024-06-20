#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <string.h>
// Obtenido con el @Botfather
const String tokenTelegram = "7338302845:AAGJZKwiphPwhpbQCdA1KqlqRCyzJbiu0uo";
const String ID_Chat = "6112934258";
// Red WiFi para conectarse
const char *NOMBRE_RED_WIFI = "Galaxy S10+df50";
const char *CLAVE_RED_WIFI = "ifin6468";

byte cont = 0;
byte max_intentos = 50;
byte mensaje_intentos = 5;
int TRIG = 15;
int ECO = 13;
int DURACION;
int DISTANCIA;
int BUZZER = 14;
int PIR = 12;
int pirValue = 0;
int LED = 2;
int ultimoIdDeActualizacion = 0;
int limite = 1;
bool alarmaActivada = true;
bool mensajeAlarmaDesactivadaEnviado = false;
bool movimientoDetectado = false;
unsigned long tiempoUltimaAlarma = 0;
unsigned long intervaloReactivacion = 5000;

void enviarMensajeTelegram(const String &mensaje) {
  std::unique_ptr<BearSSL::WiFiClientSecure> clienteWifi(new BearSSL::WiFiClientSecure);
  clienteWifi->setInsecure();

  HTTPClient clienteHttp;
  String url = "https://api.telegram.org/bot" + tokenTelegram + "/sendMessage";
  String cargaUtil = "{\"chat_id\":\"" + ID_Chat + "\",\"text\":\"" + mensaje + "\"}";

  if (clienteHttp.begin(*clienteWifi, url)) {
    clienteHttp.addHeader("Content-Type", "application/json", false, true);
    int httpCode = clienteHttp.POST(cargaUtil);

    if (httpCode > 0) {
      String respuestaDelServidor = clienteHttp.getString();
      if (httpCode == HTTP_CODE_OK) {
        Serial.println("Petición OK. Respuesta: " + respuestaDelServidor);
      } else {
        Serial.printf("Error en el servidor. Código HTTP: %d. Respuesta: %s\n", httpCode, respuestaDelServidor.c_str());
      }
    } else {
      Serial.printf("Error haciendo petición: %s\n", clienteHttp.errorToString(httpCode).c_str());
    }
    clienteHttp.end();
  } else {
    Serial.println("Error con clienteHttp.begin");
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("\n");
  pinMode(LED, OUTPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECO, INPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(PIR, INPUT);
  WiFi.mode(WIFI_STA);
  WiFi.begin(NOMBRE_RED_WIFI, CLAVE_RED_WIFI);
  Serial.println("Conectando...");
  Serial.println(NOMBRE_RED_WIFI);
  // WiFi.mode(WIFI_STA);
  WiFi.begin(NOMBRE_RED_WIFI, CLAVE_RED_WIFI);
  while (WiFi.status() != WL_CONNECTED and cont < max_intentos) {
    cont++;
    delay(500);
    Serial.print(".");
  }

  if (cont < max_intentos) {
    Serial.println("Conectado a la red");
    Serial.println(WiFi.SSID());
    Serial.println(WiFi.localIP());
    Serial.println(WiFi.macAddress());

    enviarMensajeTelegram("Alarma activada");
  } else {
    Serial.println("Error de conexión");
  }
}

void loop() {
  digitalWrite(TRIG, HIGH);
  delay(1);
  digitalWrite(TRIG, LOW);
  DURACION = pulseIn(ECO, HIGH);
  DISTANCIA = DURACION / 58.2;
  pirValue = digitalRead(PIR);
  Serial.print("Distancia: \n");
  Serial.println(DISTANCIA);


  if (alarmaActivada) {
    if (pirValue == HIGH || (DISTANCIA <= 20)) {
      if (!movimientoDetectado) {
        enviarMensajeTelegram("Movimiento detectado!!");
        Serial.print(" cm, Movimiento detectado PIR: ");
        Serial.println(pirValue);
        digitalWrite(LED, HIGH);
        tone(BUZZER, 1000);
        pirValue = 0;
        movimientoDetectado = true;
      }

    } else {
      digitalWrite(LED, LOW);
      noTone(BUZZER);
      movimientoDetectado = false;
    }
  } else if (!mensajeAlarmaDesactivadaEnviado) {
    enviarMensajeTelegram("Alarma desactivada");
    mensajeAlarmaDesactivadaEnviado = true;
    alarmaActivada = true;
    enviarMensajeTelegram("Alarma reactivada");
  }

  if ((WiFi.status() != WL_CONNECTED)) {

    Serial.println("No hay WiFi");
    return;
  }
  std::unique_ptr<BearSSL::WiFiClientSecure> clienteWifi(new BearSSL::WiFiClientSecure);

  clienteWifi->setInsecure();

  HTTPClient clienteHttp;

  String url = "https://api.telegram.org/bot" + tokenTelegram + "/getUpdates?limit=" + String(limite);


  if (ultimoIdDeActualizacion != 0) {
    url += "&offset=" + String(ultimoIdDeActualizacion + 1);
  }

  if (!clienteHttp.begin(*clienteWifi, url)) {

    Serial.println("Error con clienteHttp.begin");
  }
  Serial.println("Haciendo petición a " + url);
  clienteHttp.addHeader("Content-Type", "application/json", false, true);
  int httpCode = clienteHttp.GET();
  if (httpCode > 0) {
    String respuestaDelServidor = clienteHttp.getString();
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("Petición OK");
      DynamicJsonDocument documentoJson(2048);
      DeserializationError errorAlDecodificarJson = deserializeJson(documentoJson, respuestaDelServidor);
      if (errorAlDecodificarJson) {
        Serial.print(F("Error al parsear JSON: "));
        Serial.println(errorAlDecodificarJson.c_str());

        return;
      }
      JsonArray actualizaciones = documentoJson["result"].as<JsonArray>();
      for (JsonObject actualizacion : actualizaciones) {
        int idUsuarioRemitente = actualizacion["message"]["from"]["id"];
        int idActualizacion = actualizacion["update_id"];
        if (actualizacion["message"].containsKey("text")) {
          const char *contenidoDelMensajeRecibido = actualizacion["message"]["text"];
          // Imprime los valores en el serial
          Serial.print("Id remitente: ");
          Serial.println(idUsuarioRemitente);
          Serial.print("Contenido del mensaje: ");
          Serial.println(contenidoDelMensajeRecibido);
          if (strcmp(contenidoDelMensajeRecibido, "/desactivar") == 0) {
            alarmaActivada = false;
            digitalWrite(LED, LOW);
            noTone(BUZZER);
            Serial.println("Alarma desactivada");
            mensajeAlarmaDesactivadaEnviado = false;

            delay(5000);
            alarmaActivada = true;
            Serial.println("Alarma reactivada");
          }
        }
        ultimoIdDeActualizacion = idActualizacion;
      }
    } else {
      Serial.printf(
        "Petición realizada pero hubo un error en el servidor. Código HTTP: %d. Respuesta: %s\n",
        httpCode,
        respuestaDelServidor.c_str());
    }
  } else {
    // Petición Ok pero código no es 200
    Serial.printf("Error haciendo petición: %s\n", clienteHttp.errorToString(httpCode).c_str());
  }
  clienteHttp.end();

  if (!alarmaActivada && mensajeAlarmaDesactivadaEnviado) {
    if (millis() - tiempoUltimaAlarma >= intervaloReactivacion) {
      alarmaActivada = true;
      enviarMensajeTelegram("Alarma reactivada");
      Serial.println("Alarma reactivada");
      mensajeAlarmaDesactivadaEnviado = false;
    }
  }
}