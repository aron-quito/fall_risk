#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

// Offsets
const float Accel_X_Offset = 0.0, Accel_Y_Offset = 0.0, Accel_Z_Offset = 1.0;
const float Gyro_X_Offset  = 2.6185, Gyro_Y_Offset  = -4.271, Gyro_Z_Offset  = -0.1445;

const unsigned long SAMPLE_INTERVAL_MICROS = 20000; // 50 Hz
unsigned long lastSampleMicros = 0;

struct __attribute__((packed)) IncomingSample {
  float ax, ay, az, gx, gy, gz, a_total, temp;
  uint16_t checksum;
};

IncomingSample currentSample;
uint8_t tx_buffer[sizeof(IncomingSample)];

// Configuración SPI HARDWARE (no simulación)
#include "driver/spi_slave.h"

// SPI Hardware pins (VSPI)
const int SPI_SCK_PIN = 18;
const int SPI_MISO_PIN = 19;  
const int SPI_MOSI_PIN = 23;
const int SPI_SS_PIN = 5;
const int SPI_READY_PIN_SLAVE = 4;  // Handshake con master

// Buffer para SPI hardware
static uint8_t spi_tx_buffer[sizeof(IncomingSample)];
static uint8_t spi_rx_buffer[sizeof(IncomingSample)];

// Configuración SPI slave hardware - API correcta para ESP32 3.3.2
spi_bus_config_t spi_bus_cfg = {
    .mosi_io_num = SPI_MOSI_PIN,
    .miso_io_num = SPI_MISO_PIN,
    .sclk_io_num = SPI_SCK_PIN,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1
};

spi_slave_interface_config_t spi_slv_cfg = {
    .spics_io_num = SPI_SS_PIN,
    .flags = 0,
    .queue_size = 3,
    .mode = 3
};

spi_slave_transaction_t spi_trans = {
    .length = sizeof(IncomingSample) * 8,  // bits
    .tx_buffer = spi_tx_buffer,
    .rx_buffer = spi_rx_buffer
};

uint16_t calculateChecksum(const IncomingSample& sample) {
  uint16_t checksum = 0;
  const uint8_t* data = reinterpret_cast<const uint8_t*>(&sample);
  
  // Calcular checksum solo de los datos, excluyendo el campo checksum
  for (int i = 0; i < sizeof(IncomingSample) - sizeof(uint16_t); i++) {
    checksum ^= data[i];
    checksum = (checksum << 1) | (checksum >> 15); // Rotación
  }
  
  return checksum;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!mpu.begin()) {
    Serial.println("❌ Error MPU6050");
    while (1);
  }

  // Configurar pin READY para handshake
  pinMode(SPI_READY_PIN_SLAVE, OUTPUT);
  digitalWrite(SPI_READY_PIN_SLAVE, LOW);

  // Inicializar SPI SLAVE hardware (no simulación)
  ESP_ERROR_CHECK(spi_slave_initialize(HSPI_HOST, &spi_bus_cfg, &spi_slv_cfg, SPI_DMA_DISABLED));
  
  // Enviar primera transacción al hardware SPI
  ESP_ERROR_CHECK(spi_slave_queue_trans(HSPI_HOST, &spi_trans, portMAX_DELAY));

  lastSampleMicros = micros();
  Serial.println("🚀 SLAVE hardware SPI listo");
}

void loop() {
  unsigned long now = micros();

  // 1. GENERAR MUESTRA (Cada 20ms)
  if (now - lastSampleMicros >= SAMPLE_INTERVAL_MICROS) {
    lastSampleMicros += SAMPLE_INTERVAL_MICROS;

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    currentSample.ax = a.acceleration.x - Accel_X_Offset;
    currentSample.ay = a.acceleration.y - Accel_Y_Offset;
    currentSample.az = a.acceleration.z - Accel_Z_Offset;
    currentSample.gx = g.gyro.x - Gyro_X_Offset;
    currentSample.gy = g.gyro.y - Gyro_Y_Offset;
    currentSample.gz = g.gyro.z - Gyro_Z_Offset;
    currentSample.a_total = sqrt(sq(currentSample.ax) + sq(currentSample.ay) + sq(currentSample.az));
    currentSample.temp = temp.temperature;
    
    // Calcular checksum
    IncomingSample tempSample = currentSample;
    tempSample.checksum = 0;
    currentSample.checksum = calculateChecksum(tempSample);

    // Copiar al buffer del hardware SPI
    memcpy(spi_tx_buffer, &currentSample, sizeof(IncomingSample));
    
    // Indicar al master que hay datos listos
    digitalWrite(SPI_READY_PIN_SLAVE, HIGH);
    
    Serial.printf("TX bytes: %02X %02X %02X %02X %02X %02X %02X %02X | Checksum: 0x%04X\n", 
                  spi_tx_buffer[0], spi_tx_buffer[1], spi_tx_buffer[2], spi_tx_buffer[3], 
                  spi_tx_buffer[4], spi_tx_buffer[5], spi_tx_buffer[6], spi_tx_buffer[7], currentSample.checksum);
    Serial.printf("DEBUG SLAVE -> AX: %.2f | ATOT: %.2f\n", currentSample.ax, currentSample.a_total);
  }

  // 2. ESPERAR TRANSFERENCIA SPI HARDWARE (automática)
  spi_slave_transaction_t *ret_trans;
  esp_err_t ret = spi_slave_get_trans_result(HSPI_HOST, &ret_trans, pdMS_TO_TICKS(10));
  
  if (ret == ESP_OK) {
    // Transacción completada - bajar READY y preparar siguiente
    digitalWrite(SPI_READY_PIN_SLAVE, LOW);
    
    memcpy(spi_tx_buffer, &currentSample, sizeof(IncomingSample));
    ESP_ERROR_CHECK(spi_slave_queue_trans(HSPI_HOST, &spi_trans, portMAX_DELAY));
  }
}