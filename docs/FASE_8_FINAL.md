# Fase 8 — el estado final, con lo que está medido y lo que no

Clases: **[TARGET AC791N]** = binario y mapa del enlace del toolchain; **[HOST x86-64]** = suite nativa,
renderizados y sanitizadores; **[WASM]** = emulador; **ninguna cifra es una lectura de la unidad FM-1**, que no
se toca ni se flashea (AGENTS.md §3).

## Versión y artefactos

| dato | valor |
|---|---|
| versión | **v2.5.1** (`FELUCCA_VERSION`, en `firmware/src/felucca.c`) |
| por qué no 2.5.0 | la etiqueta `v2.5.0` existe desde el 11-11-2026 sobre `138586c`; `tag-and-publish.yml` es idempotente y no mueve etiquetas, así que 2.5.0 no podía publicarse nunca |
| paquete | `release/felucca-v2.5.1.fwsc`, **610 933 B**, sha256 `b9429adda3222d53…` (el completo, en `release/SHA256SUMS`) |
| imagen de aplicación | **581 420 B** de 583 612 → **2 192 B libres** en el slot [TARGET] |
| WASM | 1 317 315 B, sha256 `7258430c…` [WASM] |
| commit del paquete | el `commit` del manifiesto; `coherence_check` exige que release/, site/, instalador, editor y emulador sean el mismo |

## Memoria libre por región [TARGET AC791N, `build/felucca.map`]

| ventana | libre | suelo |
|---|---|---|
| XIP (slot de la app) | 2 192 B | 2 048 |
| RAM (.data+.bss) | **17 756 B** | 4 096 |
| LOWBUF | 3 512 B | 1 024 |
| POOL | 13 444 B | 8 192 |
| RAMTEXT | 4 000 B | 1 024 |
| NOINIT | 124 B | 0 |

## Rendimiento y estabilidad [HOST]

- `audio_stress`: CPU p50 0,7 µs, p99 233,1 µs, **máx 614,6 µs** del presupuesto de 725 µs (0 % sobre), con
  12 voces y seis pistas.
- `six_track_live_test`: el audio renderizado es el grabado, segundo a segundo.
- `robust_test`: tramas malformadas rechazadas sin cortar el transporte.
- Suite completa: **4 200 comprobaciones**, 0 fallos salvo el `release set`, que esta ronda resolvió al recortar
  el paquete (el fallo era «el paquete es de otro commit»).
- Goldens de audio: **311 renders, 0 cambiados**.

## Gráficas [HOST]

203 pantallas × 22 paletas, 3 241 cuadros. **0 hallazgos de lint**, **0 desvíos de alineación** en 3 968 383
medidas, 0 tintas sobre su propio color, idle con **0 px cambiados**. Los únicos cuadros que cambiaron en la
última ronda son los 48 de la página ABOUT en las 24 paletas: imprimen la versión.

## Seguridad del instalador y del loader

No se modificó **nada** de `loader/`, del instalador ni de las protecciones: `ldr_flash_known`, `FL_JEDEC_OK`,
la allowlist JEDEC `0x856014` / `0xB36014`, el bloqueo de identificaciones desconocidas y el camino de
actualización quedan intactos. `tests/install_test.py`, los límites de paquete, la firma y el backup pasan en la
suite. **No hay ninguna prueba física** de instalación ni de recuperación en esta ronda.

## Pendiente de validación física (no se presenta como resuelto)

1. Instalación y actualización reales desde V15 y V16 en la unidad.
2. Lectura real de JEDEC y geometría de los dos chips permitidos.
3. Recuperación tras un corte a mitad de escritura.
4. Lecturas de memoria y CPU del firmware corriendo en el AC791N (todo lo de arriba es enlace y host).
5. Cualquier afirmación sobre la pantalla, el audio o los encoders en la unidad.

## Estado de publicación

`main` empuja el árbol de esta ronda; el workflow `tag-and-publish` crea la etiqueta `v2.5.1`, atestigua el
paquete y publica la GitHub Release a partir de `release/`. **La ejecución del workflow no se puede verificar
desde este entorno** (`gh` sin autenticar, sólo credenciales de push): lo que está verificado aquí es que el
árbol empujado contiene un paquete v2.5.1 íntegro y coherente con el sitio, el emulador y el instalador.
