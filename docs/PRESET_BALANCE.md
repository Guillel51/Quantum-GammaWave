# Balance de presets, medido (v1.5.11)

Clase de toda cifra de esta página: **host x86-64**, `tests/preset_levels`, que renderiza cada preset de
fábrica por el mix real a 32 muestras por bloque (C4, velocidad 100, 1,5 s desde la pulsación). No es una
medición del FM-1 y no sustituye a escuchar la unidad.

## Cómo se mide y cómo se corrige

1. `tests/preset_levels` imprime RMS y pico en dBFS de los 285 presets.
2. `tools/gen_preset_levels.py` convierte esa medición en `firmware/src/preset_level.c`: el nivel con el que
   cada sonido carga (índice en `LEVEL_Q12`, la misma tabla que multiplica el mixer). No se edita a mano.
3. Un preset fuera de alcance con el fader a tope (índice 127) **no se arregla con el número**: se corrige su
   patch. Es lo que se hizo en esta versión.

## Antes / después

| | antes | después |
|---|---|---|
| mediana | −18,5 dB | −18,5 dB |
| presets fuera de ±3 dB | 28 | **19** |
| dispersión (mín..máx) | 25,8 dB | **12,6 dB** |
| más fuerte | `QUANTUM BEAT` −11,0 dB | `HEARTH KIT` −16,9 dB |
| más débil | `CRYSTAL PLUCK` −36,8 dB | `AURORA FM` −29,5 dB |

## Presets corregidos (patch, no nivel)

| preset | motor | cambio |
|---|---|---|
| `CRYSTAL PLUCK` | ADDTV | DRIVE 6→22, SUB 0→28, DECAY 70→56: cuerpo y suelo, sigue siendo un pluck |
| `METAL CLUSTER` | ADDTV | DRIVE 30→40, SUB 0→26: el suboscilador estaba mudo |
| `AURORA FM` | FM6 | reverb 86→64, delay 70→52: el wash queda, el sonido deja de esconderse |
| `ABYSSAL DRONE` | ANALOG | reverb 96→72: 96 sobre un drone tapaba el tono seco |
| `STATIC HAZE` | NOISE | reverb 80→60 |
| `GEIGER CNT` | NOISE | densidad 10→28: contaba tan pocas chispas que no se oía |

## Fuera de ±3 dB a propósito

`AURORA FM` (−29,5), `STATIC HAZE` (−29,4), `GEIGER CNT` (−28,7), `ARCADE` (−24,5),
`ABYSSAL DRONE` (−24,4), `CRYSTAL PLUCK` (−24,3), `GLASS BELL` (−24,2), `POLAR AURORA` (−23,8),
más los plucks y texturas entre −21,9 y −23,7 que se ven en la tabla del binario.

Son texturas, colchones y drones: un RMS bajo es su carácter, no un defecto — un geiger contando chispas, una
aurora de ataque lento, un pluck corto que decae antes de 1,5 s. Subirlos más habría cambiado el sonido.
El criterio para tocar uno sigue siendo el mismo: si no se oye **con el fader a 127**, se arregla el patch.
