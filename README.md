# R-744 · KLANGMASKIN

Loopar blir ton. R-744 laddar en loop, eller en hel mapp med loopar, och förvandlar den till ackord, mattor och drönare, i tonart och i takt med Bitwig. VST3 och AU för macOS, VST3 för Windows. Ett plugin från **MASKINHALLEN** / Phunkshunz AUDIO.

## Ladda ner

- Mac: https://github.com/phunkshunz/R744-VSTi/releases/latest/download/R744-macOS.zip
- Windows: https://github.com/phunkshunz/R744-VSTi/releases/latest/download/R744-Windows.zip

Ladda inte ner via den gröna knappen **Code**. Den ger bara källkoden.

**Mac:** dubbelklicka på zip-filen och kör sedan i Terminal: `bash ~/Downloads/R-744/installera.command`
**Windows:** extrahera zip-filen och dubbelklicka på **installera.bat**.

R-744 ligger bland instrumenten. Den ersätter gamla R-744 (samma plugin-ID).

## Kom igång

1. **Släpp en loop eller en hel mapp** på fönstret (eller LADDA LOOP / LADDA MAPP).
2. Tryck **play i Bitwig**. Loopen spelar i takt.
3. Tryck **SLUMPA LOOP** för att byta loop, **IDÉ!** för att byta klang. Om och om igen.

## Mappen: byt snabbt och lek

- **LADDA MAPP** läser in alla ljudfiler i mappen och alla undermappar (wav, aif, flac, mp3, ogg).
- **SLUMPA LOOP**: en ny slumpad loop ur hela mappen.
- **SAMMA MAPP**: en ny slumpad loop ur samma undermapp som den som spelar.
- **MAPPAR**: välj en undermapp i listan, så kommer en slumpad loop därifrån.
- **◀ ▶**: förra och nästa loop.
- **LOOPLÄNGD**: AUTO gissar hur många takter loopen är. Välj själv om den gissar fel.
- Mappen och loopen sparas med låten.

## Klangen

| Del | Rattar | Vad det gör |
|---|---|---|
| **LOOP & SLAG** | LOOP, SLAG | Hur mycket av originalet som hörs. SLAG: tangenterna knackar på resonatorerna, så de låter även utan loop. |
| **RESONANS** | ACKORD, GRUNDTON, NIVÅ, RINGNING, LJUS, SPRIDNING, OKTAV | Åtta stämda resonatorer. Varje slag i loopen ringer som ackordet: kicken blir en bastoner, hatten glittrar högt upp. |
| **MOLN** | NIVÅ, KORN, TÄTHET, VANDRA, STÄMNING, FRYS | Korn ur loopen (40–500 ms, alltid mjuka) sträcks till mattor som vandrar genom loopen. STÄMNING stämmer kornen i ackordet. FRYS håller molnet still. |
| **ANDNING & RUM** | ANDNING, FRAS, BAND, RUM, STORLEK, UT | Ett filter som öppnar och stänger med fraserna, band-wow och ett stort, mjukt rum. |

**Håll tangenter** så stäms resonatorerna och molnet om till just de tonerna. Spela ackord, så spelar loopen dem åt dig.

**IDÉ!** ger ett av sex recept (ringande trummor, fruset moln, dubkammare, glasklockor, drönare, puls i ackord) i en slumpad tonart. **MUTERA** ändrar lite, **ÅNGRA** backar 30 steg, och varje panel har en tärning. 12 presets.

Allt glider och inget klickar: kornen har mjuka fönster, nivåer och toner glider, och slutet är mjuk mättning plus en långsam automatisk nivå, så alla lägen låter ungefär lika starkt.

## Bygga själv

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Testa motorn utan JUCE: `c++ -std=c++17 -O2 -Isrc tests/engine_test.cpp -o engine_test && ./engine_test`
