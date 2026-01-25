import serial
import time
import paho.mqtt.client as mqtt
import json

# Configuration du port série
ser = serial.Serial('/dev/serial0', 115200, timeout=1)

BROKER = "broker.hivemq.com"
PORT = 8883
USERNAME = "Nereides26"
PASSWORD = "Tuyere52"


client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.username_pw_set(USERNAME, PASSWORD)

client.tls_set(
    ca_certs="/etc/ssl/certs/ca-certificates.crt"
)

client.connect(BROKER, PORT, 60)


while True:
        if ser.in_waiting:
                data = ser.readline().decode('utf-8').strip()
                if not data:
                        continue
                print("Reçu:", data)

                try:
                        payload = json.loads(data)
                        for component, mesures in payload.items():
                                for donnee, valeur in mesures.items():
                                        topic = f"raspberry26/{component}/{donnee}"
                                        client.publish(topic, valeur)
                except json.JSONDeecodeError:
                        print("JSON invalide, ignoré:", data)


