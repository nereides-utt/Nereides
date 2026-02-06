                                                                                                                                                                                                                                                                                                                                                                                                                                                             envoie_data.py                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  import serial                                                                                                                                                                                                                                                                                                                                                                                                                                                                        envoie_data.py
import time
import paho.mqtt.client as mqtt
import json
import csv
from datetime import datetime
import socket
import threading

# Configuration du port série
SERIAL_PORT = "/dev/serial0"
BAUDRATE = 115200
ser = None

BROKER = "broker.hivemq.com"
PORT = 8883
USERNAME = "Nereides26"
PASSWORD = "Tuyere52"


#--MQTT--
mqtt_connected = False
last_mqtt_try = 0

def on_connect(client, userdata, flags, rc, properties=None):
    global mqtt_connected
    mqtt_connected = True
    print("MQTT connecté")

def on_disconnect(client, userdata, rc):
    global mqtt_connected
    mqtt_connected = False
    print("MQTT déconnecté")

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.username_pw_set(USERNAME, PASSWORD)
client.tls_set(ca_certs="/etc/ssl/certs/ca-certificates.crt")
client.on_connect = on_connect
client.on_disconnect = on_disconnect


def network_ok():
    try:
        socket.create_connection(("8.8.8.8", 53), timeout=2)
        return True
    except OSError:
        return False

def connect_mqtt():
    if mqtt_connected:
        return
    if network_ok():
        try:
            client.connect_async(BROKER, PORT, 60)
            client.loop_start()
        except Exception:
            pass


#permet de relancer la co si besoin
def mqtt_thread():
    global mqtt_connected
    while True:
        if not mqtt_connected and network_ok():
            try:
                client.connect_async(BROKER, PORT, 60)
                client.loop_start()
            except Exception:
                pass
        time.sleep(5)  # Vérifie la connexion toutes les 5 secondes

threading.Thread(target=mqtt_thread, daemon=True).start() #ligne qui lance la fonction ci-dessus pour la co en arriere plan!


# Configuration CSV
csv_file = open("/home/nereides/data_telemetrie.csv", mode="a", newline="")
csv_writer = csv.writer(csv_file)
if csv_file.tell() == 0:
        csv_writer.writerow(["Timestamp", "Composant", "Grandeur mesurée", "Valeur"])

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
        #verifie connection mqtt
#       if not mqtt_connected and time.time() - last_mqtt_try > 5:
#               connect_mqtt()
#               last_mqtt_try = time.time()

        try:
                if ser.in_waiting:
                        data = ser.readline().decode('utf-8', errors='ignore').strip()
                        if not data:
                                continue
                        print("Reçu:", data)

                        try:
                                payload = json.loads(data)
                                timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                                for component, mesures in payload.items():
                                        for donnee, valeur in mesures.items():
                                                csv_writer.writerow([timestamp, component, donnee, valeur])
                                                csv_file.flush()
                                                if mqtt_connected:
                                                        topic = f"raspberry26/{component}/{donnee}"
                                                        client.publish(topic, valeur)

                        except json.JSONDecodeError:
                                print("JSON invalide, ignoré:", data)

        except serial.SerialException:
                print("Port série perdu, tentative de reconnexion...")
                ser.close()
                ser = None
                time.sleep(2)

