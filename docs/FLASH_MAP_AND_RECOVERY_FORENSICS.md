# Mapa de Memoria Flash NOR (1 MiB) y Evaluación Forense de Recuperación

## 1. Geometría y Mapa Estructural de la Memoria Flash

El hardware M-VAVE FM-1 (SoC JieLi AC791N / WL82) utiliza una memoria flash SPI NOR de 1 MiB (1.048.576 bytes / 0x100000). A partir de la ingeniería inversa, especificaciones de `docs/FORENSIC_FM1_VIABILITY.md` y herramientas de empaquetado `tools/fm1_image.py`, la estructura de particiones se distribuye de la siguiente manera:

| Rango de Direcciones | Tamaño | Nombre / Función | Crítico / Recuperabilidad | ¿QG escribe aquí? |
|---|---|---|---|---|
| `[0x00000, 0x04000)` | 16 KiB | **Cabecera Flash y Bootloader**: Cabecera JLFS, UBOOT (`0xA0`), `isd_config.ini` (`0x38D0`), clave de chip (`0x980F`) | **MÁXIMA CRITICIDAD**. Si se corrompe, el equipo no arranca en modo normal. | **NUNCA**. El loader de QG excluye explícitamente esta zona. |
| `[0x04000, 0x93000)` | 572 KiB (0x8F000) | **Área de Aplicación (App Area)**: Código binario principal `app.bin` (`0x4120`) y herramienta de configuración `cfg_tool.bin` | **RESTAURABLE**. Si falla o queda incompleto, se reinstala vía OTA o UBOOT. | **SÍ**. Se programa sector a sector (4 KiB) con verificación de CRC16. |
| `[0x93000, 0xE9000)` | 344 KiB (0x56000) | **VM / Sistema de Archivos / Configuración**: Almacenamiento virtual y estado interno | **NO CRÍTICO**. Restaurable mediante presets y configuración de fábrica. | Solo la ventana de registro de actualización (`0xE0000..0xE9000`). |
| `[0xE5000, 0xE8000)` | 12 KiB (0x3000) | **Datos de Usuario**: Búfer de autoguardado dual A/B y bancos FM6 del usuario | **PROTEGIDO**. Contiene configuraciones activas del usuario. | **NUNCA**. Excluido deliberadamente para proteger el trabajo musical. |
| `[0xE9000, 0xEA000)` | 4 KiB | **BTIF (Bluetooth Interface)**: Parámetros de enlace Bluetooth | **RESTAURABLE**. Configuración de enlace inalámbrico. | **NUNCA**. No es modificado por QG. |
| `[0xEA000, 0xFC000)` | 72 KiB (0x12000) | **USR Resources**: Patches y datos de usuario | **RESTAURABLE**. Datos de librerías. | **NUNCA**. No es modificado por QG. |
| `[0xFC000, 0xFF000)` | 12 KiB (0x3000) | **Zona Reservada No Documentada**: Sin uso identificado en stock | **PROTEGIDO**. No se modifica. | **NUNCA**. |
| `[0xFF000, 0x100000)` | 4 KiB | **key_mac**: Registro de calibración y MAC | **CRÍTICO PARA CONECTIVIDAD**. | **NUNCA**. Protegido contra escritura. |

---

## 2. Análisis Forense de Fallas de Energía y Recuperación (Single-Bank NOR)

### 2.1 La Realidad de la Arquitectura Single-Bank
El M-VAVE FM-1 posee un único chip SPI NOR (arquitectura single-bank). Esto implica que:
- No existe un segundo banco físico de memoria que permita conmutación instantánea (dual-bank A/B boot) a nivel de chip para la aplicación completa.
- Un corte eléctrico que ocurra exactamente durante el ciclo de borrado o programación de un sector en el rango `[0x04000, 0x93000)` deja la aplicación principal `app.bin` incompleta.

### 2.2 ¿Qué ocurre si se corta la corriente durante una instalación?
1. **El Bootloader (`[0, 0x04000)`) NO es modificado**: Dado que Quantum GammaWave **nunca** borra ni escribe en el rango `[0, 0x04000)`, el bootloader nativo del SoC y la tabla JLFS permanecen 100% íntegros.
2. **Arranque en Loader de Recuperación**:
   - Si la instalación se interrumpió, el equipo no iniciará la aplicación de sonido normal.
   - Sin embargo, enumera en el puerto USB como `ota-FM-1_...`. El instalador web o la utilidad CLI detectan este estado y permiten invocar `resume()` para completar la escritura de los sectores restantes sin reescribir todo desde cero.
3. **Recuperación Absoluta por ROM UBOOT**:
   - Si el loader OTA quedase corrupto o inaccesible, la **máscara ROM del chip JieLi AC791N** (`WL82 UBOOT1.00`) es hardware inmutable grabado en silicio.
   - Mediante la combinación de botones físicos de encendido (`OCT-` / `OCT+` / `HOME`), el SoC entra forzosamente en modo UBOOT por USB, permitiendo a herramientas como M-Flasher o unbricker comunitario reescribir `app.bin` o restaurar el volcado original de 1 MiB.

### 2.3 Lo que NUNCA se debe prometer ("Unbrickable")
Ningún sistema single-bank comercial es 100% "inmune a fallos sin intervención externa". Si bien el hardware es virtualmente irrecuperable sólo si se dañase la máscara ROM o se produjese un fallo eléctrico destructivo en el silicio, un corte de energía puede requerir el uso de la ruta de recuperación ROM UBOOT. Por ello, la política de seguridad estipula mantener **`INSTALL BLOCKED`** en la unidad del usuario hasta contar con el backup físico previo.
