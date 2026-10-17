# R-744

Transkritisk sampler för techno, som VST3 och AU för macOS. Ljudet går genom en CO2-kylkrets: det förångas till ett moln, trycks ihop tills det viker sig, kyls (eller stryps) i gaskylaren och slås sönder i expansionsventilen.

Ett plugin från **MASKINHALLEN**. Klicka på loggan uppe till vänster för att läsa om båtvarvet Docklands, där allt började.

## Ladda ner

https://github.com/phunkshunz/R744-VSTi/releases/latest/download/R744-macOS.zip

Dubbelklicka på zip-filen i Hämtade filer. Då får du en mapp som heter **R-744**.

Ladda inte ner via den gröna knappen **Code**. Den ger bara källkoden, inte pluginet.

## Installera på Mac

Öppna Terminal och kör:

```
bash ~/Downloads/R-744/installera.command
```

Starta om Bitwig, eller tryck **Rescan** under Settings › Locations › Plug-ins. R-744 ligger bland instrumenten.

Kräver macOS 11 Big Sur eller nyare. Funkar på både Intel och Apple Silicon.

## Så funkar den

Släpp en ljudfil (wav, aiff, flac, mp3) på fönstret eller tryck **LADDA**. Spela med MIDI: tonen styr tonhöjden och startar om molnet från början. **HOLD** spelar utan MIDI.

| Steg | Ratt | Vad den gör |
|---|---|---|
| 1 EVAPORATOR | EXPANSION | Granulärt moln. Sträcker ljudet upp till 16 gånger, sprider ut kornen, lägger dem en oktav ner och baklänges. |
| 2 COMPRESSOR | PRESSURE | Ingen dynamik. Vikning, mättnad, sample-and-hold och bitkross. Ju högre tryck, desto mer sönder. |
| 3 GAS COOLER | AMBIENT TEMP | 24 dB lågpass. Varmt ute = toppen stryps ner mot 90 Hz och resonansen skriker. |
| 4 EXP. VALVE | PRESSURE DROP | Osynkad grind som hackar ljudet i slag, som ett plötsligt tryckfall. |
| | SUPERHEAT | Makro. Trycker alla fyra mot max. |

Mätaren visar trycket på högtryckssidan och gaskylarens temperatur. Över 73,8 bar är kretsen **TRANSCRITICAL**, precis som CO2 på riktigt.

### SYSTEM FREEZE

Håll **SUPERHEAT** i botten i 2 sekunder: ljudet bryts, säkerhetsventilen väser i 3 sekunder och sedan är R-744 död tills du trycker **RESET**.

### Säkerhet

- Ljudtråden låser aldrig och allokerar inget minne.
- Hinner datorn inte med minskar R-744 antalet korn automatiskt och ökar dem igen när det lugnat sig. Mätaren visar CPU och korn.
- Blir något fel i ljudet (NaN) nollställs motorn. Upprepas felet går den i **SÄKERT LÄGE** tills du trycker **RESET**.

## Bygga själv

GitHub bygger automatiskt vid varje push till `main` (Actions › Bygg R-744). Zippen hamnar under Releases.
