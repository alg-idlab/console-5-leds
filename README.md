# IDLAB Console

Mini‑console de jeux **sans manette ni écran classique** : un écran OLED 0.42", une bande de 5 LEDs adressables, 5 boutons de couleur, un bouton SELECT et un buzzer. Basée sur un **ESP32‑C3** : elle se met à jour **par WiFi** (OTA) sans rebrancher le PC.

Projet du **FabLab IDLAB**.

## Jeux

12 jeux : Simon, Réflexe, Réaction, Stop la lumière, Code (Mastermind), Jacques a dit, Duel 2 joueurs, Roulette, Stroop, Tape‑vite, Pierre‑Feuille‑Ciseaux, Séquence éclair.
👉 Règles détaillées dans **[REGLES.md](REGLES.md)**.

## Matériel et branchements (ESP32‑C3 0.42" OLED)

| Fonction | Broche |
|---|---|
| Écran OLED (I²C) | SDA = GPIO5, SCL = GPIO6 |
| LEDs WS2812 (data) | GPIO10 |
| Buzzer | GPIO7 |
| Bouton SELECT | GPIO0 |
| Bouton Bleu | GPIO1 |
| Bouton Rouge | GPIO3 |
| Bouton Vert | GPIO4 |
| Bouton Jaune | GPIO20 (RX) |
| Bouton Blanc | GPIO21 (TX) |

Les boutons sont en `INPUT_PULLUP` (l'autre patte au GND). On évite GPIO2/8/9 (broches de démarrage).

> Astuce alimentation : ces petites cartes peuvent manquer de courant à l'émission WiFi. La puissance d'émission est volontairement bridée dans le code (`WIFI_TX_POWER`). Pour la pleine portée, ajouter un condensateur 470–1000 µF entre 3V3 et GND.

## Compilation (IDE Arduino)

1. **Carte** : installer « esp32 » (Espressif) dans le gestionnaire de cartes ; choisir *ESP32C3 Dev Module*.
2. **Bibliothèques** : U8g2 (olikraus), FastLED (≥ 3.6), WiFiManager (tzapu).
3. **Options carte** : *USB CDC On Boot: Enabled*, et **Partition Scheme : Minimal SPIFFS (1.9MB APP with OTA)** — indispensable pour l'OTA.
4. Téléverser en USB (si l'upload ne part pas : maintenir BOOT, appuyer RST, relâcher BOOT ; Upload Speed 115200).

## Réglages (menu Système — appui long sur SELECT)

Mise à jour OTA, configuration WiFi, oubli WiFi, luminosité, volume, contraste, veille écran, niveau de difficulté, écran Infos, meilleurs scores. Tous les réglages sont **sauvegardés** (survivent au redémarrage et aux mises à jour).

## WiFi et mises à jour (OTA)

La configuration WiFi se fait depuis un téléphone (hotspot de la console), et les mises à jour se publient sur GitHub.
👉 Procédure pas à pas dans **[GUIDE_WIFI_OTA.md](GUIDE_WIFI_OTA.md)**.

En résumé pour publier une version :
1. Augmenter `FW_VERSION` dans le code.
2. *Croquis → Exporter les binaires compilés* → renommer `..._C3.ino.bin` en **`firmware.bin`**.
3. Mettre le même numéro dans **`version.txt`**.
4. Créer une **Release** GitHub avec ces deux fichiers en pièces jointes.
5. Sur la console : *Système → MAJ*.

## Organisation du dépôt

```
console-5-leds/
├── README.md            ← cette page (s'affiche sur l'accueil GitHub)
├── REGLES.md            ← règles des jeux
├── GUIDE_WIFI_OTA.md    ← guide WiFi + mises à jour
├── version.txt          ← version courante (aussi jointe aux Releases)
└── AmauryConsole_C3.ino ← code source
```
- Les fichiers `.md` et le code vont **dans le dépôt** (onglet Code).
- Le **`firmware.bin`** compilé et **`version.txt`** se mettent en **pièces jointes des Releases** (onglet Releases), c'est ce que la console télécharge.

## Versions

Le numéro de version est dans `FW_VERSION` (et affiché au démarrage). GitHub conserve automatiquement tout l'historique (commits + Releases) : pas besoin de dupliquer les fichiers pour garder les anciennes versions.

Version actuelle : **2.1.1**.
