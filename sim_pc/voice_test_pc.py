#!/usr/bin/env python3
"""
AURA Speech Recognition Simulator - Voice Test Wrapper
Requirements:
    pip install SpeechRecognition pyaudio

This script captures microphone audio, runs speech recognition (in Spanish),
and pipes the recognized text to the compiled C simulator to demonstrate
how the ESP32 code behaves in real-time.
"""

import sys
import os
import subprocess
import time

try:
    import speech_recognition as sr
except ImportError:
    print("\n[ERROR] Missing python dependencies!")
    print("Please install them by running: pip install SpeechRecognition pyaudio\n")
    sys.exit(1)

def main():
    # Detect the compiled simulator executable or fallback to python version
    exe_name = "aura_sim"
    if os.name == 'nt':
        exe_name = "aura_sim.exe"
        
    sim_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), exe_name)
    py_sim_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "aura_sim.py")
    
    use_python_sim = False
    if not os.path.exists(sim_path):
        if os.path.exists(py_sim_path):
            print("[INFO] Compiled C simulator binary not found. Falling back to python simulator...")
            use_python_sim = True
        else:
            print(f"\n[ERROR] Neither compiled simulator nor 'aura_sim.py' found.")
            sys.exit(1)

    print("==========================================================")
    # Start the simulator process
    if use_python_sim:
        p = subprocess.Popen([sys.executable, py_sim_path], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0)
    else:
        p = subprocess.Popen([sim_path], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0)
    
    # Let the C simulator print its startup messages
    time.sleep(0.5)
    # Read and print the simulator header
    startup_output = os.read(p.stdout.fileno(), 1024).decode('utf-8', errors='ignore')
    print(startup_output, end="")
    
    # Check if a device index was passed as command line argument
    device_index = None
    if len(sys.argv) > 1:
        try:
            device_index = int(sys.argv[1])
            print(f"[VOICE] Using user-specified Microphone Device Index: {device_index}")
        except ValueError:
            print("[USAGE] To select a specific microphone, run: python voice_test_pc.py <device_index>")
            
    # Initialize microphone speech recognition
    recognizer = sr.Recognizer()
    microphone = sr.Microphone(device_index=device_index)
    
    print("\n[VOICE] Calibrating microphone for ambient noise...")
    with microphone as source:
        recognizer.adjust_for_ambient_noise(source, duration=1)
    
    print("[VOICE] Calibration complete. Microphone ready!")
    print("[VOICE] Start speaking now. Try saying 'Aura encender' or 'Aura apagar'.\n")
    print("Say 'salir' or 'exit' to quit the tool.")
    
    try:
        while True:
            print("\n[VOICE] Listening...")
            with microphone as source:
                try:
                    audio = recognizer.listen(source, timeout=8, phrase_time_limit=4)
                except sr.WaitTimeoutError:
                    continue
            
            print("[VOICE] Analyzing speech...")
            try:
                # Recognize Spanish speech
                text = recognizer.recognize_google(audio, language="es-ES")
                print(f"[VOICE] Recognized Text: '{text}'")
                
                # Check for exit commands
                if text.lower() in ["salir", "exit", "quit", "terminar"]:
                    print("[VOICE] Exiting...")
                    p.stdin.write(b"exit\n")
                    p.stdin.flush()
                    break
                
                # Format text: we replace punctuation and ensure spelling match
                clean_text = text.lower().replace(",", "").replace(".", "").strip()
                
                # Split words and feed them to the simulator sequentially
                words = clean_text.split()
                for word in words:
                    p.stdin.write(f"{word}\n".encode('utf-8'))
                    p.stdin.flush()
                    time.sleep(0.15) # pause to allow simulator state transition
                
                # Wait briefly for simulator output
                time.sleep(0.3)
                output = os.read(p.stdout.fileno(), 2048).decode('utf-8', errors='ignore')
                # Filter out the "AURA > " prompt from the C simulator output to keep it clean
                clean_output = output.replace("AURA >", "").strip()
                if clean_output:
                    print(f"\n{clean_output}")
                    
            except sr.UnknownValueError:
                print("[VOICE] Could not understand audio. Speak clearly.")
            except sr.RequestError as e:
                print(f"[VOICE] Error requesting results; {e}")
                
    except KeyboardInterrupt:
        print("\n[VOICE] Voice test stopped.")
    finally:
        p.terminate()

if __name__ == '__main__':
    main()
