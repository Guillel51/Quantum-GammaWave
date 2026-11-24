# FASE 2/2 — Auditoría de eficiencia y de bugs (2026-11-24)

Clase de cada cifra, siempre: **build AC791N real** (toolchain JieLi `20260730.1` + SDK
`AC79NN_SDK_V1.2.1_2023-12-13`), **host x86-64** (`cc -O2`, y `-pg` donde se dice), **harness de UI**
(`tests/ui_render.c`, píxeles y ventanas que la app manda al panel) o **emulador** (WASM + build nativo).
Ninguna de estas cifras es una medida sobre una FM-1: no hay unidad en este entorno, así que latencia,
ciclos reales, JEDEC y escritura de flash siguen sin medirse. `INSTALL BLOCKED` sigue en pie.

---

## 1. El pendiente de la Fase 1: la publicación, cerrada con causa raíz

La Fase 1 dejó el código, el release set y el sitio coherentes y subidos, con un tag `v1.11.2` en
`43dca0b` creado por la CI y **ningún GitHub Release**. La causa no era la credencial del tag:

| evidencia | qué dijo |
|---|---|
| runs #388, #389, #390 (`tag-and-publish`) | `failed` en 20–26 s, uno por cada push de la fase |
| `release_gate.py`, `fm1_sign.py verify`, `coherence_check.py`, `make_release.py --check`, `web/test_site.py` | los cinco pasan en local, sobre el mismo árbol |
| `.github/workflows/tag-and-publish.yml`, paso *the notes, from the changelog entry for this version* | `sys.exit("no CHANGELOG section names " + version)`, **después** del paso que crea el tag |
| `CHANGELOG.md` | no tenía sección `## v1.11.2`: la última era v1.11.1 |

El job etiquetaba el commit, y moría al cortar las notas. Se añadió la entrada del CHANGELOG con las
cifras reales de la Fase 1 y el mismo job publicó **v1.11.2 el 24 Nov 05:19**, sin mover el tag.

**Prueba de regresión añadida**: `tools/coherence_check.py` ahora comprueba que `CHANGELOG.md` tiene una
sección que nombra la versión de `FELUCCA_VERSION`, antes de etiquetar nada (verificado en los dos
sentidos: con v1.11.2 pasa, con una versión inventada falla con el motivo escrito). Es el gate que
faltaba: el que se ejecuta justo antes de crear el tag.

---

## 2. Perfilado de ejecución (host x86-64, `-pg`, 44,2 s de suite)

`tests/regress.c` compilado con `cc -O2 -pg` y perfilado con `gprof` (muestreo, 2,87 M bloques de 32
muestras, 311 renders). Las funciones de audio con más tiempo propio:

| función | % propio | llamadas | qué es |
|---|---:|---:|---|
| `fx_buses` | 11,9 | 2 874 508 | coro + delay + envíos, con los bucles inlineados |
| `analog_dispatch` | 10,5 | 5 377 612 | reparto por bloque del motor ANALOG (con `analog_render` inlineado) |
| `rev_room` | 5,7 | 2 874 508 | red de la reverb |
| `mix_part` / `track_render` | 5,1 / 4,4 | 17 247 048 | mezcla y render por pista y bloque |
| `master_out` | 3,5 | 91 984 256 | una vez por muestra |
| `pd_wave` | 2,4 | 141 123 584 | una vez por muestra (PHASE) |

Nota de clase: en ese build instrumentado el gate de CPU por muestra falla por **+26 %** — es el coste de
`-pg`, no un cambio del firmware; el build real no se toca en esta fase. El perfil sirve para localizar
trabajo, no para presupuestarlo.

**Lo que el perfil descartó**: `analog_character` (el carácter que se aplica a cada bloque del ANALOG) ya
sale antes de su bucle cuando su cantidad es 0 (`if (!a) return;`), `fx_sleep` ya duerme las tres líneas
cuando nada se envía y la compuerta ya las ha silenciado, y `snd_hash` ya pasó de seis recorridos por
llamada a dos (la pista seleccionada y otra por turno). No queda un bucle redundante que quitar sin
cambiar el sonido: los goldens tienen que seguir siendo los mismos 311.

---

## 3. El desperdicio que sí está medido: el cambio de pista

`tests/ui_render.c --regions`, caso C (MIXER, paleta QUANTUM), con la instrumentación de ventanas que se
añadió y se retiró para medirlo:

| hecho | cifra |
|---|---:|
| frame entero forzado | 53 064 px escritos |
| **cambio de pista** | **31 400 px escritos** |
| píxeles que de verdad cambian en el panel | 872 (todos en las dos columnas de la selección) |
| ventanas enviadas | cabecera 240×16, 4 tarjetas 57×56, 2 columnas 38×122, pie 240×36 |
| de esas, **idénticas** | las 4 tarjetas: **12 768 px**, el 41 % de lo enviado |

El panel recibe 12 768 px que ya tenía, en la acción de navegación más usada. La causa está localizada:
`gfx.c` difunde por filas (`cv_rowh`/`cv_rowv`) solo ventanas de **240 px de ancho y ≥16 filas**; las
tarjetas son ventanas estrechas y se envían enteras.

### Por qué no se ha cambiado todavía

| candidato | coste medido | veredicto |
|---|---|---|
| caché de ventanas (`FELUCCA_WINCACHE`, ya escrito en `gfx.c`) | ahorra esas 4 ventanas y cualquier redibujo idéntico; **desborda XIP en 2 752 B** (imagen 581 508 de 583 612, 2 104 libres) | imposible con el presupuesto actual |
| saltar la fila de tarjetas con `ui.sel_only` | +~30 B de XIP, −12 768 px por cambio | **rechazado**: las tarjetas de HOME (`home_param`) y las de SHAPE (`TSEL->p`) **sí** dependen de la pista; una lista blanca por página no se puede demostrar segura sin una prueba por página, y la prueba que existe solo cubre el MIXER |
| `ui.force` en `track_select` | se quitó y se midió: **31 400 px igual**, ningún ahorro | revertido |

Lo que haría falta para cobrar los 12 768 px está escrito y medido: liberar 2 752 B de XIP (o reescribir
la caché en menos de lo que ocupa) y entonces activar `FELUCCA_WINCACHE`, que ya tiene su `.noinit`, su
invalidación por fills y su camino fuera de línea. No se activa a medias ni se abre una lista blanca
frágil: la regla del proyecto es que una mejora se demuestra, no se supone.

---

## 4. Revisión de bugs

Lo que se ejecutó en esta fase, y con qué clase de evidencia:

- **Suite completa del host** sobre las fuentes finales: goldens de audio (311 renders), salud (clipping,
  DC, colas, voces libres), secuenciador, grabación, MIDI USB/TRS, backup/restore, editor, robustez con
  ASan/UBSan, paquete, instalador y preflight. Un solo grupo en rojo: *UI pixels*, por el número de
  versión dibujado en splash/ABOUT (48 frames de 3 129); regenerado con `ui_pixels.py --update` y
  verificado después: **3 129 idénticos**.
- **Emulador**: 88 200 frames **bit a bit** contra el build nativo, 1 s de canción pesada en 25,1 ms
  (2,5 % de tiempo real), y el arranque/guardado/MIDI/lector de pantalla del harness.
- **Release y sitio**: `make_release.py --check`, `manifest_test`, `coherence_check` (ahora con el
  CHANGELOG), `site_check`, `web/test_site.py`, `install_test`, `preflight_test`.
- **Perfil y tráfico de panel**: sección 2 y 3 de este documento; sin regresiones de píxeles (el frame que
  deja un cambio de pista sigue siendo idéntico a un redibujado entero: 0 px distintos).

**Bug corregido en esta fase**: la publicación de v1.11.2 (sección 1), con prueba de regresión. No
apareció ningún defecto de audio, UI, MIDI ni concurrencia que la suite no cubriera ya: los 872 px del
cambio de pista son correctos, el resto de lo enviado es redundante pero no erróneo.

---

## 5. Recursos y estado

| recurso | valor | clase |
|---|---:|---|
| RAM `.data+.bss` | 92 688 de 98 304 (5 616 libres, suelo 4 096) | build AC791N |
| LOWBUF | 31 648 de 32 768 (1 120 libres, suelo 1 024) | build AC791N |
| POOL | 312 124, holgura 8 384 (suelo 8 KiB de `AGENTS.md`) | build AC791N |
| XIP | 446 556 B de código + 130 152 de constantes | build AC791N |
| imagen | 581 508 de 583 612 (2 104 libres) | build AC791N |
| paquete | 610 933 B, sha256 `7b04f44d…` | build AC791N |
| emulador | 1 316 630 B de WASM, zip 521 813 B, sha256 `bbfc3221…` | build emcc 6.0.11 |
| canción pesada (1 s de audio) | 25,1 ms (2,5 % de tiempo real) | emulador, esta VM |

## 6. Pendientes, en orden de valor

1. **La caché de ventanas del panel** (12 768 px por cambio de pista, sección 3): necesita 2 752 B de XIP.
   Caminos: reescribir `cv_win_seen` más corto, o liberar código midiendo con `tools/engine_size.py` y
   `target_budget`. Es el ahorro más grande y ya está diseñado.
2. **12 voces**: la Fase 1 dejó 5 616 B libres en la ventana que lo impedía (necesitaba 1 112 B). Ahora
   cabe en RAM; lo que falta es la medida de CPU por bloque con 12 voces en el target, no en host.
3. **El tick de 10 kHz** y cualquier cifra de latencia o de ciclos: necesitan la unidad.

Nada de esto se probó en la FM-1 física.
