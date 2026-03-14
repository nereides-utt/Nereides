# 📦 Librairie Capteurs

Cette librairie a pour but de regrouper les codes des différents capteurs en encapsulant leur codes dans des classes.
Cela a pour avantage de rendre les scripts utilisant différents composants à la fois plus lisibles, faciles à utiliser et à modifier.

On a par exemple pour la sonde de température :

```cpp
#include "SondeTempClass.hpp"

SondeTemp sonde(4, 0x13, printing=true); //Pin de la sonde, ID de la trame CAN utilisée, affichage dans le Serial de l'état de la sonde.

void setup() {
  

}

void loop() {
  sonde.update()
  sonde.sendDataViaCan();
}
```

Cela permet de rendre le code transféré vers l'ESP plus clair, et de bien séparer le code des différents composants.
