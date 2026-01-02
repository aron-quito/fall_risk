#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <ArduinoJson.h>

// ========================= CONFIG =========================

// WIFI
const char* ssid = "Kitox";
const char* password = "giorpa4699";

// MQTT
const char* mqtt_server = "12f114da5d6a4b0d93d624aa7d4982ee.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "esp32";
const char* mqtt_pass = "Giorpa469";
const char* mqtt_topic = "esp32/mpu6050";
const char* topic_comandos = "esp32/comandos";

WiFiClientSecure espClient;
PubSubClient client(espClient);
Adafruit_MPU6050 mpu;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -5 * 3600);

// === Calibración ===
const float Accel_X_Offset = 0.000000;
const float Accel_Y_Offset = 0.000000;
const float Accel_Z_Offset = 1.000000;
const float Gyro_X_Offset  = 2.618500;
const float Gyro_Y_Offset  = -4.271000;
const float Gyro_Z_Offset  = -0.144500;

// Muestreo
const unsigned long SAMPLE_INTERVAL_MICROS = 20000;
const int MAX_SAMPLES = 50;
unsigned long lastSampleMicros = 0;

// ========================= NUEVO: ID GLOBAL =========================
unsigned long idGlobal = 1;   // ID inicial
unsigned long ultimoID = 0;   // Último ID confirmado

// ========================= ESTRUCTURA =========================
struct Sample {
  float ax, ay, az;
  float gx, gy, gz;
  float a_total;
  float temp;
  char fecha[25];
  unsigned long id;   // <--- NUEVO CAMPO
};

Sample bufferSamples[MAX_SAMPLES];
int bufferIndex = 0;
String currentSecondKey = "";

// ========================= FUNCIONES =========================
void setup_wifi() {
  Serial.print("Conectando a WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500); Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n❌ No se pudo conectar. Reiniciando...");
    delay(5000); ESP.restart();
  }

  Serial.println("\n✅ WiFi conectado");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando a HiveMQ...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_pass)) {
      Serial.println("✅ Conectado");
      client.subscribe(topic_comandos);
    } else {
      Serial.print("❌ Error rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

String getFormattedDateTimeUTCminus5() {
  unsigned long epoch = timeClient.getEpochTime();
  setTime(epoch);
  char buf[25];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           year(), month(), day(), hour(), minute(), second());
  return String(buf);
}

// ========================= ENVÍO MQTT CON ID =========================
void sendBatchOverMQTT(Sample *arr, int len, bool completo) {

  // Si el paquete es incompleto → saltar IDs al siguiente múltiplo
  if (!completo) {
    unsigned long siguienteBase = ((ultimoID / MAX_SAMPLES) + 1) * MAX_SAMPLES + 1;
    idGlobal = siguienteBase;
    Serial.printf("⏭ Paquete incompleto → Saltando a ID inicial %lu\n", idGlobal);
  }

  const size_t cap = JSON_ARRAY_SIZE(len) + len * JSON_OBJECT_SIZE(11) + 3000;
  DynamicJsonDocument doc(cap);
  JsonObject root = doc.to<JsonObject>();

  root["completo"] = completo;
  root["num_muestras"] = len;
  JsonArray data = root.createNestedArray("data");

  for (int i = 0; i < len; ++i) {
    JsonObject o = data.createNestedObject();
    o["id"] = arr[i].id;          // ← Nuevo campo enviado
    o["ax"] = arr[i].ax;
    o["ay"] = arr[i].ay;
    o["az"] = arr[i].az;
    o["gx"] = arr[i].gx;
    o["gy"] = arr[i].gy;
    o["gz"] = arr[i].gz;
    o["a_total"] = arr[i].a_total;
    o["temp"] = arr[i].temp;
    o["fecha"] = String(arr[i].fecha);

    ultimoID = arr[i].id; // <--- Guardamos el último ID real enviado
  }

  String out;
  serializeJson(root, out);
  client.publish(mqtt_topic, out.c_str());

  Serial.printf("📤 Enviado paquete (%d muestras) → Último ID: %lu\n", len, ultimoID);
}

// ========================= LED / CALLBACK =========================
const int PIN_LED = 2;

void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for (int i = 0; i < length; i++) mensaje += (char)payload[i];
  if (mensaje == "LED_ON") digitalWrite(PIN_LED, HIGH);
  else if (mensaje == "LED_OFF") digitalWrite(PIN_LED, LOW);
}

// ========================= SETUP =========================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  setup_wifi();
  timeClient.begin();
  timeClient.update();

  Wire.begin(21, 22);
  if (!mpu.begin() && !mpu.begin(0x69)) {
    Serial.println("❌ Error MPU6050");
    while (1) delay(1000);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(10240);

  lastSampleMicros = micros();
  Serial.println("✅ Setup completo");
}

// ========================= LOOP =========================
void loop() {
  if (!client.connected()) reconnectMQTT();
  client.loop();
  timeClient.update();

  unsigned long now = micros();
  if (now - lastSampleMicros < SAMPLE_INTERVAL_MICROS) return;
  lastSampleMicros += SAMPLE_INTERVAL_MICROS;

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float ax = a.acceleration.x - Accel_X_Offset;
  float ay = a.acceleration.y - Accel_Y_Offset;
  float az = a.acceleration.z - Accel_Z_Offset;
  float gx = g.gyro.x - Gyro_X_Offset;
  float gy = g.gyro.y - Gyro_Y_Offset;
  float gz = g.gyro.z - Gyro_Z_Offset;
  float a_total = sqrt(ax * ax + ay * ay + az * az);

  String dateStr = getFormattedDateTimeUTCminus5();
  String secondKey = dateStr;

  if (currentSecondKey != "" && secondKey != currentSecondKey) {
    bool completo = (bufferIndex == MAX_SAMPLES);
    if (bufferIndex > 0) sendBatchOverMQTT(bufferSamples, bufferIndex, completo);
    bufferIndex = 0;
    currentSecondKey = secondKey;
  } else if (currentSecondKey == "") currentSecondKey = secondKey;

  // Guardamos muestra con ID
  Sample &s = bufferSamples[bufferIndex];
  s.ax = ax; s.ay = ay; s.az = az;
  s.gx = gx; s.gy = gy; s.gz = gz;
  s.a_total = a_total;
  s.temp = temp.temperature;
  strncpy(s.fecha, currentSecondKey.c_str(), 24);
  s.id = idGlobal++; // ← NUEVA ASIGNACIÓN DE ID

  bufferIndex++;

  if (bufferIndex == MAX_SAMPLES) {
    sendBatchOverMQTT(bufferSamples, bufferIndex, true);
    bufferIndex = 0;
  }
}
