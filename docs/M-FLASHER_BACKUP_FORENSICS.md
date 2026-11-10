# Auditoría Forense de Backup ROM/UBOOT (M-Flasher 0.9.1) para Stock FM-1_015

## 1. Alcance y Objetivo
Evaluar rigurosamente la viabilidad de obtener un volcado (dump) completo de 1 MiB de la memoria flash NOR física de una unidad M-VAVE FM-1 que ejecuta firmware de fábrica oficial V15 (`FM-1_015`), utilizando la ruta USB en modo ROM UBOOT con la herramienta comunitaria **M-Flasher 0.9.1**, sin desarmar el equipo, sin programadores físicos y sin realizar modificaciones destructivas ni operaciones de escritura persistente.

---

## 2. Auditoría del Protocolo y Secuencia de Comandos (M-Flasher 0.9.1)

De acuerdo con la auditoría de código fuente (`src/jl.rs`, `src/ops.rs`, `src/boards.rs` y `src/scsi.rs` en el repositorio oficial de M-Flasher):

1. **Entrada al modo de descarga de la ROM (UBOOT)**:
   - **Soft key por SysEx**: Se envía el mensaje SysEx MIDI `F0 22 24 35 7D F7`. El firmware stock reinicia el microcontrolador JieLi AC791N en su modo nativo de descarga en máscara ROM.
   - **Alternativa física (hardware key)**: En caso de que el equipo esté apagado o no responda a MIDI, encender manteniendo presionadas las teclas indicadas en la guía comunitaria (habitualmente `OCT-`, `OCT+` y `HOME`).
2. **Identificación de la interfaz USB**:
   - El equipo se desconecta como dispositivo MIDI y reenumera en el bus USB bajo el protocolo SCSI como dispositivo de almacenamiento/control vendor `WL82`, producto `UBOOT1.00`.
3. **Carga del loader temporal en memoria volátil (RAM)**:
   - Bloques de 512 bytes son transmitidos mediante el comando SCSI `WRITE_MEMORY` (`0xFB06`), ofuscados/scrambled según el esquema estándar de JieLi, dirigiéndose a los rangos de memoria RAM interna `0x01C20000` o `0x01C28000`.
   - Se invoca el comando de salto `JUMP` (`0xFB08`) indicando como parámetro el bus de la flash SPI NOR.
   - **Garantía de no persistencia**: El loader reside **únicamente en SRAM**. Desaparece por completo al reiniciar o quitar la alimentación. No toca la flash en este paso.
4. **Lectura completa de la flash (Dump)**:
   - El loader ejecuta el comando `READ_FLASH` (`0xFD05`) en bloques de 16 KiB consecutivos a lo largo del MiB completo (`0x00000` a `0x100000`, 1.048.576 bytes).
5. **Comprobación de integridad por hardware (CRC32 en chip)**:
   - El microcontrolador ejecuta el comando `CRC32` (`0xFC41`) computando la suma de verificación directamente sobre la flash física y devolviéndola al host. M-Flasher compara esta cifra con el CRC32 calculado sobre el archivo recibido.
   - Si no coinciden, se repite la lectura o se rechaza el volcado.
6. **Ausencia total de operaciones persistentes**:
   - En el código de M-Flasher (`src/ops.rs`), las rutinas `erase` y `program` solo residen dentro de `Session::apply` (utilizado exclusivamente para acciones explícitas de restauración o flasheo confirmadas por el usuario). La ruta `Session::save_backup` sólo invoca lectura y CRC32.

---

## 3. Compatibilidad con Unidades Stock `FM-1_015`

- El firmware oficial V15 no implementa los comandos SysEx de lectura de flash `0x12`/`0x13` propios de Quantum GammaWave.
- Por tanto, M-Flasher en modo ROM UBOOT es la **única vía de solo lectura** capaz de acceder a la memoria SPI NOR completa a través del cable USB antes de instalar cualquier firmware personalizado.
- El volcado obtenido se compone del archivo binario (`.bin`) de exactamente 1.048.576 bytes y de su archivo de metadatos (`.json`).

---

## 4. Integración con el Sistema de Validación Quantum GammaWave

Para que un volcado de M-Flasher sea aceptado por nuestro validador (`tools/fm1_mflasher.py`) y considerado un backup de oro:
1. Debe realizarse dos veces independientes (`--second`) y ser byte-idéntico, o corroborarse con el CRC32 reportado por el chip (`--crc32`).
2. El importador `fm1_mflasher.py` descifra la sección `app.bin` utilizando la clave de chip conocida `0x980F` y extrae la cadena de identidad interna para corroborar que corresponde verdaderamente a `FM-1_015`.
3. El archivo resultante se empaqueta en el formato estándar `qg1-backup-1` verificado por `tools/fm1_restore.py`.

---

## 5. Riesgos y Conclusiones Forenses

- **Riesgo de corte eléctrico durante la lectura**: **NULO**. Como el proceso de backup no ejecuta comandos de borrado ni de escritura en la flash, una desconexión accidental del cable USB solo cancela la lectura en RAM; la memoria física permanece 100% inalterada.
- **Conclusión técnica**: El procedimiento de backup mediante M-Flasher 0.9.1 es **estrictamente de solo lectura, seguro, compatible y técnicamente viable** para salvaguardar la unidad del usuario antes de cualquier intento futuro de instalación.
