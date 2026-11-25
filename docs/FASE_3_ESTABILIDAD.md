# Fase 3/3 — Estabilidad, estrés, audio y auditoría final (v2.3.16)

Ronda de cierre sobre el árbol de la fase 2. **El firmware no cambió**: los commits de esta fase tocan pruebas,
presupuestos y documentación, y el paquete es el mismo (610 933 B, sha256 `25a55a92…`). Sin unidad FM-1: todo lo
de aquí es `[HOST]` (runners nativos, banco de renders, emulador) salvo el build de AC791N, marcado `[TARGET]`.

## 1. Estrés y sesiones largas

| Prueba | Qué hizo | Resultado |
|---|---|---|
| `tests/stress_run.sh 5` | `project_test`, `persistence_test`, `robust_test`, `midi_control_test`, cada uno **5 corridas** bajo ASan + UBSan, y **2 pasadas completas** de los goldens | 0 fallos, 0 fugas, 0 UB |
| `audio_stress 720` | **600 s de audio en una sola corrida** (826 875 bloques): seis motores, seis pistas, todas las buses (CHORUS, DLY, REV, DIST), secuenciador con acordes, MIDI entrando y motores cambiándose bajo las notas | ver abajo |
| Suite completa | 24 notas en 12 voces, seis partes + FX dentro del bloque, cambio de motor bajo un acorde sostenido, 20 notas en 12 contando robos | todo `ok` |
| `six_live` | 61 s de audio multitrack contra su golden | 61/61 idénticos |

`audio_stress`, 600 s en una sesión:

```
peak    L 0.300  R 0.300     (0 tramas sobre escala completa)
dc      L -0.0000  R -0.0000
jump    0.172 en una muestra, 0 pasos sobre 0.5
nonfinite 0
cpu     p50 0.7 us  p99 236.8 us  de los 725.0 us del bloque (33 %)
```

Los picos de `max` (3.3–11.9 ms) son preferencia del host en corridas de 13 s sobre un runner compartido: la misma
prueba de 43 s da 602 µs. No hay deriva: p99 y pico por bloque no crecen con la duración.

## 2. Audio

Ni un click, ni DC, ni NaN, ni un paso grande en 10 minutos de material bajo carga: `audio_stress` (peak, DC, salto,
no finitos), y en la suite `click_test`, `perform_test` (FX layer), `slicer_test`, `morph_six` (cada transición con
su latencia, slew y click), `microstart`, `lofi_tclk`. Los canales quedan equilibrados (peak L = R = 0.300) y el
leveler del master no deja pasar ninguna trama sobre escala completa con seis motores y todas las buses.

## 3. Visual

`ui_render` sale **0** y la puerta de píxeles da **3 241 cuadros idénticos** a la baseline de la fase 2: esta ronda
no movió un píxel, así que no puede haber regresión visual — y la baseline ya estaba revisada pantalla por pantalla.

## 4. Seguridad y compatibilidad

`install_test.py` (marcador del loader Felucca, allowlist de JEDEC `0x856014` / `0xB36014`, restore oficial omitido
porque `FM-1_v15.fwsc` no está en el repo), `web/test_web.mjs`, `web/test_backup.mjs`, `web/test_pkg_bounds.mjs`,
`release_gate_test`, `make_release.py --check` y `coherence_check.py`: todos en verde, `SHA256SUMS` verifica y el
instalador, el editor y `versions.json` describen el mismo paquete. No se relajó ninguna protección ni se tocó la
identificación del hardware.

## 5. El último paso rojo

`tests/target_budget.py` (conteo estático sobre el desensamblado del target) estaba rojo desde antes de v2.3.16 y
tenía dos causas distintas:

1. **`events_block` 33 321 contra 45 589** (y `BLOCK_PATH` 33 437 contra 45 704): la caída del 27 % venía de en qué
   bucle cuenta el compilador cada instrucción, no de menos trabajo (1 783 → 1 797 instrucciones). Re-base revisado,
   con la nota en el archivo.
2. **`fm6_render` 599 contra 292**: el presupuesto es de `daf5e39`, anterior a `f5e0ad7`, que añadió a esa función
   los dos pases de CHARACTER (TONE y FOLD), condicionales: un patch sin CHARACTER no los ejecuta. Re-base revisado.

Las otras diez líneas de la tabla no se tocaron: estaban dentro de su 10 %. El paso, después, sale 0.

## 6. Recursos

| | Valor | Libre |
|---|---|---|
| Imagen (app slot) | 580 736 B de 583 612 | 2 876 B (suelo 2 048) |
| RAM `.data+.bss` | 93 556 B de 98 304 | 4 748 B |

## 7. Límites

- **Sin unidad FM-1**: nada de esto es una medición de hardware; el `[TARGET]` es el build, y `[HOST]` las pruebas.
- `make_release --check` y `manifest_test` necesitan los artefactos en `build/`; con `NO_PACKAGE=1` el segundo se
  salta, como dice su propio mensaje.
- La suite completa tardó ~35 min y la cortaron tres reinicios del runner; la corrida que se cita es la que terminó.
