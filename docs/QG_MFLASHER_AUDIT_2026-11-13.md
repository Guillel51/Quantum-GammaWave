# Auditoría M-Flasher 0.11.0, backup y PR #86–#89

**Fecha:** 13 de noviembre de 2026 · **Repositorio:** Quantum GammaWave (QG-1) · **Rama:** `freebuff/222b35789f0959dc0a20e352`
**Regla de clases:** `HOST TESTED` = x86, sin FM-1. `STATIC` = lectura de código/commits. `HW` = unidad física, **no ejecutado aquí**.
No se escribió, borró ni restauró ninguna flash. El backup original no se abrió ni se movió.

---

## 1. M-Flasher 0.11.0: qué aporta y qué se puede adoptar

Auditado desde el tag `v0.11.0` (merge `a3a4788`, código `50eb931`, 2026-11-10), MIT, de
[jvitkauskas/m-flasher](https://github.com/jvitkauskas/m-flasher). Se leyeron `README.md`, `docs/safety.md`,
`src/ops.rs`, `src/formats.rs`, `src/boards.rs` y los commits de `v0.10.0..v0.11.0`.

| Mejora de 0.11.0 | Qué evita | Estado en QG-1 |
|---|---|---|
| Nombre de backup tomado con `create_new` | que dos backups se pisen | **Implementado** en `tools/fm1_safewrite.py`; `fm1_mflasher.py` y `fm1_flash_read.py` usan `O_EXCL` |
| `write_synced` + `sync_dir` al guardar | un backup “guardado” que solo estaba en caché | **Implementado**: `write_new()` hace `flush` + `fsync` de cada archivo y `sync_dir()` del directorio |
| SHA-256 del `.json` comprobado antes de restaurar | restaurar un archivo cambiado o truncado | QG ya lo hacía en `fm1_restore.validate()`; el importador también lo exige |
| Backup antes del primer borrado de la sesión | quedarse sin copia si la escritura falla | **No implementado en el CLI**: el backup es responsabilidad del propietario; el instalador web sí lo pide para unidades `jiant`. No se automatiza aquí porque no hay unidad |
| Área de arranque de un paquete solo si puede arrancar | dejar la unidad oscura con un bootloader de relleno | QG **nunca** escribe `0x00000..0x03FFF`; el loader solo toca la app. La regla se mantiene y se documenta, no se copia código |
| Instalar firmware ya no borra ajustes | perder VM/BTIF fuera del paquete | QG ya escribe solo la app; el loader no borra ajustes. Sin cambio |
| Verificación de cada sector escrito | un program que “pasa” pero no persiste | QG ya relee cada sector y lo reintenta; `ldr_test` lo cubre |
| No escribir mientras hay un diálogo abierto / plan revalidado | escribir con una elección vieja | Solo GUI de M-Flasher; QG no tiene ese diálogo. No aplica |
| No saltar a un loader ya en ejecución | corromper la RAM del loader | QG no usa `JUMP` a RAM; su ruta es M-UPGRADE. No aplica |

**Lo que 0.11.0 no aporta:** el `.json` sigue sin guardar el CRC32 del chip y no hace una segunda lectura. Por eso
`tools/fm1_mflasher.py` mantiene su regla: **segunda lectura byte a byte o `--crc32` del chip**; un JSON válido por
sí solo no convierte un archivo en un backup restaurable.

---

## 2. El FULL BACKUP del propietario

### 2.1 Lo que hay en el workspace

Solo están los dos JSON adjuntos. **No hay ningún `.bin` de 1 MiB en el workspace**, así que no se pudo comprobar
tamaño, SHA-256 ni estructura. Los valores de la tabla son **declaraciones del JSON**, no verificaciones nuestras.

| Campo | `fm1-20261113-144531.json` | `fm1-20261113-144617.json` |
|---|---|---|
| `saved` | 2026-11-13 14:45:31 | 2026-11-13 14:46:17 |
| `bootloader` | `stock (M-VAVE)` | `m-bootloader 2.3-6bc8f45 (fm1)` |
| `firmware` | `FM-1 015 (stock)` | `FM-1 015 (stock)` |
| `app` | `OK` | `OK` |
| `chip key` | `0x980F` | `0x980F` |
| `flash` | `0x856014` (Puya P25Q80H) | `0x856014` (Puya P25Q80H) |
| `unit id` | `4150353835313708` | `4150353835313708` |
| `loader` | `M-Flasher 1.3, CPU at 96 MHz` | `M-Flasher 1.3, CPU at 96 MHz` |
| `tool` | `m-flasher 0.11.0` | `m-flasher 0.11.0` |
| `sha256` | `38c66ae7…d9cd` | `fb8f15f0…1ebb` |

### 2.2 Conclusión: no son dos pases del mismo estado

Los dos JSON son de la **misma unidad** (mismo `unit id`, mismo chip, misma clave), pero **no son dos lecturas
independientes de la misma flash**: el de 14:45:31 dice bootloader stock y el de 14:46:17 dice `m-bootloader 2.3`.
Los SHA-256 también difieren. Por tanto:

- **`144531` es el candidato a backup dorado de fábrica**: fue tomado con el bootloader stock.
- **`144617` es un estado posterior**, con m-bootloader instalado; sirve para volver a *ese* estado, no para
  demostrar que el anterior se leyó dos veces.
- **No se pueden pasar como `--bin` y `--second`**: `fm1_mflasher.py` los rechazaría por diferentes, y con razón.
- Para verificar `144531` hace falta su `.bin` **y** el CRC32 que M-Flasher imprimió para la flash completa, o una
  segunda lectura **del mismo estado stock** byte a byte. Sin uno de los dos, el archivo queda `unverifiable` y
  `fm1_restore.py` lo rechaza.

**No se ha subido nada al repositorio ni se ha compartido.** El backup contiene ajustes, área de usuario y datos
Bluetooth de la unidad; debe quedar fuera del repo.

### 2.3 Qué contiene y qué permite recuperar

Un volcado completo de 1 MiB cubre, según `docs/FLASH_MAP_AND_RECOVERY_FORENSICS.md`:

| Rango | Contenido | ¿La restauración de QG lo escribe? |
|---|---|---|
| `0x00000..0x03FFF` | cabecera JLFS, UBOOT, `isd_config.ini`, chip key | **No por defecto** (`fm1_restore` empieza en `0x4000`; `--include-head` lo incluye) |
| `0x04000..0x92FFF` | `app.bin`, `cfg_tool.bin` | Sí, con programador externo verificado |
| `0x93000..0xE8FFF` | VM, ajustes, área de usuario (con autosave/FM6 protegidos) | Sí, dentro del volcado |
| `0xE9000..0xE9FFF` | BTIF | Sí |
| `0xEA000..0xFBFFF` | USR (patches, datos de usuario) | Sí |
| `0xFC000..0xFFFFF` | reservado y `key_mac` | Sí si se restaura el volcado completo |

**Limitaciones reales:** `fm1_restore.py` no escribe por MIDI; exige un programador externo (`flashrom` o el
simulador `file:`) y no se ha ejecutado contra hardware. Restaurar sin `--include-head` **no recupera el
bootloader**, solo la app y los datos; para el bootloader hace falta M-Flasher o `--include-head` con un volcado
que lo contenga. Un backup completo no es una garantía de arranque si el fallo es eléctrico o de silicio.

---

## 3. Compatibilidad de los dos chips

La identidad del dispositivo (`FM-1_015` de fábrica, `FM-1_965` de QG) **no depende del chip**: el chip decide
lectura/escritura; la identidad sale del `app.bin` descifrado. No se cambió ningún offset ni geometría.

| JEDEC | Parte | Detección QG | Lectura / backup | Escritura app | Boot / bootloader | Instalación QG | Evidencia física |
|---|---|---|---|---|---|---|---|
| `0x856014` | Puya P25Q80H, 1 MiB | `fm1_flash_id.h`: dos `0x9F`, capacidad, SR idle | sí | sí | **nunca** escrita por QG | permitida por `fm1_gate` | la unidad del propietario es esta; **QG no se instaló aquí** |
| `0xB36014` | UCUN UC25HQ80, 1 MiB | misma allowlist y `ota.c` | sí | sí | **nunca** escrita por QG | permitida por `fm1_gate` | datasheet + instalación de otro proyecto en una FM-1; **QG no verificado en hardware** |
| otro | — | rechazado | rechazado | rechazado | rechazado | `chip_unknown` | — |

Geometría usada por QG: 1 MiB, página 256 B, sector 4 KiB, bloque 64 KiB en ambas. Las pruebas de host
`tests/flash_id_test.c` y `tests/gate_test.py` cubren las dos IDs, una desconocida, dos lecturas que no coinciden,
protección y estado ocupado. **Reconocer el chip no autoriza una escritura**: `fm1_gate` sigue exigiendo la
identidad de la unidad y la evidencia de la sesión.

---

## 4. PR #86–#89: integración real

Los heads de los PR no son ancestros de `main` porque se integraron con **squash**; la integración es real y se
comprueba por el commit que nombra el PR.

| PR | Título | Estado | Commit en `main` | Clasificación |
|---|---|---|---|---|
| #86 | Allow UCUN chip in M-Flasher import | mergeado | `faed67c` | **Correcto.** `WRITE_OK` incluye `0xB36014`, coincide con `fm1_gate`, el firmware y el instalador web; tests pasan. No es una validación física de QG en UCUN |
| #87 | Build and publish releases from tags | mergeado | `c77f67a` | **Correcto, luego endurecido.** El workflow actual usa un reusable workflow fijado por SHA (`zvenson/fm1-build@85a003a…`), no la acción `@main` que proponía el PR |
| #88 | Let loader check chip on stock installs | mergeado | `e218faf` | **Parcialmente verificado.** El autor reporta dos FM-1 de fábrica (una por chip) en el PR; es evidencia de terceros, no de este entorno. Se corrigió un defecto: en chip desconocido el loader escribía registros |
| #89 | Use GitHub releases, drop signing | mergeado | `afa249d` | **Integrado y luego reemplazado.** El modelo actual volvió a SHA-256 y a la documentación de firma; no se reaplica la eliminación de procedencia |

### #86 — UCUN en el importador

El cambio es coherente con `docs/JEDEC_COMPATIBILITY.md`: el respaldo de un UCUN es restaurable y la escritura de
la app está permitida por la puerta; no se afirma que QG haya arrancado en un UCUN. El test cambió de “no
write-authorised” a “write-authorised”; eso **no** es una prueba física, es la misma decisión que ya tomaba
`fm1_gate`.

### #87 — release desde tags

El PR original añadía `softprops/action-gh-release@v2` y `konsumer/mutantcloud@main` con `contents: write` e
`id-token: write`. Ese `@main` era un riesgo de cadena de suministro; el árbol actual ya no lo usa: `release.yml`
llama a un workflow externo **fijado por SHA**. La lección queda: cualquier acción de release debe ir fijada por
SHA y con permisos mínimos. No se modificó el workflow en esta pasada.

### #88 — instalar en una FM-1 de fábrica

El PR eliminó la negativa del host a instalar sin `0x12` y trasladó la comprobación al loader
(`ldr_flash_known`). El autor reporta instalación web y CLI en dos unidades de fábrica V15, una con cada chip, con
ajustes que sobreviven a un apagado. Es un reporte valioso, pero **no es `HARDWARE VERIFIED` por este proyecto**:
no hay unidad, no hay lectura de JEDEC, no hay recovery ejercitada aquí.

Defecto demostrado en el código mergeado: en la rama “chip desconocido”, el loader llamaba a `ldr_records_drop()`,
que **borraba sectores de registros**, mientras el comentario decía “nothing is erased”. Eso podía escribir
en una parte cuya geometría y comportamiento de escritura no se conocen, antes incluso de validar el paquete
completo. Corregido en esta rama: ahora devuelve `-15` **sin borrar, sin programar y sin tocar registros**. El test
`ldr_test` verifica cero erases, cero programas y registros intactos.

### #89 — GitHub Releases sin firma

Se mergeó, pero el árbol posterior volvió a un modelo de integridad SHA-256 y a la documentación de firma
(`docs/SIGNING.md`, `signed-release.yml`). No se debe reaplicar la eliminación de procedencia: el estado actual
distingue integridad (SHA-256) de autenticidad (firma histórica/atestación) y no promete una firma que no existe.

---

## 5. Hallazgos y correcciones de esta pasada

| # | Hallazgo | Corrección | Prueba |
|---|---|---|---|
| 1 | El importador estampaba `declared_commit = 0.9.1` en un backup hecho con 0.11.0 | `declared_commit` solo se rellena si el JSON dice `m-flasher 0.9.1`; para 0.11.0 queda `null` | `tests/mflasher_test.py`: “a 0.11.0 backup records 0.11.0…” |
| 2 | `write_archive()` y `fm1_flash_read` usaban `write_bytes`/`write_text`: podían sustituir un backup y no hacían `fsync` | `tools/fm1_safewrite.py`: `O_EXCL`, `flush`, `fsync`, `sync_dir`; se niega a reemplazar un archivo existente | “an existing backup is refused, not overwritten”; `flash_read_test` 16/16 |
| 3 | En chip desconocido el loader borraba registros de actualización | `ldr_core.c` devuelve `-15` sin ninguna escritura; comentario corregido | `ldr_test`: “unknown flash chip refused, no erase, no program, records untouched” |
| 4 | La documentación del importador decía 0.9.x y no cubría 0.11.0 | `docs/M-FLASHER.md` y el `--help` ahora dicen 0.9.0–0.11.x; se documenta qué aporta 0.11.0 | — |

No se encontró un defecto demostrado en #86, #87 ni #89 que justifique tocar código ahora. #87 ya está endurecido
en el árbol actual; #89 fue reemplazado por el modelo posterior.

### Riesgo residual que no se corrige sin hardware

La ruta de instalación en una unidad stock envía `UPGRADE` al actualizador del fabricante, que puede escribir el
loader del paquete antes de que `ldr_flash_known()` se ejecute. El loader ya no escribe si el chip es desconocido,
pero **no se ha medido** qué deja el actualizador del fabricante en ese caso. No se declara seguro: es la razón por
la que la instalación sigue siendo `NO-GO` en la matriz hasta tener unidad y recuperación ejercitada.

---

## 6. Pruebas ejecutadas

| Prueba | Clase | Resultado |
|---|---|---|
| `python3 tests/mflasher_test.py` | HOST | pass, incluidos 0.11.0 y no-sobrescritura |
| `python3 tests/flash_read_test.py` | HOST | 16 checks, 0 failed |
| `build/host/ldr_test tests/fixtures/felucca-v1.9.0.fwsc release/felucca-v2.9.7.fwsc` | HOST | pass, chip desconocido sin escrituras |
| `python3 tests/install_test.py` | HOST, MIDI simulado | pass; 2 skips declarados (imagen lógica vs `fm1pkg.js`, V15 oficial) |
| `python3 tests/gate_test.py` | HOST | pass |
| `python3 tests/flash_evidence_test.py` | HOST | 20 checks, 0 failed |
| AC791N target build / WASM / navegador / FM-1 física | — | **NO EJECUTADO** |

---

## 7. Qué falta para cerrar el backup

1. **Ubicar el `.bin` de `144531`** (fuera del repositorio) y pasarme la ruta; no se debe mover el original.
2. Verificar `size == 1 048 576` y `sha256 == 38c66ae7…`; después importarlo con `--crc32` (o una segunda lectura
   stock byte a byte) para que quede `complete`.
3. **No usar `144617` como segundo pase de `144531`**: tiene m-bootloader.
4. Ejecutar una restauración sólo en banco, con programador externo, y confirmar arranque; aquí no se hace.
5. No publicar ni compartir el backup: contiene datos de la unidad.

**Veredicto:** el backup está **bien planteado pero no verificado**; la unidad sigue siendo **NO-GO** para una
instalación desde este entorno. Las correcciones de esta pasada son de host y no cambian esa conclusión.
