#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h> // <-- 1. NUEVA LIBRERÍA DE SEGURIDAD
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <time.h>
#include <sys/time.h>

// ================= PROTOTIPOS DE FUNCIONES =================
void setup_wifi();
void reconnect();
void syncTime();
void readSensorTask(void * parameter);

// ================= CONFIGURACIÓN =================
const char* ssid = "";       // Pon el nombre de tu red Wi-Fi
const char* password = ""; // Tu contraseña Wi-Fi

// Configuración MQTT (HiveMQ Cloud)
// NOTA: Reemplaza con tu "Cluster URL" si es diferente a broker.hivemq.com
const char* mqtt_server = "12f114da5d6a4b0d93d624aa7d4982ee.s1.eu.hivemq.cloud"; 
const int mqtt_port = 8883; // Puerto seguro obligatorio

// <-- 2. AÑADIDAS LAS CREDENCIALES MQTT
const char* mqtt_user = "esp32";
const char* mqtt_password = "Giorpa469";
const char* mqtt_topic = "esp32/mpu6050";

// Configuración NTP (Tiempo)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -18000; // Ajusta a tu zona horaria (ej. Lima/Bogotá = -18000)
const int   daylightOffset_sec = 0;

// ================= OBJETOS =================
WiFiClientSecure espClient; // <-- 3. CAMBIO A CLIENTE SEGURO
PubSubClient client(espClient);
Adafruit_MPU6050 mpu;

// Estructura para almacenar un registro
struct SensorData {
  unsigned long id;
  int hour, min, sec, ms;
  float ax, ay, az, gx, gy, gz;
};

// El Buffer para 50 datos
const int BATCH_SIZE = 50;
SensorData buffer[BATCH_SIZE];
volatile int bufferIndex = 0;
unsigned long globalId = 0;

// ================= FUNCIONES AUXILIARES =================
void setup_wifi() {
  Serial.print("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println(" Conectado!");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando a MQTT (Seguro)...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    
    // <-- 4. CONECTAR USANDO USUARIO Y CONTRASEÑA
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println(" ¡Conectado!");
    } else {
      Serial.print(" Falló, rc=");
      Serial.print(client.state());
      Serial.println(" Reintentando en 2s...");
      delay(2000);
    }
  }
}

void syncTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  Serial.print("Sincronizando reloj NTP...");
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" Hora sincronizada.");
}

// ================= TAREA DE LECTURA (CORE 1) =================
void readSensorTask(void * parameter) {
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = 20 / portTICK_PERIOD_MS; // 20 ms = 50 Hz

  xLastWakeTime = xTaskGetTickCount();

  for(;;) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* timeinfo = localtime(&tv.tv_sec);

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    if (bufferIndex < BATCH_SIZE) {
      buffer[bufferIndex].id = globalId++;
      buffer[bufferIndex].hour = timeinfo->tm_hour;
      buffer[bufferIndex].min = timeinfo->tm_min;
      buffer[bufferIndex].sec = timeinfo->tm_sec;
      buffer[bufferIndex].ms = tv.tv_usec / 1000;
      
      buffer[bufferIndex].ax = a.acceleration.x;
      buffer[bufferIndex].ay = a.acceleration.y;
      buffer[bufferIndex].az = a.acceleration.z;
      buffer[bufferIndex].gx = g.gyro.x;
      buffer[bufferIndex].gy = g.gyro.y;
      buffer[bufferIndex].gz = g.gyro.z;
      
      bufferIndex++;
    }

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000); // Fast Mode I2C

  setup_wifi();
  
  // <-- 5. EVITAR ERROR DE CERTIFICADO SSL/TLS
  espClient.setInsecure(); 

  syncTime();

  client.setServer(mqtt_server, mqtt_port);

  if (!mpu.begin()) {
    Serial.println("No se encontró el MPU6050");
    while (1) { delay(10); }
  }
  
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);

  xTaskCreatePinnedToCore(
    readSensorTask,   
    "TaskSensor",     
    10000,            
    NULL,             
    1,                
    NULL,             
    1                 
  );
}

// ================= LOOP PRINCIPAL (CORE 0) =================
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (bufferIndex >= BATCH_SIZE) {
    String payload = "";
    payload.reserve(3000); 

    for (int i = 0; i < BATCH_SIZE; i++) {
      char row[80];
      sprintf(row, "%lu,%02d:%02d:%02d.%03d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f;", 
              buffer[i].id, 
              buffer[i].hour, buffer[i].min, buffer[i].sec, buffer[i].ms,
              buffer[i].ax, buffer[i].ay, buffer[i].az, 
              buffer[i].gx, buffer[i].gy, buffer[i].gz);
      payload += row;
      payload += "\n"; 
    }

    if (client.publish(mqtt_topic, payload.c_str(), false)) {
      Serial.println("Paquete enviado.");
    } else {
      Serial.println("ERROR al enviar paquete.");
    }

    bufferIndex = 0; 
  }
}