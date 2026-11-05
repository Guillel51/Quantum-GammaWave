# FM-1: comandos de identificación y backup, y firma de paquetes (propuesta)

**Estado:** propuesta técnica, **no implementada**. Nada de esto se aplicó al firmware, al loader ni al
protocolo de arranque. Requiere autorización explícita antes de tocar `firmware/src/ota.c` o
`firmware/loader/`.

Base: el protocolo actual tiene `0x11` (identidad: 34 bytes, modelo en `d[6:33]`) y `0x30` (el
dispositivo pide un rango del paquete). El host nunca inicia una lectura de flash. Ver
`docs/FM1_INSTALL_SAFETY.md` para las alternativas A–D.

---

## 1. Comando `0x12`: identidad física del NOR

**Dirección:** host → dispositivo (petición), dispositivo → host (respuesta). Mismo transporte SysEx
`F0 | pack7(00 59 cmd len24 cuerpo chk) | F7`, sin cambios en el framing.

**Petición** (7 bytes de cuerpo, `cmd = 0x12`, sin parámetros):

```
00 59 12 07 00 00 00
```

**Respuesta** (cuerpo de 20 bytes, `cmd = 0x12`):

| Offset | Bytes | Campo |
|---|---|---|
| 0 | 1 | `proto` = 1 (versión del comando) |
| 1 | 3 | JEDEC crudo, big-endian (el byte de manufacturer primero) |
| 4 | 4 | capacidad en KiB, little-endian |
| 8 | 2 | tamaño de página |
| 10 | 2 | tamaño de sector (borrado) |
| 12 | 2 | tamaño de bloque |
| 14 | 1 | `status`: 0 ok, 1 no soportado, 2 el NOR no respondió, 3 lectura inconsistente |
| 15 | 3 | reservado (0) |
| 18 | 2 | CRC16 del cuerpo (el mismo que usan los demás mensajes) |

**Validación obligatoria en el host:** longitud exacta, `proto == 1`, `status == 0`, JEDEC distinto de
`0x000000/0xFFFFFF/0xFFFFFFFF`, capacidad ≥ 1024 KiB, página/sector/bloque dentro de rangos físicos
razonables (256..4096 / 4096..65536 / ≥ sector), y dos lecturas separadas que coincidan. Cualquier
fallo ⇒ **no se envía `UPGRADE`** (política de `tools/fm1_gate.py`).

**Quién lo implementa:** la **aplicación** (`firmware/src/ota.c` + el HAL de flash), no el loader. La app
es la que corre durante el paso 1, así que basta con ella para la identificación previa a escribir.

**Compatibilidad y transición:**

- Un dispositivo con app antigua no responde `0x12`: el instalador interpreta timeout como
  `chip_not_identified` y se niega. No hay degradación silenciosa.
- La primera instalación de un firmware con `0x12` requiere evidencia externa (alternativa D,
  `--jedec`), porque la app que la recibe todavía es la vieja. A partir de esa versión, el comando
  responde y la evidencia es del propio dispositivo.
- Añadirlo al **loader** es un cambio separado y no es necesario para escribir: el loader sólo necesita
  que el host ya tenga la evidencia. Se recomienda igualmente para que `resume` pueda revalidar tras un
  corte; eso sí toca el loader y necesita autorización.

**Impacto:** ~120 bytes de código en la app, sin RAM nueva; no cambia el arranque, el mapa de flash ni
el formato del paquete. Presupuesto: se mide con `tests/target_budget.py` y `memory_map_test.py`.

**Pruebas necesarias antes de aceptarlo:** respuesta en el dispositivo falso (`tests/install_test.py`),
rechazo por `status != 0`, por longitud corta, por CRC malo, por JEDEC en `0xFFFFFF`, por dos lecturas
distintas; y que ninguna de esas rutas envíe `UPGRADE`.

## 2. Comando `0x13`: lectura de flash para backup

**Petición:** `cmd = 0x13`, cuerpo con `addr` (4 LE) y `len` (2 LE), `len ≤ 512`.
**Respuesta:** `cmd = 0x13`, `addr`, `len`, los bytes, CRC16.

Reglas: sólo lectura; `addr + len ≤ capacidad`; rechaza rangos que crucen el final; nunca escribe,
borra ni cambia el estado persistente. Con eso el host puede hacer el backup que hoy está bloqueado:

1. dos pasadas completas independientes, con `addr` crecientes y pausas distintas;
2. comparación byte a byte de las dos lecturas;
3. SHA-256 de cada pasada y del archivo final;
4. manifiesto con JEDEC (`0x12`), capacidad, geometría, identidad `0x11`, versión de herramienta, fecha,
   tamaños y errores;
5. si algo no coincide: **backup inválido**, no se escribe.

La restauración es un problema distinto: exige escribir fuera del área de aplicación y por tanto
loader/bootloader, que están fuera de alcance. Un backup sin restauración probada **no** es
recuperación; se documenta como tal.

## 3. Firma de paquetes (autenticidad)

Diseño ya esbozado en `docs/PHASE0_SECURITY.md`, resumido con lo que falta decidir:

- **Algoritmo:** Ed25519 (clave pública de 32 B, firma de 64 B). Disponible en WebCrypto (navegadores
  actuales) y en Python vía `cryptography`; sin dependencias pesadas en el dispositivo porque **el
  dispositivo no verifica**.
- **Qué se firma:** el `.fwsc` completo, en un archivo `.sig` separado, con `keyid` = primeros 8 bytes
  hex del SHA-256 de la clave pública, y un `versions.json` que ate versión, tamaño, hash, `keyid` y
  `min` (anti-rollback del lado del instalador).
- **Dónde se verifica:** instalador CLI y web, antes de habilitar `UPGRADE`. La clave pública va en el
  código, no en el sitio, para que comprometer el sitio no baste.
- **Qué no resuelve:** un host comprometido; una clave privada robada (hace falta rotación y
  revocación); y **no es secure boot**, porque el loader stock no verifica firmas ni hay raíz de
  confianza inmutable en el dispositivo.
- **Rotación:** `keyid` permite publicar dos claves durante una transición; el instalador acepta una
  lista explícita y avisa cuando el paquete usa una clave retirada.

Lo que falta para implementarlo: decidir el custodio de la clave privada (fuera del repositorio y de
GitHub), publicar la clave pública en el repositorio, y añadir la verificación + pruebas de vector
conocido en CLI y web. Nada de eso se hizo en esta fase.

## 4. Lo que esta propuesta no autoriza

- No se modificó el loader, el bootloader, el mapa de flash ni el arranque.
- No se añadió ningún comando al firmware.
- No se implementó firma ni anti-rollback.
- La instalación física sigue en **NO-GO** hasta que `0x12` exista y esté probado, o hasta que se
  presente evidencia externa del JEDEC de esa unidad concreta.


---

## Estado al cerrar la fase 3 (2026-11-04)

- **0x12**: implementado en la app y probado en host (`tests/ota_test.c`, `tests/chip_test.py`). La geometría
  de página/sector/bloque viene de la tabla por JEDEC, no se mide; la capacidad sí se corrobora leyendo el
  final de la ventana y exigiendo que la lectura en `capacity` falle.
- **0x13**: implementado, solo lectura, 256 B por petición. Auditoría de rangos: la app lee los 1 MiB crudos
  vía `0x0B` con XIP apagado; no hay ruta a `WREN`, borrado, programa ni registro de estado.
- **Backup**: herramienta y manifiesto implementados; **restauración no existe**. Un backup completo no es
  recuperación.
- **Firma**: Ed25519 implementado y verificado en Python y WebCrypto; **ninguna clave publicada**, así que la
  cadena rechaza todo hasta que el custodio publique una clave.
- **Lo que sigue bloqueado**: lectura real del JEDEC en una unidad, backup real, restauración en banco,
  verificación en navegador con hardware, y cualquier escritura física.
