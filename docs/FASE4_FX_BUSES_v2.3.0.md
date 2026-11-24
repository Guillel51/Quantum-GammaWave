# Fase 4 — FX, buses y la cola que saltaba (v2.3.0)

Clases de medida: `[HOST]` x86-64 con las fuentes reales, `[TARGET]` binario con el toolchain `pi32v2` fijado.
**No hay unidad FM-1 en este entorno.** Este documento no afirma nada del instrumento.

## 1. Lo que la fase pedía, y lo que el árbol ya tenía

| Pedido | Estado en v2.2.0 | Dónde |
|---|---|---|
| Routing, buses, sends | tres buses globales (delay, reverb, chorus), slicer en su bus, punch/duck del bus de batería, master; cada link con sus cuatro knobs y sus parámetros reales | `ui_fxb.c`, `params.c` (`FAM_FX`), `fx.c` |
| Wet/dry y gain staging | MIX por bus (`G_DMIX`, `G_RMIX`, `G_CDEPTH`), `fx_master_g()`, limitador y lámpara de clip | `fx.c`, `ui_fxb.c` |
| Reverb | **5 modelos**: ROOM SPRING SHIMR RESO CLOUD, SIZE/DAMP/PRE | `params.c:65`, `fx.c` |
| Delay | ANALOG (satura, resuena, pasado 100 % crece) y DIGITAL, con túnel de ecos dibujado | `params.c:222`, `ui_tech.c` |
| Chorus / Phaser / Flanger | el bus de chorus lleva los tres como TYPE | `params.c:48` (`N_CHOTYPE`) |
| Distorsión / saturación por pista | DIST con 6 modelos y DRIVE | `fx.c`, `params.c:52` |
| Vúmetros por bus | `fx_vu[0..7]` publicados por el DSP y leídos por la consola | `fx.c` |
| Medidor de salida y clip | `audio_out_peak()`, lámpara latcheada en MASTER | `ui_fxb.c` |

La arquitectura de FX ya es la que la fase describe. Lo que faltaba era **corregir la pantalla**.

## 2. El fallo: la cola del reverb se re-cuantizaba

El bloque REVERB de FX > BUSES dibujaba la cola como barras, y el número de barras era
`4 + size·5/127`: **cinco configuraciones para todo el recorrido de SIZE**. Cada escalón cambiaba a la vez
el número de barras, su anchura (`dw / n`) y su altura: el dibujo se rehacía por completo cinco veces, y en
los cuatro saltos la línea parecía irse a otro sitio al girar el knob.

## 3. El arreglo

Nueve barras fijas y **SIZE como pendiente**, no como cuenta:

```c
int32_t fall = 300 - size * 250 / 127;   /* por mil por barra */
h = top;                                  /* del retorno del bus: wet + send/2 */
for (k = 0; k < 9; k++) {
    cv_rect(dx + k * dw / 9, dy + dh - h, dw / 9 - 1, h, ...);
    h = h * (1000 - fall) / 1000;
}
```

- `top` depende del retorno real del bus (no de SIZE), así que la altura de la cabeza sigue el audio.
- `fall` va de **300 ‰** (SIZE 0: la cola muere dentro del bloque, 0,7⁹ ≈ 4 %) a **50 ‰** (SIZE 127: sigue
  sonando al final, 0,95⁹ ≈ 63 %). Es lineal en SIZE y el bucle tiene un número fijo de pasos: la función es
  continua, no hay escalones que contar.
- La anchura de barra ya no depende de SIZE, así que nada más se recoloca al girar el knob.

## 4. Medidas

| Medida | v2.2.0 | v2.3.0 |
|---|---:|---:|
| `[TARGET]` imagen | 580 180 / 583 612 (3 432 libres) | **580 180 / 583 612 (3 432 libres)** — 0 B |
| `[HOST]` lint de UI | 0 hallazgos | **0** (196 pantallas × 22 paletas) |
| `[HOST]` alineación | 0 / 3 953 168 | **0 / 3 953 168** |
| `[HOST]` baseline de píxeles | 3 129 frames | regenerado solo para FX > BUSES |
| `[HOST]` `reverb_test`, `fx_meter_test` | pasan | **pasan** |
| `[HOST]` lazo / mezcla | 1 628 ns / 277,7 ns·muestra | 1 655 ns / 277,7 |

## 5. Lo que queda de la fase, y por qué no está aquí

- **Efecto nuevo**: los **127 ids de parámetro están agotados** (`P_COUNT`), así que un efecto nuevo solo puede
  entrar como *valor* dentro de un parámetro existente (como NOTCH/DIP en `P_FTYPE` o PLUK/TRAP/STEP en
  `P_LWAVE`). Candidatos viables con ese molde, con su coste estimado `[HOST]`: un modo más del bus de chorus
  (p. ej. un *ensemble* de 3 voces) ~350 B, o un tercer modelo de delay (cinta con *wow/flutter*) ~300 B.
  Ninguno se hizo en esta pasada: el presupuesto de esta versión era **corregir lo que la fase reporta como
  roto**, y añadir un modelo sin medirlo contra los 31 destinos de modulación y los presets habría sido
  volver a lo que la fase anterior dejó fuera por falta de prueba audible.
- **Suavizado de parámetros y clicks**: el DSP ya rampa niveles y mezclas por bloque (`lev_*`, `dist_w`,
  `fx.cl_fill`); un cambio de modelo de reverb a mitad de cola sigue siendo un corte, y arreglarlo de verdad
  pide un crossfade de dos colas en paralelo — **+59 520 B de `.pool`**, medido sobre `cv_buf`. No cabe.
- **UI**: la consola de FX > BUSES ya usa icono + tres letras por link, seis bloques de 76 × 52 con vúmetro de
  send/retorno y lámpara de actividad; lo único que mentía era el conteo de barras del reverb, y eso es lo que
  se arregló. Las zonas de los cuatro knobs son las del motor de páginas común y coinciden por construcción
  (`params.c` `page_desc`, verificado por el lint de alineación).
