#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"
#include <time.h>   // Para NTP (hora da internet)

// ====== CONFIGURAÇÃO WI-FI ======
const char* ssid     = "VIVOFIBRA-1731";
const char* password = "Etq8xHMTYZ";

// ====== CONFIGURAÇÃO DHT ======
#define DHTPIN 27
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ====== CONFIGURAÇÃO FIREBASE ======
const char* firebaseHost = "https://smart-temphumidity-default-rtdb.firebaseio.com";

// ====== CONFIGURAÇÃO NTP ======
const char* ntpServer = "pool.ntp.org";
// salvando em UTC, o navegador ajusta pro fuso
const long gmtOffset_sec     = 0;
const int  daylightOffset_sec = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\nIniciando DHT...");
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
  Serial.println("Sincronizando hora com NTP...");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Falha ao obter hora da internet!");
  } else {
    Serial.print("Hora atual (UTC): ");
    Serial.println(&timeinfo, "%d/%m/%Y %H:%M:%S");
  }
}

void loop() {
  // ---- Lê DHT ----
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Falha ao ler DHT11!");
    delay(5000);
    return;
  }

  Serial.printf("Temp: %.2f°C | Umidade: %.2f%%\n", t, h);

  if (WiFi.status() == WL_CONNECTED) {

    // ---- Timestamp real da internet (segundos desde 1970) ----
    time_t now;
    time(&now);  
    long ts = (long)now;

    // ---- JSON ----
    String json = "{";
    json += "\"temperature\":" + String(t, 2) + ",";
    json += "\"humidity\":" + String(h, 2) + ",";
    json += "\"timestamp\":" + String(ts);
    json += "}";

    HTTPClient http;

    // ===== 1) Atualiza /sensor (última leitura) =====
    {
      String urlLast = String(firebaseHost) + "/sensor.json";
      http.begin(urlLast);
      http.addHeader("Content-Type", "application/json");

      int codeLast = http.PUT(json);
      Serial.print("[/sensor] Código HTTP: ");
      Serial.println(codeLast);

      if (codeLast <= 0) {
        Serial.println("Erro ao enviar para /sensor");
      }

      http.end();
    }

    // ===== 2) Salva no histórico em /history/<timestamp>.json =====
    {
      // chave = próprio timestamp, ex: /history/1733246500.json
      String urlHist = String(firebaseHost) + String("/history/") + String(ts) + ".json";
      http.begin(urlHist);
      http.addHeader("Content-Type", "application/json");

      int codeHist = http.PUT(json);   // PUT porque agora a chave é única
      Serial.print("[/history] Código HTTP: ");
      Serial.println(codeHist);

      if (codeHist <= 0) {
        Serial.println("Erro ao enviar para /history");
      }

      http.end();
    }

  } else {
    Serial.println("Wi-Fi caiu! Tentando reconectar...");
    WiFi.reconnect();
  }

  // intervalo entre leituras 1 min
  delay(60000);
}
