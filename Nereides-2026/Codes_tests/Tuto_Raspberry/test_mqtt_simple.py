import paho.mqtt.client as mqtt
import json
import time
import random

BROKER = "broker.hivemq.com"
PORT = 8883
USERNAME = "Nereides26"
PASSWORD = "Tuyere52"
TOPIC = "raspberry26/temperature"

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.username_pw_set(USERNAME, PASSWORD)

client.tls_set(
    ca_certs="/etc/ssl/certs/ca-certificates.crt"
)

client.connect(BROKER, PORT, 60)

while True:
    data = {
        "temperature": 22.5,
        "SOC": random.randint(0,500),
        "device": "raspberrypi",
        "timestamp": int(time.time())
        }
    client.publish(TOPIC, json.dumps(data))
    print("Envoyé :", data)
    time.sleep(1)


