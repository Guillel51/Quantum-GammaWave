# Fase 4/4 — Cierre de pendientes, sitio, banner y entrega (v2.5.0)

Última vuelta de la ronda. No hay camino de sonido nuevo: lo que se cierra es lo que el proyecto **dice** de sí
mismo, lo que el repositorio arrastraba y lo que faltaba para publicar sin deuda conocida.

## 1. Lo que se investigó antes de tocar nada

- `git log` completo (1 553 commits), tags, ramas y el estado del árbol: el manifiesto de la 2.4.0 (`862c1cd`) y la
  línea 2.3.15/2.3.16 de las fases 1–3 conviven en el historial; la numeración se fija **una** vez en
  `firmware/src/felucca.c` y de ahí sale todo (`docs/VERSIONING.md`).
- Pendientes: no quedaba ningún `TODO`/`FIXME` en `firmware/`, `tests/`, `tools/` ni `web/` (una sola coincidencia,
  en un comentario de código).
- Números públicos contra el árbol: 290 presets (`tests/preset_slots_test.py`), 22 paletas
  (`build/gen/ui_palettes.h`), 10 motores mostrables (`core.h`, `NENG_SHOWN`), 311 renders golden, 4 163
  comprobaciones host. README/banner decían 285, 19 y 303.
- Issues y PRs de GitHub: **no se pudieron leer**. La API sin credencial devuelve `API rate limit exceeded` para
  esta IP y `gh` no está autenticado en este entorno. Se revisa lo que sí es inspeccionable: el árbol, los
  workflows, los manifiestos y el historial de commits (que cita los PRs mergeados).

## 2. Pendientes

| Pendiente | Estado |
|---|---|
| Números de presets/paletas/goldens viejos en README y banner | **Resuelto** — leídos del árbol |
| `og:image` duplicado y `twitter:image` con el marco de 240 × 240 | **Resuelto** — una tarjeta 1200 × 630 |
| `fmhub.json` con 291 presets y el naranja del linaje | **Resuelto** — 290 y `#3ADFFF` |
| Banner con recuentos viejos y sin recursos sociales | **Resuelto** — `tools/make_banner.py` |
| Versión 2.3.16 en el binario mientras el sitio anunciaba 2.4.0 | **Resuelto** — una sola versión, 2.5.0 |
| Escritura en la FM-1 física | **Pendiente por diseño** — límite del dueño |
| Lectura de Issues/PRs de GitHub | **Bloqueado** — sin credencial en este entorno |
| GitHub Release de la 2.5.0 | **Bloqueado** — el tag `v2.5.0` ya existe en el remoto (`138586c`, línea anterior); `tag-and-publish.yml` no mueve tags |

## 3. Banner y recursos

`tools/make_banner.py` dibuja, desde archivos que el firmware produjo:

| Archivo | Tamaño | Uso |
|---|---|---|
| `assets/branding/quantum_banner.png` | 2400 × 900 | README y escaparate |
| `web/site/assets/banner.png` | 1600 × 600 | el portal |
| `web/site/assets/og.png` | 1200 × 630 | Open Graph y Twitter |
| `web/site/assets/banner-mobile.png` | 1080 × 1350 | recorte vertical |

Las tres pantallas del banner son capturas reales de 240 × 240 (`assets/screenshots/`), no fotos ni renders de un
dispositivo. **No hay ninguna fotografía del M-VAVE FM-1 en el repositorio**, así que no se inventa una: el
instrumento aparece como lo que el proyecto puede mostrar con verdad, su propia pantalla.

## 4. Identidad y metadatos

Nombre, versión, enlaces y créditos se revisan en `README.md`, `fmhub.json`, `web/site/index.html`, `i18n.js`,
`docs/`, los manifiestos de `release/` y `site/`. Las atribuciones (Leo Kuroshita / Hügelton Instruments, JIANT,
Felucca, Pocket GammaWave como linaje) se conservan: el linaje es historia, no una referencia obsoleta.

## 5. Verificación

Ver §35 de `docs/VALIDATION_MATRIX.md` para la tabla completa. Resumen: build AC791N de la 2.5.0, emulador WASM
reconstruido, set de release sincronizado (`make_release --check`, `coherence_check`), suite host completa, y el
sitio regenerado desde `web/site/`.
