# Règles des jeux — IDLAB Console

La console possède **5 boutons de couleur** (bleu, rouge, vert, jaune, blanc), un **bouton SELECT**, une **bande de 5 LEDs** et un **écran**.

**Commandes générales**
- **Menu** : appui **court** sur SELECT = item suivant · **Jaune** = item précédent · appui **long** sur SELECT = ouvrir/fermer le menu *Système* · un **autre bouton couleur** (bleu, rouge, vert, blanc) lance le jeu affiché.
- La **LED blanche** sur la bande indique ta position dans la liste ; un double bip grave signale que la liste a rebouclé.
- **Pendant un jeu** : SELECT quitte et revient au menu.
- **Réglages** (Système) : **Rouge = −**, **Vert = +**, **SELECT = valider**. « WiFi RAZ » demande confirmation (**Vert = oui**).

Le **meilleur score** de chaque jeu est sauvegardé et affiché en fin de partie (« RECORD ! » si tu le bats). Le menu *Système → Niveau* règle la difficulté (Facile / Normal / Difficile) de tous les jeux.

---

## 1. Simon — mémoire
Une séquence de couleurs s'affiche sur la LED centrale. Reproduis‑la **dans l'ordre** avec les boutons. Elle s'allonge à chaque réussite et accélère. **Score = longueur atteinte.**

## 2. Réflexe — vitesse
Une couleur s'affiche : appuie sur le **bon bouton** avant la fin du temps imparti, qui raccourcit à chaque manche. **Score = bonnes réponses d'affilée.**

## 3. Réaction — chronomètre
Les LEDs sont rouges. Dès qu'elles passent au **vert**, appuie le plus vite possible. Appuyer pendant le rouge = **faux départ**. Ton **temps en millisecondes** s'affiche.

## 4. Stop la lumière — précision
Un point lumineux fait des allers‑retours sur la bande. Appuie pour l'**arrêter pile sur la cible** (LED rouge sombre). Réussi → ça accélère. **Score = réussites.**

## 5. Code (Mastermind) — logique
Devine le **code secret de 3 couleurs** en 8 essais. Après chaque essai : LEDs **vertes** = bonne couleur **bien placée**, **oranges** = bonne couleur **mal placée**. **Score = essais restants si tu trouves.**

## 6. Jacques a dit — attention
Tape la couleur affichée **seulement si** le signal « Jacques a dit » (double flash cyan + jingle) a été donné. Sinon, **ne touche à rien** ! **Score = bonnes décisions.**

## 7. Duel — 2 joueurs
Joueur 1 = **bleu**, Joueur 2 = **rouge**. Au départ, choisis le mode : **Bleu = Normal**, **Rouge = Piège**. La bande est rouge : on attend. Au **vert**, le plus rapide marque le point. Appuyer trop tôt = point pour l'adversaire. En mode **Piège**, de fausses couleurs s'allument parfois avant le vert : celui qui tombe dans le panneau offre le point ! À 2‑2 c'est la **mort subite**. **Premier à 3 points gagne.**

## 8. Roulette — chance (jetons)
Tu as une **banque de jetons** sauvegardée. Chaque tour coûte **1 jeton** ; choisis ta couleur, la roue tourne et ralentit. Si elle s'arrête sur **ta couleur**, tu gagnes **5 jetons**. Banque vide → la console te reprête 5. **Record = ta plus grosse fortune.**

## 9. Stroop — jeu de cerveau
Un **mot de couleur** s'affiche, pendant que les LEDs montrent une **autre couleur** pour te tromper. Tape le bouton correspondant au **MOT** (pas aux LEDs). **Score = bonnes réponses.**

## 10. Tape-vite — rapidité
Après le décompte, appuie **le plus de fois possible en 5 secondes**… mais il faut **alterner les boutons** : deux fois de suite le même ne compte pas (bip grave) ! La jauge LED se remplit (1 LED = 8 appuis). **Score = nombre d'appuis valides.**

## 11. Pierre-Feuille-Ciseaux — match contre la console
**Bleu = Pierre**, **Rouge = Feuille**, **Vert = Ciseaux**. Match au **premier à 5 points** (les égalités ne comptent pas). Tes points en **bleu** à gauche, ceux de la console en **rouge** à droite. **3 manches gagnées d'affilée** = jingle bonus doré !

## 12. Séquence éclair — mémoire rapide
Une suite de couleurs défile **rapidement** sur la LED centrale. Mémorise‑la et reproduis‑la dans l'ordre. Elle s'allonge à chaque réussite. **Score = niveau atteint.**

## 13. Tir à la corde — 2 joueurs
Joueur 1 **martèle bleu**, joueur 2 **martèle rouge** : chaque appui tire la lumière blanche vers son camp. Le premier qui l'amène **tout au bout** de la bande gagne. Revanche possible à la fin !

## 14. Chrono caché — sens du temps
Au top départ, plus aucun repère : appuie pile quand tu penses que **5 secondes** se sont écoulées. Ton écart s'affiche en ms. **Score = précision (100 = parfait).**

## 15. Rythme — cadence
Écoute **4 battements**, puis continue à appuyer **en cadence** (n'importe quel bouton couleur). Trop tôt, trop tard ou battement raté = fin. Le tempo **accélère** tous les 8 battements. **Score = battements tenus.**

## 16. Mémoire inversée — le Simon retors
Une séquence s'affiche… mais tu dois la reproduire **à l'envers** (du dernier au premier) ! Elle s'allonge à chaque réussite. **Score = niveau atteint.**
