# Remaster de presets — fase 3, medido sobre los archivos actuales

**Estado:** fase 3 cerrada. Este documento ya no es un plan: dice lo que hay, cómo se mide y qué se cambió, con los
números del inventario que se genera desde el firmware y del render de cada preset.

---

## 1. La biblioteca real

**285 presets de fábrica en 10 motores** (285 visibles y
2 legacy). El número sale de `ANALOG_PRESETS`, `FM6_PRESETS`, … y de `ENGINES[]`,
no de una tabla escrita a mano: `python3 tools/preset_audit.py --inventory docs/QG_PHASE4_PRESET_INVENTORY.json`.

| Motor | presets | legacy | RMS mediano (dB) | par más cercano por SONIDO | por parámetros |
|---|---|---|---|---|---|
| ANALOG | 68 | 0 | -17,9 | 0,071 | 0,082 |
| FM6 | 37 | 0 | -19,9 | 0,095 | 0,017 |
| PHASE | 32 | 0 | -18,3 | 0,097 | 0,072 |
| LOFI | 29 | 0 | -18,1 | 0,080 | 0,068 |
| VOICE | 25 | 0 | -17,6 | 0,085 | 0,065 |
| WHEEL | 22 | 0 | -16,0 | 0,083 | 0,122 |
| NOISE | 22 | 0 | -29,0 | 0,083 | 0,110 |
| WTABLE | 22 | 0 | -16,5 | 0,088 | 0,102 |
| DRUM | 18 | 2 | -14,4 | 0,103 | 0,045 |
| ADDTV | 10 | 0 | -17,3 | 0,132 | 0,126 |

Categorías del editor: KEYS 63, TEXTURES 47, PADS 29, ORGANS 25, DRUMS 22, BASS 19, BELLS 18, PLUCKS 13, LEADS 11, FX 6, BRASS 5.

## 2. Cómo se mide (y por qué la medida anterior engañaba)

- **`preset_audit_test --fingerprint`** carga cada preset por el camino real y renderiza una nota sostenida —en DRUM
  tres golpes, bombo, caja y charles, porque un kit son ocho canales—, y de ahí saca diez bandas logarítmicas por
  Goertzel (60 Hz … 7,4 kHz) más la forma de la envolvente: el bloque en que llega al 90 % del pico y el bloque en
  que cae al 50 %. Todo host x86, 44 100 Hz.
- **`tools/preset_audit.py --sound`** ordena los pares de cada motor por esa distancia (forma espectral 0,55, nivel
  0,20, envolvente 0,25). **`--gate-sound 0.05`** es la puerta que corre la suite: hoy son 5 037 pares y **ninguno
  baja de 0,05**.
- **La distancia por parámetros** cuenta como *elección* (0 o 1) las columnas que son un selector y no un número
  —`ALG` y `SLOT` de FM6, `WAVE`, `WAV#`, `ALGO`, `TYPE`, `KIT`, `MODE`, `SCALE`, `ORDER`, `DIV`, `PATCH`—, porque
  `SLOT 5` y `SLOT 14` no son «un poco más brillantes»: son otro parche. Antes se normalizaban como continuas y el
  audit marcaba **43 pares casi iguales** que no lo eran; hoy quedan **2** (0,017 y 0,025), y los dos miden 0,148 y
  0,108 de distancia real.

## 3. Lo que se cambió en esta fase

| Preset | Motor | Antes | Ahora | Distancia del par |
|---|---|---|---|---|
| LINNDRUM | DRUM | seco pero 15 dB bajo | TONE 98, DECAY 62, DRIVE 26, `P_LEVEL` 124 | 972 → 2 653 de RMS |
| ELECTRO | DRUM | casi igual a LINNDRUM | TUNE 60, DECAY 92, NOISE 80, delay 24 | par DRUM 0,071 → 0,103 |
| QUANTUM BEAT | DRUM | casi igual a DRUM-X | MORPH 104, TONE 30, DECAY 100, FM 44, FOLD 48 | 0,033 → fuera del umbral |
| STEP LEAD | LOFI | 0,043 de CIUDAD DE PLAYA | WAVE 3, DUTY 96, VIB 40, BEND 20 | LOFI 0,043 → 0,080 |
| CIUDAD DE PLAYA | LOFI | pad de cinta genérico | CHIP 2, CRUSH 60, CUT 70, MASK 45 | idem |
| CHIP ARP / POCKET CONSOLE | LOFI | 0,054 / 0,056 entre ellos | CHIP 1, DUTY 8, BEND 30 / WAVE 2, CRUSH 34, CUT 104 | resuelto |
| SIESTA DORADA | ANALOG | 0,059 de AURA STRINGS | WAVE 6, MIX 96, RES 40, DRIVE 28, INT −12 | ANALOG 0,059 → 0,071 |
| HARP PLUCK | ANALOG | 0,062 de TUBE KEYS | CUT 74, RES 58, fenv 62 | resuelto |
| GAMELAN MET | FM6 | mismo parche que VIBES | slot 5 (barra percutida) y su shimmer | FM6 0,077 → 0,095 |
| GONG SHIMMER | FM6 | 0,009 de TEMPLE BELL | FB 4, MOD 16, MEG −10, DETUNE 26 | 0,009 → 0,343 |
| TEMPLE CIRCUIT | FM6 | 0,022 de TEMPLE BELL | MOD −12, RATIO −5, MEG 12 | 0,022 → 0,324 |
| REED HORN | ADDTV | no existía | parciales impares, codo suave, mono | nuevo, 0,132 del más cercano |
| AIR CHOIR | ADDTV | no existía | parciales estirados, LFO lento al filtro | nuevo, 0,132 del más cercano |

**Costo:** 285 presets ocupan **100 B más** de imagen que los 283 de la 1.3.3 (583 452 B de 583 612, 160 B de
margen). Todo lo demás es edición de valores in-place: **ningún ID se movió, ningún array se reordenó, ningún preset
se ocultó** — el mecanismo `legacy` sigue disponible y sigue comprobado por la auditoría (`legacy_n` por motor), pero
en esta fase no hizo falta: los candidatos tenían arreglo sonoro.

## 4. Lo que se revisó y NO se tocó, con el motivo

- **MUSIC BOX / FROST MUSIC (0,017) y TINE EP / EMBER TINES (0,025)** son el mismo parche con FB, envolvente de
  filtro y envíos distintos: miden 0,148 y 0,108 de distancia real, o sea que se distinguen al oído. Cambiarles el
  parche sería romper dos sonidos buenos para maquillar un número de parámetros.
- **Los extremos de nivel son carácter, no desbalance**: ABYSSAL DRONE (RMS −31,4 dB), ANGEL AIR (−29,9), BARKING
  PIPE (−23,7) y GEIGER CNT (−52,0, un contador que dispara de vez en cuando). El RMS mediano por motor queda entre
  −29,0 y −14,4 dB y ningún preset llega al clipper (0,00 % en los 285).
- **NOISE es el motor con el RMS mediano más bajo (−29,0 dB)** por sus texturas dispersas; sus vecinos más cercanos
  están a 0,083, así que no es redundancia, es el carácter del motor.

## 5. Cómo se reproduce

```sh
cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/preset_audit_test tests/preset_audit_test.c -lm
build/host/preset_audit_test                      # estructura, silencio, nivel, duplicados exactos
build/host/preset_audit_test --levels             # RMS y pico de cada preset
build/host/preset_audit_test --dump > build/preset_dump.jsonl
build/host/preset_audit_test --fingerprint > build/preset_fingerprint.jsonl
python3 tools/preset_audit.py                     # el banco por parámetros y por sonido
python3 tools/preset_audit.py --gate-sound 0.05   # la puerta de la suite
python3 tools/preset_audit.py --inventory docs/QG_PHASE4_PRESET_INVENTORY.json
```
