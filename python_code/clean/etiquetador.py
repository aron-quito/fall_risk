import os
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.widgets import SpanSelector, Button

# --- 1. CONFIGURACIÓN ---
CARPETA_DATOS = "/Users/aron/githubRepo/fall_risk/fall-dataset/SisFall_procesado/SA04"
CARPETA_GUARDADO = "/Users/aron/githubRepo/fall_risk/fall-dataset/SisFall_tagueados/SA04"
os.makedirs(CARPETA_GUARDADO, exist_ok=True)

archivos_csv = sorted([f for f in os.listdir(CARPETA_DATOS) if f.endswith('.csv')])

# --- 2. GESTIÓN DE ARCHIVOS Y ADLs ---
pendientes_caidas = []
print("⏳ Procesando automáticamente los archivos ADL (No Caídas)...")
for arch in archivos_csv:
    if not arch.startswith('F'):
        ruta_guardado = os.path.join(CARPETA_GUARDADO, arch)
        if not os.path.exists(ruta_guardado):
            df_adl = pd.read_csv(os.path.join(CARPETA_DATOS, arch))
            if 'tag' not in df_adl.columns:
                df_adl['tag'] = 0
            df_adl.to_csv(ruta_guardado, index=False)
    else:
        pendientes_caidas.append(arch) 

pendientes = pendientes_caidas

if not pendientes:
    print("🎉 ¡No hay archivos de caídas en esta carpeta!")
    exit()

# --- FUNCIÓN INTELIGENTE DE CARGA (MODIFICADA) ---
def cargar_datos(archivo):
    ruta_guardado = os.path.join(CARPETA_GUARDADO, archivo)
    ruta_origen = os.path.join(CARPETA_DATOS, archivo)
    es_existente = False # Asumimos que es nuevo por defecto
    
    if os.path.exists(ruta_guardado):
        df = pd.read_csv(ruta_guardado)
        es_existente = True # Activamos la bandera
        print(f"📂 Archivo previo detectado: {archivo}. Entrando en modo reedición...")
    else:
        df = pd.read_csv(ruta_origen)
        print(f"📄 Archivo nuevo cargado: {archivo}. Lienzo limpio.")
        
    if 'tag' not in df.columns: 
        df['tag'] = 0
        
    return df, es_existente

# --- 3. INICIALIZAR VARIABLES ---
archivo_actual = pendientes.pop(0)
df_actual, archivo_ya_existia = cargar_datos(archivo_actual)
columnas_sensor = ['ax', 'ay', 'az', 'gx', 'gy', 'gz']

# --- 4. CREAR INTERFAZ NATIVA ---
fig, ax = plt.subplots(figsize=(14, 6))
plt.subplots_adjust(bottom=0.2)

lineas = ax.plot(df_actual.index, df_actual[columnas_sensor], linewidth=1.2)
ax.legend(columnas_sensor, loc='upper right')
ax.set_ylabel("Amplitud del Sensor")
ax.grid(True, linestyle='--', alpha=0.6)

# --- 5. LÓGICA DEL SOMBREADO INTERACTIVO ---
def al_seleccionar(xmin, xmax):
    df_actual['tag'] = 0
    df_actual.loc[int(xmin):int(xmax), 'tag'] = 1
    print(f"✅ Impacto ajustado: de la fila {int(xmin)} a la {int(xmax)}")

selector = SpanSelector(
    ax, al_seleccionar, direction='horizontal', useblit=True,
    interactive=True, drag_from_anywhere=True,
    props=dict(alpha=0.3, facecolor='red')
)

# --- FUNCIÓN NUEVA: PRE-DIBUJAR SOMBRA CONDICIONAL ---
def aplicar_pre_dibujado(es_existente):
    # SOLO dibuja si el archivo viene de la carpeta de guardado y tiene datos tagueados
    if es_existente and (df_actual['tag'] == 1).any():
        indices_1 = df_actual.index[df_actual['tag'] == 1].tolist()
        xmin, xmax = indices_1[0], indices_1[-1]
        selector.extents = (xmin, xmax)
    else:
        # Si es nuevo, se asegura de que el lienzo esté completamente limpio
        selector.clear()

# Aplicar la lógica al primer archivo cargado
aplicar_pre_dibujado(archivo_ya_existia)
ax.set_title(f"Archivo: {archivo_actual} | Faltan por revisar: {len(pendientes)} | Presiona 'ENTER' para siguiente")

# --- 6. EL BOTÓN Y LA TECLA ENTER ---
ax_boton = plt.axes([0.75, 0.05, 0.15, 0.075])
btn_siguiente = Button(ax_boton, 'Guardar y Siguiente', color='lightgreen', hovercolor='0.975')

def avanzar(event):
    global archivo_actual, df_actual, archivo_ya_existia
    
    # Guardar estado actual
    ruta_destino = os.path.join(CARPETA_GUARDADO, archivo_actual)
    df_actual.to_csv(ruta_destino, index=False)
    print(f"💾 Guardado con éxito: {archivo_actual}")
    
    if not pendientes:
        print("🎉 ¡Has revisado todas las caídas de esta carpeta!")
        plt.close()
        return
        
    # Cargar siguiente y actualizar bandera
    archivo_actual = pendientes.pop(0)
    df_actual, archivo_ya_existia = cargar_datos(archivo_actual)
    
    for i, col in enumerate(columnas_sensor):
        lineas[i].set_ydata(df_actual[col])
        lineas[i].set_xdata(df_actual.index)
        
    ax.set_xlim(0, len(df_actual))
    limite_min = df_actual[columnas_sensor].min().min()
    limite_max = df_actual[columnas_sensor].max().max()
    ax.set_ylim(limite_min - 10, limite_max + 10)
    
    ax.set_title(f"Archivo: {archivo_actual} | Faltan por revisar: {len(pendientes)} | Presiona 'ENTER' para siguiente")
    
    # Ejecutar el pre-dibujado condicional usando la bandera del nuevo archivo
    aplicar_pre_dibujado(archivo_ya_existia)
    
    fig.canvas.draw_idle()

btn_siguiente.on_clicked(avanzar)

def al_presionar_tecla(event):
    if event.key == 'enter':
        avanzar(None)
fig.canvas.mpl_connect('key_press_event', al_presionar_tecla)

print("🚀 Abriendo interfaz de revisión... ")
plt.show()