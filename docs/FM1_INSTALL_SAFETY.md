# Seguridad de instalación en el FM-1 — identidad física, backup y recuperación

**Fecha:** 2026-11-04. **Rama:** `perf/six-track-hardening`, HEAD `8f41b85`.
**Estado:** NO-GO para instalación física. Este documento no es una certificación: es el registro de
qué está demostrado, qué está bloqueado y qué haría falta para autorizar una escritura.

Método: lectura del código real (`tools/fm1_install.py`, `web/fm1ota.js`, `web/fm1pkg.js`,
`firmware/src/ota.c`, `firmware/loader/ldr_core.c`), ejecución de las pruebas de host y del emulador
en esta sesión. No hubo FM-1 conectada, ni escritura, ni cambios de bootloader, SPL, mapa de flash o
mecanismo de arranque.

---

## 1. El protocolo, tal como es

| Comando | Dirección | Qué hace |
|---|---|---|
| `0x11` | dispositivo → host | identidad: 34 bytes, texto del modelo en `d[6:33]` (`FM-1_982`, o `ota-FM-1_982` en el loader) |
| `0x30` | dispositivo → host | el dispositivo pide un rango del **paquete**; el host responde con esos bytes |
| `0xF0000000` / `0xE0000000` | dispositivo → host | confirmación final de escritura / de chequeo |

No existe ninguna operación iniciada por el host que devuelva contenido de la flash del dispositivo.
El flujo es siempre *el dispositivo pide, el host sirve*. Esto determina todo lo que sigue: sin un
comando nuevo, el host no puede leer la flash ni el JEDEC.

El loader (`firmware/loader/ldr_core.c`) escribe **solo** `[0x4000, 0x93000)`, sector por sector,
verificando cada uno, y sólo confirma el `commit` si recibe exactamente `success` en `0xF0000000`.
Identifica un paquete por la **chip key** derivada de `isd_config.ini` en la cabecera de flash de esa
unidad, no por JEDEC: un paquete de otra clave se rechaza antes de borrar.

## 2. Identificación física: alternativas evaluadas

| Alternativa | Qué habría que tocar | ¿Antes de escribir? | ¿Reinicia? | Estado |
|---|---|---|---|---|
| **A. Comando existente que informe JEDEC** | nada | — | — | **No existe.** El `0x11` sólo lleva el modelo; no hay lectura de flash iniciada por el host. |
| **B. Diagnóstico de solo lectura por firmware** | app (`firmware/src/ota.c`) | sí, con la app corriendo | no | **Diseñable**, pero exige un comando nuevo en el protocolo de la app y del loader para el caso “ya estoy en modo actualización”. Unidades que ya estén en el loader y no tengan el comando nuevo no podrían identificar el chip. |
| **C. Ampliar loader/protocolo** | loader + `0x11`/comando nuevo | sí | no necesariamente | **Bloqueado por regla 4.** Requiere análisis de impacto y autorización explícita; no se implementó. |
| **D. Identificación externa (programador/analizador SPI)** | hardware y procedimiento | sí, fuera de USB | sí (desconectar) | **Viable pero no disponible aquí**; exige instrumental y una unidad de banco, no la unidad del usuario. |

Ninguna de las cuatro está implementada y validada en esta sesión. Por eso el preflight
(`tools/fm1_preflight.py`) **se niega a autorizar** una instalación: informa el paquete, la identidad,
el modo del dispositivo y declara el chip como `unknown`.

### La puerta es obligatoria en las dos rutas

`tools/fm1_gate.py` es la única política; `fm1_install.py` y `web/fm1ota.js` la llaman **inmediatamente
antes de enviar `UPGRADE`**, no al abrir la aplicación:

- CLI: `Updater.install()` y `Updater.write()` (esta última también en `--resume`) llaman a
  `GATE.require(...)`; sin evidencia del NOR de **esa** unidad lanzan `chip` (exit 7) y no se envía un
  solo frame. `resume` no tiene camino alternativo: comparte `write`.
- Web: `install()`, `write()` y `resume()` llaman a `requireChip(evidence)` antes de tocar WebMIDI.
  Ningún parámetro de URL puede aportar la evidencia.
- Evidencia aceptada: sólo el `0x12` leído por el instalador **en esa unidad**, dos veces, en la misma
  sesión, y sólo si el ID está en la allowlist de escritura (`0x856014` y `0xB36014`; la evidencia de
  ambos está en [JEDEC_COMPATIBILITY.md](JEDEC_COMPATIBILITY.md)). `0xFFFFFF`, `0`, `0xFFFFFFFF` y
  cualquier valor fuera de la lista se rechazan con `chip_unknown`.
- **No hay override.** `--jedec`, `--allow-unidentified-chip` y `FM1_LAB` se retiraron de `fm1_install.py`,
  `fm1_preflight.py` y `fm1_gate.py`: un ID escrito a mano no es evidencia y ninguna variable de entorno
  cambia la política. La ruta web nunca los tuvo.

Pruebas que lo demuestran, contando los frames del dispositivo simulado (`upgrades`/`served`):
`tests/preflight_test.py` (instalador sin evidencia, JEDEC desconocido, override sin variable,
`Updater.write()` llamado directamente) y `web/test_web.mjs` (instalación y reanudación sin evidencia:
`chip` y **cero peticiones servidas**).

### Política fail-closed

`tools/fm1_preflight.py` implementa la parte que hoy se puede implementar:

- hash ausente ⇒ rechazo (exit 2); hash distinto ⇒ rechazo (exit 2);
- paquete ilegible, demasiado grande, identidad inválida o sin loader ⇒ rechazo (exit 2);
- dispositivo ausente ⇒ rechazo (exit 3);
- modelo distinto ⇒ rechazo (exit 6);
- **chip no identificado ⇒ NO-GO (exit 7)**, que es el caso normal en una unidad stock;
- no hay bandera ni variable que lo convierta en GO: `--allow-unidentified-chip` se retiró junto con
  `--jedec` y `FM1_LAB`, y el preflight usa exactamente la misma lectura y la misma decisión.

El preflight no es un segundo criterio: llama a `GATE.authorise`, igual que el instalador, así que no
puede decir “adelante” donde el instalador diría que no, ni al revés.

El diseño concreto de los comandos que lo desbloquean (formato, validación, compatibilidad y
transición) está en `docs/JEDEC_BACKUP_SIGNING_PROPOSAL.md`. CI ejecuta las pruebas de la puerta en las
dos rutas (`node web/test_web.mjs` y `python3 tests/preflight_test.py`) junto a los checks de target.

## 3. Backup completo: diseño y por qué está bloqueado

Diseño requerido (dos lecturas independientes, comparación byte a byte, SHA-256 de cada una,
manifiesto con JEDEC, capacidad, geometría, mapa, identidad, versión de herramienta y errores) **no se
puede ejecutar hoy**: no hay comando de lectura de flash. Las dos lecturas independientes no pueden
provenir del mismo archivo copiado dos veces.

Lo que sí existe y no es un backup completo:

- exportación de datos de usuario por SysEx (`tests/backup_test.c`, `web/test_backup.mjs`): objetos
  persistentes, no la flash entera;
- `fm1-emu --raw-flash`: lectura de la flash **modelada** dentro del emulador, no de la unidad.

Un backup parcial **no** debe presentarse como recuperación. Sin las dos lecturas del mismo estado
físico no hay forma de detectar un error de lectura que se repita.

Qué lo desbloquearía: la alternativa B o C (comando de lectura), o la D (lectura externa). Cualquiera
de ellas requiere autorización y una unidad de banco.

## 4. Máquina de estados real de una instalación

Los estados que el código realmente distingue, con la evidencia que los respalda:

| Estado | Quién lo produce | Evidencia / prueba |
|---|---|---|
| `IDLE` | app arrancada, sin actualización | identidad `FM-1_nnn` |
| `PREFLIGHT` | `fm1_preflight.py`, `--verify-only` | `tests/preflight_test.py`, `tests/install_test.py` |
| `IDENTIFICATION` | `meta-data` de identidad, modelo | `install_test.py` (modelo, `ota-`) |
| `STAGED` | la app verificó el paquete y escribió el registro | `tests/ota_test.c` |
| `LOADER` | el loader arrancó y espera al host | `install_test.py` (`noloader`) |
| `WRITING` | el loader borra/programa/verifica sector a sector | `tests/ldr_test.c` |
| `COMMITTED` | ACK `success` exacto en `0xF0000000` | `ldr_test` (ACK incorrecto ⇒ `-14`, sin commit) |
| `VERIFIED` | reinicio y la identidad coincide | `install_test.py` (exit 6 si no coincide) |
| `ABORTED` | cualquier error previo a escribir | exit 1/2/3/4/5/6 del instalador |
| `RECOVERY_REQUIRED` | corte durante la escritura | documentado, **no demostrado en hardware** |

`RECOVERY_VERIFIED` **no existe hoy**: no hay prueba de restauración ni de arranque tras escritura
parcial en una unidad física.

## 5. Matriz de fallos: qué está probado

La matriz completa está en `docs/INSTALL_FAILURE_MATRIX.md`. Resumen de cobertura y de lo que cambió
en esta sesión:

- cortes durante **staging** de la app (cada erase y cada página) y durante el cleanup: `tests/ota_cut_test.c`;
- cortes durante la **escritura de la aplicación por el loader**, ahora en cada página que el loader
  realmente programa: `tests/ldr_test.c`. Con los dos paquetes de esta rama (`fa94ec5` → `8f41b85`)
  el instalador programa 12 288 B en 3 sectores distintos; la matriz cortó en **32 estados reales y
  los 32 reanudaron y terminaron exactos**, con cabecera, datos de usuario y sin escrituras fuera de
  rango. Los presupuestos antiguos (0x10000, 0x40000, 0x80000) no cortaban nada y ahora se informan
  como omitidos en vez de contarse como cobertura;
- política del preflight: `tests/preflight_test.py`, dentro de `tests/groups.sh security`, que pasa
  entero (197 comprobaciones, RC 0) junto con ASan, UBSan, fuzz y los cortes de staging;
- ACK final ausente o incorrecto: sin commit, registro conservado, reintento que completa;
- paquete dañado o de otra chip key: rechazado **antes** de borrar;
- datos de usuario y cabecera: intactos tras la instalación (comprobado byte a byte en el modelo).

Lo que sigue sin probar: corte eléctrico real, brownout, bits atascados, desgaste, y recuperación
física. El modelo de NOR del host aplica `AND` (1→0) como una NOR real, pero no modela tiempos,
tensión ni protección de sectores.

## 6. Niveles de recuperación

| Nivel | Situación | Qué hacer | Estado |
|---|---|---|---|
| 0 | Fallo antes de escribir | Cancelar; nada cambió | Probado en host |
| 1 | Fallo en preflight/backup | No escribir; conservar logs; repetir sólo con la causa entendida | Probado en host |
| 2 | Corte durante la escritura de la aplicación | El registro sigue válido: reconectar y **reanudar**; el loader verifica cada sector y no reescribe los iguales | Simulado en host (`ldr_test`) |
| 3 | Aplicación corrupta o que no arranca | El loader sigue siendo alcanzable por USB si el registro es válido; si no, sólo recuperación física | **No demostrado** |
| 4 | El loader no responde | Requiere USB_KEY / Mask-ROM o programador SPI; fuera de este proyecto | **No demostrado** |
| 5 | Bootloader o flash crítica dañada | Sin recuperación realista desde software | **No demostrado** |
| 6 | Backup inválido o de otra unidad | No se puede restaurar: no hay backup completo posible hoy | **Bloqueado** |

No se recomienda ningún procedimiento de recuperación que no esté en los niveles 0–2.

## 7. Riesgos residuales

| Riesgo | Probabilidad | Impacto | Mitigación actual | Evidencia |
|---|---|---|---|---|
| NOR distinta de las documentadas | Media | Alto (brick) | Ninguna antes de escribir: el instalador no lee el JEDEC | Este documento §2 |
| Corte de energía durante la escritura de la aplicación | Baja-media | Alto | Registro + reanudación + verificación por sector | `ldr_test` (host) |
| Sin firma: el SHA-256 no prueba autoría | Media | Alto | Hash obligatorio; firma sólo propuesta en `docs/PHASE0_SECURITY.md` | `install_test.py` |
| Sin anti-rollback en el arranque | Media | Medio | El instalador no ofrece versiones antiguas y ya no existe `--force`; el arranque sigue sin contador monótono | `PHASE0_SECURITY.md` |
| Sin backup completo posible | Alta | Alto | — | §3 |
| Timing real en AC791N sin medir | Alta | Medio | Presupuestos estáticos y build target | `docs/PERF_SIX_TRACK_FM1.md` |

## 8. Condición para autorizar una instalación

Se exige, como mínimo: chip identificado y en la allowlist con evidencia de esa unidad; backup completo
de dos lecturas con comparación y hashes; restauración probada en banco; corte durante la escritura de
la aplicación probado en hardware representativo; paquete con integridad y, si se implementa, firma
verificada; presupuestos y build target del commit exacto.

Hoy faltan el chip, el backup y la restauración. **La decisión es NO-GO.**


---

## Fase 3 — identificación, lectura, backup y firma (2026-11-04)

Estado: **IMPLEMENTED, HOST TESTED, TARGET COMPILED.** Nada de esto se probó en un FM-1.

| Capacidad | Estado | Dónde |
|---|---|---|
| `0x12` identificación (JEDEC del driver, geometría, estado, sesión, CRC) | implementada, host y target | `firmware/src/ota.c`, `docs/IDENTIFICATION.md` |
| `0x13` lectura solo-lectura (≤256 B, rango, sesión, CRC) | implementada, host y target | `firmware/src/ota.c`, `docs/BACKUP.md` |
| Backup de dos pasadas + manifiesto | implementado, host | `tools/fm1_backup.py` |
| Firma Ed25519 obligatoria antes de escribir | implementada, host y web | `tools/fm1_sign.py`, `web/fm1ota.js` |
| Restauración / recuperación física | **BLOCKED** | no existe, no se tocó loader/SPL |

Reglas nuevas, sin excepciones en la ruta de producción:

1. Escribir exige `0x12` de **esta** unidad, leído por el instalador, dos veces, coincidente.
2. Un JEDEC escrito a mano no es evidencia: `--jedec` y `--allow-unidentified-chip` ya no existen, y
   `FM1_LAB` no cambia nada. El preflight usa la misma lectura y la misma decisión (`GATE.authorise`).
3. Escribir exige un manifiesto firmado que verifique contra las claves públicas fijadas; sin clave fijada,
   todo se rechaza, y no hay variable que lo salte.
4. Un FM-1 en modo update (instalación cortada) **no puede identificarse**: se rechaza con exit 7. Se sale de
   ahí restaurando el backup con M-Flasher (docs/M-FLASHER.md), que escribe la parte entera que acaba de leer, y
   volviendo a instalar desde una unidad arrancada.
5. El loader no cambia: `FELUCCA_LOADER=1` excluye el código de consulta, y el CRC/tamaño del loader siguen
   siendo los mismos.

## Verify Only: dos etapas, y ninguna sustituye a la otra

"Verify Only: 101 comprobaciones, cero escrituras" acredita **sólo** la etapa previa. Son dos:

1. **Pre-install Verify Only** (implementada; `web/test_verify_only.mjs`, 101 checks, cero frames de
   escritura): identidad del modelo, `0x12` con JEDEC y geometría, evidencia de backup, firma del
   paquete, compatibilidad del paquete con la unidad y presupuesto de tamaño. Todo lectura; el
   resultado es "listo" o el motivo exacto del NO-GO, nunca una autorización por sí mismo.
2. **Post-install Verify** (definida, **no validada físicamente**): versión e identidad instaladas,
   integridad del firmware, integridad de los objetos persistentes, reinicio completo, guardado y
   recuperación de un proyecto, ajustes persistentes tras ciclo de alimentación, audio, MIDI, seis
   pistas y FX, y comprobación de que no se modificó nada fuera de los rangos autorizados. Esta etapa
   no puede marcarse GO hasta que exista una instalación real y sus resultados.

La revalidación posterior está por tanto **GO CONDITIONAL**: el procedimiento y sus comprobaciones
existen y se ejecutan contra el emulador y los simuladores, pero la evidencia física es la que falta.
