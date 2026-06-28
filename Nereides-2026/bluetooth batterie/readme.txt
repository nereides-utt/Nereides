le code scan bleutooth permet de detecter tt les appareils bleutooth aux environs; Adresse MAC et UUID
les codes reception_data_completes_batterie1 et reception_data_completes_batterie1 donnent dans le serial monitor tt les infos en francais de la batterie 1 ou 2

Batterie 1 : AA:C2:37:0A:B4:5B
Batterie 2 : aa:c2:37:0a:b0:dc


Ensuite, on l'envoie en can sur le bus, avec pour ID:
0x211 et 0x212 pour la batterie 1
0x221 et 0x222 pour la batterie 2
