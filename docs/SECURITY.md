# Seguridad de instalación en el FM-1: investigación y evaluación del instalador

Estado: **investigación de fuentes secundarias + evaluación del código, sin pruebas físicas.**
Nada de este documento se verificó en un FM-1 real desde el repositorio; lo que está simulado y lo
que no está marcado al final (§9). Ninguna instalación se declara segura: para eso haría falta una
recuperación independiente demostrada en unidades de prueba, y aquí no la hay.

Complementa a [PHASE0_SECURITY.md](PHASE0_SECURITY.md) (firma Ed25519 de los paquetes, propuesta
sin implementar) y a [INSTALL.es.md](INSTALL.es.md) (procedimiento para el usuario).

---

## 1. Veredicto en una línea

**El FM-1 es instalable y recuperable *mientras el área de arranque esté intacta y el equipo
responda por USB*; fuera de esas dos condiciones la recuperación no está demostrada, y el propio
protocolo de actualización no es un mecanismo de recuperación.**

Esto lo dicen las tres fuentes comunitarias serias de forma independiente, y es lo que hay que
tener presente antes de tocar un equipo con datos propios.

---

## 2. El SoC: JieLi AC791N / WL82

| | |
|---|---|
| SoC | JieLi AC791N (marca en el chip `C156211-11B8` / `C188612-11B8`; la variante exacta **no está confirmada**) |
| CPU | pi32v2 (derivada de Blackfin), palabras de 16 bits LE, 240 MHz con PLL desde cristal de 24 MHz |
| Núcleos | **2** (`CPU_CORE_NUM 2` en el SDK); el segundo núcleo es opcional para el firmware |
| OS | FreeRTOS derivado, SMP, con capa de API JieLi |
| Flash | **1 MiB** según todas las fuentes de ingeniería inversa; IDs documentados `0x856014` (Puya P25Q80H) y `0xB36014` — ver §3 |
| Seguro de arranque | **No hay.** Ninguna fuente describe autenticación criptográfica de la imagen de aplicación |

**Discrepancia resuelta: 1 MiB.** La ficha del Puya P25Q80H dice 8 Mbit (1 MiB); el JEDEC
`0x856014` codifica 2^20 bytes y el mapa termina en `0x100000`. El firmware lo confirma por tres
vías independientes: `web/emu/felucca_web.c` declara `nor[0x100000]`, el loader limita la imagen a
`LDR_FL_MAX 0x100000` y el paquete construido mide `0x93000` bytes de imagen (el resto del espacio
es la VM y las reservas del SDK). El valor 2 MiB que figuraba en `AGENTS.md`/
`TECHNICAL_OVERVIEW.md` era una errata heredada. Sigue siendo **NO VERIFICADO en hardware**: no se ha leído el JEDEC de una unidad
física; la conclusión es documental, no una medición.

### Cifrado de la imagen: ofuscación, no autenticación

La zona de aplicación va cifrada con `jl_sfc_cipher` y la **chip key `0x980F`**, que viaja dentro
del propio paquete (`isd_config.ini`). Cualquier paquete con el mismo contenedor y vector de
arranque se ejecuta. Esto es ofuscación, no una garantía de integridad ni de procedencia.

---

## 3. Mapa de flash (según ingeniería inversa)

Una sola copia de la aplicación. **No hay banco A/B.**

| Rango | Uso |
|---|---|
| `[0x00000, 0x04000)` | **Área de arranque**: cabecera de flash, directorio, SPL `uboot.boot` en `0xA0`, `isd_config.ini` con la chip key |
| `[0x04000, 0x93000)` | **Área de aplicación** (`app.bin` en `0x4120`); ~581 KB útiles |
| `[0x93000, 0xE9000)` | VM: almacén clave/valor JieLi (calibración, MAC, ajustes) |
| `[0xE9000, 0xEA000)` | BTIF: emparejamiento Bluetooth |
| `[0xEA000, 0xFC000)` | USR: parches y recursos de usuario |
| `[0xFF000, 0x100000)` | Reserva de fábrica (`key_mac`) |

**Variantes de flash:** `firmware/hal/fm1_flash_id.h` contiene una lista blanca explícita con
`0x856014` y `0xB36014`; `project.c` usa esa lista y rechaza cualquier otro ID (el firmware queda en
modo RAM). Aceptar el ID no valida el lote: ninguna de las dos variantes se ha leído en hardware y no
se asume que compartan fabricante, protección, OTP o temporización. Una unidad `0xB36014` sigue
**NO VERIFICADA** y no debe forzarse hasta repetir en ella identidad, backup, recuperación y escritura
controlada.

Tres advertencias que la wiki documenta y que importan para este proyecto:

1. **Los huecos no están necesariamente libres.** Otros firmwares usan estas zonas de otra forma
   (Felucca/SLOOP dejan los proyectos dentro del rango VM de stock; Baud Girl mueve su VM a
   `0xD9000`). Un firmware que busque su loader estanciado solo en `0xE0000` **no encontrará** el
   que V15 estanca en `0xE6000`.
2. **Tres espacios de direcciones para el mismo byte** (offset físico de flash, offset decodificado
   dentro de `app.bin`, dirección CPU XIP `0x02000000+`). Nunca se mezclan.
3. QG-1 usa la zona USR para los 8 temas y el banco FM6 de 32 voces, según el README: **esa
   información vive en la flash y se pierde si se escribe la zona.**

---

## 4. Cadena de arranque

```
mask ROM  ->  SPL "uboot.boot" (flash 0xA0, clave fija 0xFFFF, en RAM 0x01C02000)
                |  relojes, directorio JLFS, SFC + clave XIP, invalida caché
                +-> aplicación en 0x02000120 (XIP)              <- arranque normal
                +-> loader de actualización estancado en 0x01C0A800 (RAM)  <- si hay registro de update pendiente
```

**Modo de descarga de la ROM (USB):** la ROM escucha una palabra clave en D+/D− al encender:
`0x16EF`, 16 bits, MSB primero, D+ como reloj y D− como datos muestreado en flanco de subida, a
~50 kHz, con huecos de 150 µs. El chip responde manteniendo ambas líneas en bajo 1–2 ms y luego
enumera como almacenamiento `4C4A:8057`, `INQUIRY WL82/UBOOT1.00/1.00`. **Funciona aunque el
bootloader esté roto.** Ese es el último escalón de recuperación, y es de hardware (dongle
USB_KEY: RP2040, Flipper Zero, FM-1 Transporter de tres hilos **sin VBUS**).

En el FM-1 **no hay** cabecera JTAG, ni pads de test, ni botón de recuperación. `PB01` es una
entrada de reset con retención de 8 s (`RESET=PB01_08_0`, `UPDATE_JUMP=0`): llamarla «botón de
recuperación» sería incorrecto. La auditoría de consolas de fábrica (UART shell, CDC, comandos AT,
modos BQB/DUT) salió **negativa**.

---

## 5. El protocolo de actualización y qué verifica de verdad

Todo pasa por **USB-MIDI SysEx mientras la aplicación corre** (no hay DFU/CDC/HID). El **dispositivo
tira del paquete; el host solo contesta.**

```
identidad   F0 00 32 45 00 00 00 40 7F F7   -> bloque de 34 B ("FM-1_015")
arranque    F0 22 24 35 7F F7               -> paso 1 y paso 2, el mismo comando
paso 1      la aplicación en marcha lee el paquete, escribe el registro UPDATA_PARM y entra en el loader
            senal de verificacion completa: pide 0xE0000000, longitud 8 -> "success\0"
paso 2      el loader estancado lee cabecera/entradas/datos, escribe el area de aplicacion,
            verifica la flash y pide 0xF0000000, longitud 8 -> "success\0"; luego reset
```

Enmarcado: `00 59 30 | body_len:u24 | flashtype:u8 | addr:u32 | length:u24 | data | checksum`, un
flujo continuo de 8→7 bits entre `F0` y `F7`; `checksum = ~sum(flashtype…data) & 0xFF`.

### Qué comprueba el dispositivo (y qué no)

| Comprobación | Existe | Es autenticación |
|---|---|---|
| Checksum por bloque del canal SysEx (`~sum`) | Sí | No: detiene corrupción, no a nadie |
| `body_len + 7 == decoded_length` | Sí | No: enmarcado |
| CRC-16 (CCITT en el payload, XMODEM en el contenedor) sobre `ota.bin`, entradas, cabeceras y la imagen del loader | Sí | **No: es el único freno posterior a la copia** |
| Mágicas `@JMUA` / `JLUFW`, etiqueta `AC791N`, cadena de modelo/versión, tamaño mínimo | Sí | No |
| **Firma criptográfica (RSA/ECDSA)** | **No** | — |
| Anti-rollback | **No** (la negativa a repetir la misma versión es política del host, no un fusible) | — |

Confirmado por dos vías independientes (cadenas del actualizador y desensamblado del loader). El
«éxito» de `success\0` es **un acuse, no una autorización**: el paso 1 reintenta la lectura hasta
cuatro veces y devuelve `-13` si ninguna coincide; solo entonces escribe el registro y entra en el
loader. No es una firma ni una atestación del host.

**Matiz importante:** el paso 1 acepta un **loader ajeno** (los paquetes Felucca/SLOOP llevan su
propio `ota.bin`), y un paquete con la tabla de particiones movida **se instala**. El loader
también puede reescribir la cabecera de arranque (`uboot.boot` + `isd_config.ini`); por eso los
instaladores comunitarios anclan esos 16 KB por SHA-256.

---

## 6. Por qué un corte de energía es lo peligroso

**No hay rollback automático.** El SDK de JieLi *sí* soporta doble banco
(`CONFIG_DOUBLE_BANK_ENABLE` → «upgrade failure automatically rolls back to the old bank»), pero la
configuración del FM-1 **no tiene las claves de doble banco**: `NEW_FLASH_FS=YES`, un solo
`app.bin`. Un corte a media escritura deja el área de aplicación a medias y **el equipo no arranca**.

Y hay una ventana concreta, documentada:

> «V15, Felucca y la mayoría de los firmwares propios escriben el área de aplicación en orden de
> dirección. **Reescrito el primer sector (`0x4000`, desde donde salta el bootloader), ya no queda
> código de rescate en el equipo**, así que casi todo el resto de la escritura —de uno a dos
> minutos— queda sin protección.»

ChoralRoot (`docs/BOOT-SAFETY.md`) y TransitFM1 invierten ese orden (el primer sector se escribe
al final) y dejan la ventana en ~0,1 s. **QG-1 usa el protocolo de dos pasos de stock, con el
loader de stock, que escribe en orden de dirección: tiene exactamente la misma ventana.** Esto es
una limitación real y queda declarada.

Otras causas documentadas de ladrillo:

- **De un propietario a otro directamente** («custom over custom») **ha ladrillado unidades**. El
  consejo habitual es pasar por V15 primero — y la propia escritura de V15 es en orden de dirección,
  así que el paso *de salida* de V15 es el expuesto.
- TransitFM1 0.1.2 aceptó una imagen mayor de la que su loader escribía: «the unit was left dark»;
  0.1.3 rechaza esos paquetes.
- Batería plana a media escritura: puede dejar el equipo **sin USB alguno**, porque la mitad del
  firmware cuelga antes de su propio código de seguridad y el bootloader **no** cae en el loader de
  actualización al encender. Solo ayuda el dongle USB_KEY.
- Nunca encender dos veces seguidas en modo de actualización sin arranque normal entre medias.

---

## 7. Evaluación del instalador de QG-1

Archivos: `tools/fm1_install.py` (CLI), `web/fm1ota.js` (paso 1 y 2 en el navegador),
`web/fm1pkg.js` (lector/validador del paquete), `web/index_pkg.html` (publicador).

### 7.1 Lo que sí verifica

| Comprobación | Dónde | Resultado con el arnés |
|---|---|---|
| **SHA-256 obligatorio**: sin hash publicado no se escribe nada (exit 2) | `fm1_install.py check_sha256`, `index_pkg.html` | `no SHA-256 published -> refused (exit 2), no MIDI` **ok** |
| Hash de `--sha256`, de `PACKAGE.fwsc.sha256` o de `SHA256SUMS` | `expected_sha256` | coincide / no coincide / línea errónea: **ok** |
| CRC-16 de cabecera UFW, lista de entradas, `flash.bin`, cabecera JLFS, cabecera de flash | `fm1pkg.js` | `package bounds: all checks passed` **ok** |
| **Modelo correcto** antes de escribir (exit 6) | `fm1_install.py verify` | `another model -> exit 6, nothing sent` **ok** |
| **Identidad re-comprobada tras el reinicio** (exit 6) | `fm1_install.py` | `another identity after the restart -> exit 6` **ok** |
| Cable desconectado en el paso 1 → **nada escrito** (exit 4) | `fm1_install.py` | **ok** |
| Cable desconectado durante la escritura → para en el acto (exit 4) | `fm1_install.py` | **ok** |
| Loader que deja de contestar → timeout (exit 5) | `fm1_install.py` | **ok** |
| Leer más allá del paquete → exit 4 | `fm1_install.py` | **ok** |
| Paquete sin marcador de loader / no es paquete / archivo inexistente → exit 2 | `fm1_install.py` | **ok** |
| Solo comprobar, sin escribir (`--verify-only`) | `fm1_install.py` | **ok** |
| **Reanudación**: si el equipo sigue en modo de actualización de un intento cortado, termina la escritura | `fm1_install.py` docstring + `install: device already in update mode -> finishes the write` | **ok** |
| Volver a V15 oficial sin `--force` (hash `STOCK_V15_SHA256`) | `fm1_install.py` | **ok** con `--force` cuando no coincide |
| Nunca se sondea otro equipo que no sea FM-1 (`PORT_RE`) | `fm1_install.py` | **ok** |
| Límites del parser (512 KB sin final de lista rechazado, sin bucle) | `test_pkg_bounds.mjs` | **ok** |
| El borrado de registros no toca VM, autosave/FM6 ni USR aunque su cola parezca un registro | `ldr_core.c`, `ldr_test` | **ok** (SIMULADO; ventana `[0xE0000,0xE9000)`, sin `[0xE5000,0xE8000)`) |

`python3 tests/install_test.py` → **`install tests passed`** (31 comprobaciones, SIMULADO contra un
FM-1 simulado). Dos comprobaciones quedan **sin ejecutar** aquí: la comparación de la imagen lógica
`fm1_install.py` vs `fm1pkg.js` (necesita `node` **y** `build/felucca.fwsc`, que requiere la
toolchain JieLi) y la restauración de V15 oficial (necesita `FM-1_v15.fwsc` en la raíz).

### 7.2 Lo que NO verifica (y no debe presentarse como garantía)

1. **No firma ni verifica firmas.** El SHA-256 da integridad, no procedencia: si `versions.json` y
   el `.fwsc` se cambian juntos en el sitio, el hash coincide con el paquete del atacante. Es
   exactamente lo que propone `PHASE0_SECURITY.md` y sigue **sin implementar**.
2. **No hay anti-rollback.** Una versión vieja firmada sigue siendo válida.
3. **No protege la ventana de escritura.** El loader de stock escribe en orden de dirección: entre
   el reescrito del sector `0x4000` y el final hay uno o dos minutos sin código de rescate (§6).
4. **No hay rollback automático** si se corta la luz a media escritura (banco único, §6).
5. **La verificación es del host, no del dispositivo.** Si la máquina del usuario está
   comprometida, nada de esto la salva. El FM-1 no verifica firmas.
6. **`success\0` no es autorización** (§5). El paso 1 y el loader reintentan la lectura cuatro veces,
   pero si no coincide devuelven error (`-13` / `-14`) y no confirman el commit.
7. **No hay recuperación demostrada fuera del área de arranque intacta.** Los propios investigadores
   lo dicen: *«The update protocol is not a demonstrated recovery mechanism. Current work has not
   established ROM recovery, rollback, or safe interrupted-write behavior for the single-bank
   layout.»*

### 7.3 Comprobaciones que sí existen en el repo y pasan

```
python3 tests/install_test.py            -> install tests passed
node  web/test_web.mjs                  -> web tests passed
node  web/test_backup.mjs               -> web backup: capture, validation before writes, restore order
node  web/test_pkg_bounds.mjs           -> package bounds: all checks passed
```

Todo **SIMULADO**: contra un FM-1 emulado, no contra metal.

---

## 8. Recuperación: la escalera documentada

De la wiki de desarrolladores (`docs/Recovery.md`), **leer antes de instalar la primera vez**:

| Síntoma | Método |
|---|---|
| Arranca pero el firmware está roto | Su modo de actualización (Felucca y derivados: **OCT− y OCT+ 5 s** al encender; fm1-base: **REC**) → instalar V15 de stock |
| Arranca y se cae dos veces | Modo seguro de la familia Felucca (sin audio, USB activo) tras dos arranques fallidos |
| Tiene m-bootloader | **REC** al encender (modo descarga) · **PLAY** (menú de recuperación) |
| Aparece en USB como `4C4A:8057` / `WL80UBOOT1.00` | Ya está en **modo descarga de la ROM** → M-Flasher, `MvaveFM1Unbricker`, `fm1_rescue` |
| Nada en USB pero el firmware cae | **Dejarlo encendido dos minutos** (guardián de arranque); **OCT−/OCT+ 10 s** al encender; **tres reinicios rápidos** (10 s sí, no, ×3 → tres arranques caídos caen en el arranque de la ROM) |
| Nada en USB (ladrillo duro) | **Dongle USB_KEY** |

**Cinco reglas comunes a todas las herramientas de modo descarga** (verificadas en hardware por
la comunidad): (1) hay un loader de flash en RAM; (2) la chip key debe ser `0x980F` y la flash
`856014` (1 MiB), o no se hace nada; (3) se lee y guarda **toda** la flash primero; (4) solo se
escribe `[0x4000, 0x93000)`, solo sectores de 4 KiB que difieren, cada uno borrado/escrito/leído;
(5) la lectura final completa debe coincidir.

**Nunca:** escribir el área de arranque `[0, 0x4000)` salvo con una herramienta específica de
bootloader; copiar la flash completa de otra unidad (lleva datos y calibración propias); conectar
VBUS desde un dongle.

**Límite de la recuperación:** está probada solo con el área de arranque `0x0–0x4000` intacta — el
transportador se niega a escribir ese rango. **Recuperar un área de arranque dañada no está
demostrado.**

---

## 9. Qué está verificado, qué simulado, qué no

### VERIFICADO EN HARDWARE
**Nada de este documento.** El repositorio no ejecutó ninguna operación sobre un FM-1 y no debe
hacerlo (§2 de `AGENTS.md`). Las afirmaciones «verificadas en hardware» de §4, §6 y §8 son de los
proyectos citados, no nuestras.

### SIMULADO
- `python3 tests/install_test.py` → **31 comprobaciones en verde** contra un FM-1 simulado:
  SHA-256 obligatorio y sus fallos, modelo, identidad tras el reinicio, cable desconectado en el
  paso 1 y durante la escritura, loader mudo, lectura fuera del paquete, reanudación, V15, `--info`.
- `node web/test_web.mjs`, `node web/test_backup.mjs`, `node web/test_pkg_bounds.mjs` → en verde:
  CRC-16 del paquete, captura/validación/orden de la restauración del backup, límites del parser.
- `tests/ldr_test` **simula un corte de energía** después de 0, 4 KiB, 64 KiB, 256 KiB y 512 KiB
  programados: el registro de actualización sobrevive, la segunda sesión reanuda y termina, el
  área de usuario no se toca. Es el loader en RAM, **no** el SPL ni el tiempo real de erase/write;
  no sustituye una prueba física.

### NO VERIFICADO
- **Ninguna prueba física.** Ni escritura real, ni corte de luz, ni recuperación, ni modo descarga,
  ni dongle USB_KEY.
- **Lectura del JEDEC en una unidad física**: el tamaño se resolvió documentalmente a 1 MiB (§2),
  pero no se leyó el ID de la chip en hardware.
- Si la protección de escritura VOTP/OTP o algún eFuse está programado de fábrica.
- Si el área de arranque dañada se puede recuperar.
- La comparación `fm1_install.py` ↔ `fm1pkg.js` y la restauración de V15 oficial (falta
  `build/felucca.fwsc` y `FM-1_v15.fwsc`).
- `tests/target_budget.py` (necesita `build/felucca.dis` de la toolchain JieLi) y la prueba de CPU
  bajo callgrind (necesita `valgrind`, no instalable aquí).

**No se declara que la instalación sea segura.** Para hacerlo haría falta: (a) una vía de
recuperación *independiente* —USB_KEY en un equipo de prueba— demostrada de principio a fin,
(b) un banco A/B o escritura del primer sector al final, y (c) pruebas físicas controladas con
cortes de luz deliberados en varios instantes de la escritura. Ninguna de las tres existe hoy.

---

## 10. Fuentes

Comunitarias (ingeniería inversa, con marcadores de confianza propios; no son declaraciones del
fabricante):

- FM-1-RE — <https://github.com/AL-255/FM-1-RE> · protocolo OTA <https://github.com/AL-255/FM-1-RE/blob/main/docs/io/11-ota-protocol.md> · veredicto <https://github.com/AL-255/FM-1-RE/blob/main/TODO_aug2.md> · arquitectura <https://github.com/AL-255/FM-1-RE/blob/main/docs/architecture.md>
- FM-1 Firmware Developers' Wiki (no oficial, CC BY-SA 4.0, cada dato con etiqueta `[HW]/[SDK]/[RE]/[CODE]/[?]`) — <https://github.com/sbegueria/fm1-dev-wiki> · [Flash-Storage](https://github.com/sbegueria/fm1-dev-wiki/blob/master/docs/Flash-Storage.md) · [Boot-Chain-and-Bootloaders](https://github.com/sbegueria/fm1-dev-wiki/blob/master/docs/Boot-Chain-and-Bootloaders.md) · [Second-Core](https://github.com/sbegueria/fm1-dev-wiki/blob/master/docs/Second-Core.md) · [Update-Protocol-and-Installers](https://github.com/sbegueria/fm1-dev-wiki/blob/master/docs/Update-Protocol-and-Installers.md) · [Recovery](https://github.com/sbegueria/fm1-dev-wiki/blob/master/docs/Recovery.md)
- m-bootloader (MIT, limpieza) — <https://github.com/jvitkauskas/m-bootloader> · M-Flasher <https://github.com/jvitkauskas/m-flasher>
- lunar-modulator — [hardware](https://github.com/ip2k/lunar-modulator/blob/main/docs/01-hardware.md) · [stock-firmware](https://github.com/ip2k/lunar-modulator/blob/main/docs/02-stock-firmware.md) · [update-protocol](https://github.com/ip2k/lunar-modulator/blob/main/docs/03-update-protocol.md) · [recovery-and-risk](https://github.com/ip2k/lunar-modulator/blob/main/docs/07-recovery-and-risk.md) · [pico2w-recovery](https://github.com/ip2k/lunar-modulator/blob/main/docs/17-pico2w-recovery.md)
- fm1-custom-fw — <https://github.com/aroum/fm1-custom-fw/blob/main/REVERSE_ENGINEERING.md>
- jl-uboot-tool — <https://github.com/kagaimiq/jl-uboot-tool> (su tabla marca WL82/AC791N como **desconocido**: no es un flasheador de FM-1 establecido)
- FM-1 Transporter — <https://github.com/kurogedelic/FM-1-transporter> · MvaveFM1Unbricker <https://github.com/Quixotic7/MvaveFM1Unbricker>

Fabricante (primaria):

- JieLi docs — <https://doc.zh-jieli.com/> · descarga del proyecto <https://doc.zh-jieli.com/AC79/zh-cn/release_v1.0.3/getting_started/project_download/download.html> · herramientas de flasheo <https://doc.zh-jieli.com/SDKWiki/master/Jieli-Tech/fw-AC63_BT_SDK/2-getting-started/2.4-flashing-tools.html> · OTP <https://doc.zh-jieli.com/Tools/zh-cn/dev_tools/toolchains/otp_cfg.html>

Formatos:

- Contenedor nuevo JieLi — <https://kagaimiq.github.io/jielie/datafmt/newfw.html>

## v1.6-QG audit — touch, presets and the flash path

Scope: the 1.6 changes only (preset tables, the EVO LFO wave, the `PAT` matrix source and MIDI `0xA0`,
the HOME/EDIT knob drawing and the splash). **No bootloader, linker script, flash layout, update loader or
recovery path was modified**, and no hardware was flashed.

- **Undefined behaviour (v1.7 stress pass):** UBSan found two left shifts of negative samples in ANALOG
  (`eng_analog.c`); they are multiplications by 2 now, with byte-identical golden renders. The FUN5
  persistence test wrote past its 27-global header; the loop is bounded to the old format. Both are covered
  by `tests/stress_run.sh`, which runs the storage, project, robustness and MIDI tests under ASan/UBSan.
- **Preset table change (audited):** the two least distinct DRUM kits are retired from browsing but stay
  in `DRUM_PRESETS` as legacy entries at their original indices 10 and 11, flagged `.legacy = 1`. Loaders
  keep `preset % npresets` with `npresets` still 12, so a pre-1.6 project, favorite or user preset resolves
  to the exact same kit; browsing, the dice and the editor use `engine_library()` (10 DRUM, 200 total).
  The two legacy renders are byte-identical to v1.5. `tests/preset_library_test.py` pins the ten browsable
  indices, the two legacy names and every range; `P_COUNT`, the project size and its offsets are unchanged
  (`project.c`'s frozen-size assertions still compile), and `sizeof(preset_t)` remains 40 bytes.
- **MIDI surface:** `0xA0` is handled exactly like the existing `0xD0` branch: three bytes, channel
  validated by the same `midi_track()`/routing rules, value clamped to 0..127, and only voices already
  held on that track are touched. No new buffer, no length field, no allocation.
- **LFO EVO:** integer only (`uint64_t` intermediate bounded by 2^48), clamped to ±32767; no division by a
  user value, no table index outside `N_LWAVE`.
- **Rendering:** the new splash and knobs use the existing clipped primitives (`cv_rect`, `cv_rrect`,
  `gw_disc`, `gw_orbit_ring`, `knob`), the saved palette, and no text. The UI lint measures every screen.

### Package signing — design, deliberately not enabled

The current `.fwsc` path already checks a CRC and refuses short/oversized writes, and the loader keeps a
rollback copy; that is corruption detection, not authenticity. A signature would need:

1. An Ed25519 (or ECDSA P-256) detached signature over SHA-256 of the payload, stored in the package
   header and covered by the existing CRC.
2. Verification **before the first erase**, in the loader, with the public key in ROM — not in the same
   updatable flash region it protects.
3. A recovery window that keeps the previous image bootable until the new one has been verified after a
   reboot, plus a documented USB/UART recovery path.

None of that can be validated here: the AC791N boot chain, the ROM's key storage and a physical recovery
procedure are not available, and a key placed in updatable flash is not a root of trust. Shipping an
unverified signature check would add a new bricking path, so it stays a design and the firmware continues
to rely on CRC + rollback. This is a residual risk, stated plainly.

### Residual risks

- A power cut during any flash write is handled by the existing retry/rollback tests but cannot be proven
  safe without hardware.
- A malformed external MIDI stream is bounded by the existing ring buffers and fuzz tests, not by new code.
- The 12-voice build is not shipped; if enabled experimentally it could starve the audio ISR on a real
  board (see `docs/POLYPHONY_1.6.md`).
