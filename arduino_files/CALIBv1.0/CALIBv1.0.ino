#include <Wire.h>
#include <MPU6050_light.h>

MPU6050 mpu(Wire);

const int NUM_SAMPLES = 2000;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  Serial.println("Inicializando MPU...");
  if (mpu.begin() != 0) {
    Serial.println("Error al iniciar MPU6050");
    while (1);
  }

  delay(1000);
  Serial.println("Calibrando... No muevas el sensor.");

  long accX = 0, accY = 0, accZ = 0;
  long gyroX = 0, gyroY = 0, gyroZ = 0;

  for (int i = 0; i < NUM_SAMPLES; i++) {
    mpu.update();

    accX += mpu.getAccX();
    accY += mpu.getAccY();
    accZ += mpu.getAccZ();

    gyroX += mpu.getGyroX();
    gyroY += mpu.getGyroY();
    gyroZ += mpu.getGyroZ();

    delay(3);
  }

  float offsetAccX  = -(accX / (float)NUM_SAMPLES);
  float offsetAccY  = -(accY / (float)NUM_SAMPLES);
  float offsetAccZ  = 1.0 - (accZ / (float)NUM_SAMPLES); // debe ser +1g

  float offsetGyroX = -(gyroX / (float)NUM_SAMPLES);
  float offsetGyroY = -(gyroY / (float)NUM_SAMPLES);
  float offsetGyroZ = -(gyroZ / (float)NUM_SAMPLES);

  Serial.println("\n==== OFFSETS EN FORMATO FINAL ====\n");

  Serial.print("const float Accel_X_Offset = "); Serial.print(offsetAccX, 6); Serial.println(";");
  Serial.print("const float Accel_Y_Offset = "); Serial.print(offsetAccY, 6); Serial.println(";");
  Serial.print("const float Accel_Z_Offset = "); Serial.print(offsetAccZ, 6); Serial.println(";");

  Serial.print("const float Gyro_X_Offset  = "); Serial.print(offsetGyroX, 6); Serial.println(";");
  Serial.print("const float Gyro_Y_Offset  = "); Serial.print(offsetGyroY, 6); Serial.println(";");
  Serial.print("const float Gyro_Z_Offset  = "); Serial.print(offsetGyroZ, 6); Serial.println(";");

  Serial.println("\n==================================");
}

void loop() {}
