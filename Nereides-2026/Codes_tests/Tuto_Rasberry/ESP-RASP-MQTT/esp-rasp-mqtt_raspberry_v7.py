import serial
import time
import paho.mqtt.client as mqtt
import json
import csv
from datetime import datetime
import socket
import threading
import RPi.GPIO as GPIO #Importe la bibliothèque pour contrôler les GPIOs

#config gpio pour la led
GPIO.setmode(GPIO.BOARD) #Définit le mode de numérotation (Board)
GPIO.setwarnings(False) #On désactive les messages d'alerte

LED = 7 #Définit le numéro du port GPIO qui alimente la led
GPIO.setup(LED, GPIO.OUT) #Active le contrôle du GPIO

LED_connexion = 11 #Définit le numéro du port GPIO qui alimente la led
GPIO.setup(LED_connexion, GPIO.OUT) #Active le contrôle du GPIO



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

def on_connect(client, userdata, flags, rc, properties=None):
        global mqtt_connected
        mqtt_connected = True
        print("MQTT connecté")

def on_disconnect(client, userdata, rc, properties=None, packet_from_broker=None):
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
                GPIO.output(LED_connexion, GPIO.HIGH)
                return True
        except OSError:
                GPIO.output(LED_connexion, GPIO.LOW)
                return False


#permet de relancer la co si besoin
mqtt_loop_started = False
def mqtt_thread():
        global mqtt_connected, mqtt_loop_started
        while True:
                if network_ok():
                        if not mqtt_connected:
                                try:
                                        if not mqtt_loop_started:
                                                client.loop_start()
                                                mqtt_loop_started = True
                                        client.connect_async(BROKER, PORT, 60)
                                except Exception as e:
                                        print("MQTT erreur:", e)
                else:
                        mqtt_connected = False
                        GPIO.output(LED_connexion, GPIO.LOW)
                time.sleep(5)

threading.Thread(target=mqtt_thread, daemon=True).start() #ligne qui lance la fonction ci-dessus pour la co en arriere plan!


# Configuration CSV
csv_file = open("/home/nereides/data_telemetrie.csv", mode="a", newline="", buffering=1)
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
                                                GPIO.output(LED, GPIO.HIGH)
                                                csv_writer.writerow([timestamp, component, donnee, valeur])
                                                csv_file.flush()
                                                GPIO.output(LED, GPIO.LOW)
                                                if mqtt_connected:
                                                        try:
                                                                topic = f"raspberry26/{component}/{donnee}"
                                                                client.publish(topic, valeur)
                                                        except Exception:
                                                                mqtt_connected = False
                        except json.JSONDecodeError:
                                print("JSON invalide, ignoré:", data)

        except serial.SerialException:
                print("Port série perdu, tentative de reconnexion...")
                ser.close()
                ser = None
                time.sleep(2)
