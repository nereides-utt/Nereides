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

#valeurs à afficher sur l'ecran; en rajouter sous le meme format au besoin !
labels={}
afficher_ecran = [
        ["batterie/SOC",0], #["composant/grandeur",0], comme sur l'esp; 0 est la valeur de cette grandeur
        ["CM/RPM",0]
        ]
#config affichage ecran
def quitter_ecran(): #pour fermer l'écran proprement qd on coupe le programme
        stop_event.set()
        GPIO.cleanup()
        try:
                if ser:
                        ser.close()
        except:
                pass
        fenetre.destroy()

def start_gui(): #pour lancer l'affichage de la fenetre graphique
        global fenetre, labels
        from tkinter import Tk, Label
        fenetre = Tk()

        fenetre.update_idletasks()
        fenetre.update()
        time.sleep(0.5)  #pour le laisser charger avec plein ecran

        fenetre.attributes('-fullscreen', True)

        px_par_mm = fenetre.winfo_fpixels("1m")
        for i in afficher_ecran: #generer le dictionnaire label et les labels
                print(i)
                labels[i[0]] = Label(fenetre, text=i[0]+ ": " +str(i[1]), font=(None,int(10*px_par_mm),'bold'))
                labels[i[0]].pack()
        fenetre.protocol("WM_DELETE_WINDOW", quitter_ecran)
        fenetre.mainloop()


stop_event = threading.Event()  #cree la variable globale pour l'arret au cas ou


def gui_watcher(): #permet de détecter le branchement / la présence d'un écran
        gui_running = False

        while not stop_event.is_set():
                if gui_available() and not gui_running:
                        print("Écran détecté: lancement GUI")
                        threading.Thread(target=start_gui, daemon=True).start()
                        gui_running = True

                if not gui_available() and gui_running:
                        print("Écran perdu: arrêt GUI")
                        if fenetre:
                                fenetre.quit()
                        gui_running = False

        time.sleep(2)


#config pour l'envoie sur le google sheet
url_app_script = "https://script.google.com/macros/s/AKfycbzhZLerJqkhzzDLyG-WsIQsOeYdQV_sTxpjeH1vv4ZjSy9A3-Tq8aHbPNaxmjknmwavwg/exec"

def send_data_google(payload): #pour envoyer sur le google sheet
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



# Configuration du port série (UART avec l'ESP)
SERIAL_PORT = "/dev/serial0"
BAUDRATE = 115200
ser = None

#Config broker MQTT (hivemq ici, plan gratuit, donc sans stockage)
BROKER = "broker.hivemq.com"
PORT = 8883
USERNAME = "Nereides26"
PASSWORD = "Tuyere52" #j'crois ca sert a rien ici, c'est sans stockage


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
def network_ok(): #check si on a de la connexion internet
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
def mqtt_thread(): #pour se connecter au broker MQTT
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
                time.sleep(5) #attend 5 s, sans rien faire; ne bloque pas le prog, car lance depuis un thread

threading.Thread(target=mqtt_thread, daemon=True).start() #ligne qui lance la fonction ci-dessus pour la co en "arriere plan" du programme (comme ca, ca n'impacte pas le reste du prog, avec les sleep)!


# Configuration CSV
csv_file = open("/home/nereides/data_telemetrie.csv", mode="a", newline="", buffering=1)
csv_writer = csv.writer(csv_file)
if csv_file.tell() == 0:
        csv_writer.writerow(["Timestamp", "Composant", "Grandeur mesurée", "Valeur"])

#et notre boucle principale (main=principale tkt j'ai valide le02); ici s'execute la recolte et transfert de notre precieuse data
def main_loop():
        global stop_event
        global mqtt_connected
        global labels
        global ser
        while not stop_event.is_set():  # boucle infinie stoppable
                # Vérifie si le port série est ouvert, sinon essaie de le reconnecter
                if ser is None or not ser.is_open:
                        try:
                                ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
                                print("Port série connecté !")
                        except:
                                print("Port série non disponible, réessai dans 2s...")
                                ser = None
                                time.sleep(2)
                                continue

                try: #essaye de lire la data
                        if ser.in_waiting:
                                data = ser.readline().decode('utf-8', errors='ignore').strip()
                                if not data:
                                        continue
                                print("Reçu:", data)

                                try: #essaye de la pack dans un JSON (ca peut plnater si data corrompue)
                                        payload = json.loads(data)
                                        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                                        for component, mesures in payload.items():
                                                for donnee, valeur in mesures.items():
                                                        GPIO.output(LED, GPIO.HIGH) #pour faire clignoter la led, et informer de la reception
                                                        csv_writer.writerow([timestamp, component, donnee, valeur]) #ecriture dans le csv
                                                        csv_file.flush() #"enregistrement" du csv (pas hyper utile je crois, mais bon)
                                                        GPIO.output(LED, GPIO.LOW)
                                                        if any(item[0] == f"{component}/{donnee}" for item in afficher_ecran): #si dans la liste afficher_ecran, on l'affiche
                                                                print(f"{component}/{donnee}:{valeur}")
                                                                index_ligne = next((i for i, item in enumerate(afficher_ecran) if item[0] == f"{component}/{donnee}"), None) #pour trouver l'index dans le dico
                                                                afficher_ecran[index_ligne][1] = valeur #pour mettre a jour la valeur dans le dico, avec index
                                                                if fenetre and f"{component}/{donnee}" in labels: #si fenetre graphique active
                                                                        labels[f"{component}/{donnee}"].config(text=f"{component}/{donnee} : {valeur}") #on change le label

                                                        if mqtt_connected: #si mqtt, on envoie !!!
                                                                try:
                                                                        topic = f"raspberry26/{component}/{donnee}"
                                                                        client.publish(topic, valeur)
                                                                except Exception:
                                                                        mqtt_connected = False
                                        if Wifi_connected: #si wifi, on envoie a google !
                                                threading.Thread(target=send_data_google, args=(payload,)).start()
                                except json.JSONDecodeError:
                                        print("JSON invalide, ignoré:", data)

                except serial.SerialException:
                        print("Port série perdu, tentative de reconnexion...")
                        ser.close()
                        ser = None
                        time.sleep(2)

threading.Thread(target=main_loop, daemon=True).start() #lancer le main_loop
threading.Thread(target=gui_watcher, daemon=True).start() #lancer pour la recherche de l'ecran

while not stop_event.is_set():
    time.sleep(1)
  
