# El emulador: qué se puede probar sin un FM-1

Hay **dos** formas de ver y probar el firmware sin pedal, y conviene no confundirlas:

| | Emulador en el navegador (`web/emu`) | Renders de pantalla en el host (`tests/ui_render.c`) |
|---|---|---|
| Qué es | El **firmware real** compilado a WebAssembly, corriendo en un AudioWorklet contra un FM-1 simulado | El **código de dibujo real** del firmware, ejecutado en el host, escribiendo cada pantalla a PNG |
| Qué prueba | Interacción completa: teclas, botones, encoders, pantalla, LEDs, audio, guardado en flash, MIDI de entrada | Layout y tipografía: lint (nada cortado, solapado ni fuera de su celda), alineación de la tinta al píxel, paletas puras, coste de dibujo |
| Necesita | `emcc` (Emscripten) para construir; luego cualquier navegador con AudioWorklet | `gcc` y Python con Pillow (nada de emcc, nada de navegador) |
| Tiempo | ~1 min de build, arranque instantáneo | ~10 s, 169 pantallas × 14 paletas = 2 366 imágenes + hojas de contacto |

Ninguna de las dos es el hardware: no hay DAC, ni matriz de teclado real, ni USB, ni relojes del chip.
Ver "Lo que no simula" más abajo.

## 1. Cómo funciona el emulador

* `web/emu/felucca_web.c` incluye **las fuentes del firmware** en el mismo orden que
  `firmware/src/felucca.c` (libc, motores, voces, FX, secuenciador, UI, main.c, storage, proyectos) y
  aporta un FM-1 simulado. El firmware no cambia ni una línea para esto.
* El **reloj del dispositivo** avanza con el audio que pide el worklet: los ticks de TIMER4 y `fm1_ms`,
  el ISR de audio real (`audio.c`, `fm1_alnk0_irq`) rindiendo cada mitad de I2S como lo pediría el DMA,
  el bucle principal cada 15 ms y `ui_input` en cada milisegundo entre medias.
* **Panel**: los 14 botones (FX, SCL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC, OCT-, OCT+)
  por sus etiquetas impresas (`panel.c` los mapea a la matriz), los 7 encoders (SELECT, ALGORITHM,
  PRESETS, KNOB 1..4) con su dirección real, el potenciómetro MASTER, las 27 teclas como `fm1_in.notes`
  y los LEDs leídos de vuelta desde `fm1_led` / `fm1_led_dim`.
* **Pantalla**: `lcd_fill` / `lcd_blit` a un framebuffer de 240 × 240 RGB565 (el orden de bytes del
  panel), que la página pinta en un `<canvas>` con `image-rendering: pixelated`.
* **Flash**: una imagen NOR de 1 MiB detrás de los hooks de `storage.c` (ajustes, proyectos, presets de
  usuario, samples). La página guarda esos sectores y los devuelve al arrancar, así que **lo que guardas
  sobrevive a recargar**.
* **Página** (`web/emu/index.html`): SVG del aparato con zonas de pulsación, LEDs con sus estados
  (apagado, atenuado, verde, respiración), teclado del ordenador (flechas para encoders, teclas para
  notas), **Web MIDI de entrada** (`navigator.requestMIDIAccess`) y audio por AudioWorklet.

## 2. Lo que no simula

Del encabezado de `web/emu/felucca_web.c`, literal: **no hay USB** (MIDI, audio, el editor, las
actualizaciones), **ni TRS MIDI**, ni consola serie, ni batería (siempre llena), ni los caminos de
crash / UBOOT. Tampoco hay analógico: el sonido sale del DSP del firmware pero lo reproduce el
navegador, así que **el tiempo real y el coste por muestra no son los del FM-1** (el propio test mide
que va más rápido que tiempo real en el runner, nada más).

En consecuencia: el emulador sirve para **interfaz, flujo, estados, presets, FX, MIDI de entrada y

## 3. Probarlo sin pedal, de la forma más barata

### A. Bajar el emulador ya construido (no compila nada)

Cada push y cada PR sube el emulador como artefacto (`emulator-<sha>`, 14 días):

```sh
gh run download --repo Guillel51/Quantum-GammaWave -n emulator-<sha> -D build/emu
python3 -m http.server -d build/emu 8000        # hace falta un servidor: el navegador no carga
                                                # felucca.wasm ni worklet.js desde file://
```

y abrir <http://localhost:8000/>. Requisitos: cualquier navegador con AudioWorklet y WebAssembly
(Chrome/Edge/Firefox/Safari actuales). Nada de GPU ni de RAM especial: el wasm reserva 32 MiB.

### B. Construirlo tú (unos 300 MiB de toolchain, una sola vez)

```sh
./build.sh                                  # cabeceras generadas (necesita Pillow, numpy, fonttools)
source ~/emsdk/emsdk_env.sh                 # emcc (o docker con emscripten/emsdk)
sh web/emu/build.sh                         # -> build/emu/{felucca.wasm, index.html, worklet.js, fonts/}
python3 tools/emu_dev.py                    # servidor + rebuild al guardar + recarga automática
```

`tools/emu_dev.py` vigila `firmware/src` y `web/emu`: guardas un archivo, recompila y la página abierta
se recarga sola; si el build falla, sigue corriendo el último bueno y muestra el error.

### C. Solo mirar la interfaz (sin emcc, sin navegador, ~10 s)

```sh
gcc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/ui_render tests/ui_render.c -lm
build/host/ui_render build/ui_new build/ui_slot     # + tests/ui_render.py para las hojas en PNG si hay Pillow
```

Deja en `build/ui_new/` una carpeta por paleta con **cada pantalla** en PNG, hojas de contacto
(`sheet_<paleta>.png`) y dos informes: `report.txt` (lint y textos recortados) y `align.txt` (la tinta de
cada texto/icono contra su caja, al medio píxel). Con `-DFELUCCA_FM4=1` hace lo mismo con las pantallas
del motor DIGITAL. **Este es el camino para detectar errores visuales sin instalar el firmware y sin
navegador**, y es el que corre en CI en cada push.

## 4. Qué corre en CI y qué puedes mirar

* `c-cpp.yml` (cada push a `main` y cada PR): toda la suite de host, incluidos los renders de UI
  (lint + alineación + paletas) y los tests web (parser, instalador, editor).
* `emulator.yml` (push/PR que toquen `firmware/`, `web/`, `tools/`, `build.sh`): construye el wasm,
  pasa el smoke test y sube el artefacto; solo `main` despliega a Pages (cuando Pages esté habilitado).
* El smoke test (`web/emu/emu_test.mjs`) comprueba, sobre el wasm real: arranque (splash → HOME, LED de
  HOME, barrido de LEDs de encendido), una tecla suena y enciende su LED, una nota MIDI de entrada
  suena, PLAY arranca el secuenciador, SAVE guarda en la flash y **vuelve en una instancia nueva**,
  dos gestos iguales dan el mismo audio y es **bit a bit igual al build nativo** (`web/emu/native_check.c`),
  y el coste: 1 s de una canción pesada contra tiempo real (`BENCH_MAX`, por defecto 0.5).

## 5. Añadir una prueba nueva

* **Comportamiento** (¿qué hace el firmware al pulsar X?): añade un `check()` en
  `web/emu/emu_test.mjs`. Los exports del wasm (`web/emu/felucca_web.c`) son: `web_boot`, `web_render`,
  `web_buttons`, `web_keys`, `web_enc`, `web_master`, `web_midi`, `web_screen`, `web_screen_draws`,
  `web_lit_buttons`, `web_lit_keys`, `web_dim_buttons`, `web_dim_keys`, `web_dim_level`,
  `web_breath_buttons`, `web_breath_keys`, `web_mid_keys`, `web_anim_levels`, `web_playing`,
  `web_now_ms`, `web_nor` / `web_nor_size` / `web_nor_erase` / `web_flash_writes_count`, `web_seed`,
  `web_out_l` / `web_out_r` / `web_sample_rate`, y dos ayudas de test (`web_test_heavy`,
  `web_test_user_preset`). Corre con `node web/emu/emu_test.mjs build/emu/felucca.wasm [native.f32]`.
* **Visual** (¿cabe el texto? ¿está centrado?): no hace falta nada nuevo: `ui_render` ya recorre cada
  pantalla, página, motor y valor; si cambias el dibujo y algo se mueve un píxel de más, `align.txt` lo
  dice con pantalla, texto y caja.

audio "de código"**; no sirve para medir CPU, latencia, consumo ni comportamiento eléctrico.
