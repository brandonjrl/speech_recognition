#include "aura_sr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

void app_main(void) {
  ESP_LOGI(TAG, "================================================");
  ESP_LOGI(TAG, "Starting AURA Lightweight Speech Control System");
  ESP_LOGI(TAG, "================================================");

  // Initialize speech recognition module
  esp_err_t err = aura_sr_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize AURA Speech Recognition library!");
    return;
  }

  // Configure GPIO pin 3
  // Keyword "encender" -> turns GPIO 3 HIGH (1)
  // Keyword "apagar"   -> turns GPIO 3 LOW (0)
  // Under the hood, these will be recognized as "AURA ENCENDER" and "AURA APAGAR"
  ESP_ERROR_CHECK(aura_sr_register_action(GPIO_NUM_3, "encender", AURA_ACTION_ON));
  ESP_ERROR_CHECK(aura_sr_register_action(GPIO_NUM_3, "apagar", AURA_ACTION_OFF));

  // Optional: Add another pin for testing multiple pins
  // e.g. GPIO_NUM_4 for "luz"
  // ESP_ERROR_CHECK(aura_sr_register_action(GPIO_NUM_4, "luz",
  // AURA_ACTION_ON));

  // Start recognition engine (launches tasks and begins listening)
  err = aura_sr_start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start AURA Speech Recognition engine!");
    return;
  }

  ESP_LOGI(TAG,
           "System ready. Speak 'AURA encender' or 'AURA apagar' to test.");

  // Main loop does nothing, just yields
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
