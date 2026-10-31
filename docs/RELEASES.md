# Versiones, paquetes y publicación

Todo lo de aquí se ha ejecutado tal cual en este árbol (con la toolchain real); los números son de esa
build, no estimaciones.

## 1. Qué es un "paquete" y qué identifica

* El archivo que se instala es un **`.fwsc`** (el formato de actualización del FM-1): un UFW con la
  imagen de flash, sus entradas con CRC y el área de la app (`web/fm1pkg.js` lo lee en el navegador,
  `tests/install_test.py` lo comprueba contra el mismo lector).
* **La identidad del paquete (`FM-1_982`) no cambia entre versiones**: es lo que el firmware hub usa
  para reconocer el proyecto. La *versión* (`v1.3-QG`, `v1.4-QG`…) es el texto que se muestra y el que
  va en `versions.json`.
* El prefijo `FM-1_9NN` es el rango del **loader reanudable**: si una escritura se corta, el FM-1 queda
  en `ota-FM-1_9NN` y la instalación se termina repitiéndola.

## 2. Construir una release (2 comandos)

```sh
./build.sh                                  # cabeceras generadas (Pillow, numpy, fonttools)
python3 tools/build.py --release 1.4-QG     # JIELI_TOOLCHAIN + AC79_SDK de BUILDING.md
```

Salida (verificada ahora):

```
app      build/felucca.bin  533348 B
loader   build/loader/ota.bin  6957 B
package  build/felucca-1.4-QG.fwsc  610113 B, identity FM-1_982
release  build/release-1.4-QG/
```

`build/release-1.4-QG/` lleva **el paquete, la app, `SHA256SUMS`, la atribución de samples y las
licencias** (`LICENSE`, `LICENSES/`, `LICENSING.md`). La versión no se escribe a mano: `--release`
fija `FELUCCA_VERSION` al construir (`v1.4-qg`); el archivo `firmware/src/felucca.c` sigue siendo la
fuente de la versión para una build normal.

Comprobar la integridad de lo construido:

```sh
cd build/release-1.4-QG && sha256sum -c SHA256SUMS
# felucca-1.4-QG.fwsc: OK
# felucca-1.4-QG-app.bin: OK
```

## 3. El sitio que ofrecerá las versiones

`web/make_site.py PAQUETE VERSIÓN SALIDA [PREVIA]` escribe un sitio estático autocontenido:

```sh
python3 web/make_site.py build/felucca-1.4-QG.fwsc 1.4-QG build/site
```

Produce (medido): `webapp/installer/index.html` (87 KB, con `fm1pkg.js`/`fm1ota.js` y los metadatos
en línea), `webapp/editor/`, `firmware/felucca-1.4-QG.fwsc` y `firmware/versions.json`:

```json
[ { "version": "1.4-QG", "file": "felucca-1.4-QG.fwsc",
    "sha256": "a6f140a964a2b71e86256d9ea3bd882b3e4610188a837bc393357f219d2f9763",
    "size": 610113 } ]
```

* **El SHA-256 es obligatorio**: el instalador web no habilita INSTALL si `versions.json` no trae
  `sha256` para la versión elegida, y rechaza cualquier discrepancia (y el tamaño, si viene).
* **Versiones anteriores**: `web/fetch_prev.py URL DIR` baja el `versions.json` y los paquetes que el
  sitio ya ofrece, y `make_site.py ... build/prev` los vuelve a publicar como versiones anteriores. Así
  nadie se queda sin poder reinstalar la que tenía.
* El `index.html` del sitio es hoy una **redirección** al instalador (`web/make_site.py`), pensada para
  que las URLs antiguas sigan funcionando.

## 4. Compatibilidad (lo verificado, y solo eso)

| Modelo | Estado | Evidencia |
|---|---|---|
| **M-VAVE FM-1** (AC79NN) con la **V15 oficial** (`FM-1_015`) | Soportado: instalación y vuelta a stock | instalador + CLI + `tests/install_test.py` (contra un FM-1 simulado) |
| FM-1 con **GammaWave / JIANT** encima | Soportado, con **backup antes de escribir** | instalador web (captura y confirmación) + `web/test_backup.mjs` |
| FM-1 con **otro firmware custom** | **No** se instala: el instalador ofrece volver primero a la V15 | `fwKind()` en `web/index_pkg.html` |
| Cualquier otro modelo | **No soportado**: no se ha probado ni se anuncia | — |
| **Ningún FM-1 físico en esta etapa** | Sin validar en hardware | ver §7 |

El modelo y la versión se leen del propio aparato (identidad `FM-1_xxx` por SysEx); el instalador

## 5. Instalar: los tres caminos que existen hoy

1. **Instalador web** (`web/index_pkg.html`, publicado en `webapp/installer/`): Web MIDI en Chrome/Edge;
   SHA-256 obligatorio; botón *VERIFY* que identifica el FM-1 y dice si el paquete se puede instalar sin
   escribir; 4 casillas de la lista previa antes de habilitar INSTALL; backup + confirmación antes de
   escribir sobre GammaWave/JIANT; vuelta a la **V15 oficial** desde el propio archivo del usuario
   (que se valida por SHA-256 y layout, no por nombre).
2. **Un clic** (`tools/install/install-fm1.sh|.command|.bat`): crea un entorno Python propio con `mido`
   + `python-rtmidi`, hace `--verify-only` y pide confirmación.
3. **CLI** (`tools/fm1_install.py paquete.fwsc [--sha256 HEX] [--verify-only]`): códigos de salida 0..6
   documentados; sin hash publicado, exit 2 y **cero** MIDI.

El procedimiento de instalación y la tabla de recuperación (escritura cortada, "FM-1 not found",
identidad distinta, no arranca → OCT− + OCT+ para entrar en UBOOT, restaurar backup) están en
[`docs/INSTALL.es.md`](INSTALL.es.md).

## 6. Checklist para publicar una versión (en este orden)

```sh
git switch -c release/1.4-QG                     # nunca sobre main sin revisar
./build.sh && python3 tools/build.py --release 1.4-QG
(cd build/release-1.4-QG && sha256sum -c SHA256SUMS)
python3 web/make_site.py build/felucca-1.4-QG.fwsc 1.4-QG build/site build/prev
gh run download -n emulator-<sha> -D build/emu  # el emulador del mismo commit
```

1. CI en verde en el commit que se publica (host + emulador + presupuesto).
2. El emulador del commit **probado a mano**: navegar las pantallas de la versión, guardar un preset,
   recargar y comprobar que vuelve.
3. Notas de la versión escritas (qué cambia, qué se probó y qué no; sin prometer hardware).
4. `SHA256SUMS` + `versions.json` publicados con el paquete (nunca un paquete sin hash).
5. Tag **anotado** (`git tag -a v1.4-QG -m ...`) y release de GitHub **con el paquete y el
   `SHA256SUMS` adjuntos** (esto último requiere autorización explícita: no se ha hecho).
6. La versión anterior sigue descargable (`fetch_prev` la conserva) y el instalador la ofrece.

## 7. Lo que falta para una instalación pública *segura*

* **Firma Ed25519** (diseño en [`PHASE0_SECURITY.md`](PHASE0_SECURITY.md)): hoy hay integridad (SHA-256
  obligatorio), no autenticidad. Sin firma, quien controle el sitio puede cambiar paquete y hash juntos.
* **Ninguna prueba en un FM-1 real**: instalación, backup/restore, recuperación tras corte de energía y
  los FX punch-in solo están probados contra el FM-1 simulado del instalador y del emulador.
* **Pages**: sin habilitar (`has_pages: false`), así que no hay URL pública todavía; ver
  [`CI_WORKFLOW_APPLY.md`](CI_WORKFLOW_APPLY.md).
* **El archivo V15 oficial no está en el repositorio** (no es nuestro para redistribuirlo): el usuario
  tiene que aportarlo para volver a stock, y por eso la vuelta a stock no se puede probar en CI.
* **La toolchain no está fijada hasta que se cree la variable** `JIELI_TOOLCHAIN_SHA256` (§
  `CI_WORKFLOW_APPLY.md`).

compara el modelo con el del paquete antes de escribir nada.
