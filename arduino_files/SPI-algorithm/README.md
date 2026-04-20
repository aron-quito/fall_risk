# SPI-Algorithm Documentation

## Overview
Este proyecto implementa un sistema de comunicación SPI (Serial Peripheral Interface) entre dos dispositivos ESP32 para la transmisión de datos de sensores inerciales (MPU6050) en tiempo real. El sistema consiste en una arquitectura maestro-esclavo con comunicación robusta y validación de datos.

## Architecture

### Master-Slave Configuration
- **Master**: Dispositivo principal que recibe datos del sensor y los transmite vía MQTT
- **Slave**: Dispositivo que lee directamente del sensor MPU6050 y envía datos al master vía SPI

### Communication Protocol
- **Interface**: SPI hardware con handshake digital
- **Frequency**: 10 kHz SPI clock
- **Mode**: SPI Mode 3 (CPOL=1, CPHA=1)
- **Data Format**: MSB First
- **Handshake**: Pin GPIO 4 para sincronización

## Directory Structure

```
SPI-algorithm/
master/
  master/
    master.ino          # Código del dispositivo maestro
slave/
  slave.ino            # Código del dispositivo esclavo
```

## Files Description

### master.ino
**Purpose**: Dispositivo maestro que recibe datos del esclavo y los publica en MQTT

**Key Features**:
- Conectividad WiFi y MQTT seguro
- Sincronización de tiempo NTP
- Buffer de datos (50 muestras)
- Validación de checksum
- Formato JSON para transmisión
- Frecuencia de muestreo real-time

**Dependencies**:
- `SPI.h` - Comunicación SPI
- `WiFi.h` - Conectividad WiFi
- `WiFiClientSecure.h` - MQTT seguro
- `PubSubClient.h` - Cliente MQTT
- `WiFiUdp.h` - Protocolo UDP para NTP
- `NTPClient.h` - Cliente de tiempo
- `ArduinoJson.h` - Procesamiento JSON

**Configuration**:
```cpp
const char* ssid = "";           // WiFi SSID
const char* password = "";       // WiFi Password
const char* mqtt_server = "";    // MQTT Broker
const char* mqtt_user = "";      // MQTT User
const char* mqtt_pass = "";      // MQTT Password
const char* mqtt_topic = "esp32/mpu6050"; // MQTT Topic
```

**Data Structures**:
```cpp
struct IncomingSample {
  float ax, ay, az, gx, gy, gz, a_total, temp;
  uint16_t checksum;
};

struct FullSample {
  IncomingSample data;
  char fecha[25];
  unsigned long id;
};
```

**Key Functions**:
- `setup_wifi()` - Configuración WiFi
- `reconnectMQTT()` - Reconexión MQTT automática
- `sendBatchOverMQTT()` - Envío de lotes de datos
- `calculateChecksum()` - Validación de integridad
- `validateSample()` - Verificación de datos

### slave.ino
**Purpose**: Dispositivo esclavo que lee datos del MPU6050 y los transmite al master

**Key Features**:
- Lectura directa de sensor MPU6050
- Calibración de offsets
- Generación de checksum
- SPI slave hardware nativo
- Handshake digital
- Frecuencia de muestreo 50 Hz

**Dependencies**:
- `Wire.h` - Comunicación I2C
- `Adafruit_MPU6050.h` - Driver del sensor
- `Adafruit_Sensor.h` - Librería base de sensores
- `driver/spi_slave.h` - SPI slave hardware ESP32

**Calibration Offsets**:
```cpp
const float Accel_X_Offset = 0.0, Accel_Y_Offset = 0.0, Accel_Z_Offset = 1.0;
const float Gyro_X_Offset  = 2.6185, Gyro_Y_Offset  = -4.271, Gyro_Z_Offset  = -0.1445;
```

**SPI Configuration**:
```cpp
const int SPI_SCK_PIN = 18;      // Clock
const int SPI_MISO_PIN = 19;     // Master In Slave Out
const int SPI_MOSI_PIN = 23;     // Master Out Slave In
const int SPI_SS_PIN = 5;        // Slave Select
const int SPI_READY_PIN_SLAVE = 4; // Handshake
```

**Key Functions**:
- `calculateChecksum()` - Cálculo de integridad de datos
- `setup()` - Inicialización de sensor y SPI
- `loop()` - Generación y transmisión de muestras

## Data Flow

### 1. Sensor Reading (Slave)
- Lectura del MPU6050 a 50 Hz (cada 20ms)
- Aplicación de offsets de calibración
- Cálculo de aceleración total
- Generación de checksum

### 2. SPI Communication
- Slave indica datos listos via READY pin (GPIO 4)
- Master detecta READY y activa CS
- Transferencia de 36 bytes (IncomingSample struct)
- Validación de checksum en master

### 3. Data Processing (Master)
- Almacenamiento en buffer circular
- Timestamp con sincronización NTP
- Formato JSON batch
- Publicación MQTT

## Data Format

### IncomingSample Structure (36 bytes)
```cpp
struct IncomingSample {
  float ax;        // 4 bytes - Aceleración X (m/s²)
  float ay;        // 4 bytes - Aceleración Y (m/s²)
  float az;        // 4 bytes - Aceleración Z (m/s²)
  float gx;        // 4 bytes - Giroscopio X (rad/s)
  float gy;        // 4 bytes - Giroscopio Y (rad/s)
  float gz;        // 4 bytes - Giroscopio Z (rad/s)
  float a_total;   // 4 bytes - Aceleración total (m/s²)
  float temp;      // 4 bytes - Temperatura (°C)
  uint16_t checksum; // 2 bytes - Checksum de validación
};
```

### JSON Output Format
```json
{
  "completo": true,
  "num_muestras": 50,
  "data": [
    {
      "id": 1,
      "ax": 0.123,
      "ay": -0.456,
      "az": 9.812,
      "gx": 0.001,
      "gy": -0.002,
      "gz": 0.000,
      "a_total": 9.856,
      "fecha": "2024-01-01 12:00:00"
    }
  ]
}
```

## Hardware Requirements

### ESP32 Master
- GPIO 18: SPI Clock (SCK)
- GPIO 19: SPI MISO
- GPIO 23: SPI MOSI
- GPIO 5: SPI Slave Select (SS)
- GPIO 4: Handshake READY input
- Conexión WiFi

### ESP32 Slave
- GPIO 18: SPI Clock (SCK)
- GPIO 19: SPI MISO
- GPIO 23: SPI MOSI
- GPIO 5: SPI Slave Select (SS)
- GPIO 4: Handshake READY output
- GPIO 21: I2C SDA
- GPIO 22: I2C SCL
- MPU6050 sensor

## Performance Characteristics

### Sampling Rate
- **Target**: 50 Hz (20ms intervals)
- **SPI Speed**: 10 kHz
- **Buffer Size**: 50 samples
- **Latency**: < 1 second

### Data Integrity
- Checksum validation per sample
- Handshake synchronization
- Error detection and recovery
- Zero-padding protection

## Configuration Steps

### 1. Network Setup (Master)
```cpp
const char* ssid = "your_wifi_network";
const char* password = "your_wifi_password";
const char* mqtt_server = "your_mqtt_broker";
const char* mqtt_user = "mqtt_username";
const char* mqtt_pass = "mqtt_password";
```

### 2. Calibration (Slave)
Adjust offset values according to sensor calibration:
```cpp
const float Accel_X_Offset = 0.0;
const float Accel_Y_Offset = 0.0;
const float Accel_Z_Offset = 1.0;
const float Gyro_X_Offset = 2.6185;
const float Gyro_Y_Offset = -4.271;
const float Gyro_Z_Offset = -0.1445;
```

### 3. Hardware Connections
Connect SPI pins between master and slave:
- SCK: GPIO 18 (both devices)
- MISO: GPIO 19 (both devices)
- MOSI: GPIO 23 (both devices)
- SS: GPIO 5 (both devices)
- READY: GPIO 4 (both devices)

## Troubleshooting

### Common Issues

1. **Checksum Errors**
   - Verify SPI connections
   - Check clock synchronization
   - Ensure proper voltage levels

2. **WiFi Connection Issues**
   - Verify SSID and password
   - Check signal strength
   - Monitor serial output

3. **MQTT Connection Problems**
   - Verify broker address and port
   - Check authentication credentials
   - Ensure firewall permissions

4. **Sensor Reading Issues**
   - Verify I2C connections (SDA/SCL)
   - Check MPU6050 power supply
   - Validate calibration offsets

### Debug Output
Both devices provide detailed serial output:
- SPI transaction status
- Checksum validation
- Data integrity checks
- Frequency monitoring
- Connection status

## Dependencies

### Arduino Libraries
- `SPI.h` (built-in)
- `WiFi.h` (ESP32)
- `WiFiClientSecure.h` (ESP32)
- `PubSubClient.h` (by Nick O'Leary)
- `WiFiUdp.h` (ESP32)
- `NTPClient.h` (by Fabrice Weinberg)
- `ArduinoJson.h` (by Benoit Blanchon)
- `Wire.h` (built-in)
- `Adafruit_MPU6050.h` (Adafruit)
- `Adafruit_Sensor.h` (Adafruit)

### PlatformIO Dependencies
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
    adafruit/Adafruit MPU6050
    adafruit/Adafruit Unified Sensor
    knolleary/PubSubClient
    bblanchon/ArduinoJson
    arduino-libraries/NTPClient
```

## Future Enhancements

1. **OTA Updates**: Implementación de actualizaciones over-the-air
2. **Data Compression**: Reducción de ancho de banda
3. **Multi-sensor Support**: Expansión a múltiples sensores
4. **Edge Processing**: Procesamiento local de datos
5. **Power Management**: Optimización de consumo energético

## Version History

- **v1.0**: Implementación básica SPI master-slave
- **v1.1**: Adición de checksum y validación
- **v1.2**: Integración MQTT y timestamps
- **v1.3**: Optimización de buffer y batch processing

## License

Este proyecto está diseñado para uso educacional y de investigación en sistemas de detección de caídas.
