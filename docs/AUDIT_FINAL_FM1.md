# Auditoría final QG-1 / FM-1 — 2026-11-03

Rama de integración: `perf/security-hardening`. Este documento describe el estado del árbol en el
commit de cierre de esta auditoría. No se modificó `main`, no se fusionaron PRs, no se publicaron
releases y **no se escribió nada en un FM-1 físico**.

## Dictamen

**NO VERIFICADO EN HARDWARE — NO APTO PARA INSTALACIÓN REAL.**

No es «APTO PARA PRUEBAS FÍSICAS CONTROLADAS» todavía: falta un backup completo verificado y una
recuperación USB_KEY demostrada en una unidad de ensayo. Tampoco es un fallo de compilación: el
firmware, el loader, el emulador y las herramientas pasan las pruebas de host y el target AC791N
compila dentro de los límites estáticos. La diferencia entre ambas cosas es exactamente el objeto de
esta auditoría.

## Estado por capas

| Capa | Estado | Evidencia / límite |
|---|---|---|
| Firmware | IMPLEMENTADO | C y cabeceras generadas; sin cambios de bootloader ni de formato de usuario |
| Host | TESTEADO EN HOST | UI, presets, web, emulador, seguridad, stress y loader, incluido ASan/UBSan |
| Target | COMPILADO PARA TARGET | JieLi AC791N; `tools/build.py`, `target_budget.py` y `memory_map_test.py` en verde |
| Simulación | SIMULADO | Cortes de alimentación en `ldr_test`, emulador WASM, modelos de RAM/CPU |
| Hardware | NO VERIFICADO | Ningún arranque, audio, pantalla, USB, encoder, flash, IRQ, DMA ni recuperación real |
| Recuperación | NO DEMOSTRADA | USB_KEY descrito, no probado; no hay unidad de ensayo ni backup físico |

## Métricas del cierre

- App: **551224 B**. Paquete `.fwsc`: **610897 B**. Identidad de paquete `FM-1_982`.
- SHA-256 del paquete: **`35499e5928335809bed46cb62c941ed89f496af72530b340463e8fb32c07fe01`**.
- SHA-256 de la aplicación: **`9118f2c9dcc9897245f90bc3816b6d9bcf0a79f1de7bedb7d83caad61889c16e`**.
- Slot XIP `APP_SLOT = 0x8DFBC = 581564 B`; app real 551224 B; **30340 B** sin usar. `0x93000` es la
  imagen de flash empaquetada, no la flash física de 1 MiB.
- RAM `.data+.bss`: 94668 B de 98304 B; spare **3636 B**.
- Pool spare: **9248 B** (mínimo del proyecto: 8192 B).
- RAMTEXT spare: **2968 B**. NOINIT spare: **124 B**.
- `perf_block`: **374/522**. `DRUM_PATH`: **1189/1191**. `perf_begin` 28/28; `perf_pre` 31/31;
  `perf_master` 32/32.
- UI: 172 pantallas × 17 paletas, **0** lint, **0** desalineaciones; splash 425 pasos, 200 cambian,
  225 reescrituras idénticas omitidas.
- App: **549028 B**. Paquete `.fwsc`: **610897 B**. Identidad de paquete `FM-1_982`.
- SHA-256 del paquete: **`932d37a0d5c5d74124713555f678804589ba42b8e129bce03e1d0762ac36be8d`**.
- SHA-256 de la aplicación: **`0dcd80f23747832d7d440f9e992b032db7c40be297155c5ebe257c063512e67f`**.
- Slot XIP `APP_SLOT = 0x8DFBC = 581564 B`; app real 549028 B; **32536 B** sin usar. `0x93000` es la
  imagen de flash empaquetada, no la flash física de 1 MiB. `548708 B` fue una baseline anterior, no
  este HEAD. La imagen actual de app termina en `0x920DC`.
- App: **548708 B**. Paquete `.fwsc`: **610876 B**. Identidad de paquete `FM-1_982`.
- SHA-256 del paquete: **`38eb7655e903a5244663c7625db87e46317591d905e862d0ccb4795534d7351d`**.
- RAM `.data+.bss`: 94588 B de 98304 B; spare **3716 B**.
- Pool spare: **9248 B** (mínimo del proyecto: 8192 B).
- RAMTEXT spare: **2404 B**. NOINIT spare: **124 B**.
- `perf_block`: **569/522** (+9 %, límite +10 %; solo ~5 unidades antes del techo).
- `DRUM_PATH`: **1189/1191**. `perf_begin` 28/28; `perf_pre` 31/31; `perf_master` 32/32.
- UI: 172 pantallas × 17 paletas, **0** lint, **0** desalineaciones; splash 425 pasos, 200 cambian,
  225 reescrituras idénticas omitidas.
- UI: 172 pantallas × 17 paletas, **0** lint, **0** desalineaciones; splash 425 pasos, 198 cambian,
  227 reescrituras idénticas omitidas.
- No hay medición de ciclos, pila, latencia de interrupción, DMA ni corriente en la placa.

## Correcciones de esta pasada

1. **Sección PUNCH-IN FX (visual).** Se redujo la altura y la separación de las tarjetas, se dibujaron
   como pads redondeados con borde suave, se añadió una línea de sección `PUNCH-IN FX · A#4 TGT ·
   G5 CLR` y se mantuvo la retícula de 5 columnas. Es un cambio de dibujo: no añade parámetros,
   no cambia el formato persistente y no toca el DSP.
2. **ACK de finalización del instalador.** `web/fm1ota.js` y `tools/fm1_install.py` marcaban el paso
   como terminado aunque `link.send()` fallara al enviar `success`. Ahora un envío fallido devuelve
   `lost` y el host no presenta el paso como completado. Esto no hace atómica la escritura ni
   sustituye una recuperación; evita un falso positivo del host.
3. **Banco DSP Reverse/Glitch/Freeze/Tape Stop: DESCARTADO.** La implementación de buffer probada
   en esta rama quedó en `perf_block` **662/522** (+27 %) y redujo RAMTEXT por debajo del margen
   exigido. No se relajó el presupuesto ni se ocultó el fallo: se retiró el banco. Los efectos
   existentes (REPEAT, stutter MIDI, filtros, GATE, SWEEP, CRUSH, THROW, mutes y DIVE) siguen
   disponibles; Reverse/Glitch/Freeze/Tape Stop **no están implementados** en el firmware final.
   El Tape Stop no se declara: F#5 conserva el `DIVE` existente, que es un caída de pitch, no un
   tape-stop de buffer verificado.

## Corrección de la asignación HOME (2026-11-03)

El origen era `bg_knob()` en `ui_being.c`: dibujaba los cuatro mini-knobs con
`page_desc(cur_page(), c, ...)` aunque los encoders de HOME editan `home_param(c)`. Por eso KNOB 3
podía mover el indicador 1 (la página anterior seguía teniendo prioridad) y KNOB 1/2/4 parecían no
actualizar su propio indicador. Ahora `bg_knob()` usa `home_param()` solo cuando `ui.home` está activo
y conserva `page_desc()` en las páginas de motor, donde la columna de página es la correcta.

Los mini-knobs se rediseñaron como controles concéntricos: aro exterior, cuerpo hundido, indicador
radial de 270° y centro de contraste. La prueba `test_home_knobs()` recorre todos los motores
disponibles y los cuatro encoders; comprueba que cada uno mueve su parámetro y su mini-knob, y que no
mueve ningún otro. El test es de host, no una validación del encoder físico.

La variante de flash `0xB36014` ya pasa la lista blanca estática de `firmware/hal/fm1_flash_id.h`,
junto con `0x856014`; cualquier otro ID se rechaza. Esto **no** valida el lote: ninguna de las dos
variantes se ha leído en hardware. Una unidad `0xB36014` sigue **NO VERIFICADA** y no debe forzarse
hasta repetir en ella identidad, backup, recuperación y escritura controlada.

## Riesgos

### Críticos

- **Un solo banco de aplicación.** No hay A/B ni rollback automático. Un corte durante la escritura
  de `[0x4000,0x93000)` puede dejar la aplicación incompleta; el loader puede reanudar si el registro
  y el bootloader sobreviven, pero no hay garantía si se daña el área de arranque.
- **Sin firma criptográfica ni anti-rollback.** SHA-256 del instalador y CRC16 del paquete detectan
  corrupción, no sustituyen la procedencia. El sitio y el hash pueden reemplazarse juntos.
- **Recuperación no demostrada.** USB_KEY / ROM `4C4A:8057` está documentado, pero no se ejecutó en
  una unidad real. No se puede afirmar que un equipo bloqueado se recupere.

### Altos

- **Ventana de escritura ascendente.** El loader escribe el primer sector de la aplicación antes que
  el resto; la ventana sin código de rescate es de uno a dos minutos, no de milisegundos. Cambiar el
  orden sin bootloader compatible no crea un rollback real.
- **JEDEC no leído en hardware.** El mapa de 1 MiB y la lista blanca (`0x856014`, `0xB36014`) son
  documentales y coherentes con el código; no son una medición de la unidad. Un ID desconocido se
  rechaza, pero aceptar el ID no valida geometría, protección ni persistencia.
- **JEDEC no leído en hardware.** El mapa de 1 MiB (`0x856014`) es documental y coherente con el
  código; no es una medición de la unidad.
- **Sin medición de pila/ISR/DMA.** RAM y RAW text tienen margen estático, pero no hay high-water
  real de las pilas, latencia de interrupción, arbitraje DMA ni comportamiento de caché.
- **`perf_block` con ~5 unidades de margen.** El techo del +10 % es 574; el valor actual es 569. No
  se debe añadir DSP al camino caliente sin una medición de target.

### Medios

- **NOINIT con 124 B** de margen; no se debe añadir estado persistente no inicializado.
- **UI FX**: el rediseño es visual; no se verificó en la pantalla física ni con el encoder real.
- **Suite completa**: `run_tests.sh` no cabe en el límite de 5 minutos de este entorno; se ejecutaron
  los grupos y el loader por separado. Un timeout no se cuenta como aprobado.
- **CI/PR**: no hay autenticación GitHub en este entorno para abrir/actualizar PRs ni lanzar
  workflows manuales. El push de la rama se hace con las credenciales administradas por el host.

## Pruebas ejecutadas en esta pasada

| Prueba | Resultado |
|---|---|
| `tests/groups.sh ui` | OK: 172 pantallas × 17 paletas, 0 lint, 0 desalineaciones |
| `tests/groups.sh presets` | OK: 200/200 categorías, una primaria por preset |
| `tests/groups.sh web` | OK; incluye `web/test_web.mjs` y ACK final perdido |
| `tests/groups.sh emu` | OK; WASM y nativo bit a bit, 2.3 % de tiempo real |
| `tests/groups.sh security` | OK: instalador, backup, límites de paquete, audio ASan/UBSan |
| `tests/groups.sh stress 1` | OK: project, persistence, robust, MIDI y 2 pasadas de regress |
| `tests/groups.sh target` | OK: build AC791N, `target_budget`, `memory_map` |
| `tests/ldr_test OLD NEW` | OK: paquete anterior `FM-1_900`, paquete dañado, clave ajena, ACK incorrecto, reanudación y cinco cortes |
| `tests/ldr_test` con ASan/UBSan | OK, sin fugas; el harness libera sus imágenes |
| `tests/install_test.py` | OK; incluye ACK final perdido -> exit 5, nunca `done` |
| `tests/flash_id_test.c` | OK; `0x856014` y `0xB36014` aceptados, IDs desconocidos rechazados |
| `target_budget.py` | OK; `perf_block` 569/522 |
| `memory_map_test.py` | OK; spare RAM 3716, pool 9248, RAMTEXT 2404, NOINIT 124; slot 581564 vs app 549028 |
| `tests/groups.sh web` | OK |
| `tests/groups.sh emu` | OK; WASM y nativo bit a bit, 2.4 % de tiempo real |
| `tests/groups.sh security` | OK: instalador, backup, límites de paquete, audio ASan/UBSan |
| `tests/groups.sh stress 1` | OK: project, persistence, robust, MIDI y 2 pasadas de regress |
| `tests/groups.sh target` | OK: build AC791N, `target_budget`, `memory_map` |
| `tests/ldr_test OLD NEW` | OK: paquete dañado, clave ajena, reanudación y cinco cortes |
| `tests/ldr_test` con ASan/UBSan | OK |
| `target_budget.py` | OK; `perf_block` 569/522 |
| `memory_map_test.py` | OK; spare RAM 3716, pool 9248, RAMTEXT 2404, NOINIT 124 |

No se ejecutó una pasada completa de `run_tests.sh` por el límite de tiempo; no se declara que todos
los tests del repositorio hayan corrido en un solo proceso. No se ejecutó ninguna prueba física.

## Condiciones antes de instalar en un FM-1 real

1. Leer y registrar la identidad de la unidad y el JEDEC. Solo se aceptan `0x856014` o `0xB36014`
   (ambos 1 MiB y en la lista blanca estática); validar cada variante en su propia unidad. Cualquier
   otro ID es NO-GO.
1. Leer y registrar la identidad de la unidad y el JEDEC; debe ser `0x856014` (1 MiB). Si no, detenerse.
2. Leer la flash completa **dos veces** de forma independiente y comparar byte a byte y SHA-256; guardar
   copia fuera del equipo. Una sola lectura no es un backup.
3. Demostrar la recuperación USB_KEY en una unidad de ensayo, desde un estado conocido, antes de
   cualquier actualización deliberada.
4. Verificar el paquete y su SHA-256 publicado; no usar un paquete de otro modelo ni un hash pegado
   en el chat.
5. Ejecutar la guía `docs/HARDWARE_TEST_PLAN.md` en orden: identidad, backup, recuperación, memoria,
   reloj/reset/watchdog, actualización controlada, cortes y audio/datos.
6. Detenerse ante cualquier JEDEC distinto, hash de lectura distinto, reset loop, watchdog, humo,
   corriente anómala o dispositivo que no enumera.
7. No escribir el área de arranque `[0,0x4000)` ni el bootloader. No usar un único equipo funcional
   como banco de pruebas.

## Actualización posterior: PUNCH-IN FX y emulador

- El rediseño de PUNCH-IN FX y la corrección de etiquetas del WASM se documentan en
  `docs/PUNCH_IN_FX_AUDIT.md`. No añadieron DSP ni cambiaron parámetros de audio.
- Target posterior: app **549220 B** (+192 B), paquete **610897 B**, identidad `FM-1_982`; SHA-256 de la
  app `616db4d6ed191e171721c7cdc1f7ea9fde80705ef1911f0dad51c1918c206f8d`, del paquete
  `106b5f41bd8f93fa22142e9c9669375dc784f7e65a19de2b8183d7e19acedab4`.
- El emulador se regeneró (`build/emu`, WASM 1019205 B, `v1.7-QG`) y su página evita la cache con
  `?v=@VERSION@`; las capturas son del framebuffer real del WASM y del render del firmware.
- La conclusión de hardware no cambia: **NO VERIFICADO EN HARDWARE — NO APTO PARA INSTALACIÓN REAL**.

## Lo que no se declara

- No se declara «antibrick», «actualización atómica», «rollback», «recuperación garantizada» ni
  «compatible con hardware».
- No se declara que los efectos nuevos de PUNCH-IN FX estén implementados: el banco DSP fue descartado.
- No se declara que la compilación de target sustituya una medición eléctrica o de audio.

## Fuentes internas

- `docs/HARDWARE_TEST_PLAN.md`: guía física completa y puertas de seguridad.
- `docs/SECURITY.md`: mapa de flash, cadena de arranque, protocolo y límites del bootloader.
- `docs/PERF_SECURITY_AUDIT.md`: auditoría de rendimiento y seguridad de la rama.
- `docs/SYNC_1.7.md`: métricas y estado de sincronización.
- `CHANGELOG.md`: cambios de esta rama.
