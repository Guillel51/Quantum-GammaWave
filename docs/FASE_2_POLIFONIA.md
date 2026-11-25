# Fase 2 — Polifonía: qué había, qué faltaba, y qué se cambió (v2.5.0)

La petición de esta fase tiene catorce puntos. Nueve de ellos ya estaban implementados y medidos en fases
anteriores de este árbol; el trabajo de esta fase ha sido **auditarlos contra el código real**, encontrar los dos
huecos que quedaban y cerrarlos. Este documento dice, punto por punto, dónde vive cada cosa, con qué prueba cuenta,
y qué no se hizo y por qué. Clases: **[HOST]** x86-64 con las fuentes reales, **[TARGET]** mapa del link del AC791N.

## 1. Auditoría de las funciones que la fase nombra

| Función | Dónde | Qué hace |
|---|---|---|
| `voice_alloc` | `voice.c` | ventana del part → misma nota → hueco libre (round-robin o el más cercano en altura con ALLOC/GLIDE) → víctima |
| `voice_window_grow` / `voice_grow_to` | `voice.c` | presta una ranura **que no suena** (primero las que nadie tiene, luego al part de ventana más ancha, nunca por debajo de una) — nada que suene se mueve, así que nada chasquea |
| `voice_kill` | `voice.c` | gate a 0 y stage 4: el fade de ~4.4 ms (media coseno, 6 bloques) |
| `voice_victim` | `voice.c` | clases: 1 release, 2 voces extra de MONO/UNISON, 3 mantenida que no es el bajo; dentro de la clase: fresco (4 bloques) → sobre su parte justa → energía → edad |
| `quiet_tail` | `voice.c` | primero las colas de release por debajo de −40 dB (`env_out < 120`), la más floja primero |
| `shed_voice` / `shed_check` / `shed_count` | `audio.c` | guardia de sobrecarga: **dos mitades seguidas** por encima del 80 % del plazo → se cede **una** voz por episodio (no una cada 2.9 ms) |
| `load_tight` | `voice.c`, se pone en `audio.c` | presupuesto de carga: 65 % lo enciende, 60 % durante 16 mitades lo apaga (ECO: 50/45 %) |
| `voice_defer` (`vq_*`) | `voice.c` | nota que llega mientras el otro núcleo tiene un trabajo en vuelo: se aplica al principio del bloque siguiente, en orden |
| `vshare` / `trk_live` | `voice.c` | reparto justo: solo desempata, no reserva; cuenta únicamente las pistas que pueden sonar |

## 2. Concurrencia entre los dos núcleos (punto 2)

No se asignan voces en los dos núcleos a la vez: la ISR de audio (core 0) aloca y libera, y el escaneo de teclas
corre anidado en TIMER5 dentro de esa misma ISR. El único cruce real es que **core 1 esté renderizando un trabajo
por partes mientras core 0 quiere alocar**, y para eso existe `voice_defer`: si `FELUCCA_MC_BUSY()` está activo, la
nota se pospone al bloque siguiente (`VQ_N` huecos, y si se llena se aplica en el acto en vez de perder la tecla —
el bug del informe #107, donde un acorde de doce teclas salía como ocho). [HOST] `tests/voice_defer_test.c`
ejercita el camino entero: posponer, llenar la cola, reproducir en orden, no dejar notas pegadas. En la unidad el
flag es `fm1_mc_job_busy()`; ninguna prueba de host puede producirlo, y así se declara.

## 3. Propiedad y estados (punto 3)

`vpool[VPOOLN]` con `VPOOLN = NVOICE + NPART = 18`: doce de presupuesto más una ranura inerte por parte como
destino del tail de cada ventana (`VDEAD`, nunca activa, nunca libre, nunca robada). `vown[VPOOLN]` dice de quién
es cada ranura, `V(t,i)` lee el mismo lvalue que leían los 100 puntos de llamada de antes del pool. Estados:
`active/gate/stage` 0 inerte, 1 ataque, 2 sustain, 3 release, 4 fade del robo. Un release **no** es un robo: no
toca `voice_kills` ni, desde esta fase, los contadores por parte.

## 4. Lo que se implementó en esta fase

### 4.1 El pool dice **de qué parte** salió la voz (puntos 12 y 13)

Antes: `voices_given_up` y `DEN` eran totales. Con seis pistas eso responde «faltaban voces», no «el pad
desapareció y el lead conservó el acorde». Ahora `vpart_taken[NPART]` y `vpart_denied[NPART]` (24 B de `.bss`)
cuentan, por parte, las voces que el pool le quitó y las notas que se quedaron sin voz. Se incrementan donde el
evento ya ocurría —`voice_kill` conoce la ranura y `vown` da el dueño en O(1), sin cambiar ninguna firma, y
`voice_start` cuenta el caso que más duele: la voz que se reinicia en su sitio porque la ventana está llena— y el
console los imprime en una línea:

```
voices_by_part t1=12/0,t2=3/1,...
```

Coste: un `if` y un incremento en caminos que no son el render (6 × 2 B), nada en el lazo de audio. Saturación a
65535: un contador que da la vuelta lee como un número pequeño, que es una mentira.

### 4.2 FM6 devuelve la ranura de una cola que ya no existe (punto 6)

`fm6_render` avanzaba el reloj de release **después** de calcular los portadores: una nota soltada cuyos portadores
ya estaban por debajo del umbral se iba por el `return` de antes, `rel_t` no avanzaba, `fm6_note_done()` nunca decía
que sí y la voz se quedaba `active` en stage 3 **indefinidamente** —una ranura del pool por nota—. Lo encontró la
prueba nueva: cinco voces seguían ocupadas 8.7 s después de sus note-offs en una escena de seis pistas con el motor
cambiando debajo. Ahora el reloj corre primero y **una nota soltada que no calcula nada se da por terminada**: su
release es monótono, no puede volver. La cola audible no cambia (los 5 s y el fade de 60 ms siguen ahí), solo deja
de retener ranuras que ya no usaba. Cuando esta prueba no mira, la unidad no cambia: los 311 renders de referencia
salen idénticos.

## 5. Los diez escenarios obligatorios y dónde se prueban [HOST]

| Escenario | Prueba |
|---|---|
| Doce notas simultáneas | `poly_test`, `poly_six_test` A/B, `poly_budget_test` (8/10/12/16), `poly_voices_test` |
| Seis pistas sonando | `poly_six_test` A, `poly_stress_test`, `poly_parts_test` D |
| Acordes grandes y sostenidos | `poly_phase2_test` A/B, `poly_six_test` D |
| Cambios rápidos de notas | `poly_phase2_test` C (24 note-ons en un bloque), `poly_stress_test` |
| FM6 con efectos exigentes | `poly_six_test` F (FX en todas las pistas), `poly_parts_test` D (motor cambiando bajo las notas), `fm6_*`, `regress` |
| ARP y secuenciador a la vez | `seq_test`, `arp_test`, `poly_stress_test` |
| Cambios rápidos de preset y motor | `poly_phase2_test` A/F, `poly_parts_test` D (120 rondas) |
| Liberación simultánea de muchas voces | `poly_parts_test` C (doce a la vez), `poly_six_test` D |
| Sesiones prolongadas | `poly_parts_test` D (720 note-ons, 20 000 bloques de drenaje, contadores sin envolver) |
| Concurrencia entre núcleos | `voice_defer_test`, `audio_test` (mitades perdidas, TIMER5 anidado) |

La prueba nueva es `tests/poly_parts_test.c`: A un pool lleno nombra a una sola parte; B una parte sin ventana es
la que se culpa y ninguna otra paga; C un release no es un robo y las ventanas quedan intactas; D sesión larga con
`sum(vpart_taken) == voice_kills` en todo momento, pico ≤ 12, fin vacío; E los contadores saturan.

## 6. Lo que se miró y **no** se cambió

| Candidato | Por qué no |
|---|---|
| Subir de 12 voces | La fase lo prohíbe expresamente, y con razón: primero que las doce funcionen (lo hacen: `poly_budget_test` lo mide con el cap real) |
| Umbrales de `load_tight` (65/60 %) y del guardia (80 %) | Son protecciones con historia medida: 85 % llegaba tarde (un clic y la UI muerta), 70 % cortaba notas musicales, y bajar el guardia a una sola mitad cedía una voz cada 2.9 ms. Cambiarlos sin una medición en la unidad es cambiar una protección por una intuición; esta fase no tiene unidad |
| Quitar el guardia de sobrecarga | La fase lo prohíbe sin un mecanismo alternativo demostrado; el cap y el presupuesto no sustituyen al guardia (el cap no ve la carga, `load_tight` no ve la mitad tardía) |
| Víctima «la más antigua» en vez de «la más floja» | Medido en v1.5.23: la más antigua es el cuerpo del acorde (fuerte, en sustain) y la recién pulsada está en ataque a un quinto de su nivel |
| Proteger también la nota **más aguda** de cada acorde | Reduciría los candidatos sin una medida que lo justifique; el bajo ya está protegido y la regla de energía protege lo audible. Queda como idea con prueba pendiente, no como cambio |
| Medir ciclos por motor en la unidad | No hay instrumentación de ciclos por motor en el AC791N sin coste; el diagnóstico por pista existe ahora (4.1) y el coste por bloque lo mide `target_budget.py` [TARGET] y `poly_stress_test` [HOST] |

## 7. Números de esta fase [TARGET]

| | Antes | Ahora |
|---|---|---|
| RAM `.data+.bss` | 84 580 (13 724 libres) | **84 604 (13 700 libres)** |
| Imagen XIP | 580 744 (2 868 libres) | **581 016 (2 596 libres)** |
| POOL / LOWBUF / NOINIT / RAMTEXT | — | sin cambio |

+24 B de `.bss` (los doce contadores) y +272 B de imagen (el `printf` del console, el contador en `voice_kill` y
el camino nuevo de FM6).
