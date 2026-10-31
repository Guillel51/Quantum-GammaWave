# Etapa 3 — informe (estabilidad, CI, UI)

Base: `cline/13ey37k1` (etapa 2, 6 commits). `main` no se ha tocado: sin merge, sin release, sin flasheo.

## 1. CI: qué se ejecuta ahora y qué no

**El cambio de workflow sigue sin poder subirse desde aquí**: el token de este entorno es una GitHub App
sin permiso `workflows`, y GitHub rechaza cualquier push que cree o modifique
`.github/workflows/emulator.yml`. El cambio está en el árbol de trabajo y en `docs/ci-stage2.patch`
(80 líneas); se aplica con `git apply docs/ci-stage2.patch` y se sube a mano.

Contenido del cambio:

* **El presupuesto DSP y el parser `.fwsc` corren también en los PR.** Estaban detrás de
  `if: github.ref == 'refs/heads/main'`, así que un PR podía crecer un bucle de render o romper el
  lector del paquete y solo `main` se enteraba, después del merge. Ahora la toolchain (cacheada) se
  descarga también en PRs, sigue con `continue-on-error` (un fallo de gitee salta las comprobaciones
  en vez de tumbar el job) y un solo paso ejecuta las dos: construye el paquete **solo si no está**
  (en `main` ya lo acaba de construir el paso del sitio: no hay build duplicado).
* **El filtro de rutas vigila también `tests/target_budget.txt` y `tests/target_budget.py`**: antes,
  editar el presupuesto no disparaba el job que lo aplica.
* **Preflight de Pages**: `deploy-pages` falla con 404 cuando Pages no está habilitado (comprobado:
  `GET /repos/Guillel51/Quantum-GammaWave/pages` → 404). El job ahora lo dice con esas palabras en
  vez de dejar un 404; el arreglo real es un ajuste del repositorio (Settings → Pages → Build and
  deployment → Source: GitHub Actions), no una modificación del emulador.

Cobertura de las pruebas que pediste, verificada leyendo `tests/run_tests.sh` y ejecutándolas aquí:

| Prueba | Dónde corre automáticamente |
|---|---|
| Parser `.fwsc` (27 comprobaciones) | `c-cpp.yml` (job de host) y el job del emulador con el paquete real |
| DICE ALL (suite + 50 tiradas seguidas) | `c-cpp.yml` |
| Instalador (CLI, SHA-256 obligatorio, `--verify-only`) | `c-cpp.yml` |
| Memoria / bounds (ASan + UBSan en todos los tests, dos fuzzers) | `c-cpp.yml` |
| Compilación del firmware (pi32v2) | job del emulador (cualquier PR con toolchain) y `main` |

## 2. Interfaz: `ui_render` en verde

El paso estaba en rojo **antes** del PR #1 (medido en `e06bde2`: 2 hallazgos de lint y 148 medidas
desalineadas de 2 491 449) y el PR lo empeoró (6 y 1830 de 2 525 677). Tres defectos reales, ninguno
de umbral:

1. **Valor de tarjeta recortado.** En PRESETS, "111" + "/133" necesitaba 50 px de los 49 que deja la
   tarjeta de 57 px, y `cv_text_fit` recortaba un texto que no es libre ("1…"): un hallazgo de lint,
   no un problema de layout. La separación valor/unidad pasa de 3 a 2 px y, si aun así no caben los
   dos, se queda el valor entero y la unidad desaparece (el valor es lo que importa). 6 → 0 hallazgos.
2. **Caja de alineación declarada para un icono que no se dibuja** (células de los carriles punch-in,
   `lcell()`): el arnés emparejaba esa caja con lo siguiente que se dibujaba, 23 px más abajo. La caja
   se declara solo cuando el icono existe.
3. **Etiqueta centrada por mayúsculas y no por tinta**: una etiqueta con '#' (G# C# F#, las teclas de
   los FX) baja 2 px por debajo de las mayúsculas y quedaba baja en su celda de 17 px. Ahora se centra
   por su tinta (la altura de la caja de tinta es `y1 - y0`, no `y1 - y0 + 1`).
4. La "A" del chip AUTOMATION quedaba 1 px a la derecha del centro de su chip (3 px desde el borde):
   centrada.

Medido, antes → después (mismo arnés, sin tocar el umbral de 1 px):

| Build | Lint | Desalineadas | Salida |
|---|---|---|---|
| por defecto | 6 → **0** | 1830 de 2 525 677 → **0** de 2 522 877 | 1 → **0** |
| `FELUCCA_FM4=1` | 8 → **0** | 1874 de 2 901 504 → **0** de 2 898 704 | 1 → **0** |

Comprobado después del cambio: `ui_test` entero (`ui: all passed`), golden renders 151 / 0 cambiados,
0 fallos de salud, voces o routing, 0 entradas de CPU fuera de presupuesto.

## 3. Toolchain: checksum verificable

`tools/get_toolchain.sh` descargaba el archivo directamente a `tar`: no se podía comprobar nada y
nadie decía con qué compilador se había construido. Ahora descarga a un temporal, calcula el SHA-256 y:

* con `JIELI_TOOLCHAIN_SHA256` definido (el CI lo lee de la variable de repositorio del mismo nombre),
  un archivo distinto se rechaza **antes de descomprimir**, con los dos hashes en el mensaje;
* sin él, imprime el hash obtenido para poder revisarlo y fijarlo. La URL sirve la última compilación,
  así que un pin solo aguanta hasta que JieLi la reemplace: entonces el job falla en voz alta.

Hash medido aquí del archivo actual (`jieli-linux-toolchains-20260730.1`):
`a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd`.
Verificado: sin pin imprime ese valor y descomprime; con ese valor fijado verifica y descomprime; con
un valor erróneo falla y **deja el destino vacío**. `BUILDING.md` lo dice (CRLF conservado) y el paso
del workflow que pasa la variable va en el parche.

Los tres archivos del SDK ya se verificaban por SHA-256 en `tools/build.py` (`SDK_SHA256`).


## 4. Estabilidad y rendimiento: qué se revisó

**Memoria (medida con `llvm-nm` sobre `build/felucca.elf`, no estimada).** Los mayores consumidores
en RAM, en bytes: `dly_buf` 131 072 (la línea de delay), `cv_px` 59 528 (el lienzo de dibujo),
`sl_buf` 32 768 (SLICER), `cloud_buf` 32 768 (CLOUD), `proj_slot` 15 360, `comb_buf` 12 288,
`rev_comb` 9 874. Totales del enlazador: `.data+.bss` 91 156 de 98 304 (7 148 libres, 7,3 %), código
en RAM 21 244 de 24 576 (3 332 libres), pool 311 240 con 9 248 de margen. Las tablas grandes de
fuentes (`AF_S_DATA`, `AF_M_DATA`) están en XIP, no en RAM. **No he tocado el reparto**: los buffers
grandes son de audio y compartirlos entre motores cambiaría el sonido o provocaría un fallo difícil
de probar sin hardware; queda como propuesta, no como cambio.

**Estado al arrancar.** En hardware `crt0` pone `.bss`/`.data` a cero (solo `.noinit` se conserva:
`crash_seen`), y `felucca_init()` reinicia globals, pistas, sonidos y FM6; el emulador estrena memoria
por instancia. Es decir: efectos, matriz de modulación, carriles punch-in y SLICER empiezan a cero por
construcción, no por un `memset` en `felucca_init`. Lo que sí se puede heredar es lo que los tests
dejan en su propio proceso, y eso lo cubren `ui_power_on()` (GEN, etapa 2) y `startup_test.c` (el
arranque real con el emulador, incluida la restauración de la sesión anterior).

**Bloqueos.** `console.c` (`con_putc`) espera como máximo 20 ms y alimenta el watchdog: correcto.
`editor.c` espera el fin de la reproducción con un tope de 100 ms. Los únicos bucles sin cota natural
son los de hardware en `firmware/hal/`: `fl_cache_idle()` (`while (!(CACHE_CON & 0x4000u))`), la espera
de SFC inactiva y el handshake de P33 en `fm1_sys.h`; si el periférico no responde, el firmware se
queda ahí (el watchdog lo reinicia). **No los he tocado**: un timeout en una escritura de flash que
abandone a medias puede corromper más que un reinicio, y sin hardware no puedo validar el cambio; queda
documentado como riesgo y como propuesta (contador con abandono seguro, probado en el emulador).

**Bucles de DSP revisados** (`eng_noise.c` `noise_inc`, `eng_lofi.c` el divisor por muestra,
`eng_wheel.c` el plegado del drawbar): todos convergen y están acotados por el rango de parámetros;
UBSan no ve nada en la suite (que los recorre con presets y barridos).

**MIDI / presets / FX.** La suite incluye cola de MIDI con desbordamiento, SysEx malformados, UART,

## 5. Pruebas ejecutadas y resultados reales

| Prueba | Comando | Resultado |
|---|---|---|
| Suite completa de host, con toolchain y paquete | `tests/run_tests.sh` | **`ALL HOST TESTS PASSED`, exit 0** (antes: 2 pasos en rojo, `ui_render` y el parser) |
| Renders de UI, build por defecto | `build/host/ui_render build/ui_new build/ui_slot` | **exit 0**, 0 lint, 0 desalineadas de 2 522 877 |
| Renders de UI, `FELUCCA_FM4=1` | `build/host/ui_render_fm4 ...` | **exit 0**, 0 lint, 0 de 2 898 704 |
| Parser `.fwsc` | `node web/test_pkg_bounds.mjs [build/felucca.fwsc]` | 27 ok (+1 con el paquete real) |
| DICE ALL, 100 tiradas seguidas | `UI_REPEAT_DICE=100` | 700 comprobaciones ok |
| Presupuesto DSP | `python3 tests/target_budget.py` | 38 líneas ok, exit 0 |
| Instalador CLI | `python3 tests/install_test.py` | pasa |
| Toolchain, los tres caminos | `tools/get_toolchain.sh` sin pin / con pin / pin erróneo | imprime y descomprime / verifica y descomprime / falla y no descomprime |

### Qué dijo la suite completa (esta vez sí, entera)

* Golden renders: **151, 0 cambiados**, 0 fallos de salud, 0 de voces/routing (14 129 eventos), 0
  entradas de CPU fuera de presupuesto.
* Presupuesto de target: 38 líneas ok, `DRUM path 1189 (budget 1191)`.
* `ui_test` completo y `dice_all x50`; ASan/UBSan en todos los tests y los dos fuzzers.
* Instalador CLI, tests web, backup web y parser `.fwsc` con el paquete real: todos ok.
* Build del firmware: `.data+.bss` 91 156 de 98 304, código en RAM 21 244 de 24 576 (igual que antes
  del cambio de UI), pool con 9 248 de margen, XIP +48 B por el centrado nuevo.

## 6. Lo que sigue pendiente (y lo que requiere hardware)

1. **El parche del workflow hay que aplicarlo y subirlo a mano** (`docs/ci-stage2.patch`): el token de
   este entorno no puede modificar workflows. Hasta entonces, el presupuesto y el parser no corren en
   PRs.
2. **Pages sigue sin habilitarse** en el repositorio (404 en la API): es un ajuste de Settings, no código.
3. **`JIELI_TOOLCHAIN_SHA256` hay que fijarlo** como variable del repositorio con el valor medido
   (§3); mientras esté vacío, el script avisa pero no bloquea.
4. **Sin probar en hardware**: los FX punch-in (VIB/DIVE/GATE/SWEEP), los carriles 11/12 en firmware
   antiguo, el instalador, el backup/restore, y el arranque tras un corte de energía. Nada de lo escrito
   arriba dice que algo suene o funcione en un FM-1.
5. **CPU real con carga máxima** (8 voces + batería + FX + reverb): no medida. Lo único medido es el
   conteo estático del compilador del objetivo y el reloj del host/emulador, que no son el dispositivo.
6. **Margen de RAM**: 7,3 % en `.data+.bss` y 13,6 % de código en RAM. No hay sitio para funciones
   grandes nuevas sin quitar algo; compartir buffers de audio es la única vía clara y exige pruebas de
   audio y de hardware.
7. **Firma Ed25519**: sigue siendo diseño (`docs/PHASE0_SECURITY.md`). No hay claves, y la verificación
   no está en el camino de instalación.
8. **Esperas de hardware sin cota** en `firmware/hal/` (flash, SFC, P33): documentadas, no cambiadas.

## 7. Qué queda preparado para la siguiente etapa

* El job de host debería quedar verde (parser, DICE ALL, instalador, memoria/ASan, UI) cuando el
  parche del workflow esté aplicado y `JIELI_TOOLCHAIN_SHA256` fijado.
* `ui_render` es ahora una red de seguridad real: cualquier desplazamiento de 1 px o texto recortado
  falla, y el arnés mide 2,5 millones de posiciones por ejecución.
* La toolchain se puede fijar sin tocar código (variable de repositorio) y el script dice qué hash vio.
* Queda listo el terreno para: habilitar Pages y verificar el despliegue; implementar la firma con el
  orden de `docs/PHASE0_SECURITY.md`; y medir CPU/RAM en un FM-1 real, que es lo único que puede
  validar los márgenes.

`fuzz_ed` (20 000 tramas) y `fuzz_proj` (5 000 iteraciones de store → import → restore → render), y
todos pasan con ASan/UBSan. No he encontrado un fallo reproducible que justifique tocar el protocolo.

| Compilación a WASM + smoke test | job del emulador |

## 8. Commits de esta etapa (rama `cline/13ey37k1`)

```
a3ec912 docs: the workflow patch now also runs the DSP budget and the .fwsc reader on PRs
63c871d UI: ui_render goes green (0 lint findings, 0 misaligned of 2.5M); cards, FX cells, AUTOMATION chip
2aea384 toolchain: the JieLi archive is verified by SHA-256 (JIELI_TOOLCHAIN_SHA256), and the hash is printed
```

El cambio de `.github/workflows/emulator.yml` no está en ningún commit: viaja en
`docs/ci-stage2.patch` porque este entorno no puede subir workflows.
