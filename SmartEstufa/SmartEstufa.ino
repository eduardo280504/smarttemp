#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"

// ====== CONFIGURAÇÃO WI-FI ======
const char* ssid     = "VIVOFIBRA-1731";     // nome da rede (hotspot do teu celular)
const char* password = "Etq8xHMTYZ";         // senha do hotspot

// ====== CONFIGURAÇÃO DHT ======
#define DHTPIN 27          // pino de dados do DHT11 no ESP32 (GPIO27)
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ====== CONFIGURAÇÃO FIREBASE ======
// Exemplo: "https://smart-temphumidity-default-rtdb.firebaseio.com"
const char* firebaseHost = "https://smart-temphumidity-default-rtdb.firebaseio.com";

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Iniciando DHT...");
  dht.begin();

  Serial.println("Conectando ao Wi-Fi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Lê os dados do sensor
  float h = dht.readHumidity();
  float t = dht.readTemperature(); // em Celsius

  if (isnan(h) || isnan(t)) {
    Serial.println("Falha ao ler do DHT11!");
    delay(5000);
    return;
  }

  Serial.print("Temp: ");
  Serial.print(t);
  Serial.print(" °C  |  Umidade: ");
  Serial.print(h);
  Serial.println(" %");

  if (WiFi.status() == WL_CONNECTED) {
    // Monta URL: .../sensor.json
    String url = String(firebaseHost) + "/sensor.json";

    // Monta JSON
    unsigned long ts = millis(); // timestamp simples
    String json = "{";
    json += "\"temperature\":" + String(t, 2) + ",";
    json += "\"humidity\":" + String(h, 2) + ",";
    json += "\"timestamp\":" + String(ts);
    json += "}";

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.PUT(json);

    if (httpResponseCode > 0) {
      Serial.print("Firebase response code: ");
      Serial.println(httpResponseCode);
      String response = http.getString();
      Serial.println("Resposta: " + response);
    } else {
      Serial.print("Erro ao enviar para Firebase: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("Wi-Fi desconectado, tentando reconectar...");
    WiFi.reconnect();
  }

  // Atualiza a cada 5 segundos
  delay(5000);
}
