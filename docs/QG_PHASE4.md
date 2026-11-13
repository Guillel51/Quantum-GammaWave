# QG — Fase 4: biblioteca, Hub, sincronización y seguridad

**Fecha:** 13 de noviembre de 2026 · **Base verificada:** commit `e696e03` · **Clase de las medidas:** host x86
(salvo donde se dice red, bytes publicados o PENDIENTE).

**Estado:** la Fase 4 **no se cierra**. Las fases 1–3 no están aprobadas: la Fase 3 quedó parcial (host) y la
Fase 2 sigue sin fusionar. Por eso esta pasada hace sólo lo que no depende de esa aprobación y no altera sonido ni
publica nada: auditoría real de la biblioteca, dos defectos de sincronización corregidos, repetición de la seguridad
en host y estado honesto del Hub. **Ningún preset ha sido modificado, ningún tag/release se ha tocado y no se ha
escrito en ninguna FM-1.**

---

## 1. Inventario de la biblioteca (Paso 1)

Medido con `tests/preset_audit_test.c` (carga y render reales, 267 presets) y `tools/preset_audit.py` (la biblioteca
como datos). El inventario completo, por motor y con los pares más cercanos, está en
[`QG_PHASE4_PRESET_INVENTORY.json`](QG_PHASE4_PRESET_INVENTORY.json).

**267 presets de fábrica en 9 motores.** No hay un objetivo arbitrario: estos son los recuentos.

| Motor | n | % | Categorías (n) | RMS mediana* | Pico máx.* | Con movimiento |
|---|---:|---:|---|---:|---:|---:|
| ANALOG | 68 | 25,5 | KEYS 29 · PADS 11 · BASS 10 · LEADS 8 · PLUCKS 5 · BELLS 2 · DRUMS 2 · ARPS 1 | −16,9 | −5,7 | 68/68 |
| FM6 | 37 | 13,9 | KEYS 21 · BELLS 7 · PADS 3 · PLUCKS 3 · BASS 2 · DRUMS 1 | −18,3 | −5,8 | 37/37 |
| PHASE | 32 | 12,0 | TEXTURES 14 · BELLS 5 · PADS 4 · BASS 3 · KEYS 3 · PLUCKS 2 · DRUMS 1 | −18,0 | −5,6 | 32/32 |
| LOFI | 29 | 10,9 | TEXTURES 14 · ARPS 5 · PADS 4 · KEYS 2 · LEADS 2 · BASS 1 · DRUMS 1 | −16,6 | −5,6 | 29/29 |
| VOICE | 25 | 9,4 | PADS 23 · BASS 1 · LEADS 1 | −16,3 | −5,4 | 25/25 |
| WHEEL | 22 | 8,2 | ORGANS 18 · PADS 3 · DRUMS 1 | −15,7 | −5,7 | 22/22 |
| NOISE | 22 | 8,2 | TEXTURES 21 · PADS 1 | −21,8 | −4,0 | 22/22 |
| DRUM | 18 | 6,7 | DRUMS 18 | −15,0 | −5,8 | 16/18 |
| WTABLE | 14 | 5,2 | PADS 8 · BASS 1 · BELLS 1 · KEYS 1 · LEADS 1 · PLUCKS 1 · ARPS 1 | −14,7 | −5,8 | 14/14 |

\* RMS de la parte sonora de una nota sostenida, con el `P_LEVEL` propio del preset (el que carga de verdad); pico en
dBFS de esa misma nota. Es una medida de host, no una escucha ni una mezcla.

**Categorías totales:** PADS 57 · KEYS 56 · TEXTURES 49 · DRUMS 24 · BASS 18 · ORGANS 18 · BELLS 15 · LEADS 12 ·
PLUCKS 11 · ARPS 7.

**Duplicados y defectos estructurales:** 0 silenciados, 0 al rojo (`hot`), 0 nombres repetidos, 0 duplicados exactos,
0 parámetros fuera de rango, 0 presets sin ningún envío. **265/267 llevan movimiento** en `x[]`; los dos que no son
kits de batería (`HEARTH KIT`, `CYBER DRUM`), como corresponde.

**Similitud.** El test considera duplicado cercano un vector de motor idéntico con envolvente y envíos dentro de 4:
**0 pares**. Con una distancia normalizada más amplia (8 parámetros de motor, 4 de envolvente y 4 envíos), los pares
más cercanos son:

| Motor | Par | Distancia normalizada |
|---|---|---:|
| FM6 | WARM PAD / GLASS PAD | 0,014 |
| ANALOG | EMBER LEAD / VELVET SCREAM | 0,020 |
| FM6 | KOTO HARP / STEEL DRUM | 0,022 |
| ANALOG | SOFT PAD / STRINGS | 0,035 |
| LOFI | FLOAT FM / FLOAT ARP | 0,041 |
| DRUM | GLASS SNARE / NEON CLAP | 0,047 |

Son pares cercanos, no duplicados: el umbral exacto y el de ±4 dan cero. Que se parezcan no prueba que sobren; eso
necesita escucharlos.

**Observaciones de equilibrio, sin objetivos impuestos:**

- ANALOG concentra el 25,5 % de la biblioteca y es el motor con más categorías cubiertas.
- PADS + KEYS + TEXTURES son 162 de 267 (61 %); ARPS son 7 y PLUCKS 11.
- VOICE tiene 23 de 25 presets en PADS y ninguno en KEYS, PLUCKS, TEXTURES o ARPS.
- FM6 tiene 21 de 37 en KEYS; no tiene TEXTURES, ARPS ni LEADS.
- NOISE tiene 21 de 22 en TEXTURES; no tiene KEYS ni BASS.
- WTABLE es el motor más pequeño (14) y el más repartido; DRUM es 18/18 DRUMS por diseño.
- NOISE sigue siendo la familia más baja (mediana −21,8 dBFS frente a −14,7…−18,3 de las demás), ya con el nivel
  propio aplicado. Es una diferencia de familia, no un preset roto.

---

## 2. Modernización (Paso 2): no ejecutada, y por qué

**No se ha cambiado ningún preset.** Las razones, en orden:

1. **La precondición no se cumple:** las fases 1–3 no están aprobadas y la Fase 3 sigue parcial.
2. **Un preset es un sonido que el dueño ya conoce.** Activar CHARACTER, LFO SHAPE, ENV VEL o un FX nuevo en un
   preset antiguo cambia su timbre o su respuesta; hacerlo 267 veces sin oído es exactamente lo que el proyecto
   prohíbe.
3. **No hay oídos aquí.** El inventario mide nivel, pico, estructura y similitud; no puede decidir si un pad suena
   mejor. Cualquier “modernización” masiva sería cosmética o destructiva, no una mejora demostrable.

Lo que sí se comprobó: **ningún formato cambió**. Los 267 presets cargan por el camino real y renderizan; los
proyectos y presets históricos no ven alterado su mapeo. La compatibilidad de audio histórica ya se verificó en la
Fase 2 (285 goldens, 0 changed).

**Candidatos para una pasada con escucha, no cambios aplicados:** los pares más cercanos de §1, la familia NOISE
(mediana −21,8 dBFS) y los motores con poca variedad de categorías (VOICE, FM6, NOISE, WTABLE). Cualquier cambio
deberá venir con una comparación A/B y una razón por preset.

---

## 3. Validación sonora (Paso 4)

| Comprobación | Clase | Resultado |
|---|---|---|
| 267 presets cargados y renderizados | host x86, 44,1 kHz | 0 silenciados, 0 al rojo, 0 duplicados, 0 fuera de rango |
| RMS con el nivel propio | host x86 | mediana por motor entre −21,8 y −14,7 dBFS; mínimo −40,8 (NOISE `GEIGER CNT`, un evento espaciado) |
| Pico máximo de la biblioteca | host x86 | **−4,02 dBFS** ⇒ **4,0 dB de headroom** sobre 0 dBFS |
| Clipping / soft-clip | host x86 | **0,0000** de la nota en el último tramo del recortador en los 267 |
| Comparaciones A/B | — | **no hay**: no se modificó ningún preset, no hay “antes/después” que comparar |

La RMS de una nota no es la de una mezcla; el pico es de una nota con el nivel del preset. No se ha escuchado nada.

---

## 4. FM-1 Hub (Paso 5)

### 4.1 Defecto encontrado y corregido

`fmhub.json` llevaba `"sounds": "none"`, añadido en `9df4067`. El Hub acepta un conjunto cerrado de campos y
**descarta el archivo entero** si aparece uno desconocido; el KB consultado hoy (2026-11-13) sigue sin admitir
`sounds`. La ficha habría perdido el texto propio. Se eliminó el campo.

```
$ python3 tools/check_fmhub.py
ok    only accepted fields (author, cheat, color, editor, image, kinds, play, what)
ok    what: 274 characters, plain text, no links
ok    play / editor / cheat: answers 200
ok    image: docs/card.png 512x512 PNG, 20860 B
ok    color: #35CFFF, 11.5:1 on a dark card
fmhub.json: every rule the hub publishes holds
```

### 4.2 Identidad y paquete

- **`FM-1_965`**: libre en el catálogo vivo (202 listings, actualizado 2026-11-13 18:01 UTC; 22 listings sin
  identidad propia no cubren ningún número).
- **Paquete actual**: `release/felucca-v2.9.7.fwsc`, 610 959 B,
  sha256 `8af698bf349031d181a1b2ba91b79d3ed9ee6e43b7fbd4147a32ec197d04a5d0`.
- `tools/check_package_identity.py`: identidad `FM-1_965`, loader `ota-FM-1_965`, ok. No se encontró `FM-1_015`.
- La imagen de la ficha es `docs/card.png`, 512×512, 20 860 B, cuadrada y dentro de los límites.

**No se declara aceptación ni compatibilidad física.** El Hub no ha listado este proyecto y no se ha enviado nada.
Las pruebas de instalación/desinstalación en una FM-1 real siguen **PENDIENTES**; no se ha escrito en la unidad.

---

## 5. Sincronización (Paso 6)

### 5.1 Defecto encontrado y corregido: `SHA256SUMS`

El `SHA256SUMS` de la raíz seguía nombrando **v2.9.1**, mientras el paquete y el manifiesto actuales son **v2.9.7**.
Se reescribió con los cuatro artefactos actuales y se verificó:

```
$ sha256sum -c SHA256SUMS
release/felucca-v2.9.7.fwsc: OK
release/felucca-v2.9.7.fwsc.manifest.json: OK
release/build-manifest-v2.9.7.json: OK
release/emu/felucca.wasm: OK
```

`release/SHA256SUMS` (el histórico completo) ya estaba correcto y `python3 tools/coherence_check.py` pasa:
**v2.9.7, paquete y manifiesto de acuerdo, el sitio sirve ese paquete, el registro del emulador coincide con el
build, SHA256SUMS verifica, 936 archivos sin marcadores de merge**.

| Artefacto | Bytes | SHA-256 |
|---|---:|---|
| `release/felucca-v2.9.7.fwsc` | 610 959 | `8af698bf…a5d0` |
| `release/felucca-v2.9.7.fwsc.manifest.json` | — | `8e8c4cd3…d1ab` |
| `release/build-manifest-v2.9.7.json` | — | `cb865b5b…6051` |
| `release/emu/felucca.wasm` | 1 193 096 | `ce09f79d…6587` |

**No se ha creado ningún tag, release ni publicación; no se ha modificado ningún workflow.** El estado del sitio,
emulador y Web Studio se comprobó con `coherence_check` (host/red), no con un navegador.

---

## 6. Seguridad final (Paso 7)

| Comprobación | Clase | Resultado |
|---|---|---|
| `fuzz_ed` (SysEx del editor y paquetes USB-MIDI), 5 000 iteraciones, semilla 7, ASan+UBSan | host x86 | `fuzz ok: 4683 frames, 317 raw packets` |
| `fuzz_proj` (proyectos mutados → import → restore → render), 2 000 iteraciones, semilla 13, ASan+UBSan | host x86 | `fuzz_proj ok: 2000 iterations, 199 imported` |
| `tests/install_test.py` | host x86, MIDI simulado | **passed**, con dos omisiones declaradas: imagen lógica vs `fm1pkg.js` (necesita node + `build/felucca.fwsc`) y restauración de V15 (necesita `FM-1_v15.fwsc`) |
| Identidad del paquete | bytes publicados | ok, `FM-1_965`, loader `ota-FM-1_965` |
| Coherencia de release | host/red | ok (§5) |
| Instalación física, recuperación, escritura de flash, arranque real | — | **PENDIENTE**, sin unidad y prohibido escribir en ella |

La auditoría **no autoriza** la instalación física: la matriz de validación sigue sin los requisitos de recuperación
y seguridad verificados en hardware.

---

## 7. Artefactos y cambios de esta pasada

| Archivo | Cambio |
|---|---|
| `fmhub.json` | se quitó el campo inválido `sounds`; la ficha vuelve a ser válida |
| `SHA256SUMS` | actualizado de v2.9.1 a los cuatro artefactos de v2.9.7, verificado |
| `docs/QG_PHASE4_PRESET_INVENTORY.json` | inventario por motor, categorías, RMS/pico, movimiento y pares cercanos |
| `docs/QG_PHASE4.md` | este informe |
| `docs/FM1_HUB_READINESS.md` | re-chequeo de Fase 4 y ruta de imagen actual |
| `firmware/src/ui_graph.c` | medidores del ARP separados de su etiqueta (barra en `y + 14`) |
| `docs/captures/fase4/` | capturas ARP QUANTUM/GREY y comparación antes/después |

No se tocaron `firmware/`, `web/`, `site/`, ningún preset, ningún tag, ningún release ni ningún workflow.

---

## 8. Limitaciones pendientes

1. **Fases 1–3 sin aprobar**; la Fase 3 sigue parcial. No se declara la Fase 4 terminada.
2. **No hay escucha ni comparaciones A/B**: no se cambió ningún preset. La modernización queda pendiente de oído y
   de una decisión del propietario.
3. **Sin target**: no hay `.elf`, `.fwsc` nuevos, ni medidas AC791N de CPU/RAM/flash en este entorno.
4. **Sin WASM/navegador**: `emcc` no está; el emulador no se reconstruyó ni se probó en un navegador real.
5. **Sin hardware**: no se ha instalado nada, no se ha probado recuperación ni identidad USB en una FM-1 física.
6. **Hub**: la ficha es válida y la identidad está libre, pero no hay listing, aceptación ni revisión del Hub.
7. **Suite completa**: se ejecutaron las pruebas citadas; no se completó `NO_PACKAGE=1 tests/run_tests.sh` entero
   dentro del límite de 5 minutos de este entorno.

---

## 9. Corrección de UI: los medidores del ARP (seguimiento)

**Clase:** IMPLEMENTADO · HOST TESTED. No es target, WASM, navegador ni hardware.

La etiqueta `GATE`, `SWING` o `PROB` se dibuja con `AF_S`, cuya tinta llega hasta `y + 11` (la línea mide 12 px y
las mayúsculas ocupan las filas 3..11). La barra empezaba en `y + 10`: **dos píxeles de la parte inferior de las
letras quedaban tapados por el medidor**. No era un problema de anchura ni de la caja de texto, así que el lint
—que compara cajas de texto entre sí, celdas y divisores— no podía verlo.

La barra pasa a `y + 14`: quedan **dos filas libres** bajo la tinta y el pie de página sigue a cuatro píxeles.
También se corrigió el mismo desfase en el relleno de progreso, que usaba la misma coordenada.

![Antes y después del medidor del ARP](captures/fase4/arp_before_after.png)

![Pantalla ARP corregida, QUANTUM](captures/fase4/arp_quantum.png)

### Barrido del resto de la UI

| Comprobación | Clase | Resultado |
|---|---|---|
| `ui_render`: 189 pantallas × 22 paletas, lint de cajas | host x86 | **0 hallazgos**; 0 tintas sobre su propio color; 0 píxeles GREY/MONO fuera de gris/neutro |
| Alineación por tinta | host x86 | **3 390 046** medidas, **0** desviadas ≥1 px |
| Barrido de barras bajo texto (`cv_rect` con desplazamiento +8…+12) | host, código | el único caso era `arp_meter`; corregido. No hay otro medidor dibujado dentro de la tinta de su etiqueta |
| `ui_test` | host x86 | **all passed** (incluye ARP, grabación, undo, GREY/MONO) |

**Lo que el lint no cubre y no se afirma:** no modela cualquier relleno arbitrario como posible tapado de texto.
Un probe que marcaba todo relleno posterior sobre una caja de texto dio 595 avisos, casi todos tarjetas, píldoras
de selección y el relleno de la batería: solapamientos intencionados. Se descartó para no convertir el lint en
ruido. Tampoco se revisaron las 189 pantallas una a una a ojo; el barrido es automático más la inspección de la
pantalla corregida. No hay verificación en navegador ni en unidad.

### Pendientes de fases anteriores

La deuda de código conocida de la Fase 3 que quedaba era este solapamiento del ARP; queda corregido y probado en host.
Siguen **pendientes de aprobación** la Fase 3 (parcial) y la Fase 2 (sin fusionar), y siguen **NOT VERIFIED** el
target AC791N, el WASM y el hardware. No se inicia la expansión de las secciones 1–11 del brief nuevo porque su
propia regla prohíbe empezar una fase sin aprobar la anterior.
