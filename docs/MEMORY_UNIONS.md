# Memoria, medido: qué ocupa la RAM de las voces y qué puede compartirse

Clase **[HOST x86-64]** para los `sizeof` (los tipos son los del firmware; el compilador pi32v2 puede alinear
distinto) y **[TARGET AC791N]** para los totales del mapa del enlace.

## Inventario (medido, no estimado)

| estructura | tamaño | total |
|---|---|---|
| `voice_t` | **92 B** | `vpool[18]` = **1 656 B** |
| `ad_vc_t` (ADDITIVE, por ranura) | 68 B | `ad_v[18]` = **1 224 B** |
| `drw_vc_t` (WHEEL, por ranura) | **92 B** | `drw_v[18]` = **1 656 B** |
| ruido (NOISE, tres arrays por ranura) | 12 B | **216 B** |
| `fm6_note_t` (FM6, por nota) | **300 B** | `fm6_note[6][FM6_POLY]` |
| `vmod_t` (control, por voz) | 44 B | no es estado de motor |

## La unión, y su techo real

La propuesta de la fase era una unión etiquetada por motor: cada voz ocupa el estado de su motor activo y no
la suma. Con estos números el techo es **mucho menor de los 4-8 KB** que la fase pedía:

- los estados **por ranura** son `voice_t` (92), WHEEL (92), ADDITIVE (68) y NOISE (12): hoy suman 172 B de
  estado de motor por ranura más la voz. Una unión de los cuatro —y `voice_t.s[8]` ya es la parte común—
  dejaría como máximo 92 B por ranura: **1 440 B** en 18 ranuras, y no se pueden tocar `ph[3]`, `env` ni el
  resto de `voice_t`, que son del allocator y no del motor;
- el estado de FM6 **no está por ranura sino por pista y nota** (`fm6_note[6][FM6_POLY]`): con `FM6_POLY` = 4
  (eng_fm6.c:41, seis operadores por voz, el motor más pesado) son 6 × 4 × 300 = **7 200 B**, el bloque más
  grande de todos. Una unión con él no ahorra nada (él *es* el máximo), y reducirlo a la mitad sería bajar la
  polifonía del motor, que la fase prohíbe.

## Correción medida: el techo no era 1,4 KB (2026-11-25, implementado)

Este informe miró sólo el estado **por ranura** y concluyó «~1,4 KB como mucho». Faltaba la mitad del inventario:
el estado **por pista**, que también es exclusivo de un motor porque una pista corre un solo motor a la vez.
Medido e implementado en `docs/FASE1_RAM_ARENAS_2026-11-25.md`:

| unión | antes | después | devuelto |
|---|---|---|---|
| por pista: FM6 `fm6_note[6][4]` (7 200 B) ∪ DRUM `drum_kit[6][8]` (5 376 B) | 12 576 B | 6 × 1 216 = 7 296 B | **5 280 B** |
| por ranura: WHEEL 1 656 + ADDITIVE 1 224 + NOISE 216 | 3 096 B | 18 × 96 + 18 = 1 746 B | **1 350 B** |
| **total** | **15 672 B** | **9 042 B** | **6 630 B** |

El objeto host completo baja **6 752 B** de `.bss` (297 264 → 290 512), los 311 goldens de audio salen **0
cambiados** y los 61 s de la escena de seis pistas coinciden byte a byte. La disciplina que este informe pedía
se implementó y se prueba: `voice_start` etiqueta y limpia la ranura cuando otro motor la toma, y `engine_block`
limpia la fila de la pista en el mismo punto en que ya ponía a cero sus voces; `tests/engine_arena_test.c` lo
comprueba con notas sostenidas, seis pistas, acordes, release, unísono y robo.

Los kilobytes grandes que siguen fuera del alcance son los buffers de FX y `trk`, no la duplicación entre
motores: esta era la única duplicación real.

