# What this tree has measured, and what it has not

Generated from [VALIDATION_MATRIX.md](VALIDATION_MATRIX.md) by `tools/evidence_index.py`; do not edit by
hand. The matrix is the record -- one row per requirement, with the class of its evidence -- and this
page is the part a person forgets: the rows that are still **not** measured, with the line they are on.

`python3 tools/evidence_index.py --check` runs in the host suite, so a row that goes back to PENDING
cannot stay invisible: the file is regenerated in the same push.

## Counts

| | |
|---|---:|
| rows in the matrix | 526 |
| rows whose state is a verdict (GO / NO-GO / PENDING / FAIL) | 281 |
| rows still asking for evidence | 28 |

Classes of evidence named across the matrix (a row can name more than one):

| class | rows |
|---|---:|
| AC791N | 16 |
| host | 117 |
| emul | 22 |
| WASM | 28 |
| render | 34 |
| browser | 13 |
| physical | 3 |
| HW | 12 |

## Not measured yet

| line | requirement | state | evidence |
|---:|---|---|---|
| [136](VALIDATION_MATRIX.md#L136) | the build's own gate | **FAIL `RAM code headroom 1732 B < 2048 B`** → `build: checks failed`, **no `.fwsc`, no manifest** |  |
| [137](VALIDATION_MATRIX.md#L137) | same build at `7871c14` (before any of this session's UI work) | **identical**: `RAM code 22844 B`, same `FAIL` — the headroom failure is pre-existing on `main`, not a regression from the visual work |  |
| [176](VALIDATION_MATRIX.md#L176) | `NO_PACKAGE=1 ./tests/run_tests.sh` | **4 394 `ok`, 2 FAILED**, both in `web/test_provenance.mjs` §5 |  |
| [221](VALIDATION_MATRIX.md#L221) | `web/test_verify_only.mjs` | **101 checks, 0 FAILED** |  |
| [289](VALIDATION_MATRIX.md#L289) | 3 signature + pinned key | NO-GO | **GO** `qg1-release-1` pinned, package verified, refusal tests green |
| [292](VALIDATION_MATRIX.md#L292) | 13 installer/emulator/site sync | NO-GO | **GO** wasm, firmware and package from one commit; `make_site` refuses a mismatched record |
| [293](VALIDATION_MATRIX.md#L293) | 14 post-install revalidation | NO-GO | **GO** the device must report the QG product after reboot or the installer raises `mismatch` |
| [294](VALIDATION_MATRIX.md#L294) | 1, 2 reproducible build, whole package | NO-GO | **GO** byte-identical rebuilds, 611 163 B `FM-1_982` |
| [295](VALIDATION_MATRIX.md#L295) | 4, 5, 6 unit identity, JEDEC/geometry, write compatibility | NO-GO | **GO CONDITIONAL** implemented and host tested; needs one `0x11`/`0x12` reading on the owner's unit |
| [296](VALIDATION_MATRIX.md#L296) | 7 memory map | NO-GO | **GO CONDITIONAL** table in the report; the `[0xFC000,0xFF000)` window is undocumented |
| [297](VALIDATION_MATRIX.md#L297) | 8 verified prior backup | NO-GO | **NO-GO** no dump of that unit can exist here, and the installer cannot take one before the first write |
| [298](VALIDATION_MATRIX.md#L298) | 9 demonstrated recovery | NO-GO | **GO CONDITIONAL** external ROM-UBOOT restore is documented on FM-1; QG's own raw restore is refused by design |
| [859](VALIDATION_MATRIX.md#L859) | **Demonstrated recovery** | **NO-GO** | M-Flasher restore of this unit's own dump is available and audited from source; it has never been run on this unit, and no boot-area write exists in this project `[STATIC]` one restore executed on a unit that is not t… |
| [862](VALIDATION_MATRIX.md#L862) | RAM/CPU headroom, post-install revalidation | GO CONDITIONAL / NO-GO | budgets measured on host and target build; the post-install check list exists but has never run on a unit `[HOST]` / `[STATIC]` one installed unit |
| [863](VALIDATION_MATRIX.md#L863) | Installer/emulator/site sync | **NO-GO (one check)** | `tools/site_check.py` fails exactly one line: `versions.json`'s emulator record (`3d039f75…`, from the signed release build) is not the wasm the site serves (`c83c8bfb…`). Same commit, same flags, same emcc version st… |
| [931](VALIDATION_MATRIX.md#L931) | demonstrated recovery | NO-GO | no project has demonstrated recovery **from this unit's state**; MvaveFM1Unbricker and TransitFM1 are `[3rd-hw]` on other units and are cited, not adopted |
| [1194](VALIDATION_MATRIX.md#L1194) | `drumx` PUNCH y `live` (segundo a segundo) | **FAIL — not this pass** | Fallan en `main` antes de tocar nada: `firmware/src` aquí difiere de `main` en una línea (la versión). Son consecuencia de que el nivel del sonido entre en todo render. No se tocan aquí. `NO_PACKAGE=1 sh tests/run_tes… |
| [1240](VALIDATION_MATRIX.md#L1240) | whole suite before publishing | **NOT RUN IN FULL** | `tests/run_tests.sh` was not run end to end in this pass; what ran is listed above and in `CHANGELOG.md` — |
| [1277](VALIDATION_MATRIX.md#L1277) | Rediseño de EDIT de LOFI / MORPH / FM6, HOME y visualizador, optimizaciones, MICRO-START y modulación aleatoria | **NOT DONE** | Sin cambios en esta pasada. Las dos funciones nuevas necesitan un parámetro libre y el registro de proyecto no tiene byte de sobra (`project.c` §límites: P_COUNT 123, sin hueco); eso exige una versión de formato nueva… |
| [1307](VALIDATION_MATRIX.md#L1307) | GitHub Pages | **VERIFIED (before the audit's push), re-deploy pending** | The root served `v2.1.2-03d3f80` with package sha256 `1f52bf09…a303` and emulator wasm `0d437cfe…`; `/preview` served `v2.1.2-bbca837`, `manifest: null` (INSTALL refuses it); no service worker anywhere in the site, so… |
| [1359](VALIDATION_MATRIX.md#L1359) | Release firmado 2.3.0 | **NO PUBLICADO**: la firma exige la clave del custodio, que no está en este árbol; el instalador rechaza un paquete sin firmar (fail-closed). | — |
| [1711](VALIDATION_MATRIX.md#L1711) | Identidad de la unidad | **NO-GO** | no hay unidad — falta lectura real |
| [1712](VALIDATION_MATRIX.md#L1712) | JEDEC y geometría reales | **NO-GO** | allowlist `0x856014` / `0xB36014`, unknowns rechazados, todo simulado `[JEDEC]`/`[SIM]` falta lectura por unidad |
| [1713](VALIDATION_MATRIX.md#L1713) | Compatibilidad de escritura | **NO-GO** | loader con `_Static_assert` de ventana, `ldr_test` con cortes de alimentación y reanudación `[SIM]` falta escritura real |
| [1715](VALIDATION_MATRIX.md#L1715) | Backup verificado | **NO-GO** | `tests/backup_test.py`, `web/test_backup.mjs` verdes; ninguna copia de una unidad `[HOST]`/`[SIM]` falta unidad |
| [1716](VALIDATION_MATRIX.md#L1716) | Recuperación demostrada | **NO-GO** | camino de retorno probado en simulación; nunca ejecutado en silicio `[SIM]` falta unidad |
| [1721](VALIDATION_MATRIX.md#L1721) | Revalidación post-instalación | **NO-GO** | no hay instalación — falta unidad |
| [1722](VALIDATION_MATRIX.md#L1722) | **Gate del instalador** | **NO-GO** | todo lo anterior: la página sigue en Verify Only, `install.auth = none` — **no instalar** |

## What this page cannot say

A row is only as current as the last person who edited it, which is why the matrix is checked and this
index is generated -- but neither can turn a host test into a reading from a unit. Every row that needs
a physical FM-1 stays `PENDING` here until someone runs it on one, and nothing in this repository
writes to the owner's unit.
