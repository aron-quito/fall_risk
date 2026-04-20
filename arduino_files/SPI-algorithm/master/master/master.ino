#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <ArduinoJson.h>

// ===============================
// CONFIG WIFI / MQTT
// ===============================
const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";
const char* mqtt_user = "";
const char* mqtt_pass = "";
const char* mqtt_topic = "esp32/mpu6050";

const int SPI_READY_PIN = 4;  // GPIO 4 conectado al READY del slave

WiFiClientSecure espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -5 * 3600);

struct __attribute__((packed)) IncomingSample {
  float ax, ay, az, gx, gy, gz, a_total, temp;
  uint16_t checksum;
};

struct FullSample {
  IncomingSample data;
  char fecha[25];
  unsigned long id;
};

const int MAX_SAMPLES = 50;
FullSample bufferSamples[MAX_SAMPLES];
int bufferIndex = 0;
unsigned long idGlobal = 1;
unsigned long ultimoID = 0;

unsigned long lastPrintTime = 0;
unsigned long sampleCounter = 0;
unsigned long freqTimer = 0;

SPISettings spiSettings(10000, MSBFIRST, SPI_MODE3);

uint16_t calculateChecksum(const IncomingSample& sample) {
  uint16_t checksum = 0;
  const uint8_t* data = reinterpret_cast<const uint8_t*>(&sample);
  
  // Calcular checksum de todos los campos excepto el propio checksum
  for (int i = 0; i < sizeof(IncomingSample) - sizeof(uint16_t); i++) {
    checksum ^= data[i];
    checksum = (checksum << 1) | (checksum >> 15); // Rotación
  }
  
  return checksum;
}

bool validateSample(const IncomingSample& sample) {
  uint16_t calculated = calculateChecksum(sample);
  return calculated == sample.checksum;
}

void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Conectado");
}

void reconnectMQTT() {
  while (!client.connected()) {
    if (client.connect("ESP32Master", mqtt_user, mqtt_pass)) {
      client.subscribe("esp32/comandos");
    } else {
      delay(5000);
    }
  }
}

void sendBatchOverMQTT(FullSample* arr, int len, bool completo) {
  const size_t cap = JSON_ARRAY_SIZE(len) + len * JSON_OBJECT_SIZE(9) + 3000;
  DynamicJsonDocument doc(cap);
  doc["completo"] = completo;
  doc["num_muestras"] = len;
  JsonArray dataArray = doc.createNestedArray("data");

  for (int i = 0; i < len; i++) {
    JsonObject o = dataArray.createNestedObject();
    o["id"] = arr[i].id;
    o["ax"] = arr[i].data.ax;
    o["ay"] = arr[i].data.ay;
    o["az"] = arr[i].data.az;
    o["gx"] = arr[i].data.gx;
    o["gy"] = arr[i].data.gy;
    o["gz"] = arr[i].data.gz;
    o["a_total"] = arr[i].data.a_total;
    o["fecha"] = arr[i].fecha;
    ultimoID = arr[i].id;
  }

  String out;
  serializeJson(doc, out);
  client.publish(mqtt_topic, out.c_str());
  Serial.printf("📤 Lote enviado. Último ID: %lu\n", ultimoID);
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  timeClient.begin();

  pinMode(SPI_READY_PIN, INPUT);  // Handshake

  espClient.setInsecure();
  client.setServer(mqtt_server, 8883);
  client.setBufferSize(10240);

  SPI.begin(18, 19, 23, 5);  // SCK, MISO, MOSI, SS (VSPI pines)
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);

  Serial.println("🚀 MASTER listo");
}

IncomingSample raw = { 0 };

void loop() {
  if (!client.connected()) reconnectMQTT();
  client.loop();
  timeClient.update();


  uint8_t dummy_tx[sizeof(IncomingSample)] = { 0 };
  uint8_t rx_buf[sizeof(IncomingSample)]={0};

  // ESPERA AL SLAVE (HANDSHAKE)
  unsigned long startWait = millis();
  while (digitalRead(SPI_READY_PIN) == LOW && millis() - startWait < 25) {
    delayMicroseconds(100);
  }

  Serial.printf("READY pin: %d\n", digitalRead(SPI_READY_PIN));

  if (digitalRead(SPI_READY_PIN) == HIGH) {
    // Serial.println("Activando CS...");
    digitalWrite(5, LOW);  // Activar CS
    delayMicroseconds(50);  // Estabilización más larga
    
    SPI.beginTransaction(spiSettings);
    SPI.transferBytes(dummy_tx, rx_buf, sizeof(IncomingSample));
    SPI.endTransaction();
    
    delayMicroseconds(50);  // Estabilización después de transferencia
    digitalWrite(5, HIGH);  // Desactivar CS
    
    Serial.println("Transferencia completada");

    memcpy(&raw, rx_buf, sizeof(IncomingSample));
    sampleCounter++;

    // Depuración: mostrar bytes recibidos ANTES de validar
    Serial.printf("RX bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n", 
                  rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3], 
                  rx_buf[4], rx_buf[5], rx_buf[6], rx_buf[7]);

    // Validar checksum
    if (!validateSample(raw)) {
      Serial.printf("❌ Checksum inválido - RX: 0x%04X vs Calculado: 0x%04X\n", 
                    raw.checksum, calculateChecksum(raw));
      return; // Ignorar esta muestra corrupta
    }

    Serial.printf("✅ Checksum válido: 0x%04X\n", raw.checksum);

    // Verificar si los datos son válidos (no todos ceros)
    bool datosValidos = (raw.ax != 0.0 || raw.ay != 0.0 || raw.az != 0.0);
    
    if (datosValidos) {

    // Guardar en buffer
    unsigned long epoch = timeClient.getEpochTime();
    setTime(epoch);
    char buf[25];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             year(), month(), day(), hour(), minute(), second());

    FullSample& s = bufferSamples[bufferIndex];
    s.data = raw;
    s.id = idGlobal++;
    strncpy(s.fecha, buf, 24);
    bufferIndex++;
    } else {
      Serial.println("⚠️ Datos inválidos recibidos (todos ceros)");
    }
  }

  if (bufferIndex >= MAX_SAMPLES) {
    if (client.connected()) {
      sendBatchOverMQTT(bufferSamples, bufferIndex, true);
    }
    bufferIndex = 0;
  }

  if (millis() - lastPrintTime >= 500) {
    lastPrintTime = millis();
    Serial.printf("AX: %.3f | A_total: %.3f | ID: %lu\n", raw.ax, raw.a_total, idGlobal);
  }

  if (millis() - freqTimer >= 1000) {
    Serial.printf("📊 Frecuencia real: %lu Hz\n", sampleCounter);
    sampleCounter = 0;
    freqTimer = millis();
  }

  
}