import pandas as pd
from pathlib import Path

# 1. Definir las rutas de entrada y salida
input_dir = Path("/Users/aron/githubRepo/fall_risk/fall-dataset/SisFall_dataset")
output_dir = Path("/Users/aron/githubRepo/fall_risk/fall-dataset/SisFall_prepro")

# Nombres para las 6 columnas que vamos a conservar
column_names = ['ax', 'ay', 'az', 'gx', 'gy', 'gz']

# Crear la carpeta principal de salida si no existe
output_dir.mkdir(parents=True, exist_ok=True)

print("Iniciando el procesamiento y conversión de unidades...")

# Factores de conversión (caso ADXL345 y ITG3200)
FACTOR_ACCEL = 0.00390625  # Para pasar a Gravedad (g)
FACTOR_GYRO = 0.061035     # Para pasar a Grados por segundo (°/s)

# 2. Iterar sobre cada carpeta dentro del dataset original
for folder_path in input_dir.iterdir():
    if folder_path.is_dir():
        
        # Crear la subcarpeta espejo en el directorio de destino
        dest_folder = output_dir / folder_path.name
        dest_folder.mkdir(parents=True, exist_ok=True)
        
        # 3. Iterar sobre todos los archivos .txt de la subcarpeta actual
        for txt_file in folder_path.glob("*.txt"):
            try:
                # Leer el .txt original (ignorando el magnetómetro y punto y coma)
                df = pd.read_csv(
                    txt_file, 
                    sep=',', 
                    header=None, 
                    usecols=[0, 1, 2, 3, 4, 5], 
                    names=column_names
                )
                
                # 4. Reducir de 200Hz a 50Hz (submuestreo puro) y crear una copia
                df_50hz = df.iloc[::4, :].copy()
                
                # 5. CONVERSIÓN A UNIDADES FÍSICAS (g y °/s)
                # Aplicar la fórmula a las columnas del acelerómetro
                df_50hz[['ax', 'ay', 'az']] = df_50hz[['ax', 'ay', 'az']] * FACTOR_ACCEL
                
                # Aplicar la fórmula a las columnas del giroscopio
                df_50hz[['gx', 'gy', 'gz']] = df_50hz[['gx', 'gy', 'gz']] * FACTOR_GYRO
                
                # 6. Generar la ruta del nuevo archivo (.csv)
                out_file = dest_folder / f"{txt_file.stem}.csv"
                
                # Guardar el CSV final sin la columna de índices
                df_50hz.to_csv(out_file, index=False)
                
            except Exception as e:
                print(f"Error procesando el archivo {txt_file.name}: {e}")

print(f"¡Procesamiento completado! Revisa la carpeta: {output_dir}")