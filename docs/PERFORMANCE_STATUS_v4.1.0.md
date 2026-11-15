# Estado de rendimiento — v4.1.0 (medición del 15-11-2026)

Este documento reemplaza como referencia vigente a `docs/PERFORMANCE_BASELINE.md` (v4.0.0). Cada cifra
indica su **clase de medición**. Nada de aquí se midió en una FM-1 física.

## 1. Cifras de build AC791N (registro, no re-medidas aquí)

Fuente: `release/build-manifest-v4.1.0.json` (generado por la build AC791N de la v4.1.0).

| Recurso | Valor v4.1.0 | Límite | Clase |
|---|---|---|---|
| Imagen de aplicación (`felucca.bin`) | 583 600 B | slot 583 612 B (`app.ld`) | build AC791N registrada |
| Paquete `.fwsc` | 610 959 B | 1 MiB NOR | build AC791N registrada |
| Cargador (`loader/ota.bin`) | 7 803 B | — | build AC791N registrada |
| Identidad | `FM-1_965` | — | build AC791N registrada |

**Advertencia de consistencia.** `docs/PERFORMANCE_BASELINE.md` (v4.0.0) dice que la imagen ocupa
583 600 B con 12 B libres. Eso coincide con el registro v4.1.0. Pero otros documentos citan cifras
anteriores para la misma imagen: `docs/OPTIMIZATION_BASELINE.md` (532 088 B y 539 836 B, de v2.x),
`docs/QG_PHASE_1_OPTIMIZATION.md` (551 720 B) y `docs/PERF_SIX_TRACK_FM1.md` (556 152 B y 581 564 B).
Esas cifras son históricas de versiones anteriores y no deben leerse como estado actual.

No re-medí la imagen AC791N en esta sesión: el toolchain (`JIELI_TOOLCHAIN` / `AC79_SDK`) no está
disponible, y `build/felucca.dis` no existe, así que `tests/target_budget.py` no puede correr.

## 2. Coste de CPU en host (medido aquí)

Comando: `cc -O2 -w -Ibuild/gen -Ifirmware/src -o regress tests/regress.c -lm` y después
`regress tests/golden.txt tests/cpu_baseline.txt`.

Resultado (host `Linux x86_64`, VM compartida):

* 293 renders golden: **0 cambiados**, 0 desaparecidos.
* Salud: **0 fallos**. Voces y enrutado: **0 fallos**.
* CPU en instrucciones por muestra: **0 entradas fuera de presupuesto** (+25 %).
* CPU en ns por muestra: PASS con límite agregado +20 %. Siete filas por encima de +40 % quedan como
  nota y no como regresión; el propio informe pide confirmarlas antes de llamarlas regresión.
* Salida: `EXIT=0`.

Clase: **host x86, wall clock y conteo de instrucciones del host**. No es ciclo de MCU.

## 3. Lo que no se puede medir aquí

* **Ciclos en el chip:** requiere la build AC791N y una placa. Pendiente.
* **`tests/target_budget.py` (instrucciones del objetivo):** requiere `build/felucca.dis`, generado con
  el toolchain. Pendiente. Su última cifra válida es la de `tests/target_budget.txt`.
* **Audio físico y cortes en hardware:** no hay unidad.
* **Coste de render en el objetivo:** las cifras de `[RENDER]` de `docs/QG_PHASE_1_OPTIMIZATION.md` son
  host.

## 4. Cifras de documentos que ya no son vigentes

Las siguientes afirmaciones de `docs/PERFORMANCE_BASELINE.md` (v4.0.0) se conservan como historia y no se
reescriben:

* 88 148 B de `.data+.bss` y 10 156 B libres: coinciden con el registro v4.1.0 (AC791N).
* Tabla de funciones de `target_budget` (sección 2): depende de `build/felucca.dis`; no se re-verificó.

Los documentos de v2.x y v1.x citados en la sección 1 describen versiones anteriores.
## Propuesta de degradado para EDIT > FILTER 4/6 (no implementada)

Estado: propuesta documentada, sin cambio de firmware. Clase de evidencia: lectura de código; ninguna medición
de pantalla.

Hecho verificado en código (`firmware/src/ui_being.c`, `gw_draw_forge_bay`):
- El degradado de la base (cuatro bandas, mezcla `f0`→`f1`) se dibuja solo si `ui.home`. Fuera de HOME
  (EDIT > FILTER) no se pinta, por decisión explícita del comentario: una base lavada quedaría bajo la forma
  que la página se lee.
- El orden de dibujo es: rejilla, traza de fondo, degradado, espectrograma, envolvente cian, curva naranja,
  nodo de resonancia. La curva queda encima del degradado, así que no la tapa en el orden de capas.

Propuesta: habilitar el mismo degradado en FILTER con mezcla reducida (por ejemplo `f0` a 4 % y `f1` a 8 %,
la mitad de la intensidad de HOME), y sólo bajo la curva, sin llegar a las dos bandas de la curva.

Por qué no se aplica aquí:
- Los colores son variables de paleta (`ux.bg`, `ux.theme`, `ux.accent`). El contraste de la curva con el
  suelo cambia con cada una de las 14 paletas; no hay un valor único que medir.
- `tests/theme_test.c` valida contraste de texto, no de la curva sobre un degradado. No cubre este caso.
- No hay captura de la pantalla de 240×240 en este entorno. Una legibilidad afirmada sin verla es una
  suposición.

Antes de implementar: capturar EDIT > FILTER 4/6 en las paletas GREY, HI-CON y MONO con y sin degradado, y
medir el contraste de la curva naranja y del nodo contra el suelo en cada una. Si alguna baja del mínimo
del proyecto (4.00:1 para texto), no se implementa en esa paleta.
