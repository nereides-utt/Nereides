import serial
import time

# Configuration du port série
ser = serial.Serial('/dev/serial0', 115200, timeout=1)

while True:
    ser.write(b'Hello ESP\n')  # Envoi de données
    time.sleep(1)
    if ser.in_waiting:
        data = ser.readline().decode('utf-8').strip()
        print("Reçu:", data)



