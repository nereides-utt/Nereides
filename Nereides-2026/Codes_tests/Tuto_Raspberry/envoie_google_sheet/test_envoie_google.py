import json
import requests
import threading

url = "https://script.google.com/macros/s/AKfycbzhZLerJqkhzzDLyG-WsIQsOeYdQV_sTxpjeH1vv4ZjSy9A3-Tq8aHbPNaxmjknmwavwg/exec" //URL à changer, donnée par l'app script quand on le déploie
payload = {
    "sensor1": {"temperature": 2303, "humidity": 5880},
    "sensor2": {"pressure": 'yru'},
    "sensor3": {"luminosity": 40050},
    "tt":{"jj":500}
}


def send_data_google(payload):
        try:
                response = requests.post(url, json=payload)
                print("envoie google ok")
        except Exception as e:
                print("Erreur envoi google:", e)

threading.Thread(target=send_data_google, args=(payload,)).start()
