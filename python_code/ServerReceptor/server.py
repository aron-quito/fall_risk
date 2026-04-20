import paho.mqtt.client as mqtt
import json
import pandas as pd
import sys

# ========================= CONFIGURACIÓN =========================
MQTT_SERVER = "12f114da5d6a4b0d93d624aa7d4982ee.s1.eu.hivemq.cloud"
MQTT_PORT = 8883
MQTT_USER = "esp32"
MQTT_PASS = "Giorpa469"
TOPIC_DATA = "esp32/mpu6050"
TOPIC_COMANDOS = "esp32/comandos"

lista_maestra_datos = []
recopilacion_activa = True

# ========================= CALLBACKS =========================

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✅ Servidor conectado y escuchando datos...")
        # Suscribirse a ambos tópicos
        client.subscribe([(TOPIC_DATA, 0), (TOPIC_COMANDOS, 0)])
    else:
        print(f"❌ Error de conexión: {rc}")

def on_message(client, userdata, msg):
    global recopilacion_activa, lista_maestra_datos
    payload = msg.payload.decode()
    
    # Detectar comando STOP
    if msg.topic == TOPIC_COMANDOS and payload.strip().upper() == "STOP":
        print("\n🛑 Comando STOP recibido. Generando archivo y cerrando...")
        recopilacion_activa = False
        client.disconnect() # Esto rompe el loop_forever()
        return

    # Procesar datos del MPU6050
    if msg.topic == TOPIC_DATA and recopilacion_activa:
        try:
            data_dict = json.loads(payload)
            muestras = data_dict.get("data", [])
            lista_maestra_datos.extend(muestras)
            print(f"📥 Registros en memoria: {len(lista_maestra_datos)}", end='\r')
        except Exception as e:
            print(f"⚠️ Error procesando JSON: {e}")

def guardar_dataset():
    if not lista_maestra_datos:
        print("\nNo se recibieron datos para guardar.")
        return

    # Crear DataFrame y exportar
    df = pd.DataFrame(lista_maestra_datos)
    columnas = ["id", "fecha", "ax", "ay", "az", "gx", "gy", "gz", "a_total", "temp"]
    
    # Asegurarnos de que solo usamos las columnas que existen
    df = df[[c for c in columnas if c in df.columns]]
    
    nombre_archivo = "dataset.csv"
    df.to_csv(nombre_archivo, index=False)
    print(f"\n\n💾 ÉXITO: Archivo '{nombre_archivo}' creado en: {sys.path[0]}")
    print(f"📊 Total de filas: {len(df)}")

# ========================= EJECUCIÓN =========================

client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASS)
client.tls_set() # Necesario para HiveMQ Cloud
client.on_connect = on_connect
client.on_message = on_message

try:
    client.connect(MQTT_SERVER, MQTT_PORT)
    client.loop_forever() # Se queda aquí hasta recibir STOP o Ctrl+C
    guardar_dataset()
except KeyboardInterrupt:
    print("\n\nInterrupción manual detectada.")
    guardar_dataset()
except Exception as e:
    print(f"\n❌ Error inesperado: {e}")