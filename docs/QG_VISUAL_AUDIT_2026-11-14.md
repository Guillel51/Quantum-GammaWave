# Auditoría de continuidad y sistema visual — 2026-11-14

Alcance de este documento: lo que se comprobó en esta sesión, con qué clase de evidencia, y qué queda
realmente pendiente del plan de identidad visual global. Nada de lo que aparece como `IMPLEMENTED` aquí se
apoya en un commit cuyo contenido no se haya leído o en una cifra histórica reutilizada.

HEAD auditado: `7b968a9` (sobre `a1c3073`). Versión del árbol: **v3.1.3** (`FELUCCA_VERSION`), paquete
publicado 610 959 B `51ecce7b…`, WASM 1 207 637 B `09663972…` (commit `fb1c8a0`). Este árbol **no** tiene
paquete propio: sus cambios de UI no están en un `.fwsc`.

---

## 1. Matriz de pendientes

Estados: IMPLEMENTED · PARTIALLY IMPLEMENTED · NOT IMPLEMENTED · REGRESSION · IMPLEMENTED BUT NOT VERIFIED ·
BLOCKED. La columna *evidencia* dice la clase: host test, render host, lectura de código, o nada.

| # | Pendiente | Estado | Evidencia | Prioridad |
|---|---|---|---|---|
| 1 | Desplazamiento autónomo del gráfico superior de HOME (KNOB 1 único eje) | IMPLEMENTED | `ui_being.c` `gw_draw_forge_bay` lee `bg_knob(0)`, sin `ui.frame` en el eje horizontal; render host | — |
| 2 | KNOB 2/3/4 conservan su función y siguen afectando al gráfico | IMPLEMENTED | mismo archivo; `bg_knob(1..3)` en curvas y color; render host | — |
| 3 | Entrar/salir repetido de HOME, cuadros superiores corruptos | IMPLEMENTED | `hv_air()` con fase `ui.frame`; `home_roundtrip` 50 entradas × 5 vistas × 2 pasadas, 0 diferencias (host) | — |
| 4 | Toque individual de los cinco mundos | PARTIALLY IMPLEMENTED | `gw_planet_at()` va al centro más próximo (3.1.3); `capture_pokes` 97/77/53/54/93 % de disco propio. El solape de discos sigue existiendo (ver §3) | media |
| 5 | Bahía orbital de HOME 88×52 → 91×55 | IMPLEMENTED | `ui_being.c` `(8,65) 91×55`, clip `9..97 × 66..118`, `ORX[4]=39`; `capture_pokes`, lint, goldens (host) | — |
| 6 | Segundo visualizador: dos planetas de fondo con más detalle y respuesta | NOT IMPLEMENTED | `ui_graph.c` `graph_home_viz` sigue con `gw_far_world`/`gw_embers` de 4.0 | alta |
| 7 | Degradados/paneles compartidos entre FX-BUS y HOME | IMPLEMENTED (primitiva) | `gfx.c` `ux_ground_bands`; `hv_grad` y `fxb_grad` son ya dos llamadas. 3 024 frames idénticos byte a byte antes/después (host) | — |
| 8 | Extender ese ground al resto de pantallas (EDIT, SEQ, ARP, SCL, FX, MASTER, PRESETS, DRUM, GLO, SYSTEM, ABOUT) | NOT IMPLEMENTED | hoy sólo HOME (7 vistas) y FX > BUSES lo pintan; el resto dibuja `T_BG` plano | alta |
| 9 | Gráficas (espectro 28 bandas, ADSR, filtros, EQ, FM6, WTABLE, medidores) con la misma calidad | PARTIALLY IMPLEMENTED | espectro y medidores ya usan rampas y `fx_vu`/`trk[].vu` reales; EQ y curvas siguen planas | media |
| 10 | i18n del portal (35 cadenas sin EN/ZH) | IMPLEMENTED | `web/test_i18n.py`: 227 cadenas, 315 EN, 317 ZH, «every source string has EN and ZH» (host) | — |
| 11 | Descripciones del portal desfasadas («tres órbitas y tres planetas») | IMPLEMENTED | `site/index.html`, `web/site/index.html` y ambos `i18n.js` | — |
| 12 | README en v3.0.1 con 273 sonidos | IMPLEMENTED | `README.md` → v3.1.3, 275 sonidos, cifras del build reales | — |
| 13 | Banner nuevo Solar Heat / Quantum, 16-bit, sin versión | IMPLEMENTED | `tools/gen_banner.py` + `assets/branding/quantum_banner.png` (sistema de cinco mundos en el emblema) | — |
| 14 | Capturas del firmware y del sitio desde el render real | IMPLEMENTED | `tools/generate_screenshots.py`, `tools/fw_shots.py --render`; `site/assets` y `web/site/assets` sincronizados | — |
| 15 | Release v3.2.0 / attestation del paquete | BLOCKED | token del runner inválido (401 en `/user` y `/actions/variables`); sin `JIELI_TOOLCHAIN`/`AC79_SDK` no hay `.fwsc` | — |
| 16 | Compilación AC791N y medición de presupuestos de este árbol | BLOCKED | no hay toolchain pi32v2 en este entorno; las cifras del README son del build 3.1.3, no de este árbol | alta |

---

## 2. Lo que sí se cerró en esta sesión

**i18n del portal.** Faltaban 35 cadenas en EN y 35 en ZH (el portal se reescribió y el diccionario no
siguió), más una clave que existía sin indentar y el test no podía ver. Ahora `web/test_i18n.py` sale 0 con
315 claves EN y 317 ZH para 227 cadenas de origen. `node --check` sobre el diccionario, correcto.

**Una sola primitiva de degradado.** `hv_grad()` (HOME, 12 bandas) y `fxb_grad()` (FX > BUSES, 4 bandas)
construían el mismo bucle por separado. Ahora las dos llaman a `gfx.c:ux_ground_bands(x, y, w, h, n, a0,
da, b0, db, clip_y0, clip_y1)`; la última banda toma el resto, así que una altura que no es múltiplo de `n`
no pierde filas, y el recorte por pasada de HOME sigue siendo explícito.

Prueba de que no cambia un píxel: `ui_render` sobre las 189 pantallas × 22 paletas antes y después →
**3 024 frames comparados, 0 distintos**; lint **0 hallazgos**; alineación **0 desviaciones en 3 390 046
medidas**; `regress` **293 goldens, 0 cambiados**.

---

## 3. El solape de los cinco discos: por qué no se cierra con un agrandamiento

Los cuerpos miden 8–12 px y sus trayectorias están a 5–8 px. Para que no se toquen nunca, las cuatro
separaciones entre órbitas consecutivas tienen que superar la suma de los radios vecinos —9 + 11 + 12 + 10 =
42 px— más ~5 px de radio exterior y ~5 px de holgura con la estrella: **≈52 px de radio de sistema**. Una
bahía de 91 px da 44. Haría falta una bahía de ~120 px de ancho, que la maqueta de HOME no tiene (la bahía
del hogar empieza en x 100), o cambiar la topología: menos trayectorias con varios cuerpos en cada una, que
es un rediseño y no un agrandamiento. Queda como pendiente medido, no como resuelto.

---

## 4. Lo que falta para la identidad visual global (y en qué orden)

1. **Aplicar `ux_ground_bands` al cuerpo de las páginas que hoy son planas.** Una llamada por pantalla, con
   los tokens de paleta, y el mismo barrido que se usó aquí (`ui_render` 189 × 22, lint, alineación) como
   puerta. Riesgo concreto: los `cv_text` que pintan su propio fondo `T_BG` perforarían la rampa; hay que
   pasar esos fondos a `T_SURF` pantalla por pantalla, no en bloque.
2. **Gráficas**: EQ y curvas de filtro con rampa vertical y rejilla, sin tocar los valores ni las etiquetas
   (el lint de alineación ya mide esos huecos).
3. **Segundo visualizador**: los dos mundos de fondo, con tamaño, atmósfera y reacción a las pistas, y
   `planets_test`/`capture_pokes` como contrato.
4. **Capturas antes/después por pantalla** y matriz de pantalla → cambio → test, que es lo que pide la
   Fase 2 del plan.

No se ha hecho nada de 1–4 en esta sesión más allá de la primitiva: es trabajo de muchas pantallas y
requiere el barrido completo en cada una.

---

## 5. Estado de verificación (clases separadas)

- **IMPLEMENTED**: primitiva compartida, i18n del portal, textos y capturas de presentación, bahía 91×55,
  README/CHANGELOG al día.
- **HOST TESTED**: `web/test_i18n.py` (0), `ui_render` (0 lint, 0 alineación, 3 024 frames), `regress` (293
  goldens sin cambios), `capture_pokes` (97/77/53/54/93 %), `particle_bounds_test` (0), `readme_nav_test` (ok),
  suite host completa (128 grupos, 4 879 comprobaciones) en el commit anterior a este documento.
- **AC791N TARGET COMPILED**: no en este árbol (sin toolchain).
- **PACKAGE VERIFIED**: no; el paquete publicado es el de 3.1.3, de otro commit.
- **WASM TESTED**: no; sin `emcc` aquí, y el WASM del sitio es el de `fb1c8a0`.
- **BROWSER TESTED**: no.
- **HARDWARE VERIFIED**: no. Nada se flasheó ni se leyó del FM-1.
- **NOT VERIFIED / BLOCKED**: release v3.2.0, `QG_RELEASE_APPROVED`, attestation, presupuestos AC791N de
  este árbol.
