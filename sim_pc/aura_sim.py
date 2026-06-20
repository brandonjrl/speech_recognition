#!/usr/bin/env python3
import sys
import time

class AuraAction:
    OFF = 0
    ON = 1

class AuraState:
    WAKEWORD = 0
    COMMAND = 1

class AuraCommand:
    def __init__(self, pin, phrase, action):
        self.pin = pin
        self.phrase = phrase
        self.action = action

# Settings
registered_commands = []
simulated_gpio_states = [0] * 40
GPIO_INDICATOR_PIN = 3

current_state = AuraState.WAKEWORD
state_transition_time = 0.0

def aura_sr_register_action(pin, phrase, action):
    registered_commands.append(AuraCommand(pin, phrase, action))
    action_str = "HIGH" if action == AuraAction.ON else "LOW"
    print(f"[SIM] Registered action: '{phrase}' -> GPIO {pin} ({action_str})")

def aura_sr_init():
    global current_state, state_transition_time
    print("[SIM] Initializing AURA State Machine Speech Simulation on PC...")
    current_state = AuraState.WAKEWORD
    state_transition_time = 0.0
    for i in range(len(simulated_gpio_states)):
        simulated_gpio_states[i] = 0
    return 0

def process_input_line(input_line):
    global current_state, state_transition_time
    start = input_line.strip()
    if not start:
        return
    
    # Check for Command state timeout
    if current_state == AuraState.COMMAND:
        if time.time() - state_transition_time >= 3:
            current_state = AuraState.WAKEWORD
            simulated_gpio_states[GPIO_INDICATOR_PIN] = 0
            print("\n>>> [SIM TIMEOUT] 3 seconds elapsed. Status LED (GPIO 3) toggled to LOW (0). Awaiting WakeWord...\n")
            
    uppercase_input = start.upper()
    
    if current_state == AuraState.WAKEWORD:
        if uppercase_input in ["AURA", "JARVIS"]:
            current_state = AuraState.COMMAND
            state_transition_time = time.time()
            simulated_gpio_states[GPIO_INDICATOR_PIN] = 1
            print(f"\n>>> [SIM TRIGGER] WakeWord '{start}' matched -> GPIO {GPIO_INDICATOR_PIN} toggled to HIGH (1). Listening for action commands for 3s...\n")
        else:
            print(f"[SIM WARNING] Speak WakeWord first ('AURA' or 'JARVIS'). Heard: '{start}'")
            
    elif current_state == AuraState.COMMAND:
        matched = False
        for cmd in registered_commands:
            if uppercase_input == cmd.phrase.upper():
                simulated_gpio_states[cmd.pin] = cmd.action
                action_str = "HIGH" if cmd.action == AuraAction.ON else "LOW"
                print(f"\n>>> [SIM TRIGGER] Action Match: '{start}' -> GPIO {cmd.pin} set to {action_str} (Value: {cmd.action})")
                matched = True
                break
                
        current_state = AuraState.WAKEWORD
        simulated_gpio_states[GPIO_INDICATOR_PIN] = 0
        
        if matched:
            print(">>> [SIM STATUS] Action executed. Status LED (GPIO 3) toggled to LOW (0). Awaiting WakeWord...\n")
        else:
            print(f"\n>>> [SIM ERROR] Command '{start}' not recognized. Returning to WakeWord state. Status LED (GPIO 3) toggled to LOW (0).\n")

def aura_sr_start():
    print("[SIM] AURA Engine Listening (Simulation Mode)...")
    print("[SIM] Type WakeWord ('AURA' or 'JARVIS'), then type commands ('encender' or 'apagar').\n")
    
    try:
        while True:
            sys.stdout.write("AURA > ")
            sys.stdout.flush()
            line = sys.stdin.readline()
            if not line:
                break
            
            ec = line.strip()
            if ec.lower() in ["exit", "quit"]:
                print("[SIM] Stopping simulation...")
                break
                
            process_input_line(line)
    except KeyboardInterrupt:
        print("\n[SIM] Stopping simulation...")

def main():
    print("====================================================")
    print("  AURA Speech Recognition Library - PC Simulator  ")
    print("====================================================")
    
    aura_sr_init()
    
    # Register GPIO pin 2 (the simulated LED)
    # Keyword "encender" -> HIGH
    # Keyword "apagar" -> LOW
    aura_sr_register_action(2, "encender", AuraAction.ON)
    aura_sr_register_action(2, "apagar", AuraAction.OFF)
    
    aura_sr_start()

if __name__ == '__main__':
    main()
