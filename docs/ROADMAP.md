# Hoja de ruta / Roadmap

Leyenda: **✅ disponible** (en esta rama y cubierto por tests) · **🧪 experimental** (implementado, con
tests, pero **sin probar en un FM-1 físico**) · **🗺️ planificado** (no empezado).

Nada de lo planificado se promete para una fecha. Cada punto dice **qué lo daría por terminado**, que
normalmente es una prueba concreta y no una impresión.

## 1. Sonido y síntesis

| | Elemento | Estado | Qué lo cierra |
|---|---|---|---|
| ✅ | Motores heredados (ANALOG, PHASE, LOFI, VOICE, WHEEL, DRUM-X, NOISE, FM6) y los filtros VOWEL / SCREAM | disponible | — |
| ✅ | 141 presets de fábrica (+28 sobre Pocket GammaWave) | disponible | — |
| 🧪 | CHORUS > **TYPE PHASER** (cuatro all-pass barridos en el bus de chorus) | experimental | escucharlo en hardware y comparar con el chorus en A/B |
| 🗺️ | Un segundo tipo de reverb o un *shimmer* en el bus | planificado | medir RAM (hoy 7 % libre) antes de escribir una línea |
| 🗺️ | Más presets QUANTUM para FM6 y DRUM (hoy +28 se reparten en 6 motores) | planificado | 20–30 presets nuevos + goldens |
| 🗺️ | Compartir buffers de audio (`sl_buf` / `cloud_buf`, 32 KB cada uno) para liberar RAM | planificado | demostrar que nunca suenan a la vez y goldens idénticos |

## 2. Efectos

| | Elemento | Estado | Qué lo cierra |
|---|---|---|---|
| ✅ | FX heredados (delay, chorus, reverb ROOM/SPRING, SLICER, DRIVE, CLOUD…) | disponible | — |
| 🧪 | Capa **punch-in**: VIBRATO `G#4`, GATE `C#5`, SWEEP `D#5`, DIVE `F#5`; VIB/DIVE grabados en el carril 11/12 | experimental | probarlo con un FM-1 real y con proyectos de firmware anterior |
| 🗺️ | Un *tremolo* de audio y un *pitch-down* master en las teclas negras libres | planificado | dos efectos nuevos en `perform.c` + `perf_block` dentro del presupuesto (+10 %) |
| 🗺️ | Sincronizar el PHASER al tempo (LFO de 1/4, 1/8, 1/16) | planificado | con el secuenciador, sin deriva en 16 compases |

## 3. Controles y flujo de trabajo

| | Elemento | Estado | Qué lo cierra |
|---|---|---|---|
| ✅ | Los 14 botones, 7 encoders + MASTER, 27 teclas, el deck de `[OCT−]`, GEN + TAKES, MUTATE | disponible | — |
| ✅ | MIDI de entrada en el emulador (Web MIDI del navegador / cola USB-MIDI del firmware) | disponible | — |
| 🗺️ | Mapeo MIDI configurable (CC → parámetro) guardado en `PANEL`/ajustes | planificado | formato de proyecto compatible + tests de ida y vuelta |
| 🗺️ | *Scenes*: capturar los cuatro knobs de una página y recuperarlos con un gesto | planificado | sin tocar el formato actual (campo nuevo opcional) |

## 4. Interfaz y presentación

| | Elemento | Estado | Qué lo cierra |
|---|---|---|---|
| ✅ | 13 paletas (QUANTUM por defecto), MENU > LARGE, lint de layout y alineación al píxel en CI | disponible | — |
| ✅ | Emulador visual en el navegador (pantalla, LEDs, controles, audio, MIDI) | disponible | — |
| 🗺️ | Animaciones de transición entre pantallas (hoy son cortes) | planificado | medir el coste de dibujo (`ui_render` imprime µs por pantalla) y no pasar del +20 % |
| 🗺️ | Página pública (GitHub Pages) con la presentación, versiones y notas | planificado | habilitar Pages y publicar (`docs/CI_WORKFLOW_APPLY.md`) |
| 🗺️ | Landing en español además de inglés | planificado | — |

## 5. Estabilidad y rendimiento

| | Elemento | Estado | Qué lo cierra |
|---|---|---|---|
| ✅ | Suite de host con ASan/UBSan, dos fuzzers, golden renders, presupuesto DSP por función y del camino DRUM | disponible | — |
| ✅ | `ui_render`: 0 hallazgos de lint y 0 medidas desalineadas de 2,5 M | disponible | — |
| 🗺️ | Medir CPU real con carga máxima (8 voces + batería + FX + reverb) | planificado | contador de ciclos o instrumentación del ISR **en el dispositivo** |
| 🗺️ | Esperas de hardware con cota (`fl_cache_idle`, SFC, P33) y su abandono seguro | planificado | probar en el emulador y en hardware que un fallo de periférico no cuelga el arranque |
| 🗺️ | Sondeo de RAM real en el FM-1 (hoy son cifras del enlazador) | planificado | mapa de memoria leído del dispositivo |

## 6. Distribución y seguridad

| | Elemento | Estado | Qué lo cierra |
|---|---|---|---|
| ✅ | Instalador web y CLI con SHA-256 **obligatorio**, *verify only*, CSP estricta, backup antes de escribir | disponible | — |
| ✅ | Guía de instalación y recuperación ([INSTALL.es.md](INSTALL.es.md)), releases con `SHA256SUMS` y `versions.json` ([RELEASES.md](RELEASES.md)) | disponible | — |
| 🗺️ | **Firma Ed25519** de los paquetes (diseño en [PHASE0_SECURITY.md](PHASE0_SECURITY.md)) | planificado | claves offline, verificador en web y CLI con tests de clave efímera, `min` anti-rollback |
| 🗺️ | Fijar la toolchain por hash en CI | planificado | crear la variable `JIELI_TOOLCHAIN_SHA256` (valor medido en `docs/CI_WORKFLOW_APPLY.md`) |
| 🗺️ | Backup/aviso también sobre el firmware de fábrica (hoy solo GammaWave/JIANT) | planificado | leer y guardar los objetos que el V15 deja |
| 🗺️ | Restauración reanudable (hoy, un corte deja objetos a medias) | planificado | reanudar por objeto sobre el manifiesto del backup |

## 7. Herramientas y comunidad

| | Elemento | Estado | Qué lo cierra |
|---|---|---|---|
| ✅ | Emulador con `tools/emu_dev.py` (rebuild + recarga), artefacto por PR, smoke test | disponible | — |
| ✅ | Web Studio: editor por Web MIDI o simulador, 4 pistas de cinta, export WAV | disponible | — |
| 🗺️ | Exportar/importar **un preset suelto** (hoy: backup completo o proyecto) | planificado | formato nuevo compatible (campo opcional) + tests |
| 🗺️ | Galería de presets de la comunidad con su hash, publicada como datos estáticos | planificado | esquema + validación al importar |

## No está planificado

* Otros modelos de hardware: no hay documentación ni unidad para probarlos.
* Audio USB *dentro* del emulador (el firmware lo tiene desactivado en el build web).
* Cualquier forma de flasheo automático sin confirmación del usuario.
