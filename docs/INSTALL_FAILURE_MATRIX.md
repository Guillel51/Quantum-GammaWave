# Matriz de fallos de instalación — FM-1 / Quantum GammaWave

**Fecha:** 2026-11-04
**Alcance:** instalador web, instalador CLI, parser `.fwsc`, actualizador `fm1ota.js` y loader de
actualización. **No** se modificó el bootloader, el mapa de particiones ni el arranque físico.
**Clasificación:** simulación en host. No hay FM-1 física en este entorno.

## 1. Protecciones comprobadas

- Identidad estricta `FM-1` y producto del paquete antes de enviar datos.
- SHA-256 y tamaño publicados son obligatorios; un hash ausente o distinto rechaza el paquete.
- `Package()` valida cabeceras UFW/flash, CRC de listas y contenido, `app.bin`, `cfg` y límites.
- El instalador web parsea el contenedor completo **antes** de construir la imagen lógica.
- Direcciones y longitudes de `serve()` están acotadas; una petición fuera de rango no se sirve.
- El loader escribe solo el área de aplicación, en sectores de 4 KiB, y lee cada sector escrito.
- La actualización no se declara terminada si falta el ACK final o si la identidad posterior no
  coincide. Los errores `lost`, `stopped`, `noloader` y `noreturn` no se convierten en éxito.
- Un corte USB durante la escritura deja el loader en modo actualización; reconectar y volver a
  pulsar INSTALL reanuda. Esto está simulado, no medido en hardware.

## 2. Matriz

| Escenario | Qué ocurre | ¿Arranca? | Recuperación | Cobertura / límite |
|---|---|---|---|---|
| Descarga cortada antes de cargar | El tamaño o el SHA-256 no coinciden; `Package()`/CRC rechazan | Sí, firmware actual intacto | Volver a descargar y recargar | **TESTEADO EN HOST** |
| Paquete truncado en cualquier estructura | El parser rechaza; no se habilita INSTALL | Sí | Descargar el paquete completo | **TESTEADO EN HOST** |
| Byte corrupto en cabecera, listas o payload | CRC/estructura falla; no se escribe | Sí | Descargar de nuevo | **TESTEADO EN HOST** |
| Paquete de otro modelo u otra identidad | `productOf()`/identidad falla antes de MIDI | Sí | Usar el paquete correcto | **TESTEADO EN HOST** |
| Dirección/longitud fuera de rango | `serve()` la rechaza; el host termina con error | Sí | Reintentar con el paquete válido | **TESTEADO EN HOST** |
| Desconexión USB durante la escritura | `lost`; el loader queda en modo actualización | Normalmente sí, en modo loader | Reconectar y reanudar; si el loader no responde, no hay recuperación independiente | **SIMULADO** |
| ACK final perdido tras el reinicio | `noreturn`; no se informa éxito | Puede haber quedado instalado, pero no verificado | Encender/reconectar y comprobar identidad; no repetir a ciegas | **SIMULADO** |
| Corte de energía durante borrado/escritura | El loader puede quedar a medias o en modo actualización | No garantizado | Reintentar la reanudación si el loader sigue vivo. Si el bootloader/loader se dañó, **brick recuperable solo con Mask-ROM/USB_KEY**, no implementado aquí | **NO VERIFICADO EN HARDWARE** |
| Fallo de lectura/escritura de flash | El loader detecta la lectura de verificación o el error de escritura | Puede quedar en loader | Reintentar; un fallo físico persistente no se repara por software | **NO VERIFICADO EN HARDWARE** |
| Metadatos de actualización corruptos | El registro no se acepta; el loader no confirma el commit | Depende del estado del loader | Reanudar si el registro es válido; si no, recuperación física | **SIMULADO / LÍMITE** |
| Memoria insuficiente en el host | El parser o la carga fallan antes de escribir | Sí | Cerrar pestañas/reintentar | **TESTEADO EN HOST** (no es una prueba de heap del dispositivo) |
| Downgrade o paquete antiguo manipulado | Se acepta si identidad, estructura y SHA coinciden; no hay anti-downgrade firmado | Depende del firmware instalado | Volver a una versión conocida; no hay protección criptográfica | **NO IMPLEMENTADO** |
| Paquete malicioso con SHA publicado alterado | El SHA protege contra corrupción, **no** contra un editor comprometido | Depende | No hay firma Ed25519 ni secure boot; tratar la cadena de publicación como confiable | **LÍMITE DE SEGURIDAD** |
| Fallo del propio mecanismo de recuperación | Sin A/B ni rollback autónomo; el loader depende de que el bootloader siga sano | No garantizado | USB_KEY/Mask-ROM o intervención física, fuera de este proyecto | **NO VERIFICADO / BLOQUEADO** |

## 3. Lo que no se promete

- No hay doble banco, A/B, journaling de flash ni rollback automático del bootloader.
- No hay firma criptográfica ni anti-downgrade: el SHA-256 es integridad, no autenticidad.
- Un corte de energía en el momento incorrecto puede requerir recuperación física. El proyecto no
  puede garantizar que el FM-1 vuelva a arrancar en todos los casos.
- El emulador y el host no prueban el comportamiento eléctrico, la retención de flash, el borrado
  real ni la recuperación del AC791N.

## 4. Referencias de diseño

NIST SP 800-193 (platform firmware resiliency), OWASP Firmware Security, MITRE EMB3D y RFC 9019
(secure firmware update) se usaron como criterios de revisión, no como certificación. Las
protecciones que dependen de firma, A/B o recuperación independiente quedan fuera hasta que el
bootloader y el mapa de flash lo permitan.

## 5. Pruebas relacionadas

- `python3 tests/install_test.py`: identidad, SHA, truncado, desconexión, ACK perdido, loader
  ausente, dirección fuera de rango y modelo incorrecto.
- `node web/test_pkg_bounds.mjs build/felucca.fwsc`: límites y CRC del contenedor.
- `node web/test_pkg_faults.mjs build/felucca.fwsc`: truncados y corrupción de bytes críticos
  sobre el paquete real.
- `tests/groups.sh security` y `tests/groups.sh web`: integración en las suites.

## 6. Preflight de solo lectura y cortes por página (2026-11-04)

`tools/fm1_preflight.py` (nuevo) comprueba hash obligatorio, tamaño, identidad, modelo y modo del
dispositivo, y **se niega a autorizar** la instalación mientras el chip físico no esté identificado
(exit 7). Nunca envía UPGRADE: `tests/preflight_test.py` lo verifica sobre el dispositivo falso, caso
por caso (sin hash, hash distinto, otro modelo, sin dispositivo, override de laboratorio, unidad ya en
loader).

`tests/ldr_test.c` corta ahora la energía en **cada cuarta página de programa** del área de aplicación
(el modelo de NOR aplica AND, como una NOR real) y exige que la sesión siguiente complete, que el área
coincida byte a byte con el paquete, que la cabecera y los datos de usuario queden intactos y que no se
escriba fuera de los rangos permitidos.

| Escenario | Cobertura |
|---|---|
| Chip físico sin identificar | **BLOQUEADO** (preflight exit 7) |
| Backup completo de la flash | **BLOQUEADO** (el protocolo no tiene lectura iniciada por el host) |
| Corte en cada página de la escritura del loader | **SIMULADO EN HOST** (`ldr_test`) |
| Corte en staging y cleanup de la app | **SIMULADO EN HOST** (`ota_cut_test`) |
| Corte eléctrico real / brownout / recuperación | **NO VERIFICADO** |
