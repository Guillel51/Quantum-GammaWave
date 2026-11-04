# Backup, restauración y recuperación de un FM-1

Este documento separa tres cosas que suelen confundirse: **copia de seguridad**, **restauración** y
**recuperación independiente**. Sólo la primera está demostrada en software. Ninguna de las tres está
demostrada en hardware aquí: no hay unidad física en este repositorio.

## 1. Qué hay implementado

| Pieza | Herramienta | Qué hace | Estado |
|---|---|---|---|
| Copia cruda de la NOR | `tools/fm1_backup.py` | dos pasadas completas, byte a byte, SHA-256 por pasada, manifiesto con chip/geometría y registro de tramas | implementado, probado en simulación |
| Validación del archivo | `tools/fm1_restore.py` | completo, dos pasadas idénticas, 1 MiB, hash, chip, geometría, log sólo lectura | implementado, probado |
| Comparación con la unidad | `tools/fm1_restore.py --compare` | lee la NOR de nuevo (0x13) y lista los rangos que difieren | implementado, probado en simulación |
| Restauración cruda | `tools/fm1_restore.py --write` | **rechazada por diseño** | no existe ruta verificada |
| Copia de datos de usuario | `web/fm1backup.js` + `tools/fm1_backup.py` | objetos de la app (proyectos, bancos, ajustes, canciones) | implementado |

`fm1_backup.py` sólo envía `0x11`, `0x12` y `0x13`. Cualquier otra trama en el registro hace que el
manifiesto no se escriba. `fm1_restore.py` comprueba eso antes de decir que un archivo sirve.

## 2. Por qué `--write` se rechaza

La ruta de actualización (`web/fm1ota.js`, `tools/fm1_install.py`) no escribe direcciones: **sirve un
paquete firmado** al loader, que decide qué escribirá. Nadie demostró que ese loader acepte un volcado
crudo de 1 MiB, ni que respete los rangos si se le sirve uno. Ofrecer esa opción sería exactamente el
camino que deja un instrumento inservible pareciendo una recuperación.

Para restaurar de verdad hacen falta, en este orden de preferencia:

1. **Bootloader del fabricante** (el que instala el firmware oficial V15). Es la única ruta que M-VAVE
   documenta y la que el instalador ya usa para volver al firmware de fábrica
   (`STOCK_V15_SHA256` en `tools/fm1_install.py`). Requiere que el loader esté sano.
2. **Programador externo** por SPI (pinza SOIC-8 o pads): lee y escribe la NOR sin depender de que la
   aplicación o el loader arranquen. Es la recuperación independiente real.
3. **Mask-ROM / USB_KEY del AC791N**: existe en el SoC, pero su comportamiento en el FM-1 no está
   documentado aquí; no se usa ni se simula.

Un RP2040 con `pico-serprog`/`flashrom` es una alternativa de laboratorio viable para (2): es un
programador SPI, no un bootloader copiado de terceros. **No se copia ningún bootloader ajeno**: licencia,
procedencia y compatibilidad tendrían que verificarse antes.

## 3. Procedimiento de laboratorio (segunda unidad)

Requisitos: FM-1 secundaria, cable USB de datos, fuente estable, pinza SOIC-8 + programador SPI
(RP2040/flashrom o equivalente), un segundo ordenador para leer el programador, y `python3` con `mido`
y `python-rtpi` para las herramientas del repo. Conocimientos: SPI NOR, `flashrom`, SysEx y lectura de
un mapa de memoria.

1. **Identificación física**: `python3 tools/fm1_install.py --info` y `fm1_backup.py`; anotar JEDEC,
   capacidad, página, sector y bloque. Comparar con la serigrafía del chip. Una identificación tecleada
   a mano no cuenta.
2. **Backup integral**: `python3 tools/fm1_backup.py --out lab/backup-1`. Debe dar `complete`.
3. **Verificación independiente**: `python3 tools/fm1_restore.py --backup lab/backup-1` y además leer la
   NOR con el programador externo y comparar los dos volcados (dos lectores distintos).
4. **Geometría**: confirmar 1 MiB, página 256, sector 4 KiB, bloque 64 KiB.
5. **Paquete firmado**: `python3 tools/fm1_sign.py` con la clave del custodio y
   `python3 tools/fm1_install.py PKG.fwsc --verify-only` sobre la unidad.
6. **Instalación controlada**: `fm1_install.py PKG.fwsc`, con la unidad en la mesa, sin tocar el cable.
7. **Corte de comunicación**: desconectar el USB a mitad de la escritura (o `tests/ota_cut_test.c` en
   banco). La unidad debe quedar en modo loader y el instalador debe poder terminar.
8. **Corte de alimentación**: repetir 7 quitando la alimentación, en varios puntos. Anotar qué reporta
   la unidad al volver.
9. **Recuperación**: si no arranca, programador externo con el volcado del paso 2; verificar con
   `fm1_restore.py --compare` antes y después.
10. **Restauración del backup**: escribir el volcado con el programador y volver a comparar.
11. **Verificación byte a byte**: `fm1_restore.py --backup ... --compare` debe dar cero diferencias.
12. **Arranque y audio**: splash, HOME, una nota por cada motor, EQ en los extremos, y una grabación de
    referencia comparada con la de antes de empezar.

Sólo después de 1–12 en una unidad secundaria tiene sentido hablar de instalar en la principal.

## 4. Qué NO está demostrado

- Ninguna copia de seguridad hecha sobre hardware real.
- Ninguna restauración, ni cruda ni por bootloader, en hardware.
- Ningún corte de energía real durante una escritura.
- Ningún secure boot: la firma Ed25519 se verifica **en el host** antes de enviar el paquete. Que el
  bootloader del FM-1 compruebe esa firma no está demostrado y no se afirma.

## 13. Restauración por programador SPI (v1.8.9)

`tools/fm1_restore.py` ya escribe, con un programador externo y nunca por MIDI:

```
fm1_restore.py --backup DIR --write --programmer file:part.nor     # un part simulado en un archivo
fm1_restore.py --backup DIR --write --programmer flashrom:ch341a_spi
```

Borra sector a sector (4 KiB), programa página a página (256 B), relee cada sector, después relee el
part completo y compara. Devuelve 8 si algo no vuelve idéntico, 2 si el programador falla, 5 si no se
indicó programador. El camino `file:` es el que corren las pruebas; el camino `flashrom:` construye la
orden real pero **no se ejecutó sobre hardware**. Antes de usar una unidad: clip SOIC-8 con el MCU en
reset, alimentación del programador (no del FM-1), y un backup independiente verificado dos veces.
