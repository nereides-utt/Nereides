import serial
import time
import paho.mqtt.client as mqtt
import json

# Configuration du port série
SERIAL_PORT = "/dev/serial0"
BAUDRATE = 115200
ser = None

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
    # Vérifie si le port série est ouvert, sinon essaie de le reconnecter
    if ser is None or not ser.is_open:
        try:
            ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
            print("Port série connecté !")
        except serial.SerialException:
            print("Port série non disponible, réessai dans 2s...")
            time.sleep(2)
            continue

    try:
        if ser.in_waiting:
            data = ser.readline().decode('utf-8', errors='ignore').strip()
            if not data:
                continue
            print("Reçu:", data)

            try:
                payload = json.loads(data)
                for component, mesures in payload.items():
                    for donnee, valeur in mesures.items():
                        topic = f"raspberry26/{component}/{donnee}"
                        client.publish(topic, valeur)
            except json.JSONDecodeError:
                print("JSON invalide, ignoré:", data)

    except serial.SerialException:
        print("Port série perdu, tentative de reconnexion...")
        ser.close()
        ser = None
        time.sleep(2)
