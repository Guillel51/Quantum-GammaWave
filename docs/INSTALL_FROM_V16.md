# Instalar Quantum GammaWave desde el V16 oficial (FM-1_016)

Clase de toda la evidencia de esta página: **host x86-64** (node, los módulos reales del instalador) y
**build AC791N** para el tamaño. Nada de esto es una medición de la unidad física y **no se ha escrito
ningún byte en un FM-1**.

## 1. Por qué antes fallaba (causa raíz, con el código)

Dos defectos independientes, los dos en la página, ninguno en el transporte ni en el paquete:

| Síntoma | Causa | Dónde |
|---|---|---|
| `the device is FM-1_016, the package is for FM-1_965` | la página pasaba `"15"` como firmware para **cualquier** unidad stock | `web/index_pkg.html`, `ctxFor(fw.kind === "stock" ? "15" : …)` |
| `Chip: … did not answer its JEDEC Truncated identification reply` | el firmware oficial no tiene `0x12`; el eco del frame, un cuerpo corto y el silencio se informaban igual | `web/fm1ota.js`, `qryIdentify()` |

### v1.5.21: el mensaje que el informe citó, y de quién es la copia

Dos cosas más seguían mal en la página aunque el diagnóstico de `qryIdentify()` ya distinguiera los casos:

1. **La etiqueta era una sola para todos los desenlaces.** El botón CHIP escribía
   `sayK(r.ok ? "chipOk" : "chipNo", …)` y pegaba el motivo sin separador, así que el texto exacto del informe —
   `Chip: this unit did not answer its JEDEC Truncated identification reply: nothing was sent.` — se podía
   componer tal cual, y encima **afirmaba silencio en una unidad que sí había respondido** (un eco, un cuerpo de
   3 bytes). Ahora hay tres frases: no respondió (`chipNo`), respondió pero no con una identificación
   (`chipOther`, con el motivo), e identificado (`chipOk`). Los textos terminan en `: ` para que nunca se peguen
   dos frases. `web/test_v16_path.mjs` comprueba que la composición vieja ya no existe y que las dos etiquetas
   están en los tres idiomas con su separador.
2. **El registro de la copia no decía de qué unidad era.** Bastaba con un tamaño y un hash: la copia de una
   unidad satisfacía el gate en **otra**. Ahora el registro guarda la identidad que estaba verificada al cargar la
   copia; el gate rechaza una copia **sin unidad** (`v16Unbound`: pide VERIFY con la unidad conectada y volver a
   cargar) y una copia **de otra unidad** (`v16WrongUnit`). Sigue siendo un registro, no una prueba
   criptográfica — un firmware oficial no tiene comando de lectura, así que la página no puede demostrar que esos
   bytes vinieron de ese chip — y así está escrito.

`donorIdentity()` (en `web/fm1ota.js`) acepta V15 y V16 como origen, y lo exige: la identidad y la cadena de
firmware tienen que nombrar la misma release. La página le daba a una unidad de 16 un firmware de 15, así que
la regla la rechazaba — con razón. El error no era «falta V16 en una lista blanca»: era que se le mentía al
transporte sobre la unidad.

## 2. Qué hace ahora

1. **La versión la dice la unidad.** `stockVersionOf("FM-1_016")` → `"16"`. La numeración del fabricante es
   `FM-1_0NN`; una identidad `FM-1_9NN` (este proyecto) o una cadena ilegible no se interpretan: el transporte
   las rechaza.
2. **El chip se diagnostica, no se adivina.** `probeChip()` distingue, con mensajes distintos: eco del propio
   frame, cuerpo que no mide 24 bytes, silencio, CRC malo, negativa del NOR, JEDEC desconocido y geometría que no
   corresponde. En una unidad stock la página **no** manda `0x12`: informa que el firmware oficial no lo tiene y
   que el loader del paquete identifica el chip **en la unidad** antes de borrar (`firmware/loader/loader.c`,
   `ldr_flash_known()`).
3. **El camino V16 es explícito.** La página pide:
   - una **copia completa de 1 MiB de esa unidad**, tomada con **M-Flasher** y cargada en la sección
     «Install from official V16» (se calcula su SHA-256 en el navegador y se guarda el registro);
   - la confirmación de que la copia está guardada y que se sabe restaurar con M-Flasher;
   - el archivo oficial **V16** (`FM-1.fwsc`, fijado por SHA-256) cargado como camino de vuelta.
   Sin las tres, **no se envía nada** y la página dice cuál falta.

## 3. Qué se escribe y qué no

- Se escribe **`0x4000–0x92FFF`** (la ventana del loader de este proyecto). El head `0x0000–0x3FFF`
  (bootloader/SPL del V16) **no se toca**.
- Nada por encima de `0x93000`: en un V16 eso deja intactos el final de `DX7.bin`, `VM` (`0xBA000`),
  `SEQ64` (`0xDA000`) y `BTIF` (`0xEC000`).
- Los dos chips que este proyecto escribe siguen siendo los únicos: `0x856014` (P25Q80H) y `0xB36014`
  (UC25HQ80); el resto se rechaza.

## 4. Estado: GO CONDITIONAL, y qué significa

El informe de verificación dice `GO CONDITIONAL` con la copia en mano:

- `gate.backup` → **pass** (la copia es de esa unidad, 1 MiB, hasheada en la página);
- `gate.recovery` → **conditional**: el procedimiento existe y la copia es lo que necesita, pero **no se ha
  observado una restauración sobre un layout V16**;
- `gate.chip` → **at-install** en stock (lo lee el loader en la unidad).

No se presenta como demostrado lo que no se ha corrido.

## 5. Pasos para el dueño (unidad con V16 en la mano)

1. **Copia completa primero.** Con M-Flasher, leer **1 MiB completos** de la unidad y guardar el archivo. Verificar
   que el archivo mide exactamente **1 048 576 bytes**. **No seguir sin esto.**
2. Abrir el instalador (GitHub Pages, v1.5.9 o posterior), sección **«Install from official V16 (FM-1_016)»**,
   cargar esa copia. La página muestra su SHA-256 y habilita la confirmación.
3. Cargar el **`FM-1.fwsc` oficial V16** en la sección de vuelta al firmware oficial (queda fijado por digest).
4. Elegir el paquete QG (`felucca-v1.5.9.fwsc`), marcar las casillas de preflight y pulsar **Install**.
5. Si algo se corta: la unidad queda en modo actualización. **No la apagues**: volvé a pulsar Install (el
   camino reanuda). Si no vuelve a aparecer, reconectá el cable USB.
6. Para volver atrás: **M-Flasher** con la copia del paso 1, o el archivo oficial V16 con la sección de retorno.

## 6. Qué está verificado y qué no

| Afirmación | Clase |
|---|---|
| La página pasa la versión que la unidad responde | host (node), `web/test_v16_path.mjs` |
| Eco / cuerpo corto / silencio / JEDEC desconocido / geometría se distinguen | host (node), mismo archivo |
| V16 es donante con su propia versión y se rechaza con otra | host (node), `web/test_donor.mjs` |
| El gate pide la copia y la recuperación queda `conditional` | host (node), `web/test_verify_only.mjs` |
| El mensaje del informe no se puede componer: tres desenlaces, tres frases, con separador | host (node), `web/test_v16_path.mjs` |
| Una copia sin unidad, o de otra unidad, no pasa el gate | host (node), mismo archivo |
| El paquete QG cabe en la ventana del loader | host (`tools/fm1pkg_make.py` falla si no) |
| **El updater del V16 acepta el paquete y arranca el loader** | **no medido**: requiere la unidad |
| **El SPL del V16 arranca la app de QG** | **no medido**: requiere la unidad |
| **La restauración de la copia sobre un V16** | **no medido**: requiere la unidad |

## 7. El registro JSON de M-Flasher (Fase 5)

M-Flasher deja **dos** archivos de una lectura: la copia de 1 MiB (`.bin`) y un registro pequeño (`.json`) con
la parte, la *chip key*, el bootloader, la app y el SHA-256 de la copia. Desde v1.5.26 el instalador **lee el
registro** en el mismo campo de la copia:

1. Cargá primero el `.json`: la página muestra parte, *chip key* y el prefijo del digest, y guarda el registro.
2. Cargá después el `.bin` de 1 048 576 bytes: la página calcula su SHA-256 y **lo compara con el del registro**.
   Si no coincide, dice `v16DumpMismatch` y no envía nada.
3. Seguí con los pasos 3–6 de la sección 5.

El registro es metadato: **no autoriza** nada (cualquiera puede escribir ese JSON, y no requiere unidad). El gate
sigue exigiendo la copia completa, la confirmación y el paquete oficial V16, y el loader sigue leyendo la parte
en la unidad antes de borrar. El análisis del archivo entregado está en `docs/FASE5_BACKUP_V16.md`.

| Afirmación | Clase |
|---|---|
| El registro se lee y se conserva entre recargas | host (node), `web/test_v16_path.mjs` |
| La copia tiene que ser la que el registro nombra | host (node), mismo archivo |
| Un archivo que no es copia ni registro se rechaza con su tamaño | host (node), mismo archivo |
