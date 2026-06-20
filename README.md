# AURA - Sistema de Reconocimiento de Voz Local y Offline

AURA es una biblioteca y firmware embebido diseñado para el control de dispositivos mediante comandos de voz offline (sin necesidad de conexión a internet o a la nube). Este sistema procesa la voz en tiempo real para activar o desactivar pines de entrada/salida (GPIO) de un microcontrolador. 

Está optimizado para el **ESP32-S3** (utilizando hardware-accelerated DSP y el framework **ESP-SR**), incluye soporte de fallback interactivo por terminal serial para el **ESP32-C3**, y contiene un simulador completo de voz y estados para **PC**.

---

## 🏗️ Estructura del Proyecto

El código fuente está organizado de la siguiente manera:

```
├── components/
│   └── aura_sr/                  # Componente principal de reconocimiento de voz
│       ├── CMakeLists.txt        # Configuración de compilación e inclusión de dependencias
│       ├── aura_sr.c             # Lógica de la máquina de estados e inicialización del DSP de audio
│       └── include/
│           └── aura_sr.h         # API pública expuesta para controlar pines por voz
├── main/
│   ├── CMakeLists.txt            # Registro del ejecutable principal
│   ├── idf_component.yml         # Dependencias del registro de Espressif (espressif/esp-sr, espressif/esp-dsp)
│   └── main.c                    # Código de entrada que registra las acciones y arranca el sistema
├── sim_pc/                       # Carpeta del simulador interactivo para computadora
│   ├── Makefile                  # Compilador gcc para crear el ejecutable nativo en PC
│   ├── main_sim.c                # Entrada de la simulación en C
│   ├── aura_sr_sim.c             # Simulación de la lógica de estados de AURA en C
│   ├── aura_sim.py               # Lógica equivalente de simulación en Python
│   └── voice_test_pc.py          # Wrapper de voz en Python (SpeechRecognition)
├── sdkconfig.defaults            # Parámetros del proyecto por defecto (MultiNet7, WakeNet, PSRAM, etc.)
├── partitions.csv                # Tabla de particiones del ESP32 (reserva espacio para el modelo de voz)
└── README.md                     # Esta documentación
```

---

## ⚙️ ¿Qué hace el código y cómo funciona?

El sistema integra procesamiento digital de señales (DSP), redes neuronales locales y una máquina de estados finitos.

### 1. DSP de Audio (en ESP32-S3)
*   Configura e inicializa el periférico **I2S** en modo maestro a **16 kHz** en canal mono para capturar del micrófono (ej. INMP441).
*   Las muestras de audio de 32 bits recibidas se desplazan a la derecha (`>> 14`) para ajustar el nivel de la señal de entrada al rango dinámico esperado por el Pipeline de audio (AFE) de Espressif.

### 2. Procesamiento de Voz (WakeNet + MultiNet7)
*   **WakeNet:** Escucha continuamente en busca de las palabras de activación configuradas (*AURA* o *JARVIS*).
*   **MultiNet7 (Con G2P):** Dado que MultiNet está optimizado oficialmente para inglés y chino, el sistema aprovecha el modelo en inglés con mapeo fonético (G2P - Grapheme-to-Phoneme). Esto permite registrar palabras en español: por ejemplo, *"encender"* es procesado fonéticamente como *"ENSENDER"* logrando una alta precisión de reconocimiento de forma local.

### 3. Máquina de Estados Finitos
El flujo de control lógico funciona de la siguiente manera:
*   **Estado de Palabra Clave (WakeWord):** El sistema escucha continuamente en reposo. El LED de estado (GPIO 3) está apagado (0V).
*   **Estado de Comando:** Al escuchar *"AURA"* o *"JARVIS"*, el sistema activa el pin de estado **GPIO 3** (lo pone en HIGH/3.3V) indicando visualmente que está listo para recibir una orden. Se inicia un temporizador interno de **3 segundos**.
    *   Si en este intervalo dices un comando registrado (ej. *"encender"* o *"apagar"*), se aplica la acción correspondiente al pin asociado (ej. **GPIO 2**), el LED de estado se apaga y el sistema regresa al reposo.
    *   Si transcurren los 3 segundos sin detectar un comando de acción válido, el temporizador expira (Timeout), el LED de estado se apaga y el sistema regresa automáticamente al estado inicial.

---

## 📥 Entradas (Inputs) y Salidas (Outputs)

*   **Entradas:** Muestras de audio capturadas del micrófono I2S de 16kHz, o strings de texto ingresados en la consola serial (en modo CLI de fallback o en el simulador de PC).
*   **Salidas:** Nivel de voltaje físico en los pines GPIO del microcontrolador (ej. GPIO 2 para controlar un foco o relé), nivel de voltaje en el pin indicador de escucha (GPIO 3), y mensajes de log formateados a través del puerto COM.

---

## 💻 Simulador para PC: Cómo Correrlo y Qué Esperar

El simulador interactivo te permite probar la lógica exacta de la máquina de estados en tu computadora, usando entrada por teclado o entrada directa por voz.

### Opción A: Simulador interactivo por teclado (C o Python)

**1. Compilar y correr el simulador en C:**
Abre tu consola dentro de la carpeta `sim_pc` y ejecuta:
```bash
cd sim_pc
make
./aura_sim
```
*(Nota: Si no tienes `make` o `gcc` instalado en tu sistema operativo, puedes ejecutar la versión equivalente en Python ejecutando `python aura_sim.py`)*

**2. ¿Qué esperar en consola?:**
Al iniciar, verás el registro de los pines virtuales:
```
====================================================
  AURA Speech Recognition Library - PC Simulator  
====================================================
[SIM] Initializing AURA State Machine Speech Simulation on PC...
[SIM] Registered action: 'encender' -> GPIO 2 (HIGH)
[SIM] Registered action: 'apagar' -> GPIO 2 (LOW)
[SIM] AURA Engine Listening (Simulation Mode)...
[SIM] Type WakeWord ('AURA' or 'JARVIS'), then type commands ('encender' or 'apagar').

AURA > 
```
*   Si escribes una palabra aleatoria (ej: `hola`), el simulador advertirá que debes activarlo primero:
    ```
    [SIM WARNING] Speak WakeWord first ('AURA' or 'JARVIS'). Heard: 'hola'
    ```
*   Si escribes la palabra clave `AURA`, el estado cambiará:
    ```
    AURA > AURA
    >>> [SIM TRIGGER] WakeWord 'AURA' matched -> GPIO 3 toggled to HIGH (1). listening for action commands for 3s...
    ```
*   Escribe `encender` antes de 3 segundos para activar el LED virtual:
    ```
    AURA > encender
    >>> [SIM TRIGGER] Action Match: 'encender' -> GPIO 2 set to HIGH (Value: 1)
    >>> [SIM STATUS] Action executed. Status LED (GPIO 3) toggled to LOW (0). Awaiting WakeWord...
    ```
*   Si activas el sistema con `AURA` y dejas pasar más de 3 segundos antes de ingresar una orden, se imprimirá un timeout automático:
    ```
    >>> [SIM TIMEOUT] 3 seconds elapsed. Status LED (GPIO 3) toggled to LOW (0). Awaiting WakeWord...
    ```

---

### Opción B: Simulador por Voz Real (Python)

Este script captura el audio de tu micrófono físico en la PC, lo transcribe en español usando la API del motor de voz y envía los comandos automáticamente al simulador.

**1. Instalar las dependencias de Python:**
```bash
pip install SpeechRecognition pyaudio
```

**2. Iniciar el simulador por voz:**
```bash
python voice_test_pc.py
```

**3. ¿Qué esperar?:**
*   El script calibrará tu micrófono durante 1 segundo para aislar el ruido ambiente.
*   Imprimirá `[VOICE] Listening...`.
*   Habla directamente a tu micrófono. Di: **"Aura"**.
*   Verás que el script detecta tu voz: `[VOICE] Recognized Text: 'Aura'`. El simulador se activará (GPIO 3 pasa a HIGH).
*   Di inmediatamente: **"encender"**.
*   El script lo enviará al simulador: se activará la salida (GPIO 2 pasa a HIGH) y el sistema regresará al estado de reposo listo para escuchar tu voz nuevamente.
*   Para salir del script di **"salir"** o presiona `Ctrl` + `C`.

---

## 🚀 Cómo Compilar y Flasheo en ESP32-S3

Para subir el software definitivo a tu placa física:

### 1. Activar el entorno de ESP-IDF
Abre tu consola en la raíz de tu proyecto y ejecuta el script de activación del entorno de ESP-IDF:
```powershell
# En Windows (ejemplo de comando de activación genérico)
. /ruta/a/tu/esp-idf/export.ps1
```

### 2. Definir el chip destino
Establece la variable de entorno para indicarle a las herramientas de compilación que compilarás para el ESP32-S3:
```powershell
$env:IDF_TARGET="esp32s3"
```

### 3. Compilar el proyecto
Genera el binario del firmware junto con la partición que almacena los modelos de red neuronal:
```powershell
idf.py build
```

### 4. Flashear / Subir a la placa
Conecta tu placa ESP32-S3 por USB y ejecuta (reemplaza `PORT` por el puerto COM asignado a tu tarjeta, por ejemplo `COM7`):
```powershell
idf.py -p PORT flash
```

### 5. Monitorear logs del microcontrolador
Abre el canal de depuración serial para comprobar el arranque del DSP y la escucha en vivo del micrófono:
```powershell
idf.py -p PORT monitor
```
*(Para salir de la consola del monitor presiona `Ctrl` + `]`)*
