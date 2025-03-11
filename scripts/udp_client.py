#!/usr/bin/env python3
import socket
import time
import sys
import threading
import numpy as np
import wave
import os
from datetime import datetime

class UDPClient:
    def __init__(self, server_ip="192.168.4.1", server_port=5001):
        self.server_ip = server_ip
        self.server_port = server_port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(1.0)  # set the timeout to 1 second
        
        # status flags
        self.running = False
        self.connected = False
        self.receive_thread = None
        self.stats_thread = None
        
        # audio parameters
        self.sample_rate = 16000  # sampling rate
        self.wav_file = None
        self.total_bytes = 0
        self.last_update_time = time.time()
        self.bytes_since_last_update = 0
        
        # create a wav file with timestamp
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.wav_filename = f'audio_{timestamp}.wav'
        self.init_wav_file()

    def init_wav_file(self):
        if self.wav_file:
            self.wav_file.close()
            
        self.wav_file = wave.open(self.wav_filename, 'wb')
        self.wav_file.setnchannels(1)  # single channel
        self.wav_file.setsampwidth(2)  # 16-bit sampling
        self.wav_file.setframerate(self.sample_rate)
        print(f"Created new WAV file: {self.wav_filename}")

    def connect(self):
        print(f"Trying to connect to {self.server_ip}:{self.server_port}...")
        try:
            # send a initialization packet
            self.sock.sendto(b"hello", (self.server_ip, self.server_port))
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            return False

    def start_receiving(self):
        if not self.connect():
            return False

        self.running = True
        self.connected = True

        self.receive_thread = threading.Thread(target=self._receive_loop)
        self.receive_thread.daemon = True
        self.receive_thread.start()

        self.stats_thread = threading.Thread(target=self._stats_loop)
        self.stats_thread.daemon = True
        self.stats_thread.start()
        return True

    def _stats_loop(self):
        while self.running:
            time.sleep(0.2)  # update every 0.2 seconds
            current_time = time.time()
            
            elapsed = current_time - self.last_update_time
            bytes_per_second = self.bytes_since_last_update / elapsed if elapsed > 0 else 0
            
            # calculate the audio duration (seconds)
            audio_duration = self.total_bytes / (self.sample_rate * 2)  # 2 bytes/sample
            
            # show the data statistics
            print(f"\rReceived: {self.total_bytes/1024:.1f}KB "
                  f"({bytes_per_second/1024:.1f} KB/s) | "
                  f"Duration: {audio_duration:.1f}s", end='', flush=True)
            
            self.last_update_time = current_time
            self.bytes_since_last_update = 0

    def _receive_loop(self):
        while self.running:
            try:
                data, addr = self.sock.recvfrom(2048)  # increase the receive buffer to adapt to 480 sampling points
                
                # directly convert the received data to int16 array
                int16_data = np.frombuffer(data, dtype='<i2')
                
                
                if len(int16_data) >= 482 and int16_data[0] == 0 and int16_data[1] == 0:
                    int16_data = int16_data[2:]
                    print(f"\nSkipped 2 leading zeros, remaining samples: {len(int16_data)}")
                
                if len(int16_data) > 0:
                    print(f"First 8 samples: {int16_data[:8]}")
                    print(f"Data range: min={int16_data.min()}, max={int16_data.max()}, mean={int16_data.mean():.2f}")
                
                # only write the valid audio data (skip the first two samples)
                self.wav_file.writeframes(int16_data.tobytes())
                
                # update the statistics (use the actual audio data size)
                data_size = len(int16_data) * 2  # each sample is 2 bytes
                self.total_bytes += data_size
                self.bytes_since_last_update += data_size
                
            except socket.timeout:
                continue
            except Exception as e:
                print(f"\nError receiving data: {e}")
                time.sleep(1)  # wait one second after an error

    def close(self):
        self.running = False
        if self.receive_thread:
            self.receive_thread.join()
        if self.stats_thread:
            self.stats_thread.join()
        if self.wav_file:
            self.wav_file.close()
            print(f"\nSaved audio file: {self.wav_filename}")
        self.sock.close()

def main():
    if len(sys.argv) > 1:
        server_ip = sys.argv[1]
        client = UDPClient(server_ip=server_ip)
    else:
        client = UDPClient()
    
    try:
        if not client.start_receiving():
            print("Failed to start client")
            return
        
        print(f"UDP Client started, connecting to {client.server_ip}:{client.server_port}")
        print(f"Audio will be saved to: {client.wav_filename}")
        print("Press Ctrl+C to stop recording...")
        
        while True:
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\nStopping recording...")
    finally:
        client.close()

if __name__ == "__main__":
    main() 