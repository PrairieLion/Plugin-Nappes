# REVERIE – plugin guitare pour sons ambiants (VST3 / AU)

Chaîne d'effets modulaire, interface inspirée d'Alphonse Mucha.

## Les modules (l'ordre est libre, via « SIGNAL PATH »)
| Module | Rôle | Réglages |
|---|---|---|
| **Swell** | Auto-swell : détecte l'attaque de chaque note et la fait « gonfler » | Sensitivity (seuil de détection), Attack (durée du gonflement), Release, Depth (0 = effet nul, 1 = swell complet), Shape (courbe : douce ↔ tardive) |
| **Reverb** | Réverbération FDN 8 lignes, grande et diffuse | Size, Decay (jusqu'à 20 s), Damp, Pre-delay, Mod, Mix |
| **Shimmer** | Seconde réverb avec boucle de transposition | Mêmes réglages + Shimmer (quantité) et Interval (+1 oct, +5te, +oct+5te, −1 oct, +2 oct) |
| **Stereo Delay** | Delay avec réglages **gauche / droite indépendants** | Time, Feedback, Level par côté ; Cross (ping-pong), Tone, Mod, Mix ; SYNC au tempo du DAW avec une division par côté |
| **Quadravox** | 4 voix d'harmonisation | Pitch (−24 à +24 demi-tons), Level et Pan par voix ; Mix |
| **Compressor** | Compresseur (coude doux, détection stéréo liée) | Threshold, Ratio, Attack, Release, Makeup, Mix (compression parallèle) |

- Chaque module a un bouton rond (en haut à droite de son panneau) pour l'activer / le couper.
- Les 6 menus de « SIGNAL PATH » définissent l'ordre. Chaque module n'est traité qu'une fois : si vous en choisissez un en double, le doublon est ignoré et les modules manquants sont ajoutés à la fin.
- Ordre par défaut : Swell → Quadravox → Delay → Reverb → Shimmer → Compressor.
- Entrée mono (guitare) ou stéréo, sortie stéréo. Double-clic sur un bouton = valeur par défaut.
- Le swell réagit à l'entrée : placez-le en premier pour un effet « volume pedal » classique.

## Compiler avec GitHub (sans Mac)
1. Créez un dépôt **public** sur GitHub et envoyez-y tout le contenu de ce dossier (y compris `.github/`).
2. Onglet **Actions** : le workflow « Build Reverie » démarre seul (ou bouton *Run workflow*).
3. Quand il est vert, ouvrez l'exécution et téléchargez **Reverie-macOS** et/ou **Reverie-Windows** dans *Artifacts*.

## Installer
- **macOS** : décompressez, placez `Reverie.vst3` dans `~/Library/Audio/Plug-Ins/VST3/` (et `Reverie.component` dans `~/Library/Audio/Plug-Ins/Components/` pour Logic). Si macOS bloque le fichier téléchargé :
  ```
  xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Reverie.vst3
  ```
  Le plugin est signé en « ad hoc » (suffisant pour un usage perso). Pour le distribuer : compte Apple Developer + notarisation.
- **Windows** : placez `Reverie.vst3` dans `C:\Program Files\Common Files\VST3\`.
- Relancez ensuite le scan des plugins de votre DAW.

## Compiler en local
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
JUCE 8.0.9 est téléchargé automatiquement. Une version *Standalone* est aussi générée pour tester sans DAW.

## Structure
- `Source/DSP.h` : tout le traitement audio (indépendant de JUCE)
- `Source/Parameters.h` : liste des paramètres
- `Source/PluginProcessor.*` : chaîne, ordre des modules, tempo
- `Source/PluginEditor.*` : interface (palette, arches, halos, fleurs dessinés en code, aucune image externe)

## Licence JUCE
JUCE est sous double licence (AGPLv3 ou licence commerciale / gratuite sous conditions de revenus). Pour un usage personnel, aucun souci ; pour distribuer ou vendre le plugin, consultez https://juce.com/juce-8-licence/.

## Limites connues
- Le pitch shifter (Quadravox, Shimmer) est granulaire : léger effet de « battement » sur les notes tenues, caractéristique de ce type d'algorithme.
- Le delay est limité à 2 s ; en mode SYNC, les divisions plus longues sont plafonnées à 2 s.
- Ne suit pas le tempo si l'hôte ne le fournit pas (120 BPM par défaut).
