# Camino a 3.0 — CPU, estado real y qué falta para publicar

Fecha: 2026-11-26. Base: `852d1b6f`.

## 1. Qué se optimizó (y por qué solo esto)

El perfilado serio de este firmware no se puede hacer desde aquí: `regress` informa **`0 instr`** en cada escenario
(el host no expone contador de instrucciones) y los tiempos caen todos en **226,8 ns**, que es un tick del reloj de
10 ms repartido entre bloques: la misma cuantización que ya documentaron las fases 3 y 5. Con esa resolución no se
puede afirmar una mejora porcentual, así que **no la afirmo**. Lo que hice fue quitar trabajo ejecutado y demostrar
que la salida es idéntica bit a bit:

1. **`voice_render`, por voz y por bloque**: el residuo de afinación (`v->fine + tune_fine + bend_fine`) se sumaba
   tres veces — en `m.fine`, en la condición y otra vez dentro del `if`. Ahora se suma una vez y se usa el local.
2. **`tf_run`, el filtro de pista**: la posición dentro del bloque (`w = (i+1) << (12-CTL_LOG2)`) se calculaba en
   **cada muestra** aunque sólo la usan tres casos — entrada del filtro (`!was`), salida (`!on`) y crossfade de TYPE
   (`fo != TF_DRY`). En el estado estacionario, que es el caso común (el filtro simplemente filtrando, seis pistas
   a la vez), era un desplazamiento muerto por muestra y por pista. Ahora `need_w` se calcula una vez por bloque y
   el bucle no hace ese trabajo cuando no hace falta. Nada más cambió dentro del bucle.

**Prueba de que no hay cambio sonoro**: `regress` → **316 renders dorados, 0 cambiados, 0 perdidos**, 0 fallos de
salud, 0 de voces/ruteo, 0 notas por encima del +40 %, 1 fallo de presupuesto de CPU — el del host (reloj de 10 ms),
que ya venía de antes. Duración de la corrida: 53,4 s.

**Recursos [AC791N]**: imagen **580 396 B** de 583 612 (**3 216 libres**), RAM `.data+.bss` 75 664 de 98 304,
POOL 296 156 (24 352 libres), LOWBUF 28 488 de 32 768, alcance de llamadas desde RAM 10 228 B (mínimo 8 192).
No se tocaron polifonía, pistas, motores, efectos, frecuencia de muestreo ni calidad.

## 2. Lo que impide publicar 3.0 (bloqueos del entorno, no del código)

| bloqueo | consecuencia |
|---|---|
| **`emcc` ausente** | el emulador WASM no se puede reconstruir desde estos cambios. `make_release.py --sync` se niega (`build/emu/manifest.json is 'v2.12.0', not a clean vX`) y `--check` falla. Publicar 3.0 cuyo emulador sigue en 2.12.0 sería el mismo error que se limpió en `852d1b6f`. |
| **sin `gh` ni credenciales de API** | no se puede crear el tag remoto ni la Release. `git push origin refs/tags/...` responde `could not read Username`; `cloud_git` sólo empuja HEAD a una rama. |
| **sin unidad FM-1** | nada de lo anterior es prueba física: ni CPU real, ni audio escuchado, ni flash escrito. |
| **sin contador de instrucciones en el host** | CPU antes/después medible en cifras: no disponible aquí. |

Por eso el árbol **sigue anunciando v2.12.0** (el paquete que `make_release.py --check` valida como uno solo) y no
existe tag ni release 3.0.

## 3. UI y estabilidad: estado

- 207 pantallas × 22 paletas: 0 hallazgos de lint, 0 tintas sobre su propio color, 0 desalineaciones, 0 píxeles
  GREY/MONO fuera de gris (barrido completo, base de píxeles al día).
- `nav` (14 estados) y `flicker` (0 px borrados y devueltos) dentro de la suite.
- Pantallas con hueco muerto pendientes, con su medida: `presets_nofav` 63 px, `tilt` 73, `lfo_shape` 70,
  `shift`/`pattern` 102, `master` 42 (las dos peores de la fase 4 y las dos del secuenciador ya están corregidas).

## 4. Pendientes de audio/UI heredados, sin cambios

CLOUD 0,1 dB a SIZE 127 (la realimentación no corre), interletraje `Gl` 0,258 px (tablas AA por fase), dos presets a
0,044 (rediseño de contenido que arrastra goldens y CPU), Web Studio sin `GLIDE`/`STRUM` hasta que exista emulador
nuevo.
