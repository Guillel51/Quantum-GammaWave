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

## Conclusión honesta

Compartir almacenamiento entre motores da como mucho **~1,4 KB**, no 4-8 KB, y exige que cada motor cero-inicie
su estado al tomar una voz y que ninguno lo lea después de cambiar de motor — disciplina que hay que probar
nota a nota con seis pistas, acordes, release y robo. Los kilobytes grandes están en otro sitio: el bloque de
notas de FM6 (7,2 KB) y los buffers de sección (`sec_stage`), no en la duplicación entre motores.

Esta ronda **no implementó** la unión: con el techo medido en 1,4 KB y el riesgo de audio en el camino de
cambio de motor, primero hay que decidir si esa ganancia paga el refactor de los diez motores, y eso es una
decisión de diseño, no un detalle de implementación.
