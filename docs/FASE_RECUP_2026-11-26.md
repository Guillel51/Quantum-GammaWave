# Fase 1/4 — recuperación del entorno, builds y CI (2026-11-26)

Clases de medida en todo este documento: **[HOST x86]** pruebas y builds nativos en la VM de este chat,
**[AC791N]** build de destino con el toolchain pi32v2, **[CI]** lo que dicen los workflows de GitHub,
**[leído]** código o configuración, sin ejecutarlo aquí. No hubo unidad FM-1.

## 1. Estado encontrado

| componente | estado medido |
|---|---|
| Emscripten | `~/emsdk/upstream/emscripten/emcc` → **6.0.11** (el pin de `emulator.yml`, `target.yml` y `web/emu/build.sh`), **no en PATH** |
| toolchain pi32v2 | `~/.jieli/toolchain` clang **4.0.1**, pin `a1545cdd…7647bd` (`tools/toolchain.sha256`) |
| SDK AC79 | `~/fw-AC79_AIoT_SDK` completo aquí; CI usa un checkout disperso de 3 archivos |
| build AC791N | **[AC791N]** imagen **583 608 / 583 612 B** (4 B libres), XIP 578 828 B, RAM 75 664 / 98 304 B |
| build WASM | **[HOST x86]** 1 330 741 B, emcc 6.0.11 |
| `origin/main` | `90ed4a82`; ramas de trabajo `wip/ui-huecos-xip` `a97ef021` |

## 2. Causas reales encontradas

### 2.1 `emsdk_env.sh` no funciona bajo `sh` (la suite decía «no emcc» con Emscripten instalado)
**[HOST x86]** `. "$HOME/emsdk/emsdk_env.sh"` con `sh` imprime
`Error: unable to determine 'emsdk' directory` y **no** deja `emcc` en el PATH; con `bash` sí funciona.
`tests/run_tests.sh`, `tests/groups.sh` y `tools/emu_dev.py` asumían que ese sourcing bastaba, así que en un
entorno limpio (dash, sin `emsdk_env.sh` ya sourceado) el grupo del emulador no corría.

### 2.2 El grupo omitido se veía verde, y uno de ellos medía un WASM viejo
`tests/run_tests.sh` imprimía `== skip the browser emulator …` y seguía: verde sin haber medido nada.
Peor: `tests/groups.sh run_emu`, sin `emcc`, **probaba el `build/emu/felucca.wasm` que hubiera** y eso contaba
como pasar. Una omisión no es un resultado.

### 2.3 CI: la caché del toolchain saltaba el pin
`.github/workflows/emulator.yml` hacía `[ -e "$HOME/.jieli/toolchain" ] || tools/get_toolchain.sh`, con
`actions/cache` clave fija `jieli-ac79-v1`. Si la caché ya tenía un toolchain (de un archivo anterior a una
revisión del pin, o de un unpack parcial), **nunca** se ejecutaba la verificación del SHA-256. El pin sólo
aplicaba a una caché fría.

### 2.4 CI: el log que se guarda no contiene el error que importa
`ci_diag.yml` guarda `ci/last-run.log` filtrando por `error:|fatal|FAILED|…` y un `tail -400` global. El
diagnóstico real de un linker (`region 'XIP' overflowed by N bytes`, `cannot find -l…`, una nota del
compilador) no casa con esos patrones y se pierde: del fallo de `emulator` run 45847987837 sólo sobrevivió
`build: pi32v2/bin/ld failed`.

### 2.5 CI: el job `emulator` falla porque no hay paquete
**[CI]** run `45847987837` (`main` @ `90ed4a82`), job `build`:
`the package, the installer and the editor` → `build: pi32v2/bin/ld failed`, y después
`target compiler checks` (exit 2) y `a package and a site exist (fail closed)` → `no package anywhere`.
El paso del paquete es `continue-on-error`, así que el fallo real sólo se ve dos pasos más abajo.
**El error del linker no se pudo leer**: ver 2.4. La misma compilación **[AC791N]** aquí enlaza bien, de modo
que la diferencia está en el entorno del runner (toolchain de caché, ver 2.3) o en el margen de 4 B de XIP.

## 3. Correcciones aplicadas

1. **`tools/emu_env.sh`** (nuevo): `emu_find_emcc` busca en `PATH`, `$EMSDK`, `~/emsdk`, `/opt/emsdk`,
   `/tmp/emsdk` y añade `upstream/emscripten` y `upstream/bin` **directamente**, sin depender de la
   heurística de `emsdk_env.sh`. `emu_emcc_version` da la versión para compararla con el pin.
2. **`web/emu/build.sh`**: usa el helper; si no hay `emcc` lo dice con los cinco sitios que miró.
   **[HOST x86]** desde un shell sin `emcc` en PATH: construye el WASM (1 330 741 B).
3. **`tests/run_tests.sh`**: `skip` explícito con contador; resumen final de grupos **NO EJECUTADOS**, y
   `STRICT_EMU=1` convierte un grupo del emulador omitido en **fallo**. Usa el helper en vez del sourcing roto.
4. **`tests/groups.sh`**: `run_emu` sin `emcc` devuelve **2 (omitido)**, nunca prueba un WASM viejo.
5. **`tools/get_toolchain.sh`**: idempotente y verificado. Deja `DEST/toolchain.stamp` con el SHA-256 del
   archivo y no descarga nada si el stamp coincide con el pin; un `DEST` sin stamp (caché vieja, unpack
   parcial, symlink a mano) se reinstala. Descomprime al lado y hace *swap* al final: un unpack fallido no
   deja el directorio sin toolchain. **[HOST x86]** verificado: con stamp correcto imprime
   `already installed from the pinned archive … nothing to download` y sale 0.
6. **`.github/workflows/emulator.yml`**: siempre pasa por `get_toolchain.sh` (ahora barato y verificado) e
   imprime la identidad del compilador y el stamp en el log.
7. **`.github/workflows/ci_diag.yml`**: sección nueva que guarda 25 líneas antes y 8 después de cada marcador
   de fallo (`::error`, `build: … failed`, `overflowed`, `undefined reference`, `cannot find`, `No such file`).

## 4. Pruebas ejecutadas en esta fase

- **[HOST x86]** `sh web/emu/build.sh` sin `emcc` en PATH → WASM 1 330 741 B, `manifest` refrescado.
- **[HOST x86]** `tools/get_toolchain.sh` sobre un `DEST` con stamp válido → no descarga, exit 0.
- **[HOST x86]** `. tools/emu_env.sh && emu_find_emcc` → 6.0.11.
- **[AC791N]** `./build.sh` → imagen 583 608/583 612 B, XIP 578 828 B, RAM 75 664/98 304 B.
- `sh -n` / `bash -n` sobre los scripts tocados; `yaml` de los workflows revisado a mano (PyYAML no está
  instalado en la VM).

## 5. Pendiente en esta fase

- **`cloud_hot_test` (CLOUD driven hot)**: falla la comprobación «CLOUD's line never clamps». Medido
  **[HOST x86]**: con los defaults (SIZE 90, send 40 000) la línea tiene 33 muestras en **−32768**.
  `rv_knee` no puede llegar al riel (su tope es ≈32 440, `TANH_Q15[256] = 31 588`), así que el valor entra
  por otra escritura que no se identificó todavía; llenar `cloud_buf` con un centinela antes del render no
  cambia el conteo. Necesita instrumentación dedicada; **no** se tocó ningún umbral.
- **Margen XIP de 4 B**: cualquier deriva de generación o de toolchain rompe el link. Los recortes medidos
  no dan bytes sin tocar contenido visible (§ informe del 25/11: `ui_shapes.h` no entra en la imagen, las
  celdas de iconos de 12 px se dibujan todas).
- **Resto de la lista de CI** (`themes`, `UI pixels`, `PRESETS`, `UBSan strict`, tablas `exp2`/`sine`,
  ST7789): no reproducidos en esta ronda; la suite completa no entra en la ventana de 5 minutos de este
  entorno y se corrieron sólo las pruebas afectadas por los cambios.
- El primer run de CI tras este push dirá si la causa de 2.5 era el toolchain cacheado; si el link vuelve a
  fallar, el nuevo `ci/last-run.log` traerá por fin el diagnóstico del linker.
