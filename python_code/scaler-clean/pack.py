import os
import joblib
import glob
import numpy as np
import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler

# ==========================================
# 1. LA GUILLOTINA (Tu función validada)
# ==========================================
def crear_ventanas_deslizantes(df, window_size=100, step_size=25, umbral=0.25):
    X_ventanas = []
    y_ventanas = []
    
    # Asumimos que la etiqueta se llama 'tag'. Si tienes columnas de tiempo (ej. 'tick'), bórralas.
    columnas_sensores = [col for col in df.columns if col not in ['tag', 'time', 'tick']]
    
    datos_sensores = df[columnas_sensores].values 
    etiquetas = df['tag'].values
    longitud_total = len(df)
    
    for i in range(0, longitud_total - window_size + 1, step_size):
        ventana_X = datos_sensores[i : i + window_size]
        ventana_y = etiquetas[i : i + window_size]
        
        # Regla del umbral: Si el 13% de la ventana es caída, la etiquetamos como 1
        porcentaje_caida = np.sum(ventana_y) / window_size
        etiqueta_final = 1 if porcentaje_caida >= umbral else 0
            
        X_ventanas.append(ventana_X)
        y_ventanas.append(etiqueta_final)
        
    return X_ventanas, y_ventanas

# ==========================================
# 2. RECORRIDO DE CARPETAS (La Cosecha)
# ==========================================
print("⏳ Iniciando la extracción de datos...")

# CAMBIA ESTO por la ruta real donde están tus carpetas SA01, SA02...
RUTA_RAIZ = "/Users/aron/githubRepo/fall_risk/fall-dataset/SisFall_tag/"

# glob buscará todos los .csv sin importar en qué subcarpeta estén
archivos_csv = glob.glob(os.path.join(RUTA_RAIZ, "**", "*.csv"), recursive=True)

X_total = []
y_total = []

for archivo in archivos_csv:
    # Leemos el archivo
    df = pd.read_csv(archivo)
    
    # Nos aseguramos de que tenga la columna 'tag'
    if 'tag' in df.columns:
        X_arch, y_arch = crear_ventanas_deslizantes(df, window_size=100, step_size=25, umbral=0.13)
        X_total.extend(X_arch)
        y_total.extend(y_arch)

# Convertimos nuestras listas gigantes a matrices NumPy ultra rápidas
X_numpy = np.array(X_total)
y_numpy = np.array(y_total)

print(f"✅ ¡Cosecha terminada! Generamos un total de {len(X_numpy)} ventanas de entrenamiento.")
print(f"📊 Forma de X cruda: {X_numpy.shape} (Ventanas, Tiempo, Sensores)")

# ==========================================
# 3. EL CORTE (Evitando la trampa)
# ==========================================
print("\n⏳ Cortando la tabla en Train (80%) y Validation (20%)...")

# El stratify=y_numpy garantiza que el porcentaje de caídas sea idéntico en ambos grupos
X_train, X_val, y_train, y_val = train_test_split(
    X_numpy, y_numpy, 
    test_size=0.2, 
    random_state=42, 
    stratify=y_numpy
)

print(f"📚 Train: {len(X_train)} ventanas | 📝 Val: {len(X_val)} ventanas")

# ==========================================
# 4. LA REGLA DE ORO DEL ESCALADOR
# ==========================================
print("\n⏳ Aplicando StandardScaler sin fugar datos al futuro...")

escalador = StandardScaler()

# Truco matemático: Extraemos las dimensiones originales
num_ventanas_train, pasos_tiempo, num_sensores = X_train.shape
num_ventanas_val = X_val.shape[0]

# A) APLASTAR A 2D: (num_ventanas * 100, 6 sensores)
X_train_2d = X_train.reshape(-1, num_sensores)
X_val_2d = X_val.reshape(-1, num_sensores)

# B) ESCALAR CON LA REGLA DE ORO
# fit_transform para Train (El escalador APRENDE y aplasta)
X_train_scaled_2d = escalador.fit_transform(X_train_2d)

# solo transform para Val (Al examen final NO se le sacan reglas nuevas)
X_val_scaled_2d = escalador.transform(X_val_2d)

# C) INFLAR A 3D DE NUEVO
X_train_final = X_train_scaled_2d.reshape(num_ventanas_train, pasos_tiempo, num_sensores)
X_val_final = X_val_scaled_2d.reshape(num_ventanas_val, pasos_tiempo, num_sensores)

print("✅ ¡Datos normalizados y listos para PyTorch!")
print(f"🚀 X_train_final shape: {X_train_final.shape}")
print(f"🚀 X_val_final shape: {X_val_final.shape}")

# ==========================================
# 5. EMPAQUETAR PARA LLEVAR A COLAB
# ==========================================
print("\n📦 Guardando los datos en un paquete comprimido para Colab...")

# np.savez_compressed guarda todos tus tensores en un solo archivo ligero
np.savez_compressed(
    'dataset_fallrisk_listo.npz', 
    X_train=X_train_final, 
    y_train=y_train, 
    X_val=X_val_final, 
    y_val=y_val
)

print("✅ ¡Listo! Sube el archivo 'dataset_fallrisk_listo.npz' a tu Google Drive para usarlo en Colab.")

# ==========================================
# 6. GUARDAR EL ESCALADOR
# ==========================================
joblib.dump(escalador, 'scaler_fall_risk.pkl')
print("✅ Escalador guardado.")