#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AURA_ACTION_OFF = 0,
    AURA_ACTION_ON  = 1
} aura_action_t;

// Structure to hold a command mapping
typedef struct {
    gpio_num_t pin;
    const char *phrase;    // e.g. "encender", "apagar"
    aura_action_t action;  // HIGH or LOW
} aura_command_t;

/**
 * @brief Initialize the AURA speech recognition library
 * @return ESP_OK on success
 */
esp_err_t aura_sr_init(void);

/**
 * @brief Register a GPIO pin control mapped to a specific spoken phrase
 * 
 * @param pin GPIO pin number to control
 * @param phrase The word/phrase to recognize (e.g. "encender", "apagar")
 * @param action The action to apply (Turn pin ON/HIGH or OFF/LOW)
 * @return ESP_OK on success
 */
esp_err_t aura_sr_register_action(gpio_num_t pin, const char *phrase, aura_action_t action);

/**
 * @brief Start the speech recognition tasks
 * @return ESP_OK on success
 */
esp_err_t aura_sr_start(void);

#ifdef __cplusplus
}
#endif
