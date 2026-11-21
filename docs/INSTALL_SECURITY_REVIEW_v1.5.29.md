# Revisión de seguridad del camino de instalación — v1.5.29

Alcance: lo que un navegador hace entre "el usuario elige un archivo o pulsa INSTALL" y "el loader escribe un
sector", y lo que este repositorio puede demostrar de cada paso. Clase de cada afirmación: **código y tests
`[HOST]`** (node, los mismos módulos que corre la página). No hubo unidad física: nada de esto es una medición
en una FM-1 y nada se escribió en ninguna.

## 1. La cadena, eslabón por eslabón

| # | control | dónde | qué impide | estado |
|---|---|---|---|---|
| 1 | SHA-256 + tamaño exactos del paquete contra su registro | `web/fm1provenance.js` | un paquete cambiado, cortado o de otro build | ✅ |
| 2 | el registro nombra **exactamente un** `.fwsc` y su campo `package` coincide | ídem | que el "paquete" sea otro artefacto del registro, o que haya dos y se elija uno | ✅ (nuevo en v1.5.29) |
| 3 | **todo nombre de artefacto es una ruta relativa simple** (sin `..`, sin `/` inicial, sin letra de unidad, sin `__proto__`) | ídem | que el registro dirija a la página a un archivo que no es del build | ✅ (nuevo en v1.5.29) |
| 4 | árbol limpio en el registro (`dirty`) | ídem | un paquete de una copia de trabajo sin commitear | ✅ |
| 5 | commit completo, versión e identidad con forma válida; emulador del **mismo** commit | ídem | mezclar firmware de un build con emulador de otro, o con otra unidad | ✅ |
| 6 | etiqueta de versión y `product` del sitio coinciden con el registro | ídem | que la página ofrezca A y el archivo sea B | ✅ |
| 7 | el token de procedencia sólo lo puede emitir este módulo (`Symbol`), y `Updater` no escribe sin él | `fm1provenance.js`, `fm1ota.js` | que un objeto cualquiera pase por veredicto | ✅ |
| 8 | paquete de fábrica admitido **sólo** por digest clavado (V15/V16) | `fm1pkg.js` | un "stock" que no es el archivo del fabricante | ✅ |
| 9 | el dump de la unidad es un registro (1 MiB + SHA-256), no "un archivo cargado" | `fm1verify.js` | que la condición de respaldo se cumpla con cualquier cosa | ✅ (nuevo en v1.5.29) |
| 10 | el gate escribe sólo dentro de `[0x4000, 0x93000)` y el loader rechaza una parte que no escribe | `fm1verify.js`, `firmware/loader` | tocar el área de arranque o los datos de fábrica | ✅ |
| 11 | revalidación después de escribir | `fm1ota.js` | dar por bueno un install que no se comprobó | ✅ |
| 12 | **autenticidad**: quién construyó el paquete | `fm1ota.js verifyManifest`, `tools/fm1_sign.py` | que el sitio, o quien lo controle, sirva un build propio | ⚠️ **no establecida**: sin clave de producción clavada, la firma no prueba autor. La procedencia es un candado de integridad, no de identidad |

El límite está dicho en el encabezado del módulo y se mantiene: *integridad y procedencia sí, autenticidad
no*. Quien pueda reescribir el paquete puede reescribir su registro, porque viajan del mismo origen.

## 2. Lo que estaba roto y se corrigió en esta pasada

1. **El registro nombraba el paquete por su nombre de build y la página pedía otro** (`felucca.fwsc`): todo
   paquete publicado desde v1.5.26 se rechazaba como dañado. Corregido en `fm1provenance.js` (eslabón 2) y en
   `make_site.py`, que comparaba contra `{}` y por eso **publicaba un sitio con un registro falso sin decir
   nada**.
2. **La condición de respaldo de un V16 aceptaba cualquier objeto** como dump: un `{}` la cumplía. Ahora pide
   `size === 0x100000` y un SHA-256 de 64 hex (eslabón 9), y un registro inválido deja la condición en
   `required`.
3. **Nombres de artefacto sin restricción** (eslabón 3): un registro podía nombrar `../evil.bin`. No se
   explotaba en esta página, pero el nombre es lo que el sitio sirve y lo que la página etiqueta.

## 3. Hallazgos aceptados, con su razón

- **Sin firma de producción no hay autor.** Aceptado y documentado: `docs/SIGNING.md`, y el token de
  procedencia lo dice en su mensaje. Poner una clave sin custodia real sería teatro.
- **El dump del V16 se toma fuera de la página** (M-Flasher). La página lo hashea y lo compara, pero no puede
  demostrar que salió de esa unidad: un dump de otra unidad del mismo modelo pasa el tamaño y el hash.
  Residual: la recuperación de un V16 sigue **no demostrada** y el veredicto lo dice (`not-demonstrated`).
- **`localStorage` guarda el registro del dump**. Otro script del mismo origen podría cambiarlo; la defensa
  real es el CSP de la página (sin `unsafe-inline` para scripts de terceros) y que el registro sólo habilita
  una condición, no la escritura. Residual aceptado.
- **El paquete se verifica en el navegador, no en el loader.** El loader revalida al final, pero confía en lo
  que recibe por el enlace. Residual aceptado: el enlace es local (USB-MIDI) y el paquete no se puede
  redirigir.

## 4. Lo que se prueba, y con qué

| test | qué cubre |
|---|---|
| `node web/test_provenance.mjs` | la lista de rechazos (22 casos) + los 4 nuevos de nombres y de "dos paquetes" + la sección que obliga a `make_site` a rechazar un registro con hash falso |
| `node web/test_release_e2e.mjs` | los artefactos **publicados**: `release/build-manifest-vX.json` contra `release/felucca-vX.fwsc`, y que el instalador construido busca el paquete por el nombre que el registro trae |
| `node web/test_verify_only.mjs` | los diez gate rows, V16 con y sin dump verificado, tres registros de dump malformados |
| `node web/test_stock_releases.mjs` | V15/V16 clavados, la ventana `[0x4000, 0x93000)` |
| `python3 tools/make_release.py --check`, `tools/site_check.py` | que el sitio, el instalador y los sums describan un solo paquete |

## 5. Riesgo residual, en una línea

Un paquete íntegro, de un árbol limpio y con emulador del mismo commit puede no ser un firmware seguro: la
firma no está clavada, la unidad física no se leyó y la recuperación de un V16 no se observó. La página lo
dice en el veredicto (`GO CONDITIONAL`, `not-demonstrated`) en vez de esconderlo, y ninguna constante de este
repositorio cambia eso.
