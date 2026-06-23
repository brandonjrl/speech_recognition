#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aura_sr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

static const char *TAG = "AURA_SR";

#define GPIO_INDICATOR_PIN GPIO_NUM_2
#define MAX_COMMANDS 10

typedef enum {
    AURA_STATE_WAKEWORD = 0,
    AURA_STATE_COMMAND  = 1
} aura_state_t;

static aura_command_t registered_commands[MAX_COMMANDS];
static int registered_commands_count = 0;

esp_err_t aura_sr_register_action(gpio_num_t pin, const char *phrase, aura_action_t action)
{
    if (registered_commands_count >= MAX_COMMANDS) {
        return ESP_ERR_NO_MEM;
    }

    registered_commands[registered_commands_count].pin = pin;
    registered_commands[registered_commands_count].phrase = phrase;
    registered_commands[registered_commands_count].action = action;
    registered_commands_count++;

    // Configure Target GPIO Pin as Output
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << pin),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);
    gpio_set_level(pin, 0); // Initialize to LOW

    ESP_LOGI(TAG, "Registered action: '%s' -> GPIO %d (%s)", 
             phrase, pin, action == AURA_ACTION_ON ? "HIGH" : "LOW");

    return ESP_OK;
}

#if CONFIG_IDF_TARGET_ESP32S3

// ESP32-S3 Implementation using ESP-SR (WakeNet + MultiNet) State Machine
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_speech_commands.h"
#include "esp_mn_models.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "model_path.h"
#include "driver/i2s_std.h"
#include "esp_timer.h"

static volatile int task_flag = 0;
static srmodel_list_t *models = NULL;
static esp_afe_sr_iface_t *afe_handle = NULL;
static esp_afe_sr_data_t *afe_data = NULL;
static i2s_chan_handle_t rx_handle = NULL;
static esp_mn_iface_t *multinet = NULL;
static model_iface_data_t *model_data = NULL;

static aura_state_t current_state = AURA_STATE_WAKEWORD;
static int64_t state_transition_time = 0; 
#define LISTEN_WINDOW_US (3000 * 1000) // 3 seconds

static esp_err_t init_i2s(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = GPIO_NUM_NC,
            .bclk = GPIO_NUM_12,  // SCLK
            .ws   = GPIO_NUM_11,  // LRCK
            .dout = GPIO_NUM_NC,
            .din  = GPIO_NUM_10,  // SDIN
        },
    };
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    ret = i2s_channel_init_std_mode(rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize standard mode: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_channel_enable(rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

static void feed_Task(void *arg)
{
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int nch = afe_handle->get_feed_channel_num(afe_data);
    int16_t *i2s_buff = malloc(audio_chunksize * sizeof(int16_t) * nch);
    assert(i2s_buff);

    ESP_LOGI(TAG, "Feed Task started");
    while (task_flag) {
        size_t bytes_read = 0;
        esp_err_t ret = i2s_channel_read(rx_handle, i2s_buff, audio_chunksize * sizeof(int16_t) * nch, &bytes_read, portMAX_DELAY);
        if (ret == ESP_OK && bytes_read > 0) {
            int32_t *tmp_buff = (int32_t *)i2s_buff;
            int total_samples = bytes_read / sizeof(int32_t);
            for (int i = 0; i < total_samples; i++) {
                tmp_buff[i] = tmp_buff[i] >> 14; 
            }
            afe_handle->feed(afe_data, i2s_buff);
        }
    }
    free(i2s_buff);
    vTaskDelete(NULL);
}

static void detect_Task(void *arg)
{
    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    int mu_chunksize = multinet->get_samp_chunksize(model_data);
    assert(mu_chunksize == afe_chunksize);

    ESP_LOGI(TAG, "Detect Task started. Listening for wake words 'AURA' or 'JARVIS'...");
    
    // Set to 1 to run MultiNet continuously
    int detect_flag = 1;

    while (task_flag) {
        // Handle command listening window timeout
        if (current_state == AURA_STATE_COMMAND) {
            int64_t elapsed = esp_timer_get_time() - state_transition_time;
            if (elapsed >= LISTEN_WINDOW_US) {
                current_state = AURA_STATE_WAKEWORD;
                gpio_set_level(GPIO_INDICATOR_PIN, 0); // Turn OFF indicator LED
                ESP_LOGI(TAG, "Listening window timed out (3s). Pin %d OFF. Awaiting WakeWord...", GPIO_INDICATOR_PIN);
            }
        }

        afe_fetch_result_t* res = afe_handle->fetch(afe_data); 
        if (!res || res->ret_value == ESP_FAIL) {
            ESP_LOGE(TAG, "Fetch error!");
            break;
        }

        if (detect_flag == 1) {
            esp_mn_state_t mn_state = multinet->detect(model_data, res->data);

            if (mn_state == ESP_MN_STATE_DETECTING) {
                continue;
            }

            if (mn_state == ESP_MN_STATE_DETECTED) {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                for (int i = 0; i < mn_result->num; i++) {
                    int cmd_id = mn_result->command_id[i];
                    float prob = mn_result->prob[i];
                    ESP_LOGI(TAG, "Heard: '%s' (ID: %d, Probability: %.2f)", 
                             mn_result->string, cmd_id, prob);

                    if (current_state == AURA_STATE_WAKEWORD) {
                        // Match WakeWord "AURA" (ID 0) or "JARVIS" (ID 1)
                        if (cmd_id == 0 || cmd_id == 1) {
                            current_state = AURA_STATE_COMMAND;
                            state_transition_time = esp_timer_get_time();
                            gpio_set_level(GPIO_INDICATOR_PIN, 1); // Turn ON indicator LED
                            ESP_LOGI(TAG, "WakeWord '%s' detected! Status Pin %d set to HIGH. Listening for actions...", mn_result->string, GPIO_INDICATOR_PIN);
                            break;
                        }
                    } 
                    else if (current_state == AURA_STATE_COMMAND) {
                        // Match Action Commands (ID >= 2)
                        if (cmd_id >= 2 && cmd_id < 2 + registered_commands_count) {
                            int cmd_idx = cmd_id - 2;
                            gpio_num_t pin = registered_commands[cmd_idx].pin;
                            aura_action_t action = registered_commands[cmd_idx].action;
                            
                            gpio_set_level(pin, (int)action);
                            ESP_LOGI(TAG, "Command Match: '%s' -> GPIO %d set to %s", 
                                     registered_commands[cmd_idx].phrase, pin, action == AURA_ACTION_ON ? "HIGH" : "LOW");
                            
                            // Return to wake word mode
                            current_state = AURA_STATE_WAKEWORD;
                            gpio_set_level(GPIO_INDICATOR_PIN, 0); // Turn OFF indicator LED
                            break;
                        }
                    }
                }
            }
        }
    }
    vTaskDelete(NULL);
}

esp_err_t aura_sr_init(void)
{
    ESP_LOGI(TAG, "Initializing indicator LED on GPIO %d...", GPIO_INDICATOR_PIN);
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GPIO_INDICATOR_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);
    gpio_set_level(GPIO_INDICATOR_PIN, 0); // Turn OFF by default

    ESP_LOGI(TAG, "Initializing Speech Models...");
    models = esp_srmodel_init("model");
    if (!models) {
        ESP_LOGE(TAG, "Failed to load models partition.");
        return ESP_FAIL;
    }

    afe_config_t *afe_config = afe_config_init("MMNR", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    afe_config->wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
    afe_config->aec_init = false;
    afe_config->pcm_config.total_ch_num = 2;
    afe_config->pcm_config.mic_num = 1;
    afe_config->pcm_config.ref_num = 1;

    afe_handle = esp_afe_handle_from_config(afe_config);
    afe_data = afe_handle->create_from_config(afe_config);
    if (!afe_data) {
        return ESP_FAIL;
    }

    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
    multinet = esp_mn_handle_from_name(mn_name);
    model_data = multinet->create(mn_name, 6000);
    if (!model_data) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t aura_sr_start(void)
{
    esp_err_t err = init_i2s();
    if (err != ESP_OK) {
        return err;
    }

    multinet->clean(model_data);
    esp_mn_commands_alloc(multinet, model_data);
    
    // Register WakeWords at fixed IDs
    // ID 0: AURA
    // ID 1: JARVIS
    esp_mn_commands_add(0, "AURA");
    esp_mn_commands_add(1, "JARVIS");
    ESP_LOGI(TAG, "MultiNet registered WakeWord ID 0: 'AURA'");
    ESP_LOGI(TAG, "MultiNet registered WakeWord ID 1: 'JARVIS'");

    // Register Dynamic commands mapped from index 2
    for (int i = 0; i < registered_commands_count; i++) {
        char cmd_str[64];
        snprintf(cmd_str, sizeof(cmd_str), "%s", registered_commands[i].phrase);
        for (int j = 0; cmd_str[j]; j++) {
            if (cmd_str[j] >= 'a' && cmd_str[j] <= 'z') {
                cmd_str[j] = cmd_str[j] - 'a' + 'A';
            }
            if (cmd_str[j] == 'C' && j > 0 && cmd_str[j-1] == 'N') {
                cmd_str[j] = 'S'; // Convert "ENCENDER" to "ENSENDER" for G2P sound matching
            }
        }
        
        int command_id = 2 + i;
        esp_mn_commands_add(command_id, cmd_str);
        ESP_LOGI(TAG, "MultiNet registered command ID %d: '%s'", command_id, cmd_str);
    }
    
    esp_mn_commands_update();

    task_flag = 1;
    xTaskCreatePinnedToCore(&detect_Task, "aura_detect", 8 * 1024, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(&feed_Task, "aura_feed", 8 * 1024, NULL, 5, NULL, 0);

    return ESP_OK;
}

#else

// ESP32-C3 State Machine fallback simulation via Serial CLI
static aura_state_t cli_state = AURA_STATE_WAKEWORD;
static TickType_t cli_transition_tick = 0;
#define CLI_LISTEN_WINDOW_MS 3000

static void serial_cli_task(void *arg)
{
    char line[128];
    int char_idx = 0;
    
    printf("\n*** AURA State Machine CLI Active (ESP32-C3 Fallback) ***\n");
    printf("State: LISTENING FOR WAKEWORD ('AURA' or 'JARVIS')\n");
    printf("AURA > ");
    fflush(stdout);

    while (1) {
        // Handle timeout
        if (cli_state == AURA_STATE_COMMAND) {
            TickType_t elapsed = xTaskGetTickCount() - cli_transition_tick;
            if (pdTICKS_TO_MS(elapsed) >= CLI_LISTEN_WINDOW_MS) {
                cli_state = AURA_STATE_WAKEWORD;
                gpio_set_level(GPIO_INDICATOR_PIN, 0); // Turn off Pin 2
                printf("\n[TIMEOUT] 3 seconds elapsed. Status LED (Pin %d) turned OFF. Awaiting WakeWord...\n", GPIO_INDICATOR_PIN);
                printf("AURA > ");
                fflush(stdout);
            }
        }

        int c = getchar();
        if (c == '\n' || c == '\r') {
            line[char_idx] = '\0';
            if (char_idx > 0) {
                char *cmd = line;
                while (*cmd == ' ') cmd++;
                int cmd_len = strlen(cmd);
                while (cmd_len > 0 && cmd[cmd_len-1] == ' ') {
                    cmd[cmd_len-1] = '\0';
                    cmd_len--;
                }
                
                if (cli_state == AURA_STATE_WAKEWORD) {
                    if (strcasecmp(cmd, "AURA") == 0 || strcasecmp(cmd, "JARVIS") == 0) {
                        cli_state = AURA_STATE_COMMAND;
                        cli_transition_tick = xTaskGetTickCount();
                        gpio_set_level(GPIO_INDICATOR_PIN, 1); // Turn ON indicator LED
                        printf("\n[WAKE] WakeWord '%s' heard! Status LED (Pin %d) turned ON. Speak action command within 3s...\n", cmd, GPIO_INDICATOR_PIN);
                    } else {
                        printf("\n[CLI Error] Speak WakeWord first ('AURA' or 'JARVIS'). Heard: '%s'\n", cmd);
                    }
                } 
                else if (cli_state == AURA_STATE_COMMAND) {
                    int matched = 0;
                    for (int i = 0; i < registered_commands_count; i++) {
                        if (strcasecmp(cmd, registered_commands[i].phrase) == 0) {
                            gpio_num_t pin = registered_commands[i].pin;
                            aura_action_t action = registered_commands[i].action;
                            gpio_set_level(pin, (int)action);
                            printf("\n[TRIGGER] Command '%s' matched -> Pin %d set to %s\n", 
                                   cmd, pin, action == AURA_ACTION_ON ? "HIGH" : "LOW");
                            matched = 1;
                            break;
                        }
                    }
                    
                    cli_state = AURA_STATE_WAKEWORD;
                    gpio_set_level(GPIO_INDICATOR_PIN, 0); // Turn OFF indicator LED
                    
                    if (matched) {
                        printf("[WAKE_OFF] Action complete. Status Pin %d set to LOW. Awaiting WakeWord...\n", GPIO_INDICATOR_PIN);
                    } else {
                        printf("\n[CLI Error] Command '%s' not matched. Returning to WakeWord state. Pin %d set to LOW.\n", cmd, GPIO_INDICATOR_PIN);
                    }
                }
                printf("AURA > ");
                fflush(stdout);
            }
            char_idx = 0;
        } else if (c != EOF && char_idx < sizeof(line) - 1) {
            line[char_idx++] = (char)c;
            putchar(c);
            fflush(stdout);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t aura_sr_init(void)
{
    ESP_LOGI(TAG, "Initializing indicator LED on GPIO %d...", GPIO_INDICATOR_PIN);
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GPIO_INDICATOR_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);
    gpio_set_level(GPIO_INDICATOR_PIN, 0); // Turn OFF

    ESP_LOGI(TAG, "Initializing AURA CLI Fallback for ESP32-C3...");
    return ESP_OK;
}

esp_err_t aura_sr_start(void)
{
    xTaskCreate(&serial_cli_task, "aura_cli", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "AURA CLI Task started successfully on ESP32-C3 fallback mode.");
    return ESP_OK;
}

#endif
