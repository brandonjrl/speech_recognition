# AURA - Sistema de Reconocimiento de Voz Local y Offline

AURA es una biblioteca y firmware embebido en C diseñado para el control de dispositivos físicos mediante comandos de voz offline (sin necesidad de conexión a internet o a la nube). Este sistema procesa la voz en tiempo real localmente para activar o desactivar pines de entrada/salida (GPIO) de un microcontrolador. 

Está optimizado para el **ESP32-S3** (utilizando hardware-accelerated DSP y el framework **ESP-SR**), incluye soporte de fallback interactivo por terminal serial para el **ESP32-C3**, y contiene un simulador completo de voz y estados para **PC**.

---

## 📖 Documentación Relacionada

Para facilitar el desarrollo y la personalización, el proyecto cuenta con las siguientes guías específicas:
*   [🔌 Guía de Conexión de Hardware y Modificación de Pines (LIBRARY_GUIDE.md)](./LIBRARY_GUIDE.md): Explica cómo conectar relés, tiras LED, ventiladores y cómo modificar el código fuente para mapear nuevos comandos y GPIOs.

---

## 🚀 Clonación y Puesta en Marcha Rápida (Quickstart)

Sigue estos pasos para clonar el repositorio en tu máquina local y subirlo a tu microcontrolador:

### 1. Prerrequisitos
*   Tener instalado **Git** y **ESP-IDF v5.4** (u otra versión compatible de ESP-IDF v5.x).
*   Una tarjeta de desarrollo **ESP32-S3** con soporte de PSRAM (indispensable para ejecutar los modelos acústicos de ESP-SR).
*   Un micrófono digital I2S (por ejemplo, el modelo **INMP441**).

### 2. Clonar el Repositorio
Clona el repositorio en tu computadora usando tu terminal:
```bash
git clone <URL_DE_TU_REPOSITORIO_DE_GITHUB>
cd aura-speech-recognition
```

### 3. Activar el Entorno de Compilación
Activa la consola con las herramientas de ESP-IDF:
```powershell
# En Windows (ejemplo de comando de activación genérico de ESP-IDF)
. /ruta/a/tu/esp-idf/export.ps1
```

### 4. Establecer el Target (ESP32-S3)
```powershell
$env:IDF_TARGET="esp32s3"
```

### 5. Compilar
Al compilar, el gestor de componentes de ESP-IDF detectará el archivo `main/idf_component.yml` e instalará de forma automática las dependencias requeridas del registro de Espressif (`esp-sr` y `esp-dsp`).
```powershell
idf.py build
```

### 6. Subir (Flashear) al microcontrolador
Conecta tu placa ESP32-S3 por USB e introduce tu puerto COM correspondiente (ej. `COM7`):
```powershell
idf.py -p PUERTO_COM flash
```

### 7. Abrir el Monitor Serial
```powershell
idf.py -p PUERTO_COM monitor
```

### 8. Cómo interactuar con el sistema (Uso en Hardware)

Una vez que hayas flasheado y abierto el monitor serial en tu ESP32-S3, puedes probar el sistema interactuando físicamente de la siguiente forma:

1. **Asegurar las conexiones físicas:** 
   - Conecta tu micrófono I2S a los pines asignados en el código: `SCLK` al pin **GPIO 12**, `LRCK` al pin **GPIO 11** y `SDIN` al pin **GPIO 10**.
   - Conecta un LED indicador al pin **GPIO 3** (LED de estado de escucha).
   - El pin de control por defecto es el **GPIO 2** (suele ser el LED integrado en la mayoría de placas de desarrollo).

2. **Activar el sistema por voz:**
   Habla cerca del micrófono y di con voz clara: **"AURA"** (o **"JARVIS"**).
   - En la consola del monitor serial verás aparecer el log de detección:
     `I (1890) AURA_SR: WakeWord 'AURA' detected! Status Pin 3 set to HIGH. Listening for actions...`
   - El LED de estado en el **GPIO 3 se encenderá**, indicando que la placa está esperando un comando.

3. **Ejecutar un comando de acción:**
   Tienes una ventana de **3 segundos** para decir una de las órdenes registradas: **"encender"** o **"apagar"**.
   - Si dices *"encender"*, verás que la salida cambia:
     `I (1990) AURA_SR: Command Match: 'encender' -> GPIO 2 set to HIGH`
     - El LED indicador (GPIO 3) se apagará y el dispositivo del **GPIO 2 se encenderá**.
   - Si no dices ningún comando y transcurren los 3 segundos, la ventana se cierra:
     `I (1560) AURA_SR: Listening window timed out (3s). Pin 3 OFF. Awaiting WakeWord...`
     - El LED indicador (GPIO 3) se apagará automáticamente y el sistema volverá al modo de reposo a esperar la palabra clave.

---

## ⚙️ ¿Qué hace el código y cómo funciona?

El sistema integra procesamiento digital de señales (DSP), redes neuronales locales y una máquina de estados finitos.

### 1. DSP de Audio (en ESP32-S3)
*   Configura e inicializa el periférico **I2S** en modo maestro a **16 kHz** en canal mono para capturar el micrófono.
*   Las muestras de audio de 32 bits recibidas se desplazan a la derecha (`>> 14`) para ajustar el nivel de la señal de entrada al rango dinámico esperado por el Pipeline de audio (AFE) de Espressif.

### 2. Procesamiento de Voz (WakeNet + MultiNet7)
*   **WakeNet:** Escucha continuamente en busca de las palabras de activación configuradas (*AURA* o *JARVIS*).
*   **MultiNet7 (Con G2P):** Dado que MultiNet está optimizado oficialmente para inglés y chino, el sistema aprovecha el modelo en inglés con mapeo fonético (G2P - Grapheme-to-Phoneme). Esto permite registrar palabras en español: por ejemplo, *"encender"* es procesado fonéticamente como *"ENSENDER"* logrando una alta precisión de reconocimiento de forma local.

### 3. Máquina de Estados Finitos
El flujo de control lógico funciona de la siguiente manera:
*   **Estado de Palabra Clave (WakeWord):** El sistema escucha continuamente en reposo. El LED de estado (GPIO 3) está apagado (0V).
*   **Estado de Comando:** Al escuchar *"AURA"* o *"JARVIS"*, el sistema activa el pin de estado **GPIO 3** (lo pone en HIGH/3.3V) indicando visualmente que está listo para recibir una orden. Se inicia un temporizador de **3 segundos**.
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

## 🤝 Créditos y Agradecimientos

Este proyecto está inspirado y utiliza como base el trabajo de desarrollo original de [KishSan](https://github.com/KishSan) en el repositorio [mk39-speech-recognition](https://github.com/KishSan/mk39-speech-recognition), el cual provee excelentes bases para el control offline por comandos de voz en microcontroladores ESP32.
