# Guía del Desarrollador: Uso de AURA como Biblioteca Reutilizable

Esta guía explica cómo modificar, integrar y extender la biblioteca `aura_sr` para tus propios proyectos, permitiéndote conectar diferentes dispositivos (relés, focos, motores, LEDs o pulsadores) y controlar sus acciones mediante comandos de voz personalizados.

---

## 🔌 API Pública de AURA (`aura_sr.h`)

La interfaz pública expone tres funciones principales diseñadas para ser modulares y de fácil integración en cualquier archivo de código de ESP-IDF:

### 1. Inicialización del Motor
```c
esp_err_t aura_sr_init(void);
```
*   **¿Qué hace?:** Configura el pin indicador de estado (GPIO 3), monta la partición `model` de la flash SPI, carga los archivos del modelo acústico en memoria RAM/PSRAM, inicializa el frontend acústico (AFE) para reducción de ruido y reserva memoria para el reconocedor de comandos (`MultiNet7`).
*   **Retorno:** `ESP_OK` si el hardware y los modelos se cargaron con éxito; `ESP_FAIL` si la partición de modelos no fue encontrada o la memoria RAM es insuficiente.

---

### 2. Registro de Acciones Personalizadas
```c
esp_err_t aura_sr_register_action(gpio_num_t pin, const char *phrase, aura_action_t action);
```
Permite asociar una frase hablada en español con un pin físico y una acción eléctrica.
*   **Parámetros:**
    *   `pin`: El número del pin GPIO que deseas controlar (ej. `GPIO_NUM_2`, `GPIO_NUM_4`, `GPIO_NUM_18`).
    *   `phrase`: La frase o palabra en minúsculas que detonará la acción (ej. `"encender"`, `"apagar"`, `"abrir"`, `"cocina encender"`).
    *   `action`: El nivel eléctrico que se aplicará al pin:
        *   `AURA_ACTION_ON`: Pone el pin en nivel alto (**HIGH / 3.3V**).
        *   `AURA_ACTION_OFF`: Pone el pin en nivel bajo (**LOW / 0V**).
*   **Retorno:** `ESP_OK` si se registró correctamente; `ESP_ERR_NO_MEM` si se supera el límite máximo de comandos (por defecto configurado a 10).

---

### 3. Arranque del Motor
```c
esp_err_t aura_sr_start(void);
```
*   **¿Qué hace?:** Inicializa el periférico I2S para capturar el micrófono, procesa fonéticamente todos los comandos registrados a través del motor G2P de MultiNet7 y crea dos tareas asíncronas en el sistema operativo en tiempo real FreeRTOS:
    *   **Tarea `feed_Task` (Core 0):** Lee de forma continua los buffers de audio del bus I2S y los alimenta al Pipeline de procesamiento.
    *   **Tarea `detect_Task` (Core 1):** Procesa el reconocimiento neuronal de la palabra clave, gestiona la ventana de comandos (timeout de 3s) y aplica los niveles lógicos a los pines GPIO registrados.
*   **Retorno:** `ESP_OK` si las tareas y el bus I2S se iniciaron correctamente.

---

## 🛠️ Cómo Modificar los Pines y Conectar Nuevos Dispositivos

Para conectar un nuevo dispositivo electrónico (por ejemplo, un módulo de **relé** de 5V/110V o un **LED** externo), sigue estos pasos prácticos:

### Paso 1: Conexión Eléctrica (Hardware)

> [!WARNING]
> Los pines del ESP32-S3 operan a **3.3V** con una corriente máxima recomendada de **20mA**. No conectes cargas de potencia (como motores, focos de 110V/220V o tiras LED grandes) directamente al pin del microcontrolador o podrías quemarlo. Usa siempre un módulo intermedio (Optoacoplador, Relé o Transistor MOSFET).

*   **LED Externo:** Conecta el pin positivo (ánodo) a tu pin de control (ej. GPIO 4) a través de una resistencia de $220\ \Omega$ a $330\ \Omega$, y el pin negativo (cátodo) a GND.
*   **Módulo Relé:** Conecta la alimentación del módulo (VCC y GND) y el pin de señal de control (IN) a tu pin de control (ej. GPIO 5).

---

### Paso 2: Modificar el código en `main/main.c`

Para registrar los nuevos dispositivos, edita el archivo `main/main.c` de tu proyecto y reemplaza la sección de registro con tus nuevos pines:

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "aura_sr.h"

// Definición de tus nuevos pines de control
#define GPIO_FOCO_SALA   GPIO_NUM_4
#define GPIO_VENTILADOR  GPIO_NUM_5

void app_main(void)
{
    // 1. Inicializar la biblioteca AURA
    ESP_ERROR_CHECK(aura_sr_init());

    // 2. Registrar acciones para el Foco de la Sala (GPIO 4)
    // Cuando digas "AURA encender", el GPIO 4 se pondrá en 3.3V
    ESP_ERROR_CHECK(aura_sr_register_action(GPIO_FOCO_SALA, "encender", AURA_ACTION_ON));
    
    // Cuando digas "AURA apagar", el GPIO 4 se pondrá en 0V
    ESP_ERROR_CHECK(aura_sr_register_action(GPIO_FOCO_SALA, "apagar", AURA_ACTION_OFF));

    // 3. Registrar acciones para el Ventilador (GPIO 5) usando comandos compuestos
    // Cuando digas "AURA ventilador encender", el GPIO 5 se pondrá en 3.3V
    ESP_ERROR_CHECK(aura_sr_register_action(GPIO_VENTILADOR, "ventilador encender", AURA_ACTION_ON));
    ESP_ERROR_CHECK(aura_sr_register_action(GPIO_VENTILADOR, "ventilador apagar", AURA_ACTION_OFF));

    // 4. Arrancar el motor de reconocimiento
    ESP_ERROR_CHECK(aura_sr_start());

    ESP_LOGI("main", "Sistema configurado. Esperando comandos de voz...");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## ⏱️ Personalización Interna de la Biblioteca

Si deseas modificar comportamientos internos de la biblioteca (como el tiempo de escucha o el pin del indicador visual), puedes hacerlo en el archivo `components/aura_sr/aura_sr.c`:

*   **Cambiar el Pin del LED Indicador de Escucha:**
    Busca la macro en el archivo `aura_sr.c`:
    ```c
    #define GPIO_INDICATOR_PIN GPIO_NUM_3
    ```
    Puedes cambiar el `3` por cualquier otro pin libre de tu placa.
*   **Modificar la ventana de tiempo de espera (Timeout):**
    Por defecto, tras decir "AURA", el sistema espera 3 segundos por tu comando. Si deseas ampliarlo a 5 segundos para hablar con más calma, localiza la macro:
    ```c
    #define LISTEN_WINDOW_US (3000 * 1000) // 3 segundos en microsegundos
    ```
    Modifícala a:
    ```c
    #define LISTEN_WINDOW_US (5000 * 1000) // 5 segundos en microsegundos
    ```
