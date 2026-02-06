import serial
import time
import paho.mqtt.client as mqtt
import json
import csv
from datetime import datetime
import socket
import threading
import RPi.GPIO as GPIO #Importe la bibliothèque pour contrôler les GPIOs
import requests

import os
def gui_available():
        return os.environ.get("DISPLAY") is not None
fenetre = None

#valeurs à afficher
labels={}
afficher_ecran = [
        ["batterie/SOC",0],
        ["CM/RPM",0]
        ]
#config affichage ecran
def quitter_ecran():
        stop_event.set()
        GPIO.cleanup()
        try:
                if ser:
                        ser.close()
        except:
                pass
        fenetre.destroy()


def start_gui():
        global fenetre, labels
        from tkinter import Tk, Label
        fenetre = Tk()
        # laisse X respirer
        fenetre.update_idletasks()
        fenetre.update()
        time.sleep(0.5)  # utile si écran branché au boot

        fenetre.attributes('-fullscreen', True)

        px_par_mm = fenetre.winfo_fpixels("1m")
        for i in afficher_ecran:
                print(i)
                labels[i[0]] = Label(fenetre, text=i[0]+ ": " +str(i[1]), font=(None,int(10*px_par_mm),'bold'))
                labels[i[0]].pack()
        fenetre.protocol("WM_DELETE_WINDOW", quitter_ecran)
        fenetre.mainloop()


stop_event = threading.Event()  # ← cree la variable globale pour l'arret au cas ou


def gui_watcher():
        gui_running = False

        while not stop_event.is_set():
                if gui_available() and not gui_running:
                        print("Écran détecté → lancement GUI")
                        threading.Thread(target=start_gui, daemon=True).start()
                        gui_running = True

                if not gui_available() and gui_running:
                        print("Écran perdu → arrêt GUI")
                        if fenetre:
                                fenetre.quit()
                        gui_running = False

        time.sleep(2)


#config pour l'envoie sur le google sheet
url_app_script = "https://script.google.com/macros/s/AKfycbzhZLerJqkhzzDLyG-WsIQsOeYdQV_sTxpjeH1vv4ZjSy9A3-Tq8aHbPNaxmjknmwavwg/exec"

def send_data_google(payload):
        try:
                response = requests.post(url_app_script, json=payload)
                print("envoie google ok")
        except Exception as e:
                print("Erreur envoi google:", e)


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

Wifi_connected = False
def network_ok():
        global Wifi_connected
        try:
                socket.create_connection(("8.8.8.8", 53), timeout=2)
                GPIO.output(LED_connexion, GPIO.HIGH)
                Wifi_connected = True
                return True
        except OSError:
                GPIO.output(LED_connexion, GPIO.LOW)
                Wifi_connected = False
                return False


#permet de relancer la co si besoin
mqtt_loop_started = False
def mqtt_thread():
        global mqtt_connected, mqtt_loop_started, Wifi_connected
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
                        Wifi_connected = False
                time.sleep(5)

threading.Thread(target=mqtt_thread, daemon=True).start() #ligne qui lance la fonction ci-dessus pour la co en arriere plan!


# Configuration CSV
csv_file = open("/home/nereides/data_telemetrie.csv", mode="a", newline="", buffering=1)
csv_writer = csv.writer(csv_file)
if csv_file.tell() == 0:
        csv_writer.writerow(["Timestamp", "Composant", "Grandeur mesurée", "Valeur"])



def main_loop():
        global stop_event
        global mqtt_connected
        global labels
        global ser
        # Vérifie si le port série est ouvert, sinon essaie de le reconnecter
        while not stop_event.is_set():  # boucle infinie stoppable
                if ser is None or not ser.is_open:
                        try:
                                ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
                                print("Port série connecté !")
                        except:
                                print("Port série non disponible, réessai dans 2s...")
                                ser = None
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
                                                        if any(item[0] == f"{component}/{donnee}" for item in afficher_ecran):
                                                                print(f"{component}/{donnee}:{valeur}")
                                                                index_ligne = next((i for i, item in enumerate(afficher_ecran) if item[0] == f"{component}/{donnee}"), None)
                                                                afficher_ecran[index_ligne][1] = valeur
                                                                if fenetre and f"{component}/{donnee}" in labels:
                                                                        labels[f"{component}/{donnee}"].config(text=f"{component}/{donnee} : {valeur}")

                                                        if mqtt_connected:
                                                                try:
                                                                        topic = f"raspberry26/{component}/{donnee}"
                                                                        client.publish(topic, valeur)
                                                                except Exception:
                                                                        mqtt_connected = False
                                        if Wifi_connected:
                                                threading.Thread(target=send_data_google, args=(payload,)).start()
                                except json.JSONDecodeError:
                                        print("JSON invalide, ignoré:", data)

                except serial.SerialException:
                        print("Port série perdu, tentative de reconnexion...")
                        ser.close()
                        ser = None
                        time.sleep(2)

threading.Thread(target=main_loop, daemon=True).start()
threading.Thread(target=gui_watcher, daemon=True).start()

while not stop_event.is_set():
    time.sleep(1)
