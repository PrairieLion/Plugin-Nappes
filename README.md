# REVERIE – plugin guitare pour sons ambiants (VST3 / AU)

Chaîne d'effets modulaire, interface inspirée d'Alphonse Mucha.

## Les modules (l'ordre est libre, via « SIGNAL PATH »)
| Module | Rôle | Réglages |
|---|---|---|
| **Swell** | Auto-swell : détecte l'attaque de chaque note et la fait « gonfler » | Sensitivity (seuil de détection), Attack (durée du gonflement), Release, Depth (0 = effet nul, 1 = swell complet), Shape (courbe : douce ↔ tardive) |
| **Reverb** | Réverbération FDN 8 lignes, grande et diffuse | Size, Decay (jusqu'à 20 s), Damp, Pre-delay, Mod, Mix |
| **Shimmer** | Seconde réverb avec boucle de transposition | Mêmes réglages + Shimmer (quantité) et Interval (+1 oct, +5te, +oct+5te, −1 oct, +2 oct) |
| **Superego** | « Gel » du son façon Electro-Harmonix Superego : sustain infini, nappes, glissandos entre accords | voir ci-dessous |
| **Stereo Delay** | Delay avec réglages **gauche / droite indépendants** | Time, Feedback, Level par côté ; Cross (ping-pong), Tone, Mod, Mix ; SYNC au tempo du DAW avec une division par côté |
| **Quadravox** | 4 voix d'harmonisation | Pitch (−24 à +24 demi-tons), Level et Pan par voix ; Mix |
| **Equalizer** | Coupe-bas, plateau grave, 2 cloches (fréquence / gain / Q), plateau aigu, coupe-haut, avec courbe de réponse | 12 réglages ; Low Cut à 20 Hz et High Cut à 20 kHz = filtres désactivés |
| **Compressor** | Compresseur (coude doux, détection stéréo liée) | Threshold, Ratio, Attack, Release, Makeup, Mix (compression parallèle) |

- **Bypass séparé de chaque module** : bouton rond « ON » en haut à droite de chaque panneau. Un module coupé voit ses commandes estompées.
- Les 6 menus de « SIGNAL PATH » définissent l'ordre. Chaque module n'est traité qu'une fois : si vous en choisissez un en double, le doublon est ignoré et les modules manquants sont ajoutés à la fin.
- Ordre par défaut : Swell → Superego → Quadravox → Delay → Reverb → Shimmer → Equalizer → Compressor.
- La fenêtre est redimensionnable (tirez le coin en bas à droite).
- Entrée mono (guitare) ou stéréo, sortie stéréo. Double-clic sur un bouton = valeur par défaut.
- Le swell réagit à l'entrée : placez-le en premier pour un effet « volume pedal » classique.

## Le Superego en pratique
- **Mode** : *Moment* (le son est gelé tant que vous maintenez FREEZE), *Latch* (un clic fige le son, il continue jusqu'à CLEAR ; chaque nouveau clic ajoute une couche), *Auto* (une fois FREEZE activé, chaque note jouée est figée automatiquement selon Threshold), *Live* (pas de gel : les effets internes agissent directement sur la guitare).
- **Attack / Decay** : durée d'apparition / de disparition du son gelé. **Layer** : niveau conservé du gel précédent quand on en fige un nouveau (0 = remplacé). **Glide** : glissement entre deux gels successifs. **Dry / Effect** : niveaux du son direct et du son gelé.
- **FX** : Detune, Chorus, Tremolo, Filter, Pitch (Depth = intervalle de -12 à +12 demi-tons), réglés par FX Rate / FX Depth.
- **CLEAR** : fait disparaître le son gelé (selon Decay).
- Astuce : jouez un accord, cliquez FREEZE en Latch, puis jouez par-dessus.

## Compiler avec GitHub (sans Mac)
1. Créez un dépôt **public** sur GitHub et envoyez-y tout le contenu de ce dossier. Le dossier `.github` est invisible sur Mac et souvent ignoré au glisser-déposer : si `Actions` n'affiche aucun workflow, créez le fichier à la main (*Add file > Create new file*, nom `.github/workflows/build.yml`) et collez-y le contenu de `workflow-visible/build.yml` (ce fichier est au format JSON : l'indentation n'a aucune importance).
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
- `Source/DSP.h` : tout le traitement audio (indépendant de JUCE), y compris le moteur de gel du Superego (FFT / vocodeur de phase) et l'égaliseur
- `Source/Parameters.h` : liste des paramètres
- `Source/PluginProcessor.*` : chaîne, ordre des modules, tempo
- `Source/PluginEditor.*` : interface (palette, arches, halos, fleurs dessinés en code, aucune image externe)

## Licence JUCE
JUCE est sous double licence (AGPLv3 ou licence commerciale / gratuite sous conditions de revenus). Pour un usage personnel, aucun souci ; pour distribuer ou vendre le plugin, consultez https://juce.com/juce-8-licence/.

## Limites connues
- Le pitch shifter (Quadravox, Shimmer) est granulaire : léger effet de « battement » sur les notes tenues, caractéristique de ce type d'algorithme.
- Le delay est limité à 2 s ; en mode SYNC, les divisions plus longues sont plafonnées à 2 s.
- Le gel du Superego analyse les ~170 dernières ms : en mode Auto, la capture a lieu environ 0,2 s après l'attaque de la note. Les notes graves se figent mieux avec un peu de sustain avant le clic.
- Ne suit pas le tempo si l'hôte ne le fournit pas (120 BPM par défaut).
