#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "aura_sr.h"

#define MAX_COMMANDS 10
#define GPIO_INDICATOR_PIN 2

typedef enum {
    AURA_STATE_WAKEWORD = 0,
    AURA_STATE_COMMAND  = 1
} aura_state_t;

static aura_command_t registered_commands[MAX_COMMANDS];
static int registered_commands_count = 0;

static int simulated_gpio_states[40] = {0};
static aura_state_t current_state = AURA_STATE_WAKEWORD;
static time_t state_transition_time = 0;

esp_err_t aura_sr_register_action(gpio_num_t pin, const char *phrase, aura_action_t action)
{
    if (registered_commands_count >= MAX_COMMANDS) {
        return -1;
    }

    registered_commands[registered_commands_count].pin = pin;
    registered_commands[registered_commands_count].phrase = phrase;
    registered_commands[registered_commands_count].action = action;
    registered_commands_count++;

    printf("[SIM] Registered action: '%s' -> GPIO %d (%s)\n", 
           phrase, pin, action == AURA_ACTION_ON ? "HIGH" : "LOW");

    return 0;
}

esp_err_t aura_sr_init(void)
{
    printf("[SIM] Initializing AURA State Machine Speech Simulation on PC...\n");
    memset(simulated_gpio_states, 0, sizeof(simulated_gpio_states));
    current_state = AURA_STATE_WAKEWORD;
    return 0;
}

static void to_uppercase(char *str)
{
    for (int i = 0; str[i]; i++) {
        str[i] = toupper((unsigned char)str[i]);
    }
}

void process_input_line(const char* input)
{
    char cmd[128];
    strncpy(cmd, input, sizeof(cmd) - 1);
    cmd[sizeof(cmd) - 1] = '\0';
    
    // Trim leading whitespace
    char *start = cmd;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        start++;
    }
    
    // Trim trailing whitespace
    int len = strlen(start);
    while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t' || start[len - 1] == '\r' || start[len - 1] == '\n')) {
        start[len - 1] = '\0';
        len--;
    }
    
    if (strlen(start) == 0) {
        return;
    }

    // Check for Command state timeout before processing input
    if (current_state == AURA_STATE_COMMAND) {
        time_t now = time(NULL);
        if (now - state_transition_time >= 3) {
            current_state = AURA_STATE_WAKEWORD;
            simulated_gpio_states[GPIO_INDICATOR_PIN] = 0;
            printf("\n>>> [SIM TIMEOUT] 3 seconds elapsed. Status LED (GPIO %d) toggled to LOW (0). Awaiting WakeWord...\n\n", GPIO_INDICATOR_PIN);
        }
    }
    
    char uppercase_input[128];
    strncpy(uppercase_input, start, sizeof(uppercase_input) - 1);
    uppercase_input[sizeof(uppercase_input) - 1] = '\0';
    to_uppercase(uppercase_input);

    if (current_state == AURA_STATE_WAKEWORD) {
        if (strcmp(uppercase_input, "AURA") == 0 || strcmp(uppercase_input, "JARVIS") == 0) {
            current_state = AURA_STATE_COMMAND;
            state_transition_time = time(NULL);
            simulated_gpio_states[GPIO_INDICATOR_PIN] = 1;
            printf("\n>>> [SIM TRIGGER] WakeWord '%s' matched -> GPIO %d toggled to HIGH (1). listening for action commands for 3s...\n\n", 
                   start, GPIO_INDICATOR_PIN);
        } else {
            printf("[SIM WARNING] Speak WakeWord first ('AURA' or 'JARVIS'). Heard: '%s'\n", start);
        }
    } 
    else if (current_state == AURA_STATE_COMMAND) {
        int matched = 0;
        for (int i = 0; i < registered_commands_count; i++) {
            char expected_cmd[128];
            strncpy(expected_cmd, registered_commands[i].phrase, sizeof(expected_cmd) - 1);
            expected_cmd[sizeof(expected_cmd) - 1] = '\0';
            to_uppercase(expected_cmd);
            
            if (strcmp(uppercase_input, expected_cmd) == 0) {
                gpio_num_t pin = registered_commands[i].pin;
                aura_action_t action = registered_commands[i].action;
                simulated_gpio_states[pin] = (int)action;
                
                printf("\n>>> [SIM TRIGGER] Action Match: '%s' -> GPIO %d set to %s (Value: %d)\n", 
                       start, pin, action == AURA_ACTION_ON ? "HIGH" : "LOW", (int)action);
                
                matched = 1;
                break;
            }
        }
        
        current_state = AURA_STATE_WAKEWORD;
        simulated_gpio_states[GPIO_INDICATOR_PIN] = 0;
        
        if (matched) {
            printf(">>> [SIM STATUS] Action executed. Status LED (GPIO %d) toggled to LOW (0). Awaiting WakeWord...\n\n", GPIO_INDICATOR_PIN);
        } else {
            printf("\n>>> [SIM ERROR] Command '%s' not recognized. Returning to WakeWord state. Status LED (GPIO %d) toggled to LOW (0).\n\n", start, GPIO_INDICATOR_PIN);
        }
    }
}

esp_err_t aura_sr_start(void)
{
    printf("[SIM] AURA Engine Listening (Simulation Mode)...\n");
    printf("[SIM] Type WakeWord ('AURA' or 'JARVIS'), then type commands ('encender' or 'apagar').\n\n");
    
    char line[256];
    printf("AURA > ");
    fflush(stdout);
    
    while (fgets(line, sizeof(line), stdin)) {
        char exit_check[64];
        strncpy(exit_check, line, sizeof(exit_check) - 1);
        exit_check[sizeof(exit_check) - 1] = '\0';
        
        char *ec = exit_check;
        while(*ec == ' ') ec++;
        int el = strlen(ec);
        while(el > 0 && (ec[el-1] == '\r' || ec[el-1] == '\n' || ec[el-1] == ' ')) {
            ec[el-1] = '\0';
            el--;
        }
        
        if (strcasecmp(ec, "exit") == 0 || strcasecmp(ec, "quit") == 0) {
            printf("[SIM] Stopping simulation...\n");
            break;
        }
        
        process_input_line(line);
        printf("AURA > ");
        fflush(stdout);
    }
    return 0;
}
