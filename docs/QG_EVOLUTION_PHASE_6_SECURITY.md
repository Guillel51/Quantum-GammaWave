# Fase 6 — Auditoría de seguridad

**Ámbito:** el firmware Quantum GammaWave (aplicación + loader), el paquete `.fwsc`, la persistencia, los
parsers (MIDI/USB/editor/proyectos), la recuperación y las dependencias.

**Clase de cada afirmación**, siempre explícita: *leído en el código*, *compilado*, *medido en host x86*,
*leído de los bytes publicados*, **PENDING** (haría falta un FM-1 físico y no se ha hecho).

**Lo que este documento NO afirma:** que haya firma digital, autenticidad del paquete, anti-rollback ni
protección antibrick. No las hay, y §3 dice exactamente qué hay en su lugar.

---

## 1. Resumen

| # | Hallazgo | Severidad | Estado |
|---|---|---|---|
| F-1 | `voice_alloc` dividía por cero y devolvía un puntero nulo con un proyecto cuyo ventana de voces sea 0 | **Media** | **corregido** (este commit) |
| F-2 | El `.fwsc` no lleva firma criptográfica; no hay autenticidad de paquete | Media | por diseño, **no implementado** |
| F-3 | No hay anti-rollback: se puede instalar un paquete más antiguo | Baja | por diseño, **no implementado** |
| F-4 | Margen de flash 204 B: cualquier arreglo compite con quitar una función | Baja (disponibilidad) | vigilado por `target_budget` |
| F-5 | Recuperación tras actualización interrumpida: probada en host, **no en hardware** | Alta si fallara | **PENDING** |
| F-6 | Sin `strcpy`/`strcat`/`sprintf`/`gets`/`alloca` en el firmware ni en el loader | — | verificado |
| F-7 | ASan/UBSan limpio en editor SysEx, USB-MIDI crudo y proyectos mutados | — | verificado |
| F-8 | `tests/test_polyphony.c` no compila y no está en la suite (copia muerta de `quiet_tail`) | Informativa | reportado, sin tocar |

---

## 2. F-1 — División por cero y desreferencia nula al importar un proyecto corrupto

**Evidencia (medida):**

```
$ cc -O1 -g -fsanitize=address,undefined ... tests/fuzz_proj.c
$ ./fuzz_proj 20000 7
AddressSanitizer: FPE tests/../firmware/src/voice.c:653 in voice_alloc
```

**Causa (leída en el código).** `trk_nvoice()` devuelve `min(cap_del_motor, t->vn)`. Un proyecto
importado con `t->vn == 0` (o un motor con `poly == 0` y presupuesto 0) hace `np == 0`; entonces

- la línea 653 calculaba `t->rr = (…) % np` → **división por cero** (FPE: en el AC791N, un hard fault), y
- si no hubiera reventado ahí, el resto de la función devolvía `&V(t, 0)` — un puntero a una voz que la
  parte no tiene — y el llamador escribía a través de él.

**Impacto.** Un proyecto corrupto en flash (un bit volteado fuera del CRC), un backup restaurado a mano o
un SysEx de proyecto manipulado desde el editor puede tumbar el aparato al cargarlo. No escribe flash: es
un fallo de disponibilidad, no de integridad.

**Arreglo (compilado y medido).**

```c
np = trk_nvoice(t);
if (!np) return 0;             /* una parte sin voces no da voz: la nota se cae */
...
voice_t *v = voice_alloc(t, note);
if (!v) return;
```

**Coste:** imagen 581 344 → **581 360 B** (+16 B), margen **204 B**. RAM sin cambios.
**Verificación:** el mismo binario de fuzz, mismo seed, 20 000 iteraciones, 1 871 importaciones:
`fuzz_proj ok`. `fuzz_ed` sigue limpio.

**Por qué no se arregla en el cargador de proyectos en vez de aquí:** se podría clampear `t->vn` al
importar, y sería una segunda barrera razonable. No se ha hecho porque el margen de flash es de 204 B y
la guarda en el punto de la división cubre todos los caminos, vengan de donde vengan.

---

## 3. Lo que NO hay (y no se declara en ninguna parte)

| Propiedad | ¿Existe? | Qué hay en su lugar |
|---|---|---|
| Firma digital del `.fwsc` | **No** | cabecera con identidad `FM-1_965` y los checks del loader de M-VAVE |
| Autenticidad del paquete | **No** | ninguna; cualquiera puede empaquetar con el formato |
| Anti-rollback | **No** | el loader acepta cualquier versión válida |
| Arranque seguro (secure boot) | **No** | el AC791N arranca de flash sin verificación criptográfica |
| Recuperación garantizada (anti-brick) | **Parcial** | rescate USB (OCT− al encender) y particiones separadas; **no probado en hardware** |
| Integridad de datos guardados | **Sí** | CRC32 por bloque en `storage.c` (ajustes, proyectos, bancos) |
| Atestación de compilación | **Sí** | `zvenson/fm1-build@v1` (GitHub Actions) — dice **de dónde salió el fichero**, no que sea seguro |

**Integridad no es autenticidad.** El CRC32 detecta corrupción accidental; no impide que alguien
construya un paquete a propósito. Y la atestación de compilación tampoco: garantiza que el binario es el
que produjo ese commit público, nada más. El propio Hub lo dice en su KB.

## 4. Fronteras de memoria y parsers

Medido, no revisado a ojo:

| Superficie | Prueba | Resultado |
|---|---|---|
| Editor SysEx + paquetes USB-MIDI crudos | `tests/fuzz_ed.c` bajo ASan+UBSan, 20 000 iteraciones, seed 7 | `fuzz ok: 18 777 frames, 1 223 raw packets` |
| Proyectos mutados → importar → restaurar → renderizar | `tests/fuzz_proj.c` bajo ASan+UBSan, 20 000 iteraciones | limpio tras F-1 |
| Parser MIDI (notas, CC, reloj, running status) | `tests/midi_uart_test.c` | pasa |
| Control MIDI, canales, clock externo | `tests/midi_control_test.c` | pasa |
| Escrituras y cortes de almacenamiento | `tests/storage_test.c`, `storage_cut_test.c` | pasan |
| Actualización interrumpida en cada punto | `tests/ota_cut_test.c`, 23 cortes de staging + 3 de limpieza | pasa |
| Actualización sin sesión | `tests/ota_test.c` | `0x13 without a session refused`, `identification and reads write nothing` |
| Robustez general (basura en todas las entradas) | `tests/robust_test.c` | pasa |

**Sin copias de cadena sin límite:** `grep -rn "strcpy|strcat|sprintf|gets(|alloca" firmware/src
firmware/loader` devuelve 24 líneas y **todas son comentarios**; ninguna llamada real. El firmware no usa
`printf` de formato variable.

## 5. Fronteras de flash y actualización

- **Mapa de particiones** (leído del enlace): aplicación 581 360 B de 581 564 → **204 B libres**;
  siguiente partición libre con 3 493 B. El loader vive aparte (imagen 9 236 B) y no se toca.
- **Identidad**: `FM-1_965` en la aplicación y `ota-FM-1_965` en el loader, comprobado sobre los bytes
  del paquete (`tools/check_package_identity.py`), no sobre el código fuente.
- **Ambas flash**: `0x856014` (Puya) y `0xB36014` (UC25HQ80) en la misma lista blanca, aplicación y
  loader; `tests/flash_id_test.c` pasa.
- **Presupuesto estático**: `target_budget` falla el build si la imagen no cabe; hoy pasa con 204 B de
  margen. Es la única defensa real contra «un arreglo que ya no entra».

## 6. Dependencias

- **En la imagen**: código propio + el SDK de JieLi (AC79), propietario y no auditable aquí. No hay
  librerías de terceros enlazadas más allá de eso: sin TLS, sin malloc, sin libc de formato.
- **En herramientas y CI**: Python de la biblioteca estándar, más `Pillow`, `fonttools` y `numpy` sólo en
  el workflow de release (para generar iconos y fuentes), y las acciones de GitHub fijadas por SHA.
- **Sin secretos en el repositorio**: el workflow no recibe claves; la atestación usa el token efímero de
  GitHub (`id-token: write`).

## 7. Mapa contra las referencias pedidas

- **NIST SP 800-193** pide tres cosas: arranque protegido, actualización protegida y recuperación
  protegida. Aquí: **ninguna de las tres está garantizada criptográficamente**; la recuperación existe
  como rescate USB y particiones separadas, y está **sin probar en hardware**.
- **OWASP IoT**: los riesgos que aplican de verdad a este aparato son actualización insegura (F-2/F-3) y
  falta de gestión de parches (no hay canal firmado). Los de red/nube no aplican: no hay red.
- **EMB3D**: el modelo de amenaza relevante es «paquete malicioso instalado por el usuario» — que la
  arquitectura actual **no impide**; el usuario instala lo que le dan, y el Hub sólo comprueba que es un
  paquete FM-1.
- **RFC 9019** describe la arquitectura de actualización: identificación del dispositivo, metadatos,
  verificación e instalación por etapas. Hay identificación (F-1_965), hay instalación por etapas con
  reanudación (probada con 23 cortes en host), **no hay verificación criptográfica ni metadatos firmados**.

## 8. Pendiente (necesita hardware, no se ha hecho)

1. Actualización interrumpida **en el aparato** (cortar alimentación en plena escritura, reconectar,
   reanudar). En host está probado en 23 puntos; en hardware, **no**.
2. Instalación desde V15, desde otro firmware y salida hacia otro firmware — los tres ensayos del Hub.
3. Rescate USB (`OCT−` al encender) en un aparato real.
4. Identidad USB vista por un PC real (`lsusb`, descriptor de cadena).

**No se ha tocado la unidad del dueño, ni el bootloader, ni el mapa de flash.**
