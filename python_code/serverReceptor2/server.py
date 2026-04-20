import paho.mqtt.client as mqtt
import pandas as pd
import sys

# ========================= CONFIGURACIÓN =========================
# Asegúrate de que estos datos coincidan con tu HiveMQ
MQTT_SERVER = "12f114da5d6a4b0d93d624aa7d4982ee.s1.eu.hivemq.cloud"
MQTT_PORT = 8883
MQTT_USER = "esp32"
MQTT_PASS = "Giorpa469"

# IMPORTANTE: El tópico debe ser exactamente el mismo que pusiste en el ESP32
TOPIC_DATA = "esp32/mpu6050" 
TOPIC_COMANDOS = "esp32/comandos"

lista_maestra_datos = []
recopilacion_activa = True

# ========================= CALLBACKS =========================

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✅ Servidor conectado. Escuchando datos a 50Hz...")
        client.subscribe([(TOPIC_DATA, 0), (TOPIC_COMANDOS, 0)])
    else:
        print(f"❌ Error de conexión: {rc}")

def on_message(client, userdata, msg):
    global recopilacion_activa, lista_maestra_datos
    payload = msg.payload.decode('utf-8')
    
    # Detectar comando STOP
    if msg.topic == TOPIC_COMANDOS and payload.strip().upper() == "STOP":
        print("\n🛑 Comando STOP recibido. Generando archivo y cerrando...")
        recopilacion_activa = False
        client.disconnect() # Rompe el loop_forever()
        return

    # Procesar datos del MPU6050 (Desempaquetar el bloque de 50 datos)
    if msg.topic == TOPIC_DATA and recopilacion_activa:
        try:
            # El payload viene así: "1,12:00:00.100,0.1,0.2,0.3,0.1,0.2,0.3;\n2,12..."
            lineas = payload.split('\n') 
            
            for linea in lineas:
                linea = linea.strip()
                if linea: # Ignorar líneas vacías
                    # Quitamos el punto y coma final y separamos por comas
                    datos = linea.replace(';', '').split(',')
                    
                    # Verificamos que tengamos exactamente las 8 columnas esperadas
                    if len(datos) == 8:
                        fila = {
                            "id": int(datos[0]),
                            "tiempo": datos[1],
                            "ax": float(datos[2]),
                            "ay": float(datos[3]),
                            "az": float(datos[4]),
                            "gx": float(datos[5]),
                            "gy": float(datos[6]),
                            "gz": float(datos[7])
                        }
                        lista_maestra_datos.append(fila)
                        
            print(f"📥 Registros en memoria: {len(lista_maestra_datos)}", end='\r')
        except Exception as e:
            print(f"\n⚠️ Error procesando la trama de texto: {e}")

def guardar_dataset():
    if not lista_maestra_datos:
        print("\n⚠️ No se recibieron datos para guardar.")
        return

    # Crear DataFrame con las columnas exactas
    columnas = ["id", "tiempo", "ax", "ay", "az", "gx", "gy", "gz"]
    df = pd.DataFrame(lista_maestra_datos, columns=columnas)
    
    nombre_archivo = "dataset_mpu6050.csv"
    df.to_csv(nombre_archivo, index=False)
    print(f"\n\n💾 ÉXITO: Archivo '{nombre_archivo}' creado en: {sys.path[0]}")
    print(f"📊 Total de filas guardadas: {len(df)} (Equivale a {len(df)/50:.2f} segundos de grabación)")

# ========================= EJECUCIÓN =========================

# Nota: Si usas paho-mqtt 2.0+, la librería pide definir explícitamente la API. 
# Usamos el código base de la v1 que es compatible con tu sintaxis.
client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASS)
client.tls_set() # Necesario para HiveMQ Cloud
client.on_connect = on_connect
client.on_message = on_message

try:
    print("Iniciando conexión a HiveMQ...")
    client.connect(MQTT_SERVER, MQTT_PORT)
    client.loop_forever() # Se queda aquí hasta recibir STOP o Ctrl+C
except KeyboardInterrupt:
    print("\n\n🛑 Interrupción manual (Ctrl+C) detectada.")
    guardar_dataset()
except Exception as e:
    print(f"\n❌ Error inesperado: {e}")