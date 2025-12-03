#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"
#include <time.h>   // Para NTP (hora da internet)

// ====== CONFIGURAÇÃO WI-FI ======
const char* ssid     = "VIVOFIBRA-1731";
const char* password = "Etq8xHMTYZ";

// ====== CONFIGURAÇÃO DHT ======
#define DHTPIN 27
#define DHTTYPE DHT22     // <<< SENSOR ATUAL
DHT dht(DHTPIN, DHTTYPE);

// ====== CONFIGURAÇÃO FIREBASE ======
const char* firebaseHost = "https://smart-temphumidity-default-rtdb.firebaseio.com";

// ====== CONFIGURAÇÃO NTP ======
const char* ntpServer = "pool.ntp.org";
// Salva em UTC, front converte para fuso correto
const long gmtOffset_sec      = 0;
const int  daylightOffset_sec = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\nIniciando DHT22...");
  dht.begin();

  Serial.println("Conectando ao Wi-Fi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // ---- CONFIGURA HORA VIA NTP ----
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Sincronizando hora NTP...");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Falha ao obter hora da internet!");
  } else {
    Serial.print("Hora atual (UTC): ");
    Serial.println(&timeinfo, "%d/%m/%Y %H:%M:%S");
  }
}

void loop() {

  // ---- Lê DHT22 ----
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Falha ao ler DHT22!");
    delay(60000);
    return;
  }

  Serial.printf("Temperatura: %.2f°C | Umidade: %.2f%%\n", t, h);

  if (WiFi.status() == WL_CONNECTED) {

    // ---- Timestamp real ----
    time_t now;
    time(&now);
    long ts = (long)now;  // segundos

    // ---- Monta JSON ----
    String json = "{";
    json += "\"temperature\":" + String(t, 2) + ",";
    json += "\"humidity\":"   + String(h, 2) + ",";
    json += "\"timestamp\":"  + String(ts);
    json += "}";

    HTTPClient http;

    // ================================
    // 1) ATUALIZA /sensor (PUT)
    // ================================
    {
      String url = String(firebaseHost) + "/sensor.json";
      http.begin(url);
      http.addHeader("Content-Type", "application/json");

      int code = http.PUT(json);
      Serial.print("[/sensor] Código HTTP: ");
      Serial.println(code);

      http.end();
    }

    // =============================================
    // 2) ADICIONA NO HISTÓRICO: /history/<timestamp>
    // =============================================
    {
      String url = String(firebaseHost) + "/history/" + String(ts) + ".json";
      http.begin(url);
      http.addHeader("Content-Type", "application/json");

      int code = http.PUT(json);
      Serial.print("[/history] Código HTTP: ");
      Serial.println(code);

      http.end();
    }

  } else {
    Serial.println("Wi-Fi desconectado! Tentando reconectar...");
    WiFi.reconnect();
  }

  // ---- INTERVALO DE 1 MINUTO ----
  delay(60000);
}
