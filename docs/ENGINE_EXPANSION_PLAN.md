# Plan de expansión de motores — por qué no se puede ejecutar todavía

Documento de trabajo para la fase de evolución de motores. No es un informe de auditoría: es el
presupuesto que falta y lo que ya está verificado sobre él.

## 1. El bloqueo, medido

`docs/PERFORMANCE_BASELINE.md` §1: **12 bytes** libres en la región XIP del AC791N (imagen 583 600 B de
583 612 B). Cada control nuevo implica, como mínimo:

| Pieza | Coste típico | Comprobado en |
|---|---|---|
| Código DSP de un control (unas 8-20 instrucciones por muestra, con su clamp y su modulación) | 80-250 B de XIP | 12 B libres |
| Una fila en la página EDIT (`param_desc_t` con nombre, rango, unidad) | 30-60 B de rodata + ~20 B de código | ídem |
| Un slot de parámetro nuevo (`P_*`) | 0 B si cabe en un hueco ya declarado | ver §2 |

No es una estimación de humo: la reescritura de `ux_mix` que **no** añadía funciones costaba 16 B y no
linkeaba. Con 12 B no entra ni el control más pequeño.

## 2. Lo que sí está ya verificado y no cuesta flash

* **FM6 tiene dos slots libres**: `eng_fm6.c` usa `P_E0, P_E2, P_E3, P_E4, P_E6, P_E7`; **`P_E1` y `P_E5` están
  declarados y sin usar**. Un control nuevo para FM6 puede ocupar uno de ellos sin tocar `P_COUNT`, sin
  migración de proyectos ni de presets de usuario y sin cambiar ids de parámetro. Es el candidato más barato
  del árbol.
* Los otros ocho motores usan los ocho slots `P_E0..P_E7`: ahí un control nuevo **sí** obliga a subir
  `P_COUNT` (que está en 125 de los 127 ids que caben en un registro de 7 bits, `core.h`) y a escribir la
  migración de `param_load`/`param_save`. No hay espacio para dos controles nuevos en ningún motor.
* RAM: 10 156 B libres en `.data+.bss` y 15 392 B en el pool. Un parámetro nuevo por motor cuesta ~8 B de RAM
  por track (6 tracks) y una entrada en el banco de presets.

## 3. De dónde saldría el flash

El código ya no tiene grasa medible: 41 funciones instrumentadas, 0 divisiones en bucles, el ISR completo a
−2 % de su presupuesto. Lo que ocupa son **assets generados**:

| Asset | Tamaño | Cómo se recuperaría sin perder un píxel ni una muestra |
|---|---|---|
| `AF_M_DATA` + `AF_S_DATA` + `AF_L_DATA` (mapas de fuentes) | 31 273 B | decodificar en RAM al arrancar, o 3 bpp con decodificador de ~60 B |
| `WT_DATA` (24 tablas de 256 puntos) | 12 288 B | antisimetría de media onda + espejo, o regenerar las tablas band-limited al arrancar |
| `ui_shapes` (line art de JIANT) | 10 861 B | decodificador RLE sobre las polilíneas ya delta-codificadas |
| `UAC_FIR` (160 fases × 24 taps) | 7 680 B | simetría por fase (line-phase) o generación al arrancar |
| `KC_DATA` (píldoras de keycaps) | 4 792 B | ya son 28 píldoras: parametrizar ancho/grosor en vez de dibujarlas |

Regla para hacerlo sin romper nada: el barrido de render (`tests/ui_render`, 189 pantallas × 22 paletas) y el
regresor de audio (`tests/regress`, hashes FNV-1a por motor × preset) tienen que dar **exactamente** lo
mismo. Si un asset se comprime, esos dos bancos son la prueba, y hoy los dos pasan.

## 4. Qué haría cada motor, en orden de coste

Ninguna de estas líneas se implementó: van con su coste para que la fase se ejecute cuando haya flash.

1. **FM6 — el más barato** (slots libres): feedback por operador y nivel de modulación por operador donde el
   DSP ya tiene la cadena de operadores (`eng_fm6.c`), más la vista de algoritmo que ya dibuja `ui_draw.c`.
   Coste: DSP + fila EDIT + preset; sin migración.
2. **WTABLE**: interpolación entre tablas adyacentes (hoy `WT_GET` elige frame y interpola dentro), posición
   modulable por el `m->shape` que ya llega; alimenta la visualización existente. Coste: DSP + fila EDIT.
3. **NOISE**: densidad y movimiento sobre el `drift` y los `NOISE_SEQ` que ya existen; texturas de viento y
   drones salen de COLR/mode sin tablas nuevas. Coste: DSP + fila EDIT. Ojo: `noise_render` está **en el
   techo** (+9 %, límite +10 %): cualquier instrucción añadida ahí necesita quitar otra.
4. **DRUM-X**: transitorio/cuerpo/cola por componente sobre los `P_E*` del kit, sin nueva familia de presets.
   Coste: DSP en `eng_drum.c` + fila EDIT; es el que más instrucciones por voz consume hoy (1 153).
5. **ANALOG / PHASE / LOFI / VOICE / WHEEL**: cada uno necesita un slot `P_E` nuevo → `P_COUNT` 125→126 y
   migración de formato. Se pueden hacer **de a uno** y con su prueba de compatibilidad (un proyecto guardado
   en 4.0.0 tiene que abrir igual, `tests/project_test`, `tests/preset_*`).

## 5. Lo que no puedo afirmar

* Ninguna mejora audible fue implementada ni medida en esta fase.
* No hay medición de audio en hardware: ni clics, ni aliasing, ni polifonía real. Todo esto se verifica con
  el regresor de host (hashes) y la compilación, que es otra clase de prueba.
