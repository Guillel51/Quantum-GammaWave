# Fase 6 — nuevo agregado, optimización, auditoría y publicación (v1.5.27)

Informe de lo implementado, lo medido y lo que sigue necesitando la unidad. Clases: **[ESTÁTICO]** leído en el
código, **[HOST]** runners de este contenedor, **[AC791N]** build del target, **[HW]** FM-1 física (no hay ninguna).

## 1. Estado de partida

Árbol limpio en `main` (`c67102f7`, v1.5.26), paquete en `release/` que `make_release --check` describe como uno
solo, Pages sirviendo `v1.5.26-86fa85e` (`c156a28c…`) y el Release con `b2d736ec…`; la diferencia entre los dos
digests está explicada en `docs/FASE5_AUDITORIA_v1.5.26.md` §7 y no cambió aquí. Los pendientes de hardware
(`docs/FASE2_ISSUES_106_107.md`, Issue #106, la matriz `[HW]`) siguen abiertos.

## 2. Nuevo agregado: la canción como Standard MIDI File (`.MID`)

**Qué faltaba.** El Web Studio renderiza la pieza de 4 compases a `.WAV` y guarda el proyecto (`.GW1`), pero las
**notas** sólo existían dentro de la página: para llevar un bajo a un DAW, o para conservar la pieza sin la FM-1,
había que volver a tocarla. No había ninguna ruta de exportación MIDI en el repositorio (`grep -i "\.mid"` en
`web/`: cero), y el firmware no puede escribir archivos.

**Qué hace.** Botón **🎼 EXPORT .MID** en *1 · DEMO SONGS & EXPORT*, escritor `gwMidiBytes(parts, bpm)` en
`web/editor.html`:

| Decisión | Valor |
|---|---|
| Formato | SMF **tipo 1**, `MThd` + una pista por lane vivo |
| Resolución | **480 ppq**; una semicorchea = **120 ticks**; 64 pasos = 4 compases en 4/4 |
| Tempo | el de la canción, como meta `0x51` (500 000 = 120 BPM) |
| Compás | meta `0x58`, 4/4, 24 clocks por negra |
| Melódicas | una pista por lane, notas del propio lane, velocidad 96, puerta de una semicorchea (el **pad** se sostiene hasta el siguiente ataque) |
| Batería | **canal 10**, notas GM 36/38/42 (KICK/SNARE/HAT, las lanes 1..3 del kit de las demos) |
| Muteadas | no se escriben (ni su pista) |
| No incluye | SysEx ni datos de parche: notas, tempo y compás, que es lo que un DAW importa sin ayuda |

**Interfaz.** El botón queda junto a `.WAV` y `.GW1`, con `title` que dice formato y resolución; el estado de la
sección informa cuántas pistas se escribieron, y un fallo se reporta en la misma línea en vez de en la consola. La
fila ahora **envuelve** (`flex-wrap`) porque tiene tres botones.

**Tests `[HOST]`.** `web/test_mid.mjs`, **64 comprobaciones**, con un **lector SMF** escrito en el test: cabecera,
formato, división, tempo (comparado con `60 000 000 / bpm`), compás, pistas, nombres, notas en sus semicorcheas,
velocidad, cada nota-on cerrada por su nota-off, el pad sostenido 16 pasos, el canal 10 y sus notas GM, la lane
muteada ausente, y los bordes: canción vacía (sólo el conductor), lane de 200 pasos (recortada a 4 compases), tempo
0, valor de batería fuera de rango. Se sumó a `tests/groups.sh web`.

## 3. Optimización (la cuarta, y distinta de las tres anteriores)

Las anteriores fueron recursos (F1), píxeles del panel (F4) y la conversión 565→RGBA del emulador (F5). Esta es el
**trabajo por frame del barrido de encendido de los LEDs**: `animFill` calculaba, para cada uno de los **41 LEDs**
(14 botones + 27 teclas), tres `** 2.2` y tres `** 1/2.2`: **246 `pow()` por frame** para un byte con **256**
valores posibles por tipo.

Ahora el color de cada `(tipo, nivel)` se recuerda (perezoso: sólo se calcula el nivel que el barrido alcanza) y el
nodo de cada LED (`.face`, `.slit`) se busca una vez en lugar de un `querySelector` por LED y frame.

| Medida | Antes | Después |
|---|---|---|
| `[HOST]` 41 LEDs × 20 000 frames, node | 0,021 ms/frame | **0,001 ms/frame** (**19,6×**) |
| `[HOST]` `pow()` por frame | 246 | **0** tras el primer ciclo |
| `[HOST]` valores distintos (btn 256 + slit 256) | — | **0** (`web/emu/emu_test.mjs`) |

No se quitó ninguna animación ni se cambió ningún color: la cadena que sale para los **512** niveles es la misma que
la de la fórmula, y el test lo comprueba.

## 4. Mejoras y correcciones

- **UI:** fila de exportación con `flex-wrap` (tres botones ya no se aprietan), texto de la sección y `title` del
  botón nombran `.WAV`, `.MID` y `.GW1`.
- **Test de regresión nuevo** para el exportador, integrado en el grupo `web`.
- **Auditoría:** el firmware no cambia en esta fase (sólo la cadena de versión), así que las rutas de audio, USB,
  SysEx, backup y gates se revisaron pero no se tocaron; los runners `security`, `web`, `ui` y los fuzzers de la
  Fase 5 siguen siendo la evidencia vigente y se volvieron a correr para el cierre (§5).

## 5. Resultados de los tests de cierre

| Runner | Resultado |
|---|---|
| `node web/test_mid.mjs` (nuevo) | **64 checks, 0 fallos** |
| `bash tests/groups.sh web` (incluye el nuevo) | **pasa** |
| `node web/emu/emu_test.mjs` | **all ok** |
| `bash tests/groups.sh ui` (191 pantallas × 22 paletas) | 0 lint, 0 GREY/MONO, 3 785 084 medidas de alineación sin desviación |
| `bash tests/groups.sh security` | **pasa** |
| `make_release --check` | `release/`, `site/`, instalador, editor y ambos `SHA256SUMS` describen un paquete |

## 6. Informe pedido, en corto

- **Nuevo agregado:** exportación `.MID` (Standard MIDI File tipo 1) de la canción del Web Studio, con tests.
- **Mejoras:** UI de la fila de exportación, texto y `title`; el exportador respeta mutes y sostiene el pad.
- **Bugs corregidos:** el escritor MIDI ordenaba mal el meta de nombre frente a la primera nota (lo detectó el test
  antes de publicar: el nombre caía después de la nota a tick 0).
- **Mejoras de UI:** envoltura de la fila, textos de la sección; ningún cambio de color ni de animación.
- **Optimización:** caché del barrido de LEDs, **19,6×** en la parte medida, 512/512 valores idénticos.
- **Tests y seguridad:** los de arriba; sin cambios en firmware, gates ni allowlists.
- **Pendientes reales:** todo lo que necesita la FM-1 (updater del V16, restauración, escritura del chip, sonido y
  panel, Issue #106) sigue `[HW]` y no se afirma. El firmware de esta versión sólo cambia la cadena de versión: la
  mejora de sonido que la fase pedía no tenía una candidata medible sin unidad, y no se inventó una.
