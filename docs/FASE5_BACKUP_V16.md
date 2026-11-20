# Fase 5 — el backup de una unidad V16, tal como M-Flasher lo entrega

Lectura del archivo que el dueño generó con M-Flasher sobre su FM-1 con firmware oficial V16, y de lo que el
instalador de este proyecto hace con él. El archivo es privado: **no se guarda en el repositorio, ni en
`site/`, ni en `release/`, ni en fixtures de test**. Lo que sigue describe su estructura y las conclusiones
técnicas; no reproduce su contenido, su `unit id` ni el digito completo de la copia (ver §6).

Clases usadas, como en el resto de la documentación: **[ESTÁTICO]** lo que se lee en el archivo entregado,
**[HOST]** lo que corren los tests del repo, **[HW]** lo que sólo puede medirse con la unidad delante.

## 1. Qué archivo es — y cuál no es

**[ESTÁTICO]** El archivo es **un JSON de 385 bytes**: el registro (*sidecar*) que M-Flasher escribe junto a una
lectura. **No es la copia de 1 MiB.** Esa copia es un `.bin` de 1 048 576 bytes que el registro describe, pero
no contiene.

```
{ "model": "FM-1", "bootloader": "m-bootloader 2.3-… (fm1)", "firmware": "FM-1 016 (stock)",
  "app": "OK", "chip key": "0x980F", "flash": "0x856014", "unit id": "…(privado)",
  "loader": "M-Flasher …, CPU at 96 MHz", "sha256": "…(privado)",
  "saved": "20261120-113923", "tool": "m-flasher …" }
```

| Campo | Qué dice | Cómo se lee en este proyecto |
|---|---|---|
| `model` | `FM-1` | la unidad es una FM-1; no es una identificación de parte |
| `flash` | `0x856014` | **[ESTÁTICO]** el id JEDEC que M-Flasher leyó del chip: **Puya P25Q80H** (ver `docs/JEDEC_COMPATIBILITY.md`; el otro id conocido, `0xB36014`, es el UCUN) |
| `chip key` | `0x980F` | la *chip key* que este proyecto espera de la familia que escribe |
| `bootloader` | `m-bootloader 2.3-…` | la unidad tiene el bootloader **2.3** (no el 1.x de las unidades más viejas) |
| `firmware` | `FM-1 016 (stock)` | la app que hay en la unidad es la V16 **oficial**, no la de este proyecto |
| `app` | `OK` | M-Flasher comprobó la app que leyó; no dice nada de la escritura |
| `loader` | `M-Flasher …, CPU at 96 MHz` | con qué herramienta y a qué reloj corrió la lectura |
| `sha256` | 64 hex | el digest de **la copia** que el registro acompaña (privado, §6) |
| `unit id` | — | identifica la unidad del dueño: **privado**, no se publica ni se usa en tests |
| `saved`, `tool` | fecha y versión | cuándo y con qué se tomó |

## 2. Qué información real da sobre el V16

**[ESTÁTICO]** De este registro, con certeza:

- El V16 oficial corre sobre la **misma familia de parte** que este proyecto ya escribe: id `0x856014`
  (P25Q80H), chip key `0x980F`.
- La unidad está en **bootloader 2.3**, es decir, en la generación que el camino V16 documenta
  (`docs/INSTALL_FROM_V16.md`), no en la del bootloader viejo.
- La app presente es **`FM-1 016` (stock)**, la identidad que también se lee por MIDI (`FM-1_016`).
- M-Flasher **pudo leer la parte completa** y calculó un SHA-256 sobre esa lectura: la copia existe y es
  coherente con la unidad (si no lo fuera, el registro no se habría escrito).

## 3. Qué **no** prueba — y por qué importa

- **No es una identificación JEDEC hecha por el loader de este proyecto.** El comando `0x12` es propio de este
  firmware; una unidad con V16 no lo implementa. El id `0x856014` lo leyó **M-Flasher**, no nuestra consulta.
- **`FM-1_016` es identidad MIDI/app, no identificación de parte.** Confundirlas fue el defecto de la fase 4
  que `web/test_v16_path.mjs` vigila: la versión se toma de la identidad de la unidad, y la parte se lee aparte.
- **No demuestra compatibilidad de escritura.** Que M-Flasher haya *leído* el chip no dice que el loader de este
  proyecto pueda *escribirlo*: eso lo decide la lectura del `0x12` en vivo, y se mide en la unidad **[HW]**.
- **No prueba el contenido de la copia**: el registro sólo ata un digest. Su contenido no se ha inspeccionado
  aquí (es privado y no se publica).
- **No autoriza nada.** Es un JSON que cualquiera puede escribir con un editor de texto y no requiere unidad.

## 4. Por qué el instalador responde lo que responde

Los dos mensajes del reporte tienen causas distintas, y las dos son rechazos correctos:

1. `Chip: this unit did not answer its JEDEC Truncated identification reply: nothing was sent.` — es el
   **transporte** (`web/fm1ota.js`, `probeChip`) contestando a una consulta `0x12` sobre una unidad **stock**:
   esa unidad no conoce el comando. Fase 4 ya separó los desenlaces (eco, cuerpo corto, silencio, id
   desconocido, geometría) con una frase cada uno, y `web/test_v16_path.mjs` lo verifica **[HOST]**. En una V16
   el resultado esperado es éste: **no hay identificación que leer**, y el instalador no la inventa.
2. `V16: take the complete 1 MiB copy of this unit with M-Flasher first and load it here; nothing was sent.` —
   es el **gate de la página**: sin copia de la unidad, la vuelta atrás no está cubierta y la página no manda
   nada. Cargar el **registro JSON** en ese campo disparaba «that file is not a 1 MiB full copy», que es cierto
   y no ayuda: el registro no es la copia, es la declaración de lo que la copia tiene que ser.

La copia se exige por una razón concreta y documentada: en una V16 la app no ocupa la ventana que el loader de
este proyecto escribe por defecto y **nada por encima de `0x93000` se toca**, así que el camino de vuelta es la
copia completa de **esta** unidad tomada con M-Flasher.

## 5. Qué cambió en Fase 5 (sin debilitar el gate)

**[HOST]** `web/index_pkg.html`:

- **El registro se lee, no se rechaza.** `readMflasherRecord()` acepta el JSON de M-Flasher (modelo `FM-1` y un
  `sha256` de 64 hex como mínimo), guarda parte, *chip key*, bootloader, firmware y digest, y lo muestra en la
  línea de estado del bloque V16. Se conserva entre recargas (`qg1.v16rec`) igual que la copia.
- **La copia se comprueba contra el registro.** Al cargar el `.bin` de 1 MiB, su SHA-256 tiene que ser el que el
  registro nombra; si no coincide, `v16DumpMismatch` y **no se envía nada**. Una copia de otra unidad, o una
  copia alterada, ya no puede pasar por la de ésta.
- **El gate no cambia**: sigue exigiendo la copia completa, la confirmación de restauración y el paquete oficial
  V16, y la unidad sigue leyéndose en vivo antes de borrar. El registro es metadato, nunca autorización.
- **El selector de archivo acepta `.json`** (antes sólo `.bin`, así que el registro ni siquiera se podía elegir),
  y el mensaje del gate nombra los dos archivos que M-Flasher deja: *el `.bin` que queda junto al `.json`*.
- **Tests**: `web/test_v16_path.mjs` sube de 45 a **62** comprobaciones: lectura del registro, campos inválidos,
  digest obligatorio, comprobación de la copia contra el registro, persistencia, textos en los tres idiomas del
  instalador y **20 000 entradas basura** contra el lector (no lanza; sólo acepta lo que trae digest).

## 6. Privacidad

El `unit id` y el `sha256` de la copia **no** se publican, no se escriben en tests ni en logs, y el archivo no se
versiona. El fixture de `web/test_v16_path.mjs` está inventado en el propio test (digest `0123…`), no copiado del
archivo del dueño.

## 7. Lo que sigue pendiente, y es hardware

| Afirmación | Clase |
|---|---|
| El registro se lee, se guarda y ata la copia | **[HOST]** `node web/test_v16_path.mjs` (62 checks) |
| Una copia que no es la del registro se rechaza | **[HOST]** mismo archivo |
| El id `0x856014` es P25Q80H en la tabla del proyecto | **[ESTÁTICO]** `docs/JEDEC_COMPATIBILITY.md` |
| **El updater del V16 acepta el paquete y arranca el loader** | **[HW]** no medido: requiere la unidad |
| **La restauración de la copia sobre el V16** | **[HW]** no medido: requiere la unidad |
| **Escritura del chip por el loader de este proyecto** | **[HW]** no medido: requiere la unidad |

Nada de esta fase se escribió, borró ni restauró en la FM-1 física.
