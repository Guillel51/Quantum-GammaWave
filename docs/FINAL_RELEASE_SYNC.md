# Sincronización final de release — Quantum GammaWave v2.9.0

Estado de la cadena **código → compilación → firmware → WASM → JavaScript → emulador → sitio →
instalador → release**, con la clase de cada comprobación y lo que queda pendiente. Escrito el
2026-11-12.

> **Cómo leer este documento.** Cada fila dice **compilado**, **medido en host x86**, **emulador**,
> **build AC791N**, **red** o **PENDIENTE**. Nada de lo que sigue se midió en una unidad física: el FM-1
> del dueño no se escribe desde este repositorio (AGENTS.md §3).

---

## 1. Versión final

| | |
|---|---|
| Versión | **v2.9.0** (`FELUCCA_VERSION` en `firmware/src/felucca.c`) |
| Commit del código | **`0f40f7cccafad6037e27165b3de6b9b5c0ffa55a`** (`0f40f7c`) |
| Commit de la entrega | el que añade los artefactos de `release/` y este informe (su hash está en `git log`) |
| Identidad | **`FM-1_965`**; loader `ota-FM-1_965` |
| Paquete | `release/felucca-v2.9.0.fwsc`, **610 959 B**, sha256 **`edd2a23f59d513c20c286f11b0f55d0b75519fe7a24afe63bba1dd7334b4ac1b`** |
| Imagen de la app | `felucca.bin`, 574 984 B, sha256 `f2cfa6637e50a86589dfbe5e845590b76945db06170eb67639cb1e1f7eece582` |
| Loader | `loader/ota.bin`, 7 803 B, sha256 `3ca785b93edaabe9fe383c777a0a9507b0d32e10977574731af65982639c33e1` |
| Emulador | `build/emu/felucca.wasm`, 1 196 427 B, sha256 `8565c2ec0516cc406a6f1ceeddeaef50b02fda5949ee4e3660d02ed5da80cc37` |
| Registro de build | `release/build-manifest-v2.9.0.json` (`qg1-build-1`, árbol **limpio**, commit `0f40f7c`) |
| Manifiesto de release | `release/felucca-v2.9.0.fwsc.manifest.json` (`qg1-release-1`, `keyid: ""` — sin firma Ed25519, desde 2.8.0) |

**La cadena de esta entrega se construyó acá, con las mismas entradas que la release anterior**, no con
un sustituto:

```
toolchain  jieli-linux-toolchains-20260730.1  clang c45189ee624d…  (pin sha256 verificado al bajar)
SDK        fw-AC79_AIoT_SDK @ 0617f4cc09685332aaf1104893718291c54ee5bb
           uboot.boot 64ccc63e… · cfg_tool.bin 27657995… · cfg/eq_cfg_hw.bin 179ac9f4…
emcc       Emscripten 6.0.11 (a0014542110d6078c3a1a7941fa1ddb3a2281f16)
```

Los tres blobs del SDK y el nombre de la toolchain son **idénticos** a los que declara el manifiesto de
v2.8.4: el paquete de esta versión sale de las mismas entradas que el anterior, con el árbol de hoy.

## 2. Cambios integrados

Las cinco fases, agrupadas como las agrupa el `CHANGELOG.md` de esta versión:

1. **Optimización medida** — memoria, `.ram_text`, caminos calientes (`docs/QG_PHASE_1_OPTIMIZATION.md`).
2. **Interfaz** — HOME y sus vistas, medidas en el render de host y en el emulador (`docs/QG_PHASE_2_UI.md`).
3. **Motores** — WAVETABLE como motor propio; nueve vivos (`docs/QG_PHASE_3_ENGINES.md`).
4. **Herramientas musicales** — FLANGER, HOME **9/9** con ROUTING, una balística de medidores
   (`docs/QG_PHASE_4_MUSICAL_TOOLS.md`).
5. **The Firmware Hub** — tarjeta validada, identidad leída de los bytes, job de atestación
   (`docs/FM1_HUB_READINESS.md`, `docs/FM1_HUB_AUDIO_PROVENANCE.md`).

Y la sincronización de esta entrega, que es el objeto de este informe:

* **El portal dejó de mentir sobre sí mismo**: decía `v2.7.2`, ocho motores (WAVETABLE es el noveno desde
  la fase 3), 17 paletas (son 22) y un conteo de presets que ninguna herramienta del árbol puede verificar.
  Ahora la versión llega del despliegue (`{{VERSION}}` → `versions.json`), la lista de motores está
  completa y los dos conteos que quedan son los que el código declara.
* **La advertencia de instalación dice lo que es**: los manifiestos van **sin firma Ed25519 desde 2.8.0**
  (integridad por SHA-256 + tamaño y trazabilidad del build, no autenticidad criptográfica).
* **Capturas nuevas del código de esta versión**: `tools/generate_screenshots.py` ahora también renderiza
  el navegador de presets, la página del motor, la matriz de modulación, el desk, los ajustes en vivo y la
  novena vista de HOME (ROUTING); `tools/fw_shots.py` compone con ellas las imágenes del portal
  (`fw-routing.png`, `fw-matrix.png`, `fw-browser.png`).
* **Artefactos de release generados desde este árbol** (§1) y **espejo `site/`** regenerado con
  `web/make_site.py` a partir del paquete de release.

## 3. Archivos e imágenes actualizados

| Qué | Dónde |
|---|---|
| Versión del firmware | `firmware/src/felucca.c` (`FELUCCA_VERSION "v2.9.0"`) |
| Textos, metadatos y advertencias del portal | `web/site/index.html` (y su salida en `site/index.html`) |
| Instalador (modelo de manifiesto sin firma) | `web/index_pkg.html` |
| Capturas del firmware (12 actualizadas, 6 nuevas) | `assets/screenshots/quantum_*.png`, `assets/ui-art/art/` |
| Imágenes del portal (9) | `web/site/assets/fw-*.png` |
| Generadores de capturas | `tools/generate_screenshots.py`, `tools/fw_shots.py` |
| Registro, manifiesto y checksums | `release/build-manifest-v2.9.0.json`, `release/felucca-v2.9.0.fwsc{,.manifest.json}`, `release/SHA256SUMS` |
| Emulador publicado en el espejo | `site/emulator/` (wasm + su `manifest.json`) |
| Portal, instalador y editor publicados en el espejo | `site/index.html`, `site/webapp/installer/index.html`, `site/webapp/editor/index.html`, `site/firmware/` |
| Notas | `CHANGELOG.md` (entrada v2.9.0), este informe, `docs/FM1_HUB_READINESS.md` |

No se borró documentación ni créditos: `LICENSES/`, `LICENSING.md`, `LICENSE`, los avisos OFL/MIT y el
linaje (Felucca · JIANT FM · Pocket GammaWave) siguen donde estaban, y el portal los enlaza.

## 4. Estado del repositorio

* `main` = commit de esta entrega, con la versión **v2.9.0** en el código, en el paquete, en el manifiesto
  y en el espejo del sitio.
* Árbol limpio al construir (`dirty: []` en el registro de build).
* `release/` conserva los paquetes anteriores y añade el de esta versión con su registro y su manifiesto;
  `release/SHA256SUMS` se regeneró sobre el contenido real del directorio.

## 5. Estado de la web

La página publicada se construye en CI (`emulator.yml`) desde `release/`: la raíz sirve **el paquete de
release más nuevo** y el emulador **compilado en el commit de ese manifiesto**; `/preview/` sirve `main`.
Con este commit, la raíz pasa a servir `v2.9.0` desde `0f40f7c`.

* Verificado acá: el espejo `site/` que genera `web/make_site.py` declara `"version": "2.9.0"` en el
  portal, en el instalador y en el editor, y `firmware/versions.json` encabeza 2.9.0 con su SHA-256.
* **Pendiente de confirmar**: que `https://guillel51.github.io/Quantum-GammaWave/` sirva esa versión,
  lo que sólo se puede leer después de que corra el workflow de Pages para este commit.

## 6. Estado del instalador

* El instalador del espejo lleva incrustado el metadato real del paquete de release: `version 2.9.0`,
  `product FM-1_965`, `sha256 edd2a23f…`, `size 610959`, y el registro de build completo.
* `web/make_site.py` **se niega** a publicar un paquete cuyo SHA-256 no sea el de su registro o cuya
  identidad no sea `FM-1_9xx` y no traiga el loader de Felucca: el paquete de esta entrega pasó esa puerta.
* El instalador verifica SHA-256 + tamaño antes de habilitar INSTALL, y el loader lee el chip real antes
  de borrar (allowlist `0x856014` / `0xB36014`).
* **Lo que no se puede afirmar**: que la instalación funcione en una unidad. Eso exige las pruebas
  físicas (§10).

## 7. Estado del emulador

* `build/emu/felucca.wasm` se compiló desde este árbol con Emscripten 6.0.11 y lleva en su manifiesto
  `v2.9.0`, commit `0f40f7c` y su SHA-256.
* Prueba de humo del emulador (`web/emu/emu_test.mjs build/emu/felucca.wasm`): **pasa entera** — arranca,
  respira, guarda un preset de usuario con sus sectores, lo recupera desde una instancia nueva, reproduce
  la canción pesada dos veces con las mismas muestras y mide **2,6 % de tiempo real**.
* Sin caché que pueda mentir: el sitio publica el wasm con el nombre del build y su `manifest.json`, y
  `tools/site_check.py` (en CI) compara commit del manifiesto y bytes contra el origen publicado.

## 8. Estado de la release

* El paquete, su registro y su manifiesto están en `release/` **antes** de etiquetar: el tag `v2.9.0`
  publica desde el código que produjo esos bytes.
* La release de GitHub la crea el workflow compartido `zvenson/fm1-build/.github/workflows/build.yml`
  (pineado al commit del tag `v1`), que compila, firma la atestación y sube el `.fwsc`; el job `verify`
  del mismo workflow descarga esos bytes y corre `gh attestation verify` contra el workflow firmante.
* **Pendiente de confirmar**: que el asset publicado tenga el mismo SHA-256 que el paquete construido acá
  (`edd2a23f…`). Las entradas son las mismas (toolchain, SDK, emcc y árbol limpio), así que la
  reproducibilidad es esperable, pero **no se declara verificada hasta leer el asset**.

## 9. Verificaciones ejecutadas (2026-11-12, en este entorno)

| Comprobación | Clase | Resultado |
|---|---|---|
| `python3 tools/build.py` (target AC791N) | build de destino | imagen 574 984 B; RAM `.data+.bss` 87 760 / 98 304 B; pool spare 15 392 B; RAM code 19 588 / 24 576; paquete 610 959 B |
| `tools/check_package_identity.py build/felucca.fwsc` | bytes del paquete | `FM-1_965`, loader `ota-FM-1_965`, sin `015`, rc=0 |
| `sh web/emu/build.sh` + `web/emu/emu_test.mjs` | emulador | wasm 1 196 427 B; arranque, audio, guardado y recuperación ok |
| `python3 tests/install_test.py` | host | pasa (se salta la restauración V15: no hay `FM-1_v15.fwsc` en la raíz) |
| `node web/test_web.mjs` | host | pasa |
| `node web/test_backup.mjs` | host | pasa |
| `node web/test_pkg_bounds.mjs` | host | pasa |
| `node web/test_provenance.mjs` | host | pasa |
| `web/make_site.py` sobre el paquete de release | generación del sitio | portal + instalador (247 176 B) + editor + `firmware/felucca-2.9.0.fwsc` con su registro |

**No ejecutado acá**: la suite completa `NO_PACKAGE=1 ./tests/run_tests.sh` (13–15 min, no cabe en el
límite de 5 minutos por comando; corrió entera en la fase 4 sobre el código base y este cambio no toca
DSP ni UI), `gh attestation verify` (el `gh` de este entorno no tiene el subcomando y no hay sesión) y
cualquier prueba sobre hardware.

## 10. Problemas pendientes y por qué

| Pendiente | Motivo | Qué lo cierra |
|---|---|---|
| Confirmar el asset de la release y su atestación | se publica en CI al empujar el tag; la atestación no se puede verificar acá | el job `verify` en el tag `v2.9.0`, o `gh attestation verify` desde una máquina con `gh` y token |
| Que Pages sirva v2.9.0 en la raíz | depende del workflow de Pages para este commit | el chequeo de `tools/site_check.py` que corre al final del deploy |
| Instalación desde V15, desde otro firmware, salida hacia otro, identidad USB, interrupción/reanudación | exigen una unidad física y el FM-1 del dueño no se escribe | las cinco pruebas de `docs/FM1_HUB_READINESS.md` §7 |
| JEDEC real de la unidad | no hay lectura de hardware | `docs/FLASH_READER.md` sobre una unidad |
| Listado en The Firmware Hub | el Hub decide; no hay confirmación oficial | respuesta de Sven |

## 11. Enlaces

* Repositorio — <https://github.com/Guillel51/Quantum-GammaWave>
* Portal — <https://guillel51.github.io/Quantum-GammaWave/>
* Emulador — <https://guillel51.github.io/Quantum-GammaWave/emulator/>
* Instalador — <https://guillel51.github.io/Quantum-GammaWave/webapp/installer/>
* Web Studio — <https://guillel51.github.io/Quantum-GammaWave/webapp/editor/>
* Preview de `main` — <https://guillel51.github.io/Quantum-GammaWave/preview/>
* Releases — <https://github.com/Guillel51/Quantum-GammaWave/releases>
* Paquete de esta versión — `release/felucca-v2.9.0.fwsc` en el repositorio (y el asset del release `v2.9.0`)
* Documentación — <https://github.com/Guillel51/Quantum-GammaWave/tree/main/docs>
