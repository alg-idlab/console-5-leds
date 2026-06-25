# Guide WiFi + mise à jour OTA — IDLAB Console (ESP32‑C3)

## 1. Préparer l'ESP32-C3 (à faire une fois)

- **Bibliothèque** : Outils → Gérer les bibliothèques → installer **WiFiManager** (par *tzapu*).
- **Partition avec OTA** : Outils → *Partition Scheme* → **Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)**.
  C'est indispensable : l'OTA a besoin de deux emplacements firmware.
- **Carte** : *ESP32C3 Dev Module*, *USB CDC On Boot: Enabled*, Flash 4 Mo.

## 2. Personnaliser le code

En haut du sketch `.ino`, remplace `USER/REPO` par ton dépôt GitHub :

```
const char* VERSION_URL  = "https://github.com/TON_USER/TON_REPO/releases/latest/download/version.txt";
const char* FIRMWARE_URL = "https://github.com/TON_USER/TON_REPO/releases/latest/download/firmware.bin";
```

`FW_VERSION` est le numéro de la version **actuellement installée**. Tu l'incrémenteras à chaque nouvelle version.

## 3. Premier flash (par USB)

Branche la carte et téléverse normalement, une seule fois. À partir de là, les mises à jour pourront se faire sans fil.

## 4. Enregistrer le WiFi depuis le téléphone

1. Dans le menu, appuie sur SELECT jusqu'à **SYSTEME → WiFi**, puis un bouton couleur.
2. La console crée un point d'accès **IDLAB-Setup**.
3. Sur le téléphone, connecte-toi au WiFi *IDLAB-Setup* le mot de passe de base est idlab1234. Une page s'ouvre (sinon va sur `192.168.4.1`).
4. Choisis ton réseau, saisis le mot de passe, valide.
5. La console retient le réseau (même après extinction). Un petit **W** apparaît dans le menu quand elle est connectée.

Pour changer de réseau plus tard : **SYSTEME → WiFi RAZ** (oublie l'ancien), puis refais l'étape 4.

## 5. Publier une mise à jour

1. Dans le code, **augmente** `FW_VERSION` (ex. `"1.0.0"` → `"1.0.1"`).
2. Arduino : **Croquis → Exporter les binaires compilés**. Récupère le fichier `.bin` généré (dossier du croquis) et renomme‑le **`firmware.bin`**.
3. Crée un fichier texte **`version.txt`** contenant exactement le même numéro (ex. `1.0.1`), sans rien d'autre.
4. Sur GitHub : crée une **Release**, et ajoute les deux fichiers (`firmware.bin` et `version.txt`) en *assets*.
   Les URL `latest/download/...` pointent automatiquement vers la dernière release.

## 6. Mettre à jour la console

Menu → **SYSTEME → MAJ** → un bouton couleur.
La console lit `version.txt` ; si le numéro diffère de `FW_VERSION`, elle télécharge `firmware.bin`, affiche la progression en %, puis redémarre sur la nouvelle version. Sinon elle affiche « A jour ».

## Notes

- La vérification HTTPS du certificat est désactivée (`setInsecure`) pour rester simple. C'est acceptable pour un projet perso ; on pourra la durcir plus tard.
- Les téléchargements de release GitHub passent par une redirection : le code la suit déjà. Si jamais l'OTA échoue dessus, on basculera l'hébergement sur GitHub Pages.
- Le Bluetooth/BLE n'est pas utilisé ici (WiFiManager passe par le WiFi), ce qui garde le firmware léger.
