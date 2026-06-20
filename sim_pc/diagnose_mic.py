#!/usr/bin/env python3
import sys
import time
import math

try:
    import pyaudio
    import speech_recognition as sr
except ImportError:
    print("\n[ERROR] Missing dependencies! Run: pip install SpeechRecognition pyaudio\n")
    sys.exit(1)

def list_devices():
    p = pyaudio.PyAudio()
    info = p.get_host_api_info_by_index(0)
    numdevices = info.get('deviceCount')
    
    print("\n=== Available Audio Input Devices ===")
    default_device_index = -1
    try:
        default_info = p.get_default_input_device_info()
        default_device_index = default_info.get('index')
        print(f"Default Input Device index: {default_device_index} ({default_info.get('name')})\n")
    except IOError:
        print("No default input device found!\n")

    input_devices_found = False
    for i in range(0, numdevices):
        device_info = p.get_device_info_by_host_api_device_index(0, i)
        max_channels = device_info.get('maxInputChannels')
        if max_channels > 0:
            input_devices_found = True
            is_default = " (DEFAULT)" if i == default_device_index else ""
            print(f"Index {i}: {device_info.get('name')} [Channels: {max_channels}, Rate: {int(device_info.get('defaultSampleRate'))}Hz]{is_default}")
            
    p.terminate()
    if not input_devices_found:
        print("[ERROR] No input microphones detected by PyAudio. Make sure your microphone is connected and enabled in system settings.")
    return default_device_index

def test_recording(device_index):
    p = pyaudio.PyAudio()
    chunk_size = 1024
    sample_format = pyaudio.paInt16
    channels = 1
    fs = 16000
    seconds = 3

    print(f"\n=== Testing Audio Capture on Device Index {device_index} ===")
    print(f"Recording {seconds} seconds of audio to measure input level. Speak or make noise...")

    try:
        stream = p.open(format=sample_format,
                        channels=channels,
                        rate=fs,
                        frames_per_buffer=chunk_size,
                        input_device_index=device_index,
                        input=True)
    except Exception as e:
        print(f"[ERROR] Failed to open audio stream on device index {device_index}: {e}")
        p.terminate()
        return

    frames = []
    max_amplitude = 0
    
    # Record for 3 seconds
    for _ in range(0, int(fs / chunk_size * seconds)):
        try:
            data = stream.read(chunk_size, exception_on_overflow=False)
            frames.append(data)
            
            # Calculate peak amplitude in this chunk
            import struct
            count = len(data) / 2
            format_str = "%dh" % count
            shorts = struct.unpack(format_str, data)
            for sample in shorts:
                abs_val = abs(sample)
                if abs_val > max_amplitude:
                    max_amplitude = abs_val
        except Exception as e:
            print(f"Stream read error: {e}")
            break

    print("Finished recording.")
    
    # Close stream
    stream.stop_stream()
    stream.close()
    p.terminate()

    # Calculate loudness percentage (0 to 100%)
    # max possible amplitude for 16-bit signed is 32767
    loudness_pct = (max_amplitude / 32767.0) * 100
    print(f"Peak Amplitude captured: {max_amplitude} ({loudness_pct:.1f}% of maximum)")
    
    if max_amplitude == 0:
        print("[CRITICAL] Capture was COMPLETELY SILENT. Check physical microphone mute switch or Windows privacy permissions.")
    elif max_amplitude < 500:
        print("[WARNING] Volume level is extremely low. Check microphone gain/volume in Windows settings, or speak louder.")
    else:
        print("[SUCCESS] Audio signal captured successfully! Levels are healthy.")

def test_speech_rec(device_index):
    print("\n=== Testing Speech Recognition ===")
    recognizer = sr.Recognizer()
    
    # Use selected device
    mic = sr.Microphone(device_index=device_index)
    
    print("Calibrating ambient noise...")
    with mic as source:
        recognizer.adjust_for_ambient_noise(source, duration=1.5)
    
    print("Microphone ready. Say 'Hola AURA' clearly now...")
    with mic as source:
        try:
            audio = recognizer.listen(source, timeout=5, phrase_time_limit=3)
            print("Analyzing...")
            text = recognizer.recognize_google(audio, language="es-ES")
            print(f"[SUCCESS] Recognized text: '{text}'")
        except sr.WaitTimeoutError:
            print("[ERROR] Listening timed out. No speech detected.")
        except sr.UnknownValueError:
            print("[WARNING] Google could not understand the audio. Make sure you speak clearly and your microphone level is high enough.")
        except sr.RequestError as e:
            print(f"[ERROR] Connection to Google Speech API failed: {e}")
        except Exception as e:
            print(f"[ERROR] Unexpected error: {e}")

if __name__ == "__main__":
    default_idx = list_devices()
    if default_idx != -1:
        test_recording(default_idx)
        test_speech_rec(default_idx)
