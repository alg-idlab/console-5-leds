/* =====================================================================
   IDLAB CONSOLE  -  Version ESP32-C3 (01Space 0.42" OLED)
   PHASE 1 (ecran) + PHASE 2 (WiFi via WiFiManager + OTA GitHub)
   ---------------------------------------------------------------------
   - Ecran OLED : nom du jeu (menu) + score.
   - WiFi enregistre depuis le telephone (hotspot "IDLAB-Setup"),
     sans WiFi code en dur, modifiable a tout moment (menu SYSTEME).
   - Mise a jour OTA : la console telecharge le firmware depuis
     GitHub Releases (menu SYSTEME -> "MAJ").

   BROCHAGE (a recabler par rapport a l'Arduino) :
     - OLED SSD1306 0.42"  : I2C  SDA=GPIO5 , SCL=GPIO6
     - LEDs WS2812 (data)  : GPIO10
     - Buzzer              : GPIO7
     - SELECT=GPIO0  BLEU=GPIO1  ROUGE=GPIO3  VERT=GPIO4
       JAUNE=GPIO20(RX)  BLANC=GPIO21(TX)

   LIBRAIRIES (Gestionnaire de bibliotheques) :
     - U8g2 (olikraus)
     - FastLED (>= 3.6)
     - WiFiManager (tzapu)            <-- pour la phase 2
   Carte : "ESP32C3 Dev Module", USB CDC On Boot: Enabled.
   IMPORTANT : choisir un schema de partition AVEC OTA, par ex.
     "Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)".
   ===================================================================== */

#include <Wire.h>
#include <U8g2lib.h>
#include <FastLED.h>

// --- Reseau / OTA (phase 2) ---
#include <WiFi.h>
#include <WiFiManager.h>          // tzapu
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include <Preferences.h>

/* =====================================================================
   A PERSONNALISER POUR L'OTA
   Remplace USER/REPO par ton depot GitHub (ex: alg-idlab/console-5-leds).
   Les URL "latest/download" pointent toujours vers la derniere release.
   ===================================================================== */
#define FW_VERSION   "2.1.1"
const char* VERSION_URL  = "https://github.com/alg-idlab/console-5-leds/releases/latest/download/version.txt";
const char* FIRMWARE_URL = "https://github.com/alg-idlab/console-5-leds/releases/latest/download/firmware.bin";
const char* AP_NAME      = "IDLAB-Setup";   // hotspot de configuration WiFi
const char* AP_PASS      = "idlab1234";      // mot de passe du hotspot (>= 8 caracteres)

// Puissance d'emission WiFi reduite : evite les coupures dues aux pics de
// courant quand l'alim/cable sont un peu justes (cas tres frequent sur ces
// petites cartes). Pour une console posee pres de la box c'est large.
// Tu pourras remonter a WIFI_POWER_19_5dBm si tu ajoutes un gros
// condensateur (470-1000 uF) entre 3V3 et GND.
#define WIFI_TX_POWER WIFI_POWER_8_5dBm

/* =====================================================================
   ECRAN OLED 72x40 (mappe au centre du buffer 128x64 du SSD1306)
   ===================================================================== */
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, /*SCL=*/6, /*SDA=*/5);
const int OW = 72;
const int OH = 40;
const int OX = 30;
const int OY = 12;

/* =====================================================================
   CONFIGURATION MATERIEL
   ===================================================================== */
#define NUM_LEDS   5
#define DATA_PIN   10
#define BUZZER_PIN 7

#define BTN_SELECT 0
#define BTN_BLUE   1
#define BTN_RED    3
#define BTN_GREEN  4
#define BTN_YELLOW 20
#define BTN_WHITE  21

#define BRIGHTNESS_DEFAUT 25     // luminosite par defaut (2..80), reglable au menu
#define VOLUME_DEFAUT     100    // volume par defaut (0..100), reglable au menu

CRGB leds[NUM_LEDS];
const int CENTER = NUM_LEDS / 2;

// Reglages persistants (sauves en memoire flash, survivent au redemarrage).
Preferences prefs;
int ledBrightness  = BRIGHTNESS_DEFAUT;  // luminosite courante des LEDs
int buzzerVolume   = VOLUME_DEFAUT;      // volume courant du buzzer
int screenContrast = 255;                // contraste de l'ecran (10..255)
int sleepMinutes   = 0;                  // veille ecran : 0=off, sinon minutes
int difficulte     = 1;                  // 0=Facile 1=Normal 2=Difficile
unsigned long buzzerStopAt = 0;          // pour couper le buzzer (non bloquant)
unsigned long lastActivity = 0;          // dernier appui (pour la veille ecran)
int jeuCourant = 0;                      // index du jeu en cours (meilleurs scores)

/* =====================================================================
   SYSTEME DE BOUTONS  (anti-rebond + detection de front)
   Index : 0=BLEU 1=ROUGE 2=VERT 3=JAUNE 4=BLANC 5=SELECT
   ===================================================================== */
#define NB_BTN 6
const uint8_t BTN_PINS[NB_BTN] = {
  BTN_BLUE, BTN_RED, BTN_GREEN, BTN_YELLOW, BTN_WHITE, BTN_SELECT
};
const uint8_t IDX_SELECT = 5;
const unsigned long DEBOUNCE_MS = 25;

bool          btnStable[NB_BTN];
bool          btnLecture[NB_BTN];
unsigned long btnChangeT[NB_BTN];
bool          btnFront[NB_BTN];

void serviceBuzzer();   // prototype (defini plus bas)

void majBoutons() {
  serviceBuzzer();        // coupe le buzzer quand sa duree est ecoulee
  unsigned long t = millis();
  for (int i = 0; i < NB_BTN; i++) {
    bool lecture = (digitalRead(BTN_PINS[i]) == LOW);
    if (lecture != btnLecture[i]) { btnLecture[i] = lecture; btnChangeT[i] = t; }
    if (t - btnChangeT[i] > DEBOUNCE_MS && lecture != btnStable[i]) {
      btnStable[i] = lecture;
      if (lecture) btnFront[i] = true;
    }
  }
}
bool prendreAppui(int i) { if (btnFront[i]) { btnFront[i] = false; return true; } return false; }
void viderAppuis() { majBoutons(); for (int i = 0; i < NB_BTN; i++) btnFront[i] = false; }

const int EV_RIEN   = -1;
const int EV_SELECT = 5;

int lireEvenement() {
  majBoutons();
  for (int i = 0; i < NB_BTN; i++) {
    if (prendreAppui(i)) { beep(1200, 12); lastActivity = millis(); return i; }
  }
  return EV_RIEN;
}
bool estCouleur(int ev) { return ev >= 0 && ev <= 4; }

int attendreEvenement(unsigned long timeout) {
  unsigned long debut = millis();
  while (true) {
    int ev = lireEvenement();
    if (ev != EV_RIEN) return ev;
    if (timeout != 0 && millis() - debut >= timeout) return EV_RIEN;
  }
}

/* =====================================================================
   SON  (buzzer pilote en PWM/LEDC -> volume reglable)
   ---------------------------------------------------------------------
   Le volume vient du rapport cyclique : plus il est faible, plus le son
   est discret. beep() est non bloquant ; serviceBuzzer() (appele dans
   majBoutons et dans attendre) coupe le son a la fin de sa duree.
   ===================================================================== */
const int notesCouleur[5] = { 392, 440, 523, 587, 659 };

void beep(int freq, int dur) {
  if (freq > 0 && buzzerVolume > 0) {
    ledcWriteTone(BUZZER_PIN, freq);
    uint32_t duty = (uint32_t)map(buzzerVolume, 0, 100, 0, 127); // 127 = 50% = max sonore
    ledcWrite(BUZZER_PIN, duty);
    buzzerStopAt = (dur > 0) ? millis() + (unsigned long)dur : 0;
  } else {
    ledcWrite(BUZZER_PIN, 0);
    buzzerStopAt = 0;
  }
}
void beepStop() { ledcWrite(BUZZER_PIN, 0); buzzerStopAt = 0; }
void serviceBuzzer() { if (buzzerStopAt != 0 && millis() >= buzzerStopAt) beepStop(); }

// Pause non bloquante pour le buzzer : coupe le son a la fin de sa duree
// et cede la main aux taches systeme/WiFi (le delay(2) yield est sur).
void attendre(unsigned long ms) {
  unsigned long t = millis();
  while (millis() - t < ms) { serviceBuzzer(); delay(2); }
}

void son(int freq, int duree) { beep(freq, duree); attendre(duree); beepStop(); }
void melodieDepart()   { son(523, 90); son(659, 90); son(784, 140); }
void melodieVictoire() { son(523, 90); son(659, 90); son(784, 90); son(1047, 220); }
void melodieDefaite()  { son(330, 150); son(262, 150); son(196, 350); }
void jinglePalier(int score) {
  if (score <= 0)      { son(220, 180); son(196, 260); }
  else if (score < 5)  { son(523, 110); son(659, 170); }
  else if (score < 10) { son(523, 100); son(659, 100); son(784, 200); }
  else                 { son(523, 90);  son(659, 90);  son(784, 90); son(1047, 280); }
}

/* =====================================================================
   LEDs
   ===================================================================== */
void effacer() { FastLED.clear(); FastLED.show(); }
CRGB couleurDe(int idx) {
  switch (idx) {
    case 0: return CRGB::Blue;
    case 1: return CRGB::Red;
    case 2: return CRGB::Green;
    case 3: return CRGB::Yellow;
    case 4: return CRGB::White;
  }
  return CRGB::Black;
}
const char* couleurNom(int idx) {
  switch (idx) {
    case 0: return "Bleu";
    case 1: return "Rouge";
    case 2: return "Vert";
    case 3: return "Jaune";
    case 4: return "Blanc";
  }
  return "?";
}
int couleurAleatoire() { return random(0, 5); }

// Ajuste une duree selon la difficulte (Facile = plus de temps).
int dms(int ms) {
  if (difficulte == 0) return ms * 13 / 10;   // Facile
  if (difficulte == 2) return ms * 7 / 10;    // Difficile
  return ms;                                   // Normal
}
void flashTout(CRGB c, int freq, int duree) {
  fill_solid(leds, NUM_LEDS, c); FastLED.show();
  if (freq > 0) beep(freq, duree);
  attendre(duree); effacer();
}

/* =====================================================================
   ECRAN : helpers d'affichage
   ===================================================================== */
void texteCentre(const char* s, int y, const uint8_t* font) {
  u8g2.setFont(font);
  int w = u8g2.getStrWidth(s);
  int x = OX + (OW - w) / 2;
  if (x < OX) x = OX;
  u8g2.drawStr(x, OY + y, s);
}
void cadre() { u8g2.drawFrame(OX, OY, OW, OH); }

void hud2(const char* titre, const char* info) {
  u8g2.clearBuffer();
  cadre();
  texteCentre(titre, 13, u8g2_font_5x7_tr);
  texteCentre(info, 31, u8g2_font_7x13B_tr);
  u8g2.sendBuffer();
}
void hudScore(const char* titre, int score) {
  char b[16]; snprintf(b, sizeof(b), "Score %d", score);
  hud2(titre, b);
}
void splashJeu(const char* nom) {
  u8g2.clearBuffer();
  cadre();
  texteCentre(nom, 26, u8g2_font_7x13B_tr);
  u8g2.sendBuffer();
  attendre(800);
}
void afficherScore(int n) {
  jinglePalier(n);
  // Meilleur score du jeu en cours (sauvegarde en memoire flash).
  char cle[8]; snprintf(cle, sizeof(cle), "hs%d", jeuCourant);
  int record = prefs.getInt(cle, 0);
  bool nouveau = (n > record);
  if (nouveau) { record = n; prefs.putInt(cle, record); }

  u8g2.clearBuffer();
  cadre();
  texteCentre(nouveau ? "RECORD !" : "SCORE", 11, u8g2_font_5x7_tr);
  char b[8]; snprintf(b, sizeof(b), "%d", n);
  texteCentre(b, 30, u8g2_font_logisoso16_tn);
  char r[16]; snprintf(r, sizeof(r), "record %d", record);
  texteCentre(r, 39, u8g2_font_4x6_tr);
  u8g2.sendBuffer();
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = (n > 0) ? CRGB::Gold : CRGB::DarkRed;
    FastLED.show();
    attendre(80);
  }
  attendre(1800);
  effacer();
}

/* =====================================================================
   RESEAU : WiFi (WiFiManager) + OTA (HTTPUpdate sur GitHub)
   ===================================================================== */
WiFiManager wm;
bool wifiOk = false;

// Tentative de reconnexion silencieuse aux identifiants deja enregistres.
void wifiTryConnect(unsigned long timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_TX_POWER);   // puissance reduite -> connexion stable
  WiFi.begin();                 // reutilise les identifiants stockes
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < timeoutMs) {
    attendre(150);
  }
  wifiOk = (WiFi.status() == WL_CONNECTED);
}

// Ouvre le portail de configuration : le telephone se connecte au
// hotspot "IDLAB-Setup", une page web permet de choisir le reseau.
void wifiConfigPortal() {
  hud2("Config WiFi", AP_NAME);
  effacer();                         // LEDs eteintes : moins de courant pendant le WiFi
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_AP_STA);            // force explicitement le mode point d'acces
  WiFi.setSleep(false);              // desactive la veille radio -> AP visible et stable
  WiFi.setTxPower(WIFI_TX_POWER);    // puissance reduite -> AP qui emet malgre alim juste
  attendre(200);
  wm.setConfigPortalBlocking(true);
  wm.setConfigPortalTimeout(180);    // 3 min puis on abandonne
  bool ok;
  if (strlen(AP_PASS) >= 8) ok = wm.startConfigPortal(AP_NAME, AP_PASS);
  else                      ok = wm.startConfigPortal(AP_NAME);
  wifiOk = ok && (WiFi.status() == WL_CONNECTED);
  hud2("WiFi", wifiOk ? "Connecte" : "Echec");
  attendre(1500);
}

// Oublie le reseau enregistre (pour en mettre un autre ensuite).
void wifiReset() {
  hud2("WiFi", "Oubli...");
  wm.resetSettings();
  WiFi.disconnect(true, true);
  wifiOk = false;
  attendre(1200);
  hud2("WiFi", "Efface");
  attendre(1200);
}

// Lit un petit fichier texte en HTTPS (suit les redirections GitHub).
String httpGetTexte(const char* url) {
  WiFiClientSecure client;
  client.setInsecure();              // pas de verif de certificat (simple)
  HTTPClient http;
  String body = "";
  if (http.begin(client, url)) {
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    int code = http.GET();
    if (code == 200) body = http.getString();
    http.end();
  }
  body.trim();
  return body;
}

// GitHub ne sert pas le fichier directement : il REDIRIGE vers
// objects.githubusercontent.com. httpUpdate ne suit pas les redirections,
// donc on resout l'URL finale ici avant de telecharger.
String resoudreRedirection(const char* url) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) return "";
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  const char* entetes[] = { "Location" };
  http.collectHeaders(entetes, 1);
  int code = http.GET();
  String dest = "";
  if (code == 301 || code == 302 || code == 307 || code == 308) dest = http.header("Location");
  else if (code == 200) dest = String(url);   // deja l'URL finale
  http.end();
  return dest;
}

// Affiche la progression du telechargement OTA (throttle ~ tous les 5%).
void otaProgress(int courant, int total) {
  static int dernier = -1;
  int pct = total > 0 ? (courant * 100 / total) : 0;
  if (pct / 5 != dernier / 5) {
    dernier = pct;
    char b[12]; snprintf(b, sizeof(b), "%d%%", pct);
    hud2("MAJ", b);
  }
}

// Verifie s'il existe une version plus recente, et l'installe.
void verifierMaj() {
  effacer();                          // LEDs eteintes pendant la MAJ
  if (!wifiOk) { wifiTryConnect(6000); }
  if (!wifiOk) { hud2("MAJ", "Pas de WiFi"); attendre(1800); return; }

  hud2("MAJ", "Verif...");
  String latest = httpGetTexte(VERSION_URL);
  if (latest.length() == 0) { hud2("MAJ", "Err reseau"); attendre(1800); return; }
  if (latest == String(FW_VERSION)) { hud2("MAJ", "A jour"); attendre(1800); return; }

  hud2("Nouvelle", latest.c_str());
  attendre(1200);

  // Resout d'abord la redirection GitHub vers l'URL reelle du .bin.
  String urlBin = resoudreRedirection(FIRMWARE_URL);
  if (urlBin.length() == 0) urlBin = String(FIRMWARE_URL);

  WiFiClientSecure client;
  client.setInsecure();
  httpUpdate.rebootOnUpdate(true);
  httpUpdate.onProgress(otaProgress);
  hud2("MAJ", "Telech...");

  t_httpUpdate_return ret = httpUpdate.update(client, urlBin);
  if (ret == HTTP_UPDATE_FAILED) {
    char b[20]; snprintf(b, sizeof(b), "Echec %d", httpUpdate.getLastError());
    hud2("MAJ", b);
    attendre(3000);
  }
  // Si succes : redemarrage automatique sur le nouveau firmware.
}

/* =====================================================================
   ETATS / MENU
   ===================================================================== */
enum State { MENU, JEU };
State state = MENU;

const int NB_JEUX = 12;
const int NB_SYS  = 10;
const int NB_ITEMS = NB_JEUX + NB_SYS;
int  menuIndex = 0;       // jeu selectionne (menu principal)
int  sysIndex  = 0;       // entree systeme selectionnee (sous-menu)
bool sousMenuSys = false; // false = menu jeux, true = sous-menu systeme

const char* NOMS[NB_JEUX] = {
  "Simon", "Reflexe", "Reaction", "Stop",
  "Code", "Jacques", "Duel", "Roulette",
  "Stroop", "Tape vite", "PFC", "Eclair"
};
const char* NOMS_SYS[NB_SYS] = {
  "MAJ", "WiFi", "WiFi RAZ", "Lumiere", "Volume", "Contraste",
  "Veille", "Niveau", "Infos", "Scores"
};

CRGB themeJeu(int i) {
  switch (i) {
    case 0: return CRGB::Blue;
    case 1: return CRGB::Red;
    case 2: return CRGB::Green;
    case 3: return CRGB::Yellow;
    case 4: return CRGB::Magenta;
    case 5: return CRGB::Cyan;
    case 6: return CRGB::Orange;
    case 7: return CRGB::White;
    case 8: return CRGB(255, 0, 128);   // Stroop
    case 9: return CRGB(0, 200, 200);   // Tape vite
    case 10: return CRGB(150, 255, 0);  // PFC
    case 11: return CRGB(160, 0, 255);  // Eclair
  }
  return CRGB::White;
}
CRGB themeItem(int i) {
  if (i < NB_JEUX) return themeJeu(i);
  switch (i - NB_JEUX) {
    case 0: return CRGB(0, 150, 0);     // MAJ
    case 1: return CRGB(0, 80, 160);    // WiFi
    case 2: return CRGB(160, 0, 0);     // WiFi RAZ
    case 3: return CRGB(120, 120, 120); // Lumiere
    case 4: return CRGB(150, 90, 0);    // Volume
    case 5: return CRGB(0, 130, 130);   // Contraste
    case 6: return CRGB(80, 0, 130);    // Veille
    case 7: return CRGB(130, 50, 0);    // Niveau
    case 8: return CRGB(40, 90, 130);   // Infos
    case 9: return CRGB(90, 90, 0);     // Scores
  }
  return CRGB::White;
}
const char* nomItem(int i) {
  if (i < NB_JEUX) return NOMS[i];
  return NOMS_SYS[i - NB_JEUX];
}

void afficherMenu() {
  uint8_t puls = beatsin8(40, 120, 255);
  CRGB c; const char* nom; char buf[16];
  if (sousMenuSys) {
    c = themeItem(NB_JEUX + sysIndex);
    nom = NOMS_SYS[sysIndex];
    snprintf(buf, sizeof(buf), "SYS %d/%d", sysIndex + 1, NB_SYS);
  } else {
    c = themeItem(menuIndex);
    nom = NOMS[menuIndex];
    snprintf(buf, sizeof(buf), "JEU %d/%d", menuIndex + 1, NB_JEUX);
  }
  c.nscale8_video(puls);
  fill_solid(leds, NUM_LEDS, c);
  FastLED.show();

  u8g2.clearBuffer();
  cadre();
  texteCentre(buf, 12, u8g2_font_5x7_tr);
  texteCentre(nom, 30, u8g2_font_7x13B_tr);
  u8g2.setFont(u8g2_font_5x7_tr);
  if (wifiOk) u8g2.drawStr(OX + OW - 9, OY + 8, "W");
  u8g2.sendBuffer();
}

/* Navigation du menu :
   - appui COURT sur SELECT  -> passe au jeu (ou a l'entree systeme) suivant
   - appui LONG sur SELECT    -> bascule entre menu JEUX et sous-menu SYSTEME
   - bouton couleur           -> lance le jeu / l'entree selectionnee        */
void menuLoop() {
  // Veille ecran apres inactivite (anti burn-in)
  if (sleepMinutes > 0 && millis() - lastActivity > (unsigned long)sleepMinutes * 60000UL) {
    veille();
  }

  afficherMenu();
  majBoutons();

  // Lancement par un bouton couleur
  for (int i = 0; i <= 4; i++) {
    if (prendreAppui(i)) {
      lastActivity = millis();
      melodieDepart();
      lancer(sousMenuSys ? (NB_JEUX + sysIndex) : menuIndex);
      return;
    }
  }

  // Gestion de SELECT : court = defiler, long (>700 ms) = changer de menu
  static unsigned long selDown = 0;
  static bool selLong = false;
  if (btnStable[IDX_SELECT]) {
    if (selDown == 0) selDown = millis();
    if (!selLong && millis() - selDown > 700) {
      selLong = true;
      sousMenuSys = !sousMenuSys;
      lastActivity = millis();
      beep(700, 120);                 // bip grave = bascule de menu
    }
  } else {
    if (selDown != 0 && !selLong) {    // relache avant 700 ms = appui court
      lastActivity = millis();
      beep(350, 60);
      if (sousMenuSys) sysIndex  = (sysIndex + 1) % NB_SYS;
      else             menuIndex = (menuIndex + 1) % NB_JEUX;
    }
    selDown = 0;
    selLong = false;
  }
}

/* =====================================================================
   JEU 1 - SIMON
   ===================================================================== */
void jeuSimon() {
  const int MAX = 50; int seq[MAX]; int longueur = 1; int vitesse = dms(420);
  viderAppuis(); attendre(400);
  while (true) {
    seq[longueur - 1] = couleurAleatoire();
    hudScore("Simon", longueur - 1);
    for (int i = 0; i < longueur; i++) {
      leds[CENTER] = couleurDe(seq[i]); FastLED.show();
      beep(notesCouleur[seq[i]], vitesse); attendre(vitesse);
      effacer(); attendre(vitesse / 3);
      majBoutons(); if (prendreAppui(IDX_SELECT)) return;
    }
    for (int i = 0; i < longueur; i++) {
      int ev = attendreEvenement(0); if (ev == EV_SELECT) return;
      leds[CENTER] = couleurDe(ev); FastLED.show();
      beep(notesCouleur[ev], 120); attendre(150); effacer();
      if (ev != seq[i]) { melodieDefaite(); afficherScore(longueur - 1); return; }
    }
    longueur++;
    if (longueur >= MAX) { melodieVictoire(); afficherScore(longueur - 1); return; }
    if (vitesse > 180) vitesse -= 20;
    attendre(500);
  }
}

/* =====================================================================
   JEU 2 - REFLEXE EXPRESS
   ===================================================================== */
void jeuReflexe() {
  int score = 0; int fenetre = dms(1500);
  viderAppuis(); attendre(400);
  while (true) {
    hudScore("Reflexe", score);
    attendre(random(300, 900));
    int cible = couleurAleatoire();
    fill_solid(leds, NUM_LEDS, couleurDe(cible)); FastLED.show();
    beep(notesCouleur[cible], 80);
    int ev = attendreEvenement(fenetre); effacer();
    if (ev == EV_SELECT) return;
    if (ev == EV_RIEN || ev != cible) { melodieDefaite(); afficherScore(score); return; }
    score++; beep(880, 60);
    if (fenetre > 450) fenetre -= 90;
  }
}

/* =====================================================================
   JEU 3 - TEMPS DE REACTION
   ===================================================================== */
void jeuReaction() {
  viderAppuis();
  while (true) {
    hud2("Reaction", "Pret...");
    fill_solid(leds, NUM_LEDS, CRGB::Red); FastLED.show();
    unsigned long attente = random(1500, 4500);
    unsigned long debut = millis(); bool fauxDepart = false;
    while (millis() - debut < attente) {
      int ev = lireEvenement();
      if (ev == EV_SELECT) return;
      if (estCouleur(ev)) { fauxDepart = true; break; }
    }
    if (fauxDepart) {
      hud2("Reaction", "Trop tot!");
      flashTout(CRGB::Red, 150, 600); melodieDefaite();
      effacer(); attendre(600); continue;
    }
    fill_solid(leds, NUM_LEDS, CRGB::Green); FastLED.show();
    beep(880, 60);
    unsigned long t0 = millis();
    int ev = attendreEvenement(3000);
    unsigned long reaction = millis() - t0; effacer();
    if (ev == EV_SELECT) return;
    if (ev == EV_RIEN) { hud2("Reaction", "Rate"); melodieDefaite(); attendre(900); continue; }
    char b[16]; snprintf(b, sizeof(b), "%lu ms", reaction);
    hud2("Reaction", b);
    int note = (reaction < 200) ? 5 : (reaction < 280) ? 4 : (reaction < 380) ? 3 : (reaction < 500) ? 2 : 1;
    for (int i = 0; i < note; i++) { leds[i] = CRGB::Cyan; FastLED.show(); beep(500 + i * 120, 80); attendre(120); }
    attendre(1500); effacer();
  }
}

/* =====================================================================
   JEU 4 - STOP LA LUMIERE
   ===================================================================== */
void jeuStop() {
  int score = 0; int delaiPas = dms(220);
  viderAppuis(); attendre(400);
  while (true) {
    hudScore("Stop", score);
    int cible = random(0, NUM_LEDS); int pos = 0, dir = 1;
    while (true) {
      FastLED.clear();
      leds[cible] = CRGB(40, 0, 0);
      leds[pos]   = CRGB::White;
      FastLED.show();
      int ev = attendreEvenement(delaiPas);
      if (ev == EV_SELECT) return;
      if (estCouleur(ev)) {
        if (pos == cible) { score++; flashTout(CRGB::Green, 880, 250); if (delaiPas > 70) delaiPas -= 20; attendre(250); break; }
        else { melodieDefaite(); afficherScore(score); return; }
      }
      pos += dir;
      if (pos == NUM_LEDS - 1 || pos == 0) dir = -dir;
    }
  }
}

/* =====================================================================
   JEU 5 - MASTERMIND (Code secret)
   ===================================================================== */
void jeuMastermind() {
  const int LON = 3; const int ESSAIS = 8; int code[LON];
  for (int i = 0; i < LON; i++) code[i] = couleurAleatoire();
  viderAppuis();
  for (int i = 0; i < NUM_LEDS; i++) { leds[i] = CRGB::Magenta; FastLED.show(); attendre(60); }
  attendre(300); effacer();
  for (int essai = 0; essai < ESSAIS; essai++) {
    char b[16]; snprintf(b, sizeof(b), "Essai %d/%d", essai + 1, ESSAIS);
    hud2("Code", b);
    int prop[LON];
    for (int i = 0; i < LON; i++) {
      FastLED.clear();
      for (int k = 0; k <= i; k++) leds[k] = CRGB(60, 0, 60);
      FastLED.show();
      int ev = attendreEvenement(0); if (ev == EV_SELECT) return;
      prop[i] = ev; leds[i] = couleurDe(ev); FastLED.show();
      beep(notesCouleur[ev], 100); attendre(200);
    }
    bool prisCode[LON] = {false, false, false};
    bool prisProp[LON] = {false, false, false};
    int bienPlaces = 0, malPlaces = 0;
    for (int i = 0; i < LON; i++) if (prop[i] == code[i]) { bienPlaces++; prisCode[i] = true; prisProp[i] = true; }
    for (int i = 0; i < LON; i++) {
      if (prisProp[i]) continue;
      for (int j = 0; j < LON; j++) if (!prisCode[j] && prop[i] == code[j]) { malPlaces++; prisCode[j] = true; break; }
    }
    if (bienPlaces == LON) { melodieVictoire(); afficherScore(ESSAIS - essai); return; }
    char r[16]; snprintf(r, sizeof(r), "%d bien %d mal", bienPlaces, malPlaces);
    hud2("Code", r);
    FastLED.clear();
    int p = 0;
    for (int i = 0; i < bienPlaces && p < NUM_LEDS; i++, p++) leds[p] = CRGB::Green;
    for (int i = 0; i < malPlaces  && p < NUM_LEDS; i++, p++) leds[p] = CRGB::Orange;
    FastLED.show();
    beep(400 + bienPlaces * 150, 200); attendre(2200); effacer();
  }
  hud2("Code", "Perdu!");
  melodieDefaite();
  for (int i = 0; i < LON; i++) { leds[i] = couleurDe(code[i]); FastLED.show(); attendre(400); }
  attendre(1500); effacer();
}

/* =====================================================================
   JEU 6 - JACQUES A DIT
   ===================================================================== */
void jeuJacquesADit() {
  int score = 0; int fenetre = dms(1400);
  viderAppuis(); attendre(500);
  while (true) {
    hudScore("Jacques", score);
    attendre(random(400, 1000));
    bool jacques = (random(0, 100) < 60);
    if (jacques) { for (int k = 0; k < 2; k++) { flashTout(CRGB::Cyan, 784, 90); attendre(70); } }
    int couleur = couleurAleatoire();
    fill_solid(leds, NUM_LEDS, couleurDe(couleur)); FastLED.show();
    beep(notesCouleur[couleur], 80);
    int ev = attendreEvenement(fenetre); effacer();
    if (ev == EV_SELECT) return;
    bool ok = jacques ? (ev == couleur) : (ev == EV_RIEN);
    if (!ok) { melodieDefaite(); afficherScore(score); return; }
    score++; beep(880, 60);
    if (fenetre > 600) fenetre -= 50;
  }
}

/* =====================================================================
   JEU 7 - DUEL 2 JOUEURS  (J1=BLEU, J2=ROUGE)
   ===================================================================== */
void jeuDuel() {
  const int J1 = 0; const int J2 = 1; int score1 = 0, score2 = 0;
  viderAppuis(); attendre(500);
  while (true) {
    char b[16]; snprintf(b, sizeof(b), "%d  -  %d", score1, score2);
    hud2("Duel J1/J2", b);
    fill_solid(leds, NUM_LEDS, CRGB::Red); FastLED.show();
    unsigned long attente = random(1500, 4500);
    unsigned long debut = millis(); int gagnant = -1; bool fauxDepart = false;
    while (millis() - debut < attente) {
      int ev = lireEvenement();
      if (ev == EV_SELECT) return;
      if (ev == J1) { gagnant = J2; fauxDepart = true; break; }
      if (ev == J2) { gagnant = J1; fauxDepart = true; break; }
    }
    if (!fauxDepart) {
      fill_solid(leds, NUM_LEDS, CRGB::Green); FastLED.show(); beep(880, 80);
      while (gagnant == -1) {
        int ev = lireEvenement();
        if (ev == EV_SELECT) return;
        if (ev == J1) gagnant = J1;
        if (ev == J2) gagnant = J2;
      }
    } else { flashTout(CRGB::Red, 150, 400); }
    if (gagnant == J1) score1++; else score2++;
    FastLED.clear();
    for (int i = 0; i < score1; i++) leds[i]               = CRGB::Blue;
    for (int i = 0; i < score2; i++) leds[NUM_LEDS - 1 - i] = CRGB::Red;
    FastLED.show();
    son((gagnant == J1) ? 700 : 500, 250); attendre(900);
    if (score1 >= 3 || score2 >= 3) {
      CRGB c = (score1 >= 3) ? CRGB::Blue : CRGB::Red;
      hud2("Duel", (score1 >= 3) ? "J1 gagne!" : "J2 gagne!");
      melodieVictoire();
      for (int k = 0; k < 3; k++) { flashTout(c, 0, 250); attendre(150); }
      return;
    }
    attendre(300);
  }
}

/* =====================================================================
   JEU 8 - ROULETTE (chance)
   ===================================================================== */
void jeuRoulette() {
  viderAppuis();
  while (true) {
    hud2("Roulette", "Ta couleur?");
    int pari = -1; int pos = 0;
    while (pari == -1) {
      FastLED.clear();
      leds[pos % NUM_LEDS] = couleurDe(pos % 5);
      FastLED.show(); pos++;
      int ev = attendreEvenement(90);
      if (ev == EV_SELECT) return;
      if (estCouleur(ev)) pari = ev;
    }
    char b[16]; snprintf(b, sizeof(b), "Mise %s", couleurNom(pari));
    hud2("Roulette", b);
    flashTout(couleurDe(pari), notesCouleur[pari], 350); attendre(300);
    int resultat = couleurAleatoire();
    int tours = 14 + random(0, NUM_LEDS); int delai = 50;
    for (int s = 0; s < tours; s++) {
      int idx = s % NUM_LEDS;
      FastLED.clear(); leds[idx] = couleurDe(idx % 5); FastLED.show();
      beep(300 + idx * 40, 25); attendre(delai);
      if (s > tours - 8) delai += 35;
    }
    fill_solid(leds, NUM_LEDS, couleurDe(resultat)); FastLED.show();
    char r[16]; snprintf(r, sizeof(r), "%s", couleurNom(resultat));
    if (resultat == pari) {
      hud2("Gagne!", r); melodieVictoire();
      for (int k = 0; k < 3; k++) { flashTout(couleurDe(resultat), 0, 200); attendre(120); }
    } else {
      hud2("Perdu", r); melodieDefaite();
    }
    attendre(900); effacer(); attendre(300);
  }
}

/* =====================================================================
   REGLAGES : luminosite des LEDs et volume du buzzer
   ROUGE = diminuer, VERT = augmenter, SELECT = valider et sortir.
   Les valeurs sont sauvees en memoire flash.
   ===================================================================== */
void reglerLuminosite() {
  while (true) {
    FastLED.setBrightness(ledBrightness);
    fill_solid(leds, NUM_LEDS, CRGB::White);   // apercu en direct
    FastLED.show();
    char b[16]; snprintf(b, sizeof(b), "Lumiere %d", ledBrightness);
    hud2("Rouge- / Vert+", b);
    int ev = attendreEvenement(0);
    if (ev == EV_SELECT) break;
    if (ev == 1) ledBrightness -= 5;   // ROUGE
    if (ev == 2) ledBrightness += 5;   // VERT
    if (ledBrightness < 2)  ledBrightness = 2;
    if (ledBrightness > 80) ledBrightness = 80;
  }
  prefs.putInt("bright", ledBrightness);
  effacer();
}

void reglerVolume() {
  while (true) {
    char b[16]; snprintf(b, sizeof(b), "Volume %d", buzzerVolume);
    hud2("Rouge- / Vert+", b);
    int ev = attendreEvenement(0);
    if (ev == EV_SELECT) break;
    if (ev == 1) buzzerVolume -= 10;   // ROUGE
    if (ev == 2) buzzerVolume += 10;   // VERT
    if (buzzerVolume < 0)   buzzerVolume = 0;
    if (buzzerVolume > 100) buzzerVolume = 100;
    beep(880, 200);                    // apercu sonore au nouveau volume
  }
  prefs.putInt("vol", buzzerVolume);
}

void reglerContraste() {
  while (true) {
    u8g2.setContrast(screenContrast);          // apercu en direct
    char b[16]; snprintf(b, sizeof(b), "Contraste %d", screenContrast);
    hud2("Rouge- / Vert+", b);
    int ev = attendreEvenement(0);
    if (ev == EV_SELECT) break;
    if (ev == 1) screenContrast -= 15;   // ROUGE
    if (ev == 2) screenContrast += 15;   // VERT
    if (screenContrast < 10)  screenContrast = 10;
    if (screenContrast > 255) screenContrast = 255;
  }
  prefs.putInt("contrast", screenContrast);
}

/* =====================================================================
   VEILLE ECRAN (anti burn-in) : eteint ecran + LEDs, reveil au 1er appui
   ===================================================================== */
void veille() {
  u8g2.setPowerSave(1);
  effacer();
  while (true) {
    majBoutons();
    bool press = false;
    for (int i = 0; i < NB_BTN; i++) if (prendreAppui(i)) press = true;
    if (press) break;
    delay(20);
  }
  u8g2.setPowerSave(0);
  lastActivity = millis();
}

/* =====================================================================
   ECRAN INFOS / A PROPOS
   ===================================================================== */
void afficherInfos() {
  effacer();
  u8g2.clearBuffer();
  cadre();
  u8g2.setFont(u8g2_font_4x6_tr);
  char l[24];
  u8g2.drawStr(OX + 3, OY + 8, "IDLAB  v" FW_VERSION);
  snprintf(l, sizeof(l), "WiFi: %s", wifiOk ? "connecte" : "non");
  u8g2.drawStr(OX + 3, OY + 16, l);
  if (wifiOk) {
    snprintf(l, sizeof(l), "IP %s", WiFi.localIP().toString().c_str());
    u8g2.drawStr(OX + 3, OY + 24, l);
    String s = WiFi.SSID(); if (s.length() > 15) s = s.substring(0, 15);
    snprintf(l, sizeof(l), "net %s", s.c_str());
    u8g2.drawStr(OX + 3, OY + 32, l);
  }
  snprintf(l, sizeof(l), "RAM %u o", (unsigned)ESP.getFreeHeap());
  u8g2.drawStr(OX + 3, OY + 39, l);
  u8g2.sendBuffer();
  attendreEvenement(0);   // un appui pour sortir
}

/* =====================================================================
   MEILLEURS SCORES (lecture)
   ===================================================================== */
void afficherScores() {
  for (int i = 0; i < NB_JEUX; i++) {
    char cle[8]; snprintf(cle, sizeof(cle), "hs%d", i);
    int record = prefs.getInt(cle, 0);
    char b[16]; snprintf(b, sizeof(b), "%d", record);
    hud2(NOMS[i], b);
    int ev = attendreEvenement(0);
    if (ev == EV_SELECT) return;     // SELECT quitte, une couleur = suivant
  }
}

/* =====================================================================
   REGLAGES : veille ecran et difficulte
   ===================================================================== */
void reglerVeille() {
  const int vals[4] = { 0, 1, 5, 15 };
  while (true) {
    char b[16];
    if (sleepMinutes == 0) snprintf(b, sizeof(b), "Off");
    else                   snprintf(b, sizeof(b), "%d min", sleepMinutes);
    hud2("Veille ecran", b);
    int ev = attendreEvenement(0);
    if (ev == EV_SELECT) break;
    int idx = 0; for (int i = 0; i < 4; i++) if (vals[i] == sleepMinutes) idx = i;
    if (ev == 1) sleepMinutes = vals[(idx + 3) % 4];   // ROUGE
    if (ev == 2) sleepMinutes = vals[(idx + 1) % 4];   // VERT
  }
  prefs.putInt("sleepm", sleepMinutes);
  lastActivity = millis();
}

void reglerDifficulte() {
  const char* noms[3] = { "Facile", "Normal", "Difficile" };
  while (true) {
    hud2("Niveau", noms[difficulte]);
    int ev = attendreEvenement(0);
    if (ev == EV_SELECT) break;
    if (ev == 1) difficulte = (difficulte + 2) % 3;   // ROUGE
    if (ev == 2) difficulte = (difficulte + 1) % 3;   // VERT
  }
  prefs.putInt("diff", difficulte);
}

/* =====================================================================
   JEU - STROOP (jeu de cerveau)
   Un MOT de couleur s'affiche, les LEDs montrent une autre couleur pour
   te tromper : tape le bouton correspondant au MOT (pas aux LEDs).
   ===================================================================== */
void jeuStroop() {
  int score = 0; int fenetre = dms(1600);
  viderAppuis(); attendre(400);
  while (true) {
    hudScore("Stroop", score);
    attendre(random(300, 700));
    int mot = couleurAleatoire();
    int led = couleurAleatoire();
    fill_solid(leds, NUM_LEDS, couleurDe(led)); FastLED.show();
    u8g2.clearBuffer(); cadre();
    texteCentre("Tape le MOT", 12, u8g2_font_5x7_tr);
    texteCentre(couleurNom(mot), 30, u8g2_font_7x13B_tr);
    u8g2.sendBuffer();
    int ev = attendreEvenement(fenetre);
    effacer();
    if (ev == EV_SELECT) return;
    if (ev != mot) { melodieDefaite(); afficherScore(score); return; }
    score++; beep(880, 60);
    if (fenetre > 650) fenetre -= 60;
  }
}

/* =====================================================================
   JEU - TAPE VITE : appuie le plus possible en 5 secondes
   ===================================================================== */
void jeuTapeVite() {
  viderAppuis();
  for (int i = 3; i >= 1; i--) {
    char b[4]; snprintf(b, sizeof(b), "%d", i);
    hud2("Pret ?", b); beep(440, 120); attendre(700);
  }
  hud2("Tape vite!", "GO!"); beep(880, 120);
  fill_solid(leds, NUM_LEDS, CRGB::Green); FastLED.show();
  int coups = 0;
  unsigned long debut = millis();
  while (millis() - debut < 5000) {
    int ev = lireEvenement();
    if (ev == EV_SELECT) { effacer(); return; }
    if (estCouleur(ev)) {
      coups++;
      char b[8]; snprintf(b, sizeof(b), "%d", coups);
      hud2("Tape!", b);
    }
  }
  effacer();
  melodieVictoire();
  afficherScore(coups);
}

/* =====================================================================
   JEU - PIERRE FEUILLE CISEAUX contre la console
   Bleu = Pierre, Rouge = Feuille, Vert = Ciseaux. SELECT pour quitter.
   ===================================================================== */
void jeuPFC() {
  const char* noms[3] = { "Pierre", "Feuille", "Ciseaux" };
  viderAppuis();
  while (true) {
    hud2("Pierre/Feu/Cis", "Bleu Rouge Vert");
    int joueur = -1;
    while (joueur == -1) {
      int ev = attendreEvenement(0);
      if (ev == EV_SELECT) return;
      if (ev >= 0 && ev <= 2) joueur = ev;
    }
    int console = random(0, 3);
    int res;  // 0 nul, 1 gagne, 2 perd
    if (joueur == console) res = 0;
    else if ((joueur == 0 && console == 2) || (joueur == 1 && console == 0) || (joueur == 2 && console == 1)) res = 1;
    else res = 2;
    char l[16]; snprintf(l, sizeof(l), "%c vs %c", "PFC"[joueur], "PFC"[console]);
    hud2(res == 0 ? "Egalite" : (res == 1 ? "Gagne!" : "Perdu"), l);
    if (res == 1) { melodieVictoire(); flashTout(CRGB::Green, 0, 300); }
    else if (res == 2) { melodieDefaite(); flashTout(CRGB::Red, 0, 300); }
    else { beep(500, 150); }
    attendre(1300);
  }
}

/* =====================================================================
   JEU - SEQUENCE ECLAIR : memorise une suite rapide et reproduis-la
   ===================================================================== */
void jeuSequenceEclair() {
  const int MAX = 30; int seq[MAX]; int longueur = 2;
  viderAppuis(); attendre(400);
  while (true) {
    for (int i = 0; i < longueur; i++) seq[i] = couleurAleatoire();
    char b[16]; snprintf(b, sizeof(b), "Niveau %d", longueur - 1);
    hud2("Eclair", b); attendre(700);
    for (int i = 0; i < longueur; i++) {
      leds[CENTER] = couleurDe(seq[i]); FastLED.show();
      beep(notesCouleur[seq[i]], 120);
      attendre(dms(200));
      effacer(); attendre(70);
    }
    hud2("Eclair", "A toi!");
    for (int i = 0; i < longueur; i++) {
      int ev = attendreEvenement(0);
      if (ev == EV_SELECT) return;
      leds[CENTER] = couleurDe(ev); FastLED.show();
      beep(notesCouleur[ev], 100); attendre(120); effacer();
      if (ev != seq[i]) { melodieDefaite(); afficherScore(longueur - 1); return; }
    }
    beep(700, 80);
    longueur++;
    if (longueur >= MAX) { melodieVictoire(); afficherScore(longueur - 1); return; }
    attendre(400);
  }
}

/* =====================================================================
   AIGUILLAGE (jeux + entrees systeme)
   ===================================================================== */
void lancer(int index) {
  if (index < NB_JEUX) {
    jeuCourant = index;
    splashJeu(NOMS[index]);
    switch (index) {
      case 0:  jeuSimon();          break;
      case 1:  jeuReflexe();        break;
      case 2:  jeuReaction();       break;
      case 3:  jeuStop();           break;
      case 4:  jeuMastermind();     break;
      case 5:  jeuJacquesADit();    break;
      case 6:  jeuDuel();           break;
      case 7:  jeuRoulette();       break;
      case 8:  jeuStroop();         break;
      case 9:  jeuTapeVite();       break;
      case 10: jeuPFC();            break;
      case 11: jeuSequenceEclair(); break;
    }
  } else {
    switch (index - NB_JEUX) {
      case 0: verifierMaj();      break;   // MAJ
      case 1: wifiConfigPortal(); break;   // WiFi
      case 2: wifiReset();        break;   // WiFi RAZ
      case 3: reglerLuminosite(); break;   // Lumiere
      case 4: reglerVolume();     break;   // Volume
      case 5: reglerContraste();  break;   // Contraste
      case 6: reglerVeille();     break;   // Veille
      case 7: reglerDifficulte(); break;   // Niveau
      case 8: afficherInfos();    break;   // Infos
      case 9: afficherScores();   break;   // Scores
    }
  }
  effacer();
  state = MENU;
  viderAppuis();
}

/* =====================================================================
   SETUP & LOOP
   ===================================================================== */
void setup() {
  Serial.begin(115200);
  wm.setDebugOutput(true);          // logs WiFiManager sur le moniteur serie

  u8g2.begin();
  u8g2.setContrast(255);
  u8g2.setBusClock(400000);

  // Reglages persistants (luminosite + volume)
  prefs.begin("idlab", false);
  ledBrightness  = prefs.getInt("bright",   BRIGHTNESS_DEFAUT);
  buzzerVolume   = prefs.getInt("vol",      VOLUME_DEFAUT);
  screenContrast = prefs.getInt("contrast", 255);
  sleepMinutes   = prefs.getInt("sleepm",   0);
  difficulte     = prefs.getInt("diff",     1);
  u8g2.setContrast(screenContrast);
  lastActivity = millis();

  // Buzzer en PWM/LEDC (necessaire pour le reglage de volume)
  ledcAttach(BUZZER_PIN, 2000, 8);

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(ledBrightness);

  for (int i = 0; i < NB_BTN; i++) pinMode(BTN_PINS[i], INPUT_PULLUP);

  randomSeed(esp_random());
  effacer();

  // Ecran d'accueil
  u8g2.clearBuffer();
  cadre();
  texteCentre("IDLAB", 15, u8g2_font_7x13B_tr);
  texteCentre("CONSOLE", 28, u8g2_font_7x13B_tr);
  texteCentre("v" FW_VERSION, 38, u8g2_font_4x6_tr);
  u8g2.sendBuffer();
  attendre(1000);

  // Reconnexion WiFi silencieuse (si un reseau a deja ete enregistre).
  hud2("WiFi", "...");
  wifiTryConnect(4000);

  effacer();
}

void loop() {
  menuLoop();   // le menu gere navigation et lancement (jeux + systeme)
}
