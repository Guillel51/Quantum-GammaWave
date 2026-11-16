# Estado de la release 1.0.0

Lo que esta entrega **sí** tiene medido, lo que **no**, y por qué. Se actualiza con la release: si algo de acá
deja de ser cierto, el archivo cambia en el mismo commit que el código.

## Por qué 1.0.0 y no 4.2.0

El árbol venía numerándose en la línea 4.x. El dueño pidió publicar el instrumento una sola vez como **1.0**
para que la numeración pública empiece en uno. No es un retroceso de funcionalidad: es el mismo `main`, con
`FELUCCA_VERSION` renumerado (ver el comentario en `firmware/src/felucca.c`), y todo lo de las fases anteriores
sigue adentro.

Consecuencia mecánica que hubo que resolver: `web/make_site.py` sirve como "the signed release" el paquete de
**mayor versión** que encuentre en `release/`, y 3.5.0 / 4.0.0 / 4.1.0 son mayores que 1.0.0 con cualquier
comparación de versiones. Mientras esos paquetes estuvieran en `release/`, el sitio habría seguido publicando
4.1.0 y el instalador no habría ofrecido la 1.0. Por eso los paquetes y manifiestos anteriores se retiraron de
`release/` (siguen en la historia de git y en las releases ya publicadas en GitHub) y `release/` queda con el
artefacto de esta versión. `release/SHA256SUMS` nombra solo archivos que existen: `tools/coherence_check.py` lo
verifica línea por línea.

## Medido, con su clase

| Qué | Clase | Resultado |
| --- | --- | --- |
| Paquete `.fwsc` + app + loader | build AC791N (toolchain de JieLi + los tres blobs del SDK fijados por SHA-256) | `felucca-v1.0.0.fwsc`, 610959 B, identidad `FM-1_965` |
| Firmware | imagen AC791N del mismo build | 583028 B; `.data+.bss` 96836 B de 98304; pool spare 15392 B |
| Emulador | WASM (Emscripten 6.0.11), no es el binario del aparato | `release/emu/` con su `manifest.json` |
| Coherencia del árbol | `tools/coherence_check.py` | ok: paquete y manifiesto coinciden, el sitio sirve ese paquete, el registro del build y el del emulador coinciden, `SHA256SUMS` verifica |
| Sitio e instalador | `tools/site_check.py` | ok: portal, instalador con la versión publicada, checklist de Verify Only, emulador ofrecido |
| Host, suite completa | binarios nativos en esta VM (`NO_PACKAGE=1 tests/run_tests.sh`) | goldens de audio 293/293 sin cambios; el resto según se detalla abajo |

Nada acá es una medición sobre el FM-1 físico: no hay unidad conectada y no se escribió en ninguna.

## Abierto: siete comprobaciones de `tests/ui_test.c`

Fallan **y ya fallaban en `main` antes de esta entrega** (confirmado en un worktree aparte en `7b25aa2`, sin
ninguno de los cambios de estas fases). No se relajó ninguna aserción para taparlas; se listan para que no se
confundan con verde:

1. `GLO > SYSTEM: the CPU card follows song.cpu_q8 (same value, same pixels)`
2. `roll: the roll's last frame is pixel-identical to the static render`
3. `roll: header BPM 120 -> 121 and a card 10 -> 11: only the strip changes, unchanged digits stay put`
4. `roll: a change mid-roll retargets (from the last value, restarted) and ends static`
5. `#63 soak: 150000 random frames, wrapping clocks, motion: no roll lasts, cards = a forced redraw`
6. `#15 LARGE ON: HOME's cards tall (a K1..K4 pill over each column), the scope a strip, the footer kept`
7. `#15 LARGE: PRESETS and the roll keep the layout (labels in M), the layers too; MIXER tall, its tracks in the strip`

Familia común: píxeles que quedan de un cuadro anterior (tarjetas "obsoletas") y el roll que no termina igual
al render estático, más la disposición LARGE. Se probó como sospechosa la omisión de relleno que introdujo la
optimización de Fase 8 en `cv_rrect` (`firmware/src/gfx.c`): **no es la causa** — revertirla en el worktree base
deja los mismos siete fallos.

Pendiente para la próxima: encontrar qué tarjeta no se repinta cuando el roll queda estático y arreglarlo sin
perder el ahorro de la Fase 8.

## Cerrado en esta entrega

- La capa MACRO tenía un fallo propio: su test empezaba con los valores que dejaban las barridas de páginas
  (que ahora incluyen la página MACROS). El test pone M1..M4 a cero antes, como ya hacía con M1.
