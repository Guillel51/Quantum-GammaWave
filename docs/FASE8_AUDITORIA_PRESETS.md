# FASE 8/9 — Auditoría de la biblioteca de presets

Clases de evidencia: `[HOST]` x86-64 con los motores reales renderizando a 44,1 kHz, `[STATIC]` lectura de
código, `[TARGET]` build AC791N. **No hubo escucha**: ningún archivo de audio salió de una FM-1 ni de un oído.
Todo lo que sigue es estructura y señal medidas, no juicio estético.

Herramientas: `tests/preset_audit_test.c` (sonido, vía el motor real) y `tools/preset_audit.py` (biblioteca:
distancias entre sonidos, uso de envíos, movimiento, rango usado por motor).

---

## 1. Inventario

**293 presets de fábrica en 10 motores.**

| Motor | Presets | Categorías |
|---|---:|---|
| ANALOG | 73 | PADS, KEYS, BASS, LEADS, PLUCKS, ORGANS |
| FM6 | 37 | KEYS 21, BELLS 7, PADS 3, PLUCKS 3, BASS 2, DRUMS 1 |
| PHASE | 34 | (repartidos) |
| LOFI | 29 | — |
| VOICE | 25 | — |
| NOISE | 22 | TEXTURES 21 |
| WHEEL | 22 | ORGANS 18 |
| WTABLE | 23 | PADS 15 |
| DRUM | 18 | DRUMS 18 |
| ADDTV | 10 | BELLS 5 |

Categorías totales: PADS 68, KEYS 58, TEXTURES 51, DRUMS 24, BASS 21, BELLS 21, ORGANS 18, LEADS 13,
PLUCKS 12, ARPS 7. Con movimiento (`x[]`) 291 de 293; ninguno sin envío.

## 2. Lo que la auditoría de señal encontró

`[HOST]`, un Do4 sostenido por preset (~0,23 s, la parte que suena):

| | Resultado |
|---|---|
| Silenciosos (rms < 8) | **0** |
| Al límite del clipper | **0** presets con más del 2 % de la nota en la última curva del soft clipper |
| Nombres repetidos | **0** |
| Parámetros idénticos a otro preset | **0** |
| Fuera de rango (EDIT, env, nombre) | **0** |

**Duplicados cercanos** (distancia 0 = mismo sonido; el umbral de la herramienta es 0,035):

```
0.045  DRUM   808 SUB / TRAP 808
0.054  FM6    MARIMBA / DEEP MARIMBA
0.064  FM6    GLASS KEYS / GAMELAN MET
0.065  VOICE  MYSTIC CHOIR / HALO CHORUS
0.065  DRUM   HEARTH KIT / CYBER DRUM
0.066  FM6    TINE EP / EMBER TINES
```

Ninguno por debajo del umbral: **la biblioteca no tiene duplicados**, sólo parejas que un oído distinguiría
poco. Cualquier trabajo ahí es de timbre, no de estructura, y sin escucha sería exactamente el «cambio
superficial» que la fase prohíbe.

## 3. Nuevo: DC, medido y separado de lo sub-audio

Añadí a `tests/preset_audit_test.c` la media de la señal (offset DC) como fracción de fondo de escala, y —para
no confundir DC con un cuerpo subsónico— la media de **cada mitad** de la parte que suena: el DC real conserva
el signo en las dos, un cuerpo de 1–2 Hz lo invierte.

**Siete presets quedan por encima del 2 % de offset, todos con signo constante en ambas mitades (DC real):**

| Motor | Preset | Offset | Mitades |
|---|---|---:|---|
| PHASE | MERCURY BLOOM | **12,8 %** | −14,9 / −10,7 % |
| PHASE | SYNC CATHEDRAL | 3,6 % | −3,8 / −3,3 % |
| PHASE | BELL FOUNDRY | 2,6 % | +2,9 / +2,3 % |
| LOFI | BYTE VAR | 2,6 % | +3,3 / +1,8 % |
| LOFI | BYTEBEAT | 2,2 % | +1,6 / +2,7 % |
| PHASE | ETHNIC PLUCK | 2,2 % | +3,1 / +1,4 % |
| PHASE | PD 808 | 2,1 % | +1,9 / +2,3 % |

El resto de la biblioteca está por debajo del 1 % (FM6 0,33 %, ADDTV 0,21 %, VOICE 0,47 %). Se **reporta, no
falla**: un bytebeat es asimétrico por diseño, y decidir cuánto offset es demasiado es una decisión de escucha y
de motor, no de aritmética. MERCURY BLOOM (12,8 %, un tercio de fondo de escala de desplazamiento con pico
16 986) es el que sí parece un defecto y es el primero de la lista de trabajo.

## 4. Por qué no hay colección nueva en esta pasada

La colección curada que la fase pide **no cabe hoy**: cada `preset_t` ocupa ~44 B de imagen y el margen libre
del build `[TARGET]` en v2.3.4 es de **2 408 B** con un suelo de **2 048 B** — quedan ~360 B, unos **8
presets**. Una colección que demuestre de verdad los motores nuevos (FM, híbridos, texturas, performance) son
30–40 sonidos, ~1 500 B, y dejaría el margen por debajo del suelo.

Para que entre hay que liberar espacio o recortar, y eso es una decisión sobre qué se va:

1. **Podar presets redundantes** (las parejas de §2 son el candidato natural: seis sonidos que hoy se
   distinguen poco) y usar su hueco para la colección nueva: cero coste neto.
2. **Recuperar imagen** en otro sitio (los últimos recortes vinieron de ahí) y publicar la colección encima.

Sin esa decisión, agregar presets sería romper el presupuesto de flash, que es un límite duro del proyecto.

## 5. Estado

- `tests/preset_audit_test.c` gana la columna `dc max` por motor, el listado de los que pasan del 2 % y la
  regla de las mitades; sigue pasando (`0 silent, 0 duplicates, 0 out of range`) y sigue diciendo, en su propia
  última línea, que **nadie escuchó**.
- `tools/preset_audit.py`: 293 presets, 0 problemas, 0 parejas por debajo de 0,035.
- Sin cambios de timbre en esta pasada y sin release: nada de lo medido justifica tocar un preset a ciegas.
