#include <stdio.h>
#include "aura_sr.h"

int main(void)
{
    printf("====================================================\n");
    printf("  AURA Speech Recognition Library - PC Simulator  \n");
    printf("====================================================\n");

    if (aura_sr_init() != 0) {
        printf("Failed to initialize AURA library simulation!\n");
        return 1;
    }

    // Register GPIO pin 2 (the simulated LED)
    // Keyword "encender" -> HIGH
    // Keyword "apagar" -> LOW
    aura_sr_register_action(2, "encender", AURA_ACTION_ON);
    aura_sr_register_action(2, "apagar", AURA_ACTION_OFF);

    // Start simulation loop (listens to commands via stdin)
    aura_sr_start();

    return 0;
}
