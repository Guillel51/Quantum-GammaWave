# Accesibilidad: FM-1 Screen Reader (issue #105)

Quantum GammaWave habla con el **FM-1 Screen Reader** de Charles Vestal
([charlesvestal/fm1-reader](https://github.com/charlesvestal/fm1-reader)) por el protocolo que ese proyecto
publica (`PROTOCOL.md`, versión 1). La implementación del firmware es propia
(`firmware/src/announce.c`), escrita contra esa especificación: no es una copia de `fm1-base/device/announce.c`
(MPL-2.0), así que el árbol sigue siendo GPL-3.0-only sin código de otra licencia.

## Qué manda el instrumento

SysEx por USB-MIDI: `F0 7D 53 52 <cmd> … F7` (`7D` = id de fabricante no comercial, `53 52` = "SR"). Todos
los bytes entre `F0` y `F7` son de 7 bits, así que cualquier otro equipo del mismo puerto lo ignora.

| Dirección | cmd | Significado |
|---|---|---|
| lector → equipo | 1 HELLO ver | empieza o sigue escuchando (se repite cada ~2 s; a los 5 s sin HELLO el equipo deja de hablar) |
| lector → equipo | 2 BYE | deja de escuchar |
| lector → equipo | 3 WHERE | di todo lo que hay en pantalla ahora |
| equipo → lector | 1 HELLO ver, nombre, 0, enlace | respuesta a cada HELLO (nombre: `Quantum GammaWave v1.4.17`) |
| equipo → lector | 16 SAY flags, texto | lo que hay que decir; flag 1 = corta lo que se está diciendo, 2 = aviso (se oye entero) |

El enlace a la lista de palabras (`ANN_WORDS_URL`) va **vacío** en esta entrega: el lector aplica entonces sus
reglas comunes (números, `dB`, `%`, `C#3`, mayúsculas). Publicar un `words.tsv` propio es trabajo de la fase
siguiente; el protocolo lo permite y la lectura funciona sin él.

## Qué se anuncia hoy

- **La pantalla, al cambiar**: `HOME MAIN`, `VISUALS`, `OVERVIEW`, `MIXER`, `PERFORMANCE`, `MENU`, `CONFIRM`
  y el título propio de cada página (por ejemplo `ALGORITHM`, `PRESETS`, `FX`).
- **El valor de una tarjeta de HOME mientras se gira su perilla**: etiqueta + valor + unidad, dicho cuando el
  valor lleva ~120 ms quieto (y como mínimo cada ~480 ms si se sigue girando), que es lo que pide el protocolo.
- **El sonido cargado**: `ANALOG BRASS`, `FM6 TINE EP`, … al elegir preset (incluye el motor).
- **Los mensajes de la barra superior** (guardados, avisos, errores); los que llevan el icono de "sin archivo"
  se mandan como aviso (KEEP) para que no los corte el mensaje siguiente.
- **La fila de MENU, con su tab, su nombre y su valor** (v1.5.20): al moverse la selección, y su valor cuando el
  giro se asienta (250 ms), una vez por giro y no por paso.
- **WHERE**: al abrirse el lector se reanuncia la pantalla actual — y, con MENU abierto, también su fila.

## Lo que **no** se anuncia todavía (limitaciones reales)

1. ~~**Filas de MENU**: el nombre y el valor de la fila seleccionada no se dicen aún.~~ **Hecho en v1.5.20**: el
   accessor que faltaba existe (`menu_items.c`, `menu_ann_row`), y la fila se dice con su tab, su nombre y su valor
   (`DISPLAY COLOR QUANTUM`, `CONTROL ANIM OFF`, `AUDIO TRACKS 6 FULL`). Al moverse la selección se dice en el acto;
   al girar la perilla de la fila, una sola línea cuando el valor lleva 250 ms quieto (nunca un mensaje por paso).
   WHERE la repite. `ui_render --announce` lo comprueba por el camino real (ann_ui → announce.c), no por la función
   aislada.
2. **Perillas de páginas que no son HOME**: solo HOME tiene el concepto de "tarjeta que se acaba de girar"
   (`ui.hot_t`/`ui.hot_col`). En EDIT/SEQ/FX el valor no se lee todavía.
3. **Listas y selección de pasos**: sin anuncio por ítem.
4. **Sin archivo de palabras propio**: los nombres se dicen tal cual están en pantalla (en mayúsculas el lector
   las convierte a palabras normales). Un `words.tsv` con las abreviaturas propias (`REV`, `CHR`, `DLY`,
   `PRG`, `VL`) es la fase siguiente.
5. **Emulador / Web Studio**: el camino USB-MIDI del emulador no transporta este SysEx; la accesibilidad se
   probó en el firmware de host (protocolo y empaquetado USB-MIDI), no a través del emulador.

## Qué cuesta

- Nada mientras no haya un lector: `ann_on()` es la primera comprobación de cada entrada y no se formatea ni se
  envía ninguna cadena. Un DAW en el mismo puerto no ve un solo byte.
- RAM: la estructura del protocolo (una línea en espera de 120 B, dos temporizadores, el estado) ≈ 150 B, más
  el anillo SysEx de salida (64 paquetes USB-MIDI = 256 B) que ya existía para M-UPGRADE. Ninguna reserva
  dinámica, ningún acceso a flash, ninguna espera: si el anillo no tiene sitio, el mensaje se descarta (el
  protocolo lo pide así).
- Audio: ni una llamada nueva desde el ISR. El envío ocurre en el bucle principal (`ep1_tx`, SysEx antes que
  cualquier nota) y el HELLO/BYE/WHERE se parsea donde ya se parseaba el SysEx de M-UPGRADE.

## Cómo probarlo

```bash
python3 -c "import sys; sys.path.insert(0,'tools'); import build; build.generate()"
cc -O1 -w -Ifirmware/src -o build/announce_test tests/announce_test.c && ./build/announce_test
```

`tests/announce_test.c` (host) mide el protocolo completo: silencio antes de HELLO, la respuesta con nombre y
versión, SAY/WHERE/BYE, el temporizador de 5 s, el corte a palabra completa y el **empaquetado USB-MIDI**
(CIN 4/5/6/7) que usa `usb.c`. En una FM-1 con esta versión: abrir
[charlesvestal.github.io/fm1-reader](https://charlesvestal.github.io/fm1-reader) en Chrome/Edge, conectar y
pulsar Connect; el lector debe anunciar `Quantum GammaWave v…` y la pantalla actual. **Eso no se pudo probar
aquí: no hubo unidad ni lector conectado.** La prueba con el lector real y con hardware queda pendiente y así
se registra en `docs/VALIDATION_MATRIX.md`.

## Alternativas autónomas estudiadas

- **Tonitor** y **Bigorna** llevan su propio lector dentro del firmware: hablan por la salida de auriculares y
  no necesitan computadora (se activan con una combinación de teclas). Dependen de que el firmware sintetice
  voz, lo que en la FM-1 significa ocupar el DAC y el motor de audio con habla, o añadir un sintetizador de
  voz. Para esta entrega se eligió el camino del Screen Reader (protocolo público, sin coste de audio y sin
  ocupar la salida); Tonitor/Bigorna quedan como alternativa documentada, no implementada.
