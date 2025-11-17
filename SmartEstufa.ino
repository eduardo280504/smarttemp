// Smart Estufa - ESP32 + DHT11
// DHT11 DATA -> GPIO27 (exemplo), Soil -> GPIO34 (ADC), Relay Pump -> GPIO26, Relay Fan -> GPIO25
// Relés assumidos ACTIVE LOW (LOW = ON). Ajuste se o seu módulo for diferente.

#include <DHT.h>

#define DHTPIN 27       // pino DATA do DHT
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// pinos
const int soilPin = 34;   // ADC1_CH6 (entrada analógica)
const int relayPump = 26; // saída digital
const int relayFan  = 25; // saída digital

// calibração do solo (LEIA e ajuste no campo)
int soilDry = 3500; // exemplo: valor com sensor no ar (meça com Serial e atualize)
int soilWet = 1500; // exemplo: valor com sensor em água (meça e atualize)

// limiares / histerese
const int soilOn = 40;   // % <= liga rega
const int soilOff = 60;  // % >= desliga rega
const float tempOn = 30.0;  // °C >= liga vent
const float tempOff = 28.0; // °C <= desliga vent

// controle rega
const unsigned long wateringDuration = 10000UL; // ms
const int maxWateringsPerHour = 6;

// temporizadores / estados
unsigned long lastRead = 0;
const unsigned long readInterval = 2000UL; // DHT11: use >=1000-2000ms; aqui 2s
bool watering = false;
unsigned long wateringStart = 0;
int wateringsCount = 0;
unsigned long hourWindowStart = 0;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== Smart Estufa (ESP32 + DHT11) iniciado ===");

  dht.begin();

  pinMode(relayPump, OUTPUT);
  pinMode(relayFan, OUTPUT);
  // relés OFF (active-low -> HIGH = OFF)
  digitalWrite(relayPump, HIGH);
  digitalWrite(relayFan, HIGH);

  // opcional: ajusta atenuação do ADC para melhor faixa (0-3.3V)
  // analogSetPinAttenuation(soilPin, ADC_11db); // disponível em algumas builds ESP32 cores
  hourWindowStart = millis();

  Serial.println("Faça calibração do sensor de solo (meça raw com o sensor no ar e em água).");
}

void loop() {
  unsigned long now = millis();

  // reset contador por hora
  if (now - hourWindowStart >= 3600000UL) {
    wateringsCount = 0;
    hourWindowStart = now;
    Serial.println("[INFO] Janela de 1h resetada - contador de regas zerado");
  }

  if (now - lastRead >= readInterval) {
    lastRead = now;

    // leitura DHT
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
      Serial.println("[ERRO] Leitura DHT inválida");
    } else {
      Serial.print("Temp: "); Serial.print(t); Serial.print(" °C | HumAr: ");
      Serial.print(h); Serial.println(" %");
    }

    // leitura do solo (ADC 0..4095)
    int rawSoil = analogRead(soilPin);
    // converte para 0..100% usando calibração (soilDry > soilWet)
    int soilPercent = 0;
    if (soilDry != soilWet) {
      float pct = (float)(rawSoil - soilWet) / (float)(soilDry - soilWet) * 100.0;
      soilPercent = (int) round(pct);
    }
    soilPercent = constrain(soilPercent, 0, 100);

    Serial.print("Soil raw: "); Serial.print(rawSoil);
    Serial.print(" | Soil % (estim): "); Serial.print(soilPercent); Serial.println(" %");

    // controle ventilador com histerese
    if (!isnan(t)) {
      if (t >= tempOn) {
        digitalWrite(relayFan, LOW); // ON (active-low)
        Serial.println("Ventilador: ON (temp alta)");
      } else if (t <= tempOff) {
        digitalWrite(relayFan, HIGH); // OFF
        Serial.println("Ventilador: OFF (temp normal)");
      }
    }

    // lógica de rega com histerese e limite por hora
    if (!watering) {
      if (soilPercent <= soilOn && wateringsCount < maxWateringsPerHour) {
        watering = true;
        wateringStart = now;
        digitalWrite(relayPump, LOW);
        wateringsCount++;
        Serial.print("Bomba: INICIADA (contador regas = ");
        Serial.print(wateringsCount); Serial.println(")");
      } else if (soilPercent <= soilOn && wateringsCount >= maxWateringsPerHour) {
        Serial.println("[INFO] Limite de regas por hora atingido - não iniciando nova rega");
      }
    }

    if (watering) {
      if (now - wateringStart >= wateringDuration) {
        digitalWrite(relayPump, HIGH);
        watering = false;
        Serial.println("Bomba: PARADA (tempo atingido)");
      } else {
        Serial.print("Bomba regando... ms restantes: ");
        Serial.println(wateringDuration - (now - wateringStart));
      }
    }
  }

  // aqui você pode inserir envio MQTT/HTTP, checagem de botões, OTA, etc.
}
