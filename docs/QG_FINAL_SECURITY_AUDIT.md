# QG — Auditoría final de seguridad y preparación de instalación (fase 6/6)

**Fecha:** 12 de noviembre de 2026 · **HEAD auditado:** `00f3772` (build `8e197b9`/`00f3772`, bytes idénticos)
**Veredicto, antes de los detalles:** el software está en mejor forma que al empezar esta fase y la cadena de
release vuelve a ser coherente, pero **la instalación en una unidad física sigue siendo NO-GO**. Lo que falta es
evidencia física, no código (ver §9).

---

## 0. Clases de evidencia (se usan en todo el documento)

| Etiqueta | Qué significa |
|---|---|
| `[HOST]` | ejecutado en esta máquina x86 (tests nativos, ASan/UBSan, fuzz) |
| `[SIM]` | el loader/instalador ejercitado contra un modelo de flash, no contra silicio |
| `[TARGET]` | compilado y medido con el toolchain pi32v2 real y el AC79 SDK, sobre el `.dis` del binario |
| `[JEDEC]` | identificación por allowlist documentada, **sin** lectura de una unidad |
| `[FÍSICO]` | una unidad real. **No hay ninguna evidencia de esta clase en este documento.** |

---

## 1. Hallazgos y lo que se corrigió

### C-1 (crítico, corregido) — `release/SHA256SUMS` tenía marcadores de conflicto de git

El manifiesto de integridad que viaja con cada release —y contra el que el instalador comprueba el paquete—
contenía, desde el merge de v2.7.5 (`91df789`), un conflicto sin resolver:

```
<<<<<<< HEAD
d52a799d…  release/felucca-2.7.3.fwsc        ← un nombre que no existe en release/
ca721635…  build/emu/felucca.wasm            ← un archivo que no está en release/
=======
… 13 entradas de v2.7.4 …
>>>>>>> 3376c4c0fada38873912c5e115bb43751b6fef62
```

Consecuencias reales: `sha256sum -c` fallaba en dos entradas, un nombre de archivo inexistente rompía la
verificación y `tools/coherence_check.py` **no podía** validar nada. Se regeneró desde los bytes que hay en
`release/` (58 entradas, todas verificadas) y se comprueba con `sha256sum -c` en verde.

### C-2 (alto, corregido) — el guardián de coherencia crasheaba en vez de reportar

`check_sums()` hacía `partition("  ")` y pasaba el resultado directo a `os.path.exists()`/`sha256()`. Con la línea
`=======` el nombre parseado era `""`, el camino apuntaba al directorio `release/` y el proceso moría con
`IsADirectoryError`: el gate diseñado para detectar exactamente este defecto era el que se callaba. Ahora valida
la forma (`64 hex + dos espacios + nombre`) y reporta **línea y contenido**; una entrada dañada es un mensaje, no
un traceback. Verificado: con el árbol actual `coherence: ok` (923 archivos hasta este informe, 0 marcadores), y el crash reproducido
antes del arreglo.

### C-3 (bloqueante de release, resuelto) — v2.7.5 no tenía paquete

`FELUCCA_VERSION` decía `v2.7.5` y `release/` paraba en `v2.7.4.8`: el sitio servía v2.7.4.8 mientras el árbol
decía otra cosa. Se descargó el toolchain JieLi (**el archivo coincidió con el sha256 fijado en
`tools/toolchain.sha256`**, es decir el que alguien revisó) y el AC79 SDK, y se construyó:

| Dato | Valor |
|---|---|
| Paquete | `felucca-2.7.5.fwsc`, **610 959 B**, identidad `FM-1_965` |
| App | `felucca.bin` 551 720 B, sha256 `6c3a77e6…` |
| Loader | `ota.bin` 7 803 B, sha256 `3ca785b9…` (idéntico al de releases anteriores: el loader no cambió) |

### C-4 (medio, corregido) — el registro del emulador era de otra versión

`release/emu/manifest.json` decía v2.7.4.8 (commit `2a126fd`) mientras el registro de build decía v2.7.5. Se
instaló Emscripten 6.0.11 y se reconstruyó: **1 141 186 B**, sha256 `c00edf1c…`, commit `8e197b9`, y el sitio se
regeneró para servir v2.7.5 dejando v2.7.4.8 instalable al lado. `coherence_check.py` pasa de 4 problemas a 0.

### C-5 (medio, mitigado) — el workflow de release usaba un tag mutable de un tercero

`release.yml` invocaba `zvenson/fm1-build/.github/workflows/build.yml@v1` con `contents: write`,
`id-token: write` y `attestations: write`, y ese job **produce y publica el binario oficial**. El tag `v1` es un
ref mutable y **no está firmado** (`"verification": {"verified": false, "reason": "unsigned"}`). Se fijó al commit
que ese tag apuntaba el 2026-10-18: `85a003ab455b1c074b0bd6d7c282c096a464b973`.

**Riesgo residual, que no se puede eliminar sin cambiar de arquitectura:** el código que construye el release
sigue siendo de un tercero. Fijarlo impide el retag, no un compromiso del repositorio ajeno. Ver §10.

### M-1 (bajo, corregido) — `test_pkg_faults.mjs` fallaba con exit 2 en un checkout sin build

Un checkout de solo host no tiene `build/felucca.fwsc`, así que el test abortaba con exit 2 en lugar de decir que
no había nada que probar. Ahora sale 0 con un mensaje explícito ("skipped, not checked"), como `test_release_e2e`.
Con el paquete presente **sí** corre y pasa: corrupción de bytes críticos, truncado y payload corrupto, todo
rechazado.

### M-2 (bajo, corregido) — CHANGELOG sin sección v2.7.5

`tag-and-publish` se niega a publicar una versión que el CHANGELOG no nombre, así que el tag habría quedado sin
release detrás. Se añadió la entrada con lo que cambió y la prueba que lo respalda.

---

## 2. Verificación de los PR #86–#89 en el HEAD actual

| PR | Qué se pidió | Estado real en `00f3772` |
|---|---|---|
| **#86** | JEDEC para Puya `0x856014` y UCUN `0xB36014` | **presente**: `firmware/hal/fm1_flash_id.h:23-24` define ambos; `:59` es la allowlist (`id == P25Q80H \|\| id == B36014`); `ota.c:435-436` tiene la geometría de cada uno (1 MiB, página 256 B, sector 4 K, bloque 64 K) |
| **#87** | releases por tag, compilación, `.fwsc`, hashes, notificación | **presente**: `tag-and-publish.yml` (verifica el paquete contra su manifiesto, tag anotado, `--verify-tag`, attestation), `release.yml`, `signed-release.yml` |
| **#88** | detección desde el loader, doble lectura JEDEC, protección antes de borrar, factory/update | **presente**: `loader.c:ldr_flash_known()` lee `fl_jedec_ram()` **dos veces** y el estado de protección, y usa el mismo `fl_id_verify()` que la app; `ldr_core.c` §2a no escribe nada si el chip no es conocido |
| **#89** | reorganización de releases/instaladores, tamaño, SHA-256 y chip | **presente**: `web/test_pkg_bounds.mjs` (límites del contenedor), `fm1_preflight.py`, `test_donor.mjs` (paquete de otro modelo nunca admite este), `test_pkg_faults.mjs` |

**Identidades distintas, como se pidió:** `FM-1_965` (esta build), `FM-1_909` y `FM-1_982` conviven en el árbol
como identidades de firmware distintas del JEDEC. `FM-1_909` **no** es `0xB36014`: una es una cadena de identidad
de producto, el otro es un ID de silicio; el código no los mezcla (`fm1_flash_id.h` sólo conoce JEDEC).

---

## 3. Loader: lo que garantiza, leído línea por línea

`firmware/loader/loader.c` + `ldr_core.c` (348 líneas entre ambos):

1. **Ventana de escritura, en tiempo de compilación.** `FL_RANGE_OK(off,n)` sólo admite
   `[0x4000,0x93000)`, `[0xE0000,0xE5000)` y `[0xE8000,0xE9000)`, y hay `_Static_assert` que **rompen el build**
   si alguien amplía el mapa: "VM is not writable", "autosave is not writable", "FM6 bank is not writable",
   "USR is not writable". No es una convención: es un fallo de compilación.
2. **Nada se borra antes de identificar**: (a) cabecera UFW y `flash.bin`; (b) el área de app debe descifrar con
   la clave de **este** chip, leída de su propio `isd_config.ini`; (c) `ldr_flash_known()` — dos lecturas JEDEC que
   deben coincidir, parte en la allowlist y estado sin protección; (d) **CRC16 del paquete completo** leído de la
   unidad, más CRC16 por sector. Recién entonces, escritura.
3. **Chip desconocido ⇒ no se escribe nada**, se sueltan los registros de update y arranca la app que ya está
   (`ldr_records_drop(); return -15;`).
4. **Escritura por sector con verificación**: si un sector llega distinto de la pasada verificada se **vuelve a
   leer**, nunca se escribe a ciegas; dos intentos y aborta (`-13`); tras programar, se relee y compara; dos fallos
   de verificación y aborta (`-11`). El registro queda, así que el siguiente encendido reanuda.
5. **Compromiso sólo con ACK exacto**: `"success"` (8 bytes) o el update no se confirma (`-14`).
6. **Tope de tiempo**: 20 minutos por sesión (`ota_deadline`), para que un host que se cuelga no deje un boot loop.
7. **`ldr_records_drop()`**: barre `[0xE0000,0xE9000)`, **salta** la ventana de autosave/FM6 (datos de usuario que
   podrían parecer un registro) y en el sector de backup del VM del stock invalida el registro **programando 80
   bytes a cero sin borrar el sector** (el resto del sector son datos del usuario). Sólo borra sectores cuyo
   contenido valida como registro (`0x5441` + CRC16).
8. **Sin interrupciones** durante todo el loader (`loader.c`), con watchdog alimentado en `ldr_poll()`.

Nada de esto se tocó en esta fase.

---

## 4. Integridad, autenticidad y procedencia

- **Modelo vigente (AGENTS §9, `docs/SIGNING.md`):** SHA-256 + tamaño publicados con el paquete, el registro de
  build (`qg1-build-1`) y GitHub Artifact Attestation. Verificado hoy:
  `fm1_sign.py verify release/felucca-v2.7.4.fwsc --manifest …` →
  `verified (integrity only, unsigned: SHA-256 model)`.
- **Ed25519 (`qg1-release-1`) es histórico y no se debilitó**: sigue la clave pública fijada en
  `tools/fm1_keys.json` (fail-closed: sin entrada, todo paquete se rechaza), `tests/key_custody_test.py` pasa
  (pin/revoke/repositorio intacto) y `tools/fm1_sign.py` sigue rechazando firmar sin clave fijada o `--expect-pub`.
  Ningún paquete nuevo se firmó: el manifiesto de v2.7.5 sale con `keyid: ""`, que es exactamente lo que el
  verificador reporta como "integrity only".
- **Ninguna clave privada en el repositorio**: `grep -rIl "PRIVATE KEY"` sólo devuelve dos archivos de test
  (`tests/key_custody_test.py`, `web/test_verify_only.mjs`), con material de prueba generado en el momento.
- **El paquete no se puede confundir**: identidad `FM-1_965` embebida, `test_donor.mjs` (10 checks) demuestra que
  un paquete de otro modelo nunca habilita este dispositivo.

---

## 5. Resultado por chip

| Chip | JEDEC | Estado |
|---|---|---|
| Puya P25Q80H | `0x856014` | `[JEDEC]` en allowlist, geometría en `ota.c`, lectura/escritura simuladas en `[SIM]` |
| UCUN UC25HQ80 | `0xB36014` | `[JEDEC]` ídem |
| Desconocido | cualquiera | **rechazado**: `unknown JEDEC -> unknown-part`, "unknown flash chip refused, app area kept, records dropped" |

Pruebas: `chip_test.py` (`chip / backup test passed (0 failed)`), `flash evidence: 20 checks, 0 FAILED`,
`flash read: 16 checks, 0 FAILED`, `a blank JEDEC is refused for writing`, `an undocumented JEDEC is refused`.

**Ninguno de los dos JEDEC se ha leído de una unidad física en este trabajo**, y la capacidad compartida no
demuestra geometría ni comportamiento de escritura equivalente. Eso sigue en `[JEDEC]`, no en `[FÍSICO]`.

---

## 6. Pruebas ejecutadas en esta fase

| Grupo | Etiqueta | Resultado |
|---|---|---|
| Suite completa (`tests/run_tests.sh`, con paquete presente) | `[HOST]` | **ALL HOST TESTS PASSED**, 263 `ok`, 0 `FAIL` |
| ASan/UBSan: loader, OTA, editor, project, backup, robust, fm6bank | `[HOST]` | verde |
| Fuzz: SysEx/MIDI 20 000 frames seed 7; project 5 000 seed 13 | `[HOST]` | `fuzz ok`, `fuzz_proj ok` |
| 220 renders dorados | `[HOST]` | 0 changed, 0 gone |
| `install_test.py` | `[HOST]` | pasa, incluida la comparación de imagen lógica contra `fm1pkg.js` |
| `web/test_pkg_faults.mjs` (paquete real) | `[HOST]` | pasa: bytes críticos, truncado, payload corrupto |
| `web/test_accept.mjs` / `test_state` / `test_update_mode` / `test_verify_only` / `test_donor` / `test_sign` / `test_provenance` | `[HOST]` | 23 / 65 / 16 / 153 / 10 / 6 checks, 0 FAILED |
| `coherence_check.py` | `[HOST]` | **ok** (sin marcadores, hashes y sitio concordantes) |
| `site_check.py`, `web/test_site.py` | `[HOST]` | ok; 19 ids, 7 imágenes, 31 enlaces, 0 FAILED |
| `key_custody_test.py` | `[HOST]` | pasado |
| Loader con cortes de alimentación derivados y reanudación | `[SIM]` | verde (5 cortes) |
| Modo solo lectura: no se envía ni un frame de escritura | `[SIM]` | 153 checks de `test_verify_only`, incluidos "write() called directly, with no proof, is refused and sends nothing" y "the chip check sent the identity query and one 0x12, and nothing that writes" |
| `target_budget.py` sobre el `.dis` | `[TARGET]` | **39 entradas, todas dentro de presupuesto** |

---

## 7. Recursos, medidos en el build de esta fase `[TARGET]`

| Región | Valor | Límite |
|---|---|---|
| Imagen | 551 720 B | 581 632 B |
| `.data + .bss` | **87 696 B** | 98 304 B (10 608 B libres) |
| Pool de audio | 305 096 B, spare **15 392 B** | piso exigido 8 192 B |
| Código en RAM | 19 304 B | 24 576 B |
| Código XIP | 389 568 B + 142 100 B de constantes | — |
| `.ram_text` | 1 193 instrucciones, 0 llamadas | — |

Coste por camino (`tests/target_budget.py`, instrucciones con las que se compara cada presupuesto):
`analog_render` 148 (148), `fm6_render` 322 (322), `lofi_render` 197 (197), `formant_render` 229 (229),
`noise_render` 201 (201), `phase_render` 84 (84), `wheel_render` 257 (258), `drum_render` 10 (10),
`fx_buses`/`perf_block` 254 (253), `rev_room` 243 (243), `rev_spring` 126 (126), DRUM completo 1 153 (1 153).
Ninguna división en los bucles del camino de audio salvo `dx_run`/`dx_run_sn` (1, prevista).

**Reproducibilidad:** dos builds del mismo código con el mismo toolchain, desde commits distintos
(`8e197b9` y `00f3772`, sólo documentación de diferencia), dan **bytes idénticos**: `felucca.bin`
`6c3a77e6…` y `felucca-2.7.5.fwsc` `772f0dae…` en ambas.

---

## 8. Lo que el instalador hace hoy

- Página **Verify Only**: identifica el chip por `0x12`, compara el paquete con el modelo, el tamaño y el hash,
  y **no manda ni un frame de escritura** (probado estructuralmente, no por confianza: se revisa el vocabulario
  del handler y los frames enviados).
- Rutas de escritura: se niegan **internamente** si se las llama sin prueba (`write()` directo sin proof →
  rechazo y cero frames), no sólo en la UI.
- `install.auth` sigue en `none`: la página dice explícitamente que verificar no autoriza a instalar.

---

## 9. Veredicto de instalación: **NO-GO**, y exactamente qué falta

Lo que **sí** está probado: compilación reproducible para AC791N, paquete completo y validado, integridad
SHA-256 + registro de build, rutas de escritura protegidas por `_Static_assert`, rechazo de chips desconocidos,
modo lectura sin escrituras, cortes de alimentación simulados con reanudación, presupuestos de RAM/CPU, y
sincronía firmware ↔ emulador ↔ sitio (`coherence: ok`).

Lo que **falta** y por qué bloquea la recomendación:

1. **Lectura real de JEDEC en una unidad** (y de su geometría): hoy sólo hay allowlist documentada.
2. **Backup verificado de la unidad concreta** (no de una copia ajena) antes de escribir.
3. **Recuperación demostrada en esa unidad** — el camino de retorno está simulado, no ejecutado.
4. **Una escritura real de ida y vuelta** con lectura de verificación posterior.
5. **Estabilidad y latencia en silicio** (CPU real, jitter, audio sin pops): todo lo medido es `[HOST]`/`[TARGET]`.
6. **Attestation del release publicado**: el workflow la crea, pero su corrida no es visible desde aquí.

Mientras falte cualquiera de los puntos 1–4, la recomendación es: **verificar sí, instalar no**. Las fases
anteriores prepararon el terreno; esta fase no puede convertir una simulación en una unidad.

---

## 10. Riesgos residuales que quedan abiertos

| Riesgo | Severidad | Por qué sigue abierto |
|---|---|---|
| El binario oficial lo produce un workflow de un repositorio de terceros (`zvenson/fm1-build`) | **alto** | pinneado a un commit, pero el código sigue siendo ajeno; sólo se cierra construyendo en CI propia o auditando ese workflow |
| Sin Ed25519, la autenticidad descansa en el canal (GitHub + attestation) | medio | decisión de diseño documentada en `docs/SIGNING.md`; el instalador lo dice al usuario |
| Geometría de flash igual para ambos JEDEC | medio | la allowlist no demuestra equivalencia; requiere lectura por unidad |
| `release/` acumula 58 artefactos y varios manifests con nombres históricos inconsistentes (`2.7.3` vs `v2.7.3`) | bajo | limpieza pendiente; hoy `SHA256SUMS` los cubre a todos |
| `tests/memory_map_test.py` se saltea sin `.elf` | bajo | el build no deja ELF; los presupuestos del linker sí se verifican |

---

## 11. Limitaciones de este informe

- **No hubo ninguna prueba física.** No se instaló, ni se leyó, ni se escribió en un FM-1, y no se declara
  compatibilidad física en ningún punto.
- Las cifras de CPU son del `.dis` con el modelo de coste de `tests/target_budget.py`, no mediciones de tiempo en
  silicio; las de host (`ns/muestra`) son de esta máquina x86 y sólo sirven para comparar versiones entre sí.
- La attestation del release no se pudo verificar por API desde aquí: se afirma que el workflow la produce, no que
  su corrida esté verde.
- El toolchain se verificó contra el sha256 fijado en el repositorio; eso prueba que es el archivo revisado, no que
  JieLi no haya publicado algo defectuoso.
