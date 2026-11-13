# Fase 5 — Motores: auditoría, cobertura de mandos y motores nuevos

**Ámbito:** lo que hay de verdad en el FM-1 (motores, presets, mandos) y qué se puede añadir sin romper
los recursos. Todo lo que sigue está medido en host x86 con el banco de pruebas del repositorio
(`tests/engine_param_test.c`, nuevo) y con el enlace real del AC791N. **No hay unidad física aquí**: no
se ha escuchado ni un preset.

---

## 1. Los motores que existen

| Índice | Motor | Estado | Presets |
|---|---|---|---|
| 0 | **ANALOG** | vivo | 68 |
| 1 | DIGITAL (FM4) | retirado, índice reservado (sus sonidos se convierten a FM6, `fm4_convert.c`) | — |
| 2 | **PHASE** | vivo | 32 |
| 3 | **LOFI** | vivo | 29 |
| 4 | SAMPLE | retirado en JIANT, índice reservado | — |
| 5 | **VOICE** (FORMANT) | vivo | 25 |
| 6 | TRIO | fundido en ANALOG (0.4), índice reservado | — |
| 7 | **WHEEL** (drawbar) | vivo | 22 |
| 8 | GRAIN | retirado en JIANT, índice reservado | — |
| 9 | PHYS | retirado en TONIC, índice reservado | — |
| 10 | **DRUM** (DRUM-X) | vivo | 18 |
| 11 | **NOISE** | vivo | 22 |
| 12 | **FM6** | vivo | 37 |
| 13 | SLICE | retirado en JIANT, índice reservado | — |
| 14 | **WAVETABLE** | vivo (3.0) | 14 |

**Nueve motores vivos, 267 presets.** Los índices retirados siguen reservados porque los proyectos y los
presets guardados nombran el motor por número: quitarlos rompería sonidos existentes.

De la lista de candidatos de la fase, **WAVETABLE ya está hecho** (motor 14: 8 tramas de 256 puntos con
morphing entre dos de ellas y modulación de la posición). No es un hueco.

## 2. Los mandos: 216 comprobados, ninguno muerto

`tests/engine_param_test.c` (nuevo, ya en `tests/run_tests.sh`) hace lo que hace un dedo: para cada
motor, tres de sus presets (primero, central, último), cada uno de sus ocho mandos EDIT forzado al
mínimo y luego al máximo del descriptor, la misma nota mantenida y el mismo bloque renderizado por el
camino real de audio (`mix_block`). Los dos buffers se comparan muestra a muestra.

```
engine param audit: 216 knobs x 3 presets checked, 0 with no effect at all (none)
```

**Ningún mando de ningún motor es decorativo**: todos cambian el sonido en ambos extremos de su rango.
El test distingue además los mandos que apenas mueven el sonido (menos de 2 LSB de diferencia): hoy no
hay ninguno. Es la otra cara de la auditoría de FX de la fase 4, donde los 51 parámetros también
llegaban al DSP.

## 3. El equilibrio de nivel, medido

`build/host/engine_param_test --levels` renderiza los 267 presets y mide el RMS de la parte sonante:

| Motor | n | mediana | rango |
|---|---|---|---|
| ANALOG | 68 | **−17,9 dBFS** | −30,2 … −10,3 |
| FM6 | 37 | −19,5 | −33,4 … −14,6 |
| PHASE | 32 | −18,9 | −27,6 … −10,2 |
| LOFI | 29 | −17,8 | −24,6 … −10,0 |
| VOICE | 25 | −16,9 | −30,3 … −9,9 |
| WHEEL | 22 | −16,8 | −23,4 … −12,7 |
| **NOISE** | 22 | **−29,7** | **−52,0 … −15,2** |
| WTABLE | 14 | −16,0 | −28,0 … −12,0 |
| DRUM | 18 | −15,2 | −27,4 … −9,9 |

Dos lecturas, y conviene no confundirlas:

1. **La familia NOISE está 12 dB por debajo de ANALOG** (−29,7 contra −17,9). Eso no es carácter: es un
   salto de volumen al cambiar de motor. Es el defecto de equilibrio más claro del banco.
2. **Los extremos dentro de un motor son casi siempre carácter.** Un `GEIGER CNT` a −52 dBFS son clics
   espaciados y un `RISER` empieza callado: el RMS de una ventana los hunde sin que estén mal. Los
   sospechosos de verdad son los que se alejan de su familia siendo del mismo tipo de sonido
   (`STEEL DRUM` −9,5 dB bajo la mediana de FM6, `TIN HAT` −12,2 bajo la de DRUM, `ANGEL AIR` −11,5 bajo
   la de VOICE, `CHROME PLUCK` −11,5 bajo la de WTABLE).

Nada de esto se ha «corregido» todavía, y por dos razones: subir el nivel de un preset es cambiar un
sonido que el dueño ya conoce, y cada entrada nueva en su `x[]` cuesta flash (ver §4). La lista queda
como trabajo concreto con nombre y apellido en `docs/QG_PRESET_REMASTER.md`.

## 4. Motores nuevos: el presupuesto real

| Recurso | Disponible | Qué pide cada candidato |
|---|---|---|
| Flash (región XIP) | **220 B** (581 344 de 581 564) | un motor con su render, su estado y su tabla de presets: cientos a miles de bytes |
| RAM `.data+.bss` | **10 540 B** (87 764 de 98 304) | — |
| Pool libre | **15 392 B** | un buffer granular de 0,5 s a 22,05 kHz mono 16 bits: 22 051 B |
| Particiones libres | 3 493 B antes de la siguiente | una muestra de 0,3 s a 8 kHz 8 bits: 2 400 B |

Con eso:

- **GRANULAR** — no cabe. Con 15 392 B de pool libre no hay ni medio segundo de buffer; sin buffer no
  hay granular, sólo un wavetable con otro nombre.
- **SAMPLE** — no cabe. No hay ninguna muestra almacenada y los 3 493 B libres no dan ni para un golpe
  corto; además el streaming desde flash no existe en el firmware.
- **VOCODER** — no hay fuente viable: el firmware no tiene una entrada de audio de análisis (el USB es
  reproducción) y añadirla pide un buffer de banda y una ruta de latencia que hoy no existen.
- **VECTOR** — necesitaría una segunda fuente por voz: el estado por voz crece con el pool (12 voces) y
  el margen de RAM es de 10,5 KB. Es el único «grande» que quizá quepa, y aun así pide flash que no hay.
- **SID, ACID, POLY** — no hacen falta como motores: son *formas de programar* lo que ya existe (pulsos,
  ring, sync, FOLD, filtros resonantes, sub, unison). Se entregan como **presets** sobre ANALOG / PHASE /
  WAVETABLE, que es donde suenan igual y no cuestan un byte de código.

**Conclusión:** de los siete candidatos, uno ya existe (WAVETABLE) y tres se resuelven con presets
(SID, ACID, POLY); GRANULAR, SAMPLE y VOCODER están bloqueados por memoria, y VECTOR por flash. Añadir
motores de verdad exige liberar la región de la aplicación —los 3 493 B del mapa de flash que el dueño
pidió no tocar— antes que escribir DSP.

## 5. Pruebas ejecutadas

| Prueba | Resultado |
|---|---|
| `build/host/engine_param_test` | **216 mandos × 3 presets, 0 sin efecto** |
| `build/host/engine_param_test --levels` | tabla del §3 (267 presets renderizados) |
| `build/host/preset_audit_test` | 267 presets: 0 silenciados, 0 al rojo, 0 nombres repetidos, 0 duplicados exactos, 0 fuera de rango |
| `tools/preset_audit.py` | 267 presets en 9 motores; 265/267 con movimiento (`x[]`); 0 problemas |
| Enlace AC791N | `image 581 344 B`, 220 B de margen, `.fwsc` empaquetado |

**No verificado:** cómo suenan. Ni un preset se ha escuchado en el FM-1, y ninguna de estas cifras dice
si un sonido es bueno.
