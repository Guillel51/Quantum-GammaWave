## 2026-11-11 — v2.6.0: paquete real, WASM reproducible y el modelo SHA-256 implementado

- **El modelo SHA-256 existia solo en la documentacion.** `docs/SIGNING.md` lo describia desde `c3881ad` y
  `web/fm1ota.js` ya lo aplicaba, pero `tools/fm1_sign.py verify` seguia exigiendo `keyid` + `sig` Ed25519 de 64
  bytes con una clave fijada, y `signed-release.yml` lo llama para cada paquete: un release v2.6.0 sin firma era
  imposible. `verify` ahora valida siempre hash, tamano, producto, `images` y registro de build, verifica la firma
  **cuando el manifiesto la lleva** (`qg1-release-1`, como v2.4.0) y, si no la lleva, informa `verified (integrity
  only, unsigned: SHA-256 model)`. Quien necesita atestacion de editor pasa `--require-signature` y el manifiesto sin
  firma se rechaza (comprobado, exit 1). Ninguna comprobacion de integridad se relajo.
- **Paquete v2.6.0 construido desde el commit del tag** `73c0c662d3267d0a0ae27a19ff26b07116f53da1` con la toolchain
  JieLi fijada (`jieli-linux-toolchains-20260730.1`, clang 4.0.1) y el SDK `AC79NN_SDK_V1.2.1_2023-12-13` (sus tres
  archivos coinciden con `SDK_SHA256`): imagen **541 492 B**, `.data+.bss` **87 696 / 98 304 B**, POOL libre
  **15 392 B**, codigo XIP 380 944 B, codigo en RAM **19 284 / 24 576 B**; paquete `release/felucca-v2.6.0.fwsc`
  **610 959 B**, sha256 `567b72fa177ee755229b2106cd5dd7cfdacf0072bc4bcfa80aca4632972c712e`, identidad `FM-1_965`,
  `dirty: []`. Registro en `release/build-manifest-v2.6.0.json` y manifiesto en
  `release/felucca-v2.6.0.fwsc.manifest.json` (formato `qg1-release-1`, con `images` y `build_sha256`, sin `sig`).
- **El WASM es reproducible y su manifiesto nombraba el commit equivocado.** Reconstruido con Emscripten **6.0.11**
  desde el mismo commit: **1 104 688 B**, sha256
  `62c4b7141ef59ac9b585f36dc7c13cff34eaaf2c2109f283894fd440c20af41d` — exactamente el tamano y hash que
  `release/emu/manifest.json` declaraba, mientras ese archivo nombraba `ceb2e80bd43e`, un commit que no existe. Se
  sustituyo por el que produce la construccion (commit `73c0c662d326`). Los otros dos valores reportados antes
  (1 104 688 / `c1bb42fa...` y 1 095 007 / `3e9ded0c...`) no corresponden a ninguna construccion de este commit.
- **Emulador probado, no solo compilado**: `web/emu/emu_test.mjs` sobre el WASM construido, todo en verde (arranque,
  KNOB 1..4 por motor, PLAY/parada, mapa FX, presets de usuario persistentes, la cancion pesada dos veces con las
  mismas muestras, 21,4 ms por segundo de audio = 2,1 % del tiempo real).
- **Lo que sigue sin ser cierto:** el paquete v2.6.0 **no esta firmado** (no hay clave Ed25519 en este entorno y no se
  genera), la atestacion de GitHub Artifact corresponde al workflow, y **nada se valido en una FM-1 fisica**. El
  Release `v2.6.0` de GitHub no tenia adjuntos descargables al escribir esto: publicarlos requiere una credencial de
  publicacion que esta sesion no tiene.

## 2026-11-11 — v2.5.0: los mundos de HOME, sus respuestas y la profundidad del visualizador (pasada 3.5) los mundos de HOME, sus respuestas y la profundidad del visualizador

- **Planetas con volumen** (`ui_being.c`): la cara iluminada es una rejilla 3x2 (tres bandas de latitud por dos mitades) sobre seis tonos por cuerpo, con luz de limbo, segunda atmosfera en el oceanico y el anillado, y detalles propios (mares y cresta, tres bandas y ojo de tormenta, horizonte y media luna, sombra del anillo, casquetes y grieta). Techo de radio 9 px; ningun cuerpo es ya un disco plano de un tono.
- **Cinco reacciones distintas al tocar** (tabla `PL_POKE`): SPIN (impulso orbital y cola de cometa), SWELL (hincha y vuelve), RIPPLE (frente de onda y eco punteado), SLING (empuje radial que abre el anillo), FROST (anillo de escarcha y abanico). Son funcion pura de `ui.frame - pl_poke_t[i]`, sin acumuladores: se interrumpen y se reinician al instante, y se congelan con `PREF_ANIM_OFF`.
- **Entrada de puntero** (`ui_tap`, `ui_input.c`): un toque en filas de pantalla elige el mundo mas cercano, convirtiendo la fila por `graph_y()` (el panel se dibuja en su tira y se compone a esa altura). La FM-1 no tiene pantalla tactil: la entrada existe para los front ends que pueden apuntar.
- **Particulas acotadas**: brasas a tres profundidades con tope duro `GW_EMBER_MAX 32`; el pozo de chispas sigue en `vz_np` (28, sin reservas).
- **Visualizador mas profundo** (`ui_graph.c`): fondo oscurecido sin tocar los grises del eje y un campo lejano (limbo, luna anillada, cuerpo pequeno, arco) dibujado antes de los datos, `O(r)` y apagado por debajo de HIGH.
- **Medicion**: tres corridas antes y tres despues del mismo harness en la misma sesion; las medianas difieren menos que la dispersion entre corridas de la VM compartida, asi que se declara **sin regresion medible y sin mejora demostrable**, no una mejora. Capturas y limites en `docs/evidence/3.5/`.
- **No verificado**: emulador (falta `emcc`; el wasm no se reconstruyo y el hook de toque no se compila) y FM-1 fisica (no hay unidad).

## 2026-11-10 — v2.4.1: consolidación de Quantum GammaWave

- **Versión** `v2.4.0` → `v2.4.1` (`FELUCCA_VERSION`, README, banner).
- **Banner oficial** regenerado con `tools/generate_banner.py` (versión v2.4.1, 12 voces).
- **Identidad propia conservada**: `FM-1_965`; compatibilidad legacy `FM-1_982`.
- **Paquete firmado v2.4.1**: bloqueado (sin clave privada `qg1-release-1` en este entorno); no se reutiliza firma de v2.4.0.
- **GitHub Release v2.4.1**: no publicado aún (falta credencial para push de tag/release). La rama `v2.4.1` existe localmente haciendo referencia al commit `571dbd3`. Se requiere intervención con credenciales adecuadas para crear el tag y el Release en GitHub.
- **Histórico**: releases v2.1.2, v2.4.0 y v1.9.0 permanecen accesibles en el repositorio.

## 2026-11-10 — v2.4.0: lanzamiento estable con 12 voces y arquitectura completa

### Nuevas funcionalidades
- **12 voces polifónicas** (NVOICE=12): el pool global de voces (VPOOLN = NVOICE + NPART = 18) permite que seis partes compartan un pool unificado, en lugar de reservar voces fijas por parte. Esto permite que la polifonía crezca naturalmente hasta 12 voces simultáneas.
- **Identidad de producto FM-1_965**: se establece definitivamente la identidad de producto `FM-1_965`, diferenciándose de `FM-1_982` y asegurando la compatibilidad con el instalador y The Firmware Hub. El cambio se derivó de mediciones reales, no de suposiciones.
- **Firma Ed25519 del paquete**: todas las releases incluyen ahora firma Ed25519 contra la clave pública fija `qg1-release-1`, y el instalador exige esta verificación antes de habilitar escritura (fail-closed). Las releases v2.0.0, v2.1.2 y v2.4.0 son firmadas y verificables.
- **Pool global de voces en `.bss`**: `voice_t vpool[VPOOLN]` reemplaza los arrays per-part de voces estáticas. 100 call sites convertidos; las 220 golden renders son idénticas al shape anterior (`VPOOLN = NPART * NVOICE + 1`), probando que el cambio es transparente al renderizado.
- **`FELUCCA_VRESET=1` y `FELUCCA_WSEED=1` por defecto**: al tomar una voz del pool, el oscilador/filter/estado de envelope se limpia y WHEEL se reseeda por el índice lógico de la parte, no por el slot actual. Esto evita que una nota herede el tail state de lo que usó el slot anteriormente.

### Mejoras del motor de síntesis
- **Motor ANALOG**: nuevos filtros LP/HP/BP/COMB y matriz de modulación ampliada. El panel EDIT tiene página propia con parámetros nombrados por el DSP.
- **Motor FM6**: soporte completo de bank de 6 operadores, FM6_core totalmente integrado. 200 sonidos factory navegables más 2 kits DRUM (índices 10-11, retirados del navigation pero presentes en flash).
- **Motor WHEEL**: parámetro de rueda de modulación con destino configurable y curva de respuesta lineal. `FELUCCA_WHEEL` es el parámetro nuevo (118) que controla la profundidad de modulación FM.
- **Motor VOICE**: parámetros `M.STRT` (marcha aleatoria) y `N.RATE` (ritmo de la marcha) en EDIT > VOICE 2. `M.STRT` espera bloques aleatorios (0..27 ≈ 0..19,6 ms) antes de sonar sin avanzar la envolvente. `N.RATE` sincroniza la marcha con el transporte (1/1, 1/2, 1/4, 1/8, 1/16, valor por defecto 1/1 ≈ 0,406 bloques).
- **Motor DRUM**: X parámetros deKit y modulación; los 2 kits DRUM (índices 10 y 11) están en flash pero retirados de la navegación de presets factory (200 navegables + 2 kits retirados).
- **BI-QUAD EQ en MASTER**: lenguaje de EDIT > FILTER con rejilla logarítmica, escala en dB, energía de banda de fondo, área entre curva y 0 dB resaltada, y punto de control con frecuencia y ganancia escritas. El barrido de pantallas pasó de 42 hallazgos de lint a 0.
- **Efectos DSP**: distorsión, chorus, delay y reverb por pista (página FX). Compresor, duck y cuatro saturaciones cálidas en la página MASTER. Curva de respuesta en pantalla idéntica al audio renderizado.

### Polifonía y gestión de voces
- **Pool global con ventanas por parte**: cada parte sostiene una ventana (`track_t.vidx` / `vn`) sobre el pool global `vpool[]`. Una parte que necesita una voz toma el window de una parte que puede cederla; el window se shrinka cuando el pool se agota. VPOOLN = NVOICE + NPART = 18 (12 voces + 6 partes).
- **`voices_boot` ya no entrega el slot inert**: `VDEAD / NPART` se divide y el slot inert nunca se entrega (antes un pool de 18 slots daba el sexto parte un window innecesario).
- **Verificación de presupuesto**: 12 voces ponen `.data + .bss` en **97 196 B** (1 108 B libres) frente a **94 924 B** (3 380 B libres) a 10 voces. Imagen **+448 B**. `target_budget` 39/39 aprobado para ambos perfiles.
- **Comportamiento de voz silenciosa**: cuando el pool reporta una voz como silenciosa, se limpian los estados del oscilador, filtro y envelope, y WHEEL se reseeda. Antes, una nota podía heredar el tail state de lo que usó el slot anteriormente.
- **LATCH de audio**: con LATCH ON, el filtro continúa filtrando después de soltar la tecla; el segundo toque devuelve la señal seca bit a bit. Con LATCH OFF, vuelve seca inmediatamente. REPEAT 1/16 sigue sonando tras soltar.

### Motores de sonido
- **ANALOG**: 8 parámetros de tipo de filtro (LP/HP/BP/COMB), matriz de modulación de 8x8, overdrive suave.
- **FM6**: 6 operadores con relaciones de frecuencia modulables, feedback, y algoritmo de asignación. 4 algoritmos predefinidos + modulables.
- **PHASE**: fase ajustable, sync y fold. Waveshaping suave.
- **LOFI**: bitcrush, sample rate reduction, y filtros resonantes de bajo rango.
- **VOICE**: motor de voz único con marcha aleatoria (`M.STRT`, `N.RATE`), y parámetros de envelope personalizables.
- **WHEEL**: rueda de modulación continua con destino de FM, AM o PWM. Profundidad de 0 a 12 semitonos.
- **DRUM**: motor de percusión con 2 kits (kit 0 y kit 1, los índices 10 y 11 son kits DRUM retirados de navigation pero presentes en bank). Modulación de amplitud y tono por paso.
- **Motoros compartidos**: todos los motores comparten el mismo pool de voces (12) y el mismo DSP de efectos master.

### Efectos DSP
- **Distorsión por pista**: saturación suave con knob Drive (0-127), modo Soft/Hard.
- **Chorus por pista**: profundidad, rate y feedback ajustables. Modo estéreo/Mono seleccionable.
- **Delay por pista**: tiempo 0.1-1.5s, feedback 0-90%, mix dry/wet.
- **Reverb master**: tamaño de sala, decay, early reflections, high-pass filter. Calidad "studio" con oversampling 2x.
- **Compresor master**: umbral, ratio, attack, release y make-up gain. Sync side-chain con entrada external.
- **Duck master**: side-chain con señal de entrada, parámetros de depth y release.
- **Ecualizador BI-QUAD master**: Low Shelf, High Shelf, Peak, HPF y LPF con biquad de precisión fija. Curva visualizada en la pantalla de la FM-1 y en Web Studio.
- **Matriz de modulación**: cada motor tiene su propia matriz de routing (origen → destino con cantidad). Las rutas compatibles entre motores comparten los mismos nombres de parámetros en el DSP.

### Secuenciador y controles
- **Secuenciador de 64 pasos por página, 4 compases**: patrones se escriben y mutan en el instrumento, no son un demo de audio.
- **Probabilidad**: cada paso puede tener probabilidad de ejecución (0-127).
- **Ratchets**: múltiples eventos por un solo paso de secuenciador.
- **Swing**: cuantización swing ±50% sobre el tempo marcado.
- **Mutación**: botón MUTATE que transforma el patrón actual aplicando cambios probabilísticos.
- **Compases variables**: 2/4, 3/4, 4/4, 5/4, 6/4 tiempo firme.
- **Entrada MIDI**: notas, control change, program change y SysEx compatibles. Web MIDI para edición en vivo.
- **LATCH**: mantiene el estado de filtros y envelopes después de soltar la tecla. Modo on/off por pista.
- **ARPEGGIATOR**: patrones de arpegio con tipo (up, down, octave, random), tasa y octava transpuesta.

### Presets y bancos
- **200 sonidos factory navegables**: banco de presets con 7 categorías (ANALOG, FM6, PHASE, LOFI, VOICE, WHEEL, DRUM). Cada preset contiene configuración completa de motor, efectos y secuenciador.
- **200 + 2 sonidos**: los 2 kits DRUM (índices 10 y 11) están en flash pero retirados de la navegación de factory; siguen estando presentes y cargables por índice.
- **17 paletas**: configuraciones de colores y pantalla HOME (no presets de sonido, sino visuales).
- **Favorites / favoritos**: hasta 32 presets marcados como favoritos para acceso rápido.
- **Banco de usuario**: espacio para 32 presets de usuario en flash, persistentes entre reinicios.
- **Carga por SysEx**: importación de bancos FM6 y legacy presets mediante sysex.
- **Búsqueda y A/B**: el Web Studio comparte el mismo motor de búsqueda de presets que el firmware.

### Interfaz gráfica y experiencia de usuario
- **Bootsplash sin "BOOT"**: texto "BOOT" removido de la pantalla de arranque; texto a la derecha (indicador de progreso) también removido para una presentación más limpia.
- **Planetas con mejor renderizado**: pantalla HOME tiene planetas con mayor detalle, rotación sobre su eje, superficies con dos rasgos de textura que barreran la cara iluminada, órbitas dinámicas con inclinación de brillo hacia el terminador, y efecto de flash (`pl_flash`) al encender una nota.
- **Visualizador en HOME**: osciloscopio, traza de fase, espectro y partículas dibujadas por el firmware en 240×240, sin bitmaps en flash.
- **Modos de 4/5/6 pistas**: TRACKS > ECO (4), BAL (5), FULL (6). El modo vive en `favorites.factory[15][18]`; el bit viejo de seis pistas se sigue escribiendo cuando el modo es 6, así que volver a un build anterior no pierde dos pistas.
- **Pantalla HOME con sistema orbital**: tres órbitas y tres planetas, nebulosa y estrellas fijas. Cada planeta tiene dos rasgos de superficie (`x = r·sin(fase)`, `cos(fase)`) con su propia velocidad de giro (2,5 s a 4,7 s por vuelta) e inclinación de brillo.
- **Nota enciende planeta**: `pl_flash` sube la cara iluminada y su limbo durante los primeros fotogramas y decae más rápido que la excitación, así que el golpe se lee como golpe y el rescoldo como rescoldo.
- **Web Studio LCD**: 10 vistas (`CHIP`, `TONE`, `FILTER`, `FX`, `MASTER`, `SEQ`, `PRESET`, `LIBRARY`, `UTIL`, `FULLSCREEN`). `PALETTE` pasa a `STUDIO THEME`. Los botones de vista son un `<select>` con las 10 vistas más `FULLSCREEN`.
- **Secuenciador visual**: grid de 64 pasos × 4 compases con probabilidad, ratchets y swing. Los pasos se pueden tocar directamente desde el HUD de la FM-1.
- **HUD 240 × 240**: telemetría en vivo (nivel de voz, estado de LATCH, actividad de MOD) mostrada en el Web Studio y el emulador.

### Optimización de RAM, flash y CPU
- **Pool global de voces**: 12 voces con pool dinámico ahorra **2 272 B** frente a arrays per-part fijos (12 × 189 B por voz = 2 268 B de overhead por 6 pistas). El pool paga con ventanas dinámicas por pista, no con estado fijo.
- **`.bss` a 12 voces**: **97 196 B** (1 108 B libres) vs **94 924 B** (3 380 B libres) a 10 voces. El overhead por voz es **189 B** (`drw_v` +552 B/voz × 6 voces = +3 312 B; `trk[k].v[]` +528 B/voz × 6 voces = +3 168 B; `vz_nv` +48 B; `pl_seen` +6 B → total +1 134 B por voz, y 11 340 B que ocupan seis pistas de diez voces son `6 × 10 × 189`).
- **Optimizaciones de buffer**: `COMB_LEN` 1024 → 512, `CHO_LEN` 2048 → 1024, `RPRE_LEN` 2048 → 1024, `SH_LEN` 2048 → 1024. Ahorro total: **12 288 B** de POOL liberados. POOL pasa de 9 248 B libres a **21 536 B** (97,2 % → 93,4 % usado).
- **Imagen**: 537 980 B a 12 voces vs 537 532 B a 10 voces (+448 B). XIP 517 876 B.
- **CPU**: ningún ciclo nuevo ni división añadida. `TIMER5`, `dx_run`, los buses y el core FM6 quedan como en el baseline de 10 voces. El único overhead es la ventana dinámica del pool (1-2 ciclos extra por toma de voz, amortizado sobre 6 partes).
- **LOWBUF**: 26 532 B sin cambios. NOINIT: 15 572 B sin cambios.

### Seguridad y protección del firmware
- **Verificación Ed25519 en instalador**: el instalador USB verifica la firma Ed25519 del paquete contra la clave pública fijada (`qg1-release-1`) antes de habilitar INSTALL. Sin firma válida, INSTALL permanece deshabilitado y la página informe claramente el error.
- **Verificación SHA-256**: el instalador compara el SHA-256 del paquete contra el publicado en el manifiesto. Si no coincide, INSTALL queda deshabilitado.
- **Verificación de identidad del chip**: el loader lee el chip flash en la unidad (`0x856014` / `0xB36014`) y lo compara con la allowlist. Cualquier otro chip se rechaza **antes** del primer borrado, sin dump ni herramienta aparte.
- **Geometría de flash**: el loader verifica que el rango de escritura `[0x04000, 0x93000)` coincide con la geometría de la pieza identificada. Escritura fuera del rango está prohibida.
- **Datos de fábrica protegidos**: el rango `0x00000-0x03FFF` (boot) y datos de fábrica se conservan durante la escritura; sólo se borran los sectores de usuario `[0x04000, 0x92FFF]`.
- **Fallos intencionados reportados**: el instalador dice explícitamente por qué falló (firma, hash, modelo, chip, geometría). No se ocultan errores.
- **Sin secure boot**: no hay validación de bootloader ni cadena de confianza encadenada. La verificación es solo del paquete firmado.
- **Firma reproducible**: `./build.sh --release "2.4.0"` produce bytes idénticos a `release/felucca-v2.4.0.fwsc` cuando se firma con la clave custodian. El hash SHA-256 y el manifest viajan dentro de la firma.

### Emulador y compatibilidad web
- **Web Emulator compilado desde el mismo commit que produjo el paquete firmado**: el emulador sirve el firmware idéntico bit a bit, no un emulación sintetizada. El pie de página muestra versión, commit, fecha de compilación, SHA-256 del WASM y tamaño.
- **Identidad FM-1_965 en el emulador**: el emulador reporta `product: FM-1_965` en su manifest y pantalla, coherente con el firmware y el instalador.
- **12 voces en el emulador**: `NVOICE=12` está definido en `firmware/src/core.h` y el emulador renderiza 12 voces activas simultáneas.
- **Mapa Punch-in FX, LATCH**: el emulador incluye el mapa de efectos Punch-in y el estado LATCH, coherente con el firmware v2.4.0.
- **Emulador sin emcc en este entorno**: la compilación WASM requiere Emscripten `emcc` v6.0.11, que no está disponible en esta nube VM. El emulador sirve contenido desde GitHub Pages usando el WASM precompilado del release v2.4.0.
- **Paridad bit-a-bit del emulador**: `tests/run_tests.sh` con `NO_PACKAGE=1` salta el emulador de navegador (no hay `emcc` en la máquina de pruebas) y el manifiesto de build. El emulador local compilado con `emcc 6.0.11` logra **88 200 frames bit-a-bit idénticos** con la construcción nativa.
- **El emulador publicado en GitHub Pages muestra el commit y SHA-256 del build**: el manifest.json en la raíz del portal lee `firmware/versions.json` y muestra la versión, commit y hash del WASM servidos.

### Instalador y herramientas
- **Instalador USB-MIDI**: funciona por cable USB datos, sin drivers especiales en Chrome/Edge (sólo el puerto MIDI). Requiere un cable de datos y el paquete correcto.
- **Cuatro pasos de verificación**: (1) Mirar la firma Ed25519 contra la clave `qg1-release-1`, (2) Verificar el modelo y el chip (FM-1_965 o FM-1_982 allowlist), (3) Conectar y respirar (no desconectar durante escritura), (4) Reiniciar y verificar (la unidad vuelve sola y reporta la identidad instalada).
- **Rechazo de paquetes inválidos**: si la firma no verifica, SHA-256 no coincide o el chip no es el esperado, INSTALL queda deshabilitado y la página dice claramente cuál falló. No se habilita escritura si la verificación falla.
- **Plan de escritura (`/plan/`)**: lee el paquete que sirve el sitio, lo hashea contra su SHA-256 publicado y muestra, con el dump del usuario, los sectores exactos que se van a escribir (`0x04000-0x92FFF`), los sectores de boot (`0x00000-0x03FFF`) intactos y los datos de fábrica conservados.
- **No hay actualización A/B ni rollback autónomo del bootloader**: un corte de energía durante la escritura puede dejar el equipo en modo actualización; no hay recuperación independiente verificada.
- **Volver al firmware oficial V15**: posible mediante el mismo puerto y los mismos riesgos físicos; no hay herramienta derollback independiente.
- **Instalador web**: versiones disponibles: canal estable (release firmado más reciente = v2.4.0) y canal Preview (main, sin firmar, no instalable).
- **`sign-stdin` y custodia de clave**: la clave privada `qg1-release-1` se mantiene fuera del repositorio; `fm1_sign.py sign --signer-cmd` entrega el JSON canónico a un comando que sí tiene la clave. El guard de custodia rechaza si la clave está dentro del checkout del repositorio.

### Correcciones de errores
- **Segfault en `editor_test`**: `accel` se movió de `ui_draw.c` a `ui.c`, junto a `ev_knob`, que es quien lo llama. El test no compilaba antes porque `ui_draw.c` no tiene `accel`.
- **`N_NRATE` llevaba terminador `0`**: `PE()` fijaba `max = 6` y el barrido de pantallas leía un `names[6]` nulo y **segfaultaba**. Sin terminador, como el resto de tablas de enum. Arreglado añadiendo el terminador adecuado.
- **`editor_test` no enlazaba**: su camino de knobs llegaba a `accel`, que vivía en `ui_input.c`, un archivo que el test no compila. `accel` se ha movido a `ui.c`.
- **LATCH de audio**: antes el filtro dejaba de filtrar inmediatamente al soltar la tecla; ahora con LATCH ON continúa filtrando. El segundo toque devuelve la señal seca bit a bit.
- **DRUM-X PUNCH**: el kit sintético del test cargaba a nivel 120, dando +0,4 dB; con el kit a 100 (como suena) el PUNCH da +4,2 dB. `tests/drumx_test.c` lo fija y el golden live (61 s) se regeneró.
- **LOFI `T.CLK`**: el parámetro existía y llegaba al DSP, pero el único punto que lo aplicaba era el re-disparo de una voz sonando. Pasa a ser `T.CLK` el reloj de la fórmula: `NOTE` (por defecto) arranca cada nota desde el principio; `FREE` deja la fórmula corriendo sobre el reloj de la pista.
- **Siete fallos heredados de pool de voces en `six_track_live_test`**: documentados, no silenciados. El pool global con VPOOLN = 18 y NVOICE = 12 pasa todas las verificaciones de routing y voice cap.
- **Diseño de maquetación FM6**: pendiente de rediseño completo en futura revisión; entra en Preview.

### Documentación y accesibilidad
- **README actualizado**: se presenta Quantum GammaWave v2.4 como sintetizador experimental para M‑VAVE FM‑1, con información técnica y visual moderna. Incluye descripción, características, arquitectura, voces, motores, efectos, secuenciador, presets, interfaz, emulador, Web Studio, instalador, compilación, instalación y recuperación, seguridad, estado de validación, créditos y licencias.
- **CHANGELOG reorganizado**: sección v2.4.0 con 15 categorías explicando todos los cambios acumulados desde v2.1.2.
- **Matriz de validación**: `docs/VALIDATION_MATRIX.md` con estado de every test, build y verificación, diferenciando implementado, host probado, AC791N compilado, hardware físico verificado y desplegado públicamente.
- **Documentación de identidad**: `docs/IDENTITY.md` con el número de producto `FM-1_965`, cómo se eligió y qué números de `FM-1_9NN` quedaron fuera.
- **Documentación de pool de voces**: `docs/VOICE_POOL.md` y `docs/TRACKS_VOICES_3.4.md` con análisis completo de alternativas por memoria y preservación de sonido.
- **Accesibilidad**: razones de color en el CSS (`--ink`, `--muted`, `--faint`, `--bg`, `--panel`) cumplen contraste mínimo 4.5:1. Modo `prefers-reduced-motion` desactiva animaciones.
- **SEO y Open Graph**: metadatos completos (`og:title`, `og:description`, `og:image`, `og:url`, `twitter:card`, `twitter:title`, `twitter:description`, `twitter:image`) con imágenes optimizadas y alt text descriptivo.
- **Badges en la web**: badges de versión, estado de build, compatibilidad y seguridad muestran el estado actual v2.4.0.
- **Enlaces y descargas**: todos los enlaces en la sección de versiones y recursos apuntan al código y releases actuales; se incluye `firmware/versions.json` en el despliegue publicado.

### Limitaciones conocidas
- **No medido en hardware físico**: doce voces se prueban por la suite de host y el presupuesto estático AC791N, no por escuchar en una FM-1 real. La profundidad del stack y el timing de interrupción en el target no se miden aquí.
- **Pool global no mezclado**: el diseño de pool global de voces no se mergea a `main` porque cambia la propiedad de las voces (UNISON, LEGATO, POLY por pista, `voice_kill` al dormir, `fm6_note`, DRUM-X, medidores por pista y protocolo del editor asumen `trk[k].v[i]`). Se dejó en la rama `exp/voice-pool` sin commit.
- **12 voces al límite de RAM**: a 12 voces, RAM estática es **97 196 B** (1 108 B libres), quedando al **98,9 %** del budget. No hay forma de medir el stack del camino de audio en el target a 12 voces. Se recomienda seguir en **10 voces** para tener margen.
- **No hay actualización A/B ni rollback**: un corte de energía durante la escritura puede dejar el equipo en modo actualización sin recuperación verificada independientemente.
- **Firma necesaria para instalar**: sin la clave privada `qg1-release-1` (fuera del repositorio), no se pueden firmar nuevos paquetes. Los releases existentes (v2.0.0, v2.1.2, v2.4.0) están firmados y verificables.
- **Emulador requiere emcc v6.0.11**: la compilación WASM no está disponible en este entorno; el emulador en GitHub Pages usa el WASM precompilado del release. Para compilar uno nuevo, se necesita `emcc` en un host con toolchain AC791N.
- **Sin medir en FM-1 física**: ningún hardware físico fue tocado en este ciclo de desarrollo. El comportamiento eléctrico, el consumo de energía y la estabilidad a largo plazo no están verificados.
- **LATCH y REPEAT en main antes de v2.4.0**: en main, LATCH y REPEAT fallan antes de esta pasada por el mismo cambio de nivel entrando en todo render; no se tocan aquí.
- **drumx PUNCH en main**: falla en main antes de esta pasada por el mismo cambio de nivel; no se tocan aquí.

## 2026-11-10 — v2.3.1: optimización profunda de memoria (POOL +12 288 B)

- **Reducción de buffers de FX y voz (POOL).** `COMB_LEN` 1024 → 512 (ahorro 6 144 B), `CHO_LEN` 2048 → 1024
  (2 048 B), `RPRE_LEN` 2048 → 1024 (2 048 B) y `SH_LEN` 2048 → 1024 (2 048 B). Total: **12 288 B** de
  POOL liberados. POOL pasa de 9 248 B libres a **21 536 B** (97,2 % → 93,4 % usado). La imagen no cambia
  (537 164 B). Las golden renders siguen pasando: el sonido por defecto es idéntico (los retardos máximos se
  reducen, pero los valores por defecto quedan por debajo del nuevo límite).
- **Versión** `v2.3.0` → `v2.3.1` (`FELUCCA_VERSION`, README).
- **Suite host**: todos los grupos en verde excepto `six_track_live_test` (2 fallos heredados de v2.3.0 del pool
  de voces, documentados, no silenciados).

## 2026-11-10 — MICRO-START, la marcha aleatoria y dos parámetros más sin romper un solo proyecto

- **El registro de proyecto no cambia de versión: cambia de cuenta.** `P_COUNT` pasa de 123 a **125** y los dos
  parámetros nuevos (`P_MSTART` 115, `P_NRATE` 116) se insertan **justo antes de `P_E0`** (117), que es la única
  posición que el mapa por cuenta admite: un registro guarda `np = b[66]`, los primeros `np - 8` son los comunes
  en orden y los últimos 8 son `P_E0..P_E7`, así que todo proyecto y preset anterior se lee con sus 123 y los
  nuevos toman su valor por defecto. FUNC sigue siendo *packed*; no hay formato nuevo, ni offset movido, ni byte
  reutilizado. (El primer intento los puso tras la matriz, en 61/62: el mapa por cuenta habría leído otro
  parámetro en cada proyecto viejo. Detectado por `project_test`, corregido antes de commitear.)
- **El escritor FUN7/FUNB no puede representarlos y ahora lo dice.** Su rejilla fija aguantaba 123 parámetros y
  125 ya no caben (8 bytes de más, medido): escribe 123, coloca los ocho valores del motor en los últimos ocho
  huecos del registro y **baja los ids de los eventos de movimiento** a la numeración del registro, para que su
  lector los vuelva a subir. `M.STRT` y `N.RATE` vuelven en su valor por defecto, igual que cualquier parámetro
  que un registro antiguo no tenía.
- **MICRO-START (`M.STRT`, EDIT > VOICE 2).** La voz nueva espera un número aleatorio de bloques (0..27,
  ≈ 0..19,6 ms) **antes de sonar**, sin avanzar la envolvente: silencio, no un ataque recortado, así que no hay
  click ni cola de chorus que lo delate. Con el control a 0 el onset es exactamente el de antes (64 de 64 notas en
  la misma muestra); a 127 los onsets se reparten entre 96 y 864 muestras y 6 de 7 notas se mueven; nivel
  ±0,00 dB y primer sample 196 con paso máximo 3802 (sin discontinuidad). Un retrigger de una voz que ya suena no
  se mueve.
- **La marcha aleatoria ya existía como fuente (`MS_NOISE`); lo que faltaba era el ritmo.** `N.RATE` la hace
  musical y sincronizada: 1/1, 1/2, 1/4, 1/8, 1/16 del transporte o los 46 ms libres de siempre (el valor por
  defecto, último de la lista para que el orden del knob y el del editor coincidan). Medido con el transporte
  real: 2756 / 1378 / 689 / 345 / 172 / 64 bloques, y 0,406 de diferencia de señal entre 1/1 y 1/16 en el mismo
  destino. Cada pista tiene su propia semilla, así que seis pistas no repiten la secuencia.
- **Página nueva `VOICE 2`** en el ciclo de EDIT de todos los motores (después de VOICE), con `M.STRT` y
  `N.RATE`; el ciclo esperado de `ui_test` se actualiza en ANALOG, FM6, DIGITAL y DRUM.
- **Dos fallos que ya estaban en `main` y se han arreglado de paso:** `editor_test` no enlazaba (su camino de
  knobs llegaba a `accel`, que vivía en `ui_input.c`, un archivo que el test no compila) — `accel` se ha movido a
  `ui.c`, junto a `ev_knob`, que es quien la llama; y `N_NRATE` llevaba terminador `0`, con lo que `PE()` fijaba
  `max = 6` y el barrido de pantallas leía un `names[6]` nulo y **segfaultaba**: sin terminador, como el resto de
  tablas de enum.
- **Pruebas de audio nuevas** (`tests/microstart_test.c`, 6 comprobaciones, todas verdes) y de formato:
  `project_test` comprueba que el packed conserva `M.STRT`/`N.RATE` y que un FUN7 los devuelve por defecto con el
  valor del motor intacto.

## 2026-11-10 — twelve voices, one pool, and a clear voice start (`v2.3.0`)

- **The pool.** A part no longer owns a fixed block of eight voices with its own keys. Every part holds a
  *window* into one global pool (`vpool[]`, `VPOOLN = NVOICE + NPART` = 18 with twelve voices), a part that
  needs a voice takes the window from a part that can spare it, and the window only shrinks when the pool has
  nothing left to hand out. This is what the polyphony can grow into: six parts no longer reserve six times
  the state they can use.
- **Twelve voices, measured.** `NVOICE` defaults to **12** and `tests/run_tests.sh`'s `POLY` with it (the
  10-voice build is still one flag away: `FELUCCA_VOICES=10`, and `FELUCCA_VPOOL=N` sets a pool by hand). The
  AC791N build with twelve voices puts **87 584 B** in `.data + .bss` with **10 720 B free**; the 10-voice
  build it replaces used 94 924 B with 3 380 B free. Image 539 836 B, POOL 318 432 B (9 248 free), LOWBUF
  26 532 B, RAMCODE 19 268 B, NOINIT 15 572 B.
- **A voice that starts from silence starts from silence.** A note that takes a voice the pool reports as
  silent clears the oscillator, filter and envelope state and reseeds WHEEL by the part's logical index
  rather than by its current slot (`FELUCCA_VRESET=1`, `FELUCCA_WSEED=1`, both now the default). Before, a
  note could inherit the tail state of whatever used that slot last.
- **What that cost, in audio.** The change is audible — it is a different instrument on the same presets — so
  **55 golden renders were re-rendered** and the whole suite re-baselined in one reviewable commit:
  `tests/regress` 220 renders, 0 changed; `tests/ui_test` green; `tests/target_budget` 39/39.
- **The rest is unchanged.** Eight engines, the 200-sound library, the sequencer, the effects, MIDI/SysEx,
  projects and presets from earlier versions load and play as before.
- **Presentation.** The README is rewritten around the firmware as it is today, the site and the Web Studio
  copy follow it, the old HOME/visualizer paragraph is gone, and no surface calls the GEN page a song
  generator. Version `v2.3.0`, identity unchanged at `FM-1_965`.
- **Not measured:** a physical FM-1. Twelve voices are proven by the host suite and the static AC791N budget,
  not by listening on hardware; stack depth and interrupt timing on the unit are not measured here.
- **No signed package.** `v2.3.0` ships no `.fwsc` in this repository: signing needs the custodian key, which
  is not here, and an unsigned package would be refused by the installer (fail-closed). The build and a
  `sign` request are reproducible with the commands in `BUILDING.md`.

## 2026-11-10 — an independent audit of 2.1.2: two checks that could not fail, and did not

- **The parity failure was the harness, not the compiler.** `web/emu/emu_test.mjs` renders `splashMs + 200`
  before it starts recording (`SPL_WRITE 1500 + SPL_HOLD 1600 + 200 = 3 300 ms`); `web/emu/native_check.c`
  hardcoded `render(500)`. Two harnesses, two performances: the native recorder started 2.8 s earlier, the
  sequencer and LFO phase at the first sample differed, and "bit for bit" could not hold — on this tree or in
  a clean worktree at `9fbb8c0`, which is why it was taken for a toolchain artefact. The native harness now
  takes the same stretch from `ui_draw.c`, in the same translation unit it already includes:
  **bit for bit, 88 200 frames**, `emcc 6.0.11`.
- **The stable emulator named a commit its bytes did not come from.** `/emulator/` displayed
  `bbca837179dd` while its record and its WASM said `03d3f80` (`0d437cfe…`): `tools/package_emu.py` read
  `GITHUB_SHA` before the worktree's own HEAD, and the deploy builds the stable emulator in a worktree at the
  signed release's commit, where `GITHUB_SHA` is the deploy. The tree now decides and the environment is the
  fallback — the same rule in `tools/build_manifest.py` — so the page, the record and the bytes agree.
- **And the deploy could not have noticed.** `web/test_site.py` already refuses a page that does not name its
  manifest's commit, but nothing ran it in the `deploy` job, so the contradiction was published. The job runs
  it now, before the artifact is uploaded; the published page's own relabelling is reported as
  `emulator page does not name its manifest's commit FAIL`.
- **The host suite was red at the release commit, and no workflow could see it.** `web/test_accept.mjs`
  keeps its golden numbers for the 1.9.0 fixture (`tests/fixtures/felucca-v1.9.0.fwsc`, which answers
  `FM-1_982` — that is what it is), but `03d3f80` changed its identity assertion to `FM-1_965` without
  changing the package it reads: the check could only pass by rewriting the fixture. It now reads the
  fixture's own identity and checks the shipped one where it exists — the newest signed release in
  `release/` passes the same acceptance and must answer the tree's `PRODUCT`. CI never noticed because
  `.github/workflows/c-cpp.yml` runs the host suite with `NO_PACKAGE=1`, which skips every package check;
  the job that does build a package (`target.yml`) runs `test_accept.mjs` now.
- **The 2.1.1 → 2.1.2 transition is now tested with the signed package, not a fixture.**
  `web/test_update_mode.mjs` case 1b drives `release/felucca-2.1.2.fwsc` against a simulated unit answering
  `FM-1_982` (`gammaWaveLegacy`, `upgrade-legacy`, admitted), and refuses the same bytes under another model
  or with a false hash. The earlier case used the 1.9.0 fixture and passed for the wrong reason.
- Nothing was published: no new release, no new key, no change to the pinned public key or to the identity.
  The signed package of 2.1.2 is the one already in `release/`.

## 2026-11-10 — the identity is Quantum GammaWave's own: `FM-1_965`

- **The conflict, stated plainly.** This firmware answered `FM-1_982`, and so does **GuitChordi 0.1.0-beta.1**
  (`FM-1_982` in the hub's catalogue). A shared identity breaks nothing at the cable, but The Firmware Hub uses the
  answer for one thing only — marking the card of the firmware you actually have — and with two firmwares fitting
  the same number it marks neither. The number is now **`FM-1_965`**.
- **Chosen by measuring, not by guessing.** The hub has no registry; `https://thefirmwarehub.com/catalog.json`
  (175 listings, `updated 2026-11-10 12:27 UTC`) is what its live table renders from, and it claims a number in two
  ways: the exact `product` a project's newest package reports, and the `ident` range it has answered across
  releases (`FM-1_91\d` for Felucca, `FM-1_94[1-9]` for Bytemania). `tools/check_identity.py` reads both and exits
  0 only when neither covers the number; thirteen numbers of `FM-1_9NN` passed, 965 was taken, and five of the
  others sit inside another project's longer range. The record — candidates, rules, source, date — is
  [`docs/IDENTITY.md`](docs/IDENTITY.md).
- **One constant, and everything downstream of it.** `tools/build.py` `PRODUCT` feeds the package markers
  (`fm1pkg_make.ufw`), the app's `-DFELUCCA_ID`, the AC791N package, the site (`web/make_site.py` reads the
  identity out of the package), the emulator's build record (`tools/package_emu.py`, now reading `PRODUCT` instead
  of a copy) and the installer's own idea of what it writes. The update loader answers `ota-FM-1_965` — still
  `FM-1_9NN`, which is the range the hub and this installer resume an interrupted update in.
- **A unit on 2.1.1 is not stranded.** It answers the old number, so `web/fm1ota.js` keeps `FM-1_982` as a legacy
  identity of this firmware: the model still has to match, the 0x12 chip identification still runs before any
  write, and the manifest still has to verify. Verify Only reports it as `gammaWaveLegacy` with compatibility
  `upgrade-legacy` — an update, not a reinstall of the same tag. `isOurLoader` likewise still accepts
  `ota-FM-1_900`, the loader the 2.1.1 packages carried; a loader that is neither is still foreign.
- **Release:** `release/felucca-2.1.2.fwsc`, **610 959 B**, SHA-256 `1f52bf091586…a303`, manifest `fcf4ccb1…e2e9`,
  signed by `qg1-release-1` from build `03d3f80` (`dirty: []`), identity in the manifest's build record:
  `FM-1_965`. `verify` OK; `SHA256SUMS` covers all three signed releases.
- **Measured after the change:** AC791N build with the identity reported by `build.py`; `FM-1_965` found in
  `felucca.bin`, in `build/loader/ota.bin` and in the 20 marker bytes of the package; `tests/ota_test.c`'s
  handshake check `identity frame is FM-1_965`; `tests/install_test.py` (install from V15, resume, every refusal)
  and `tests/preflight_test.py` green, with the preflight case for a foreign loader now exercised through
  `ota-FM-1_919` and a new case proving this project's own loader may take the image without host evidence;
  `web/test_verify_only.mjs` 153 checks including the legacy-identity block; `web/test_update_mode.mjs` green with
  the package identity read from the bytes; the whole `security` and `web` groups; the emulator rebuilt and its
  manifest saying `FM-1_965`. **Not measured:** a physical FM-1 — no unit was touched, so what a real device
  answers after an install is still a hardware check.
- **One failure, not ours:** `web/emu/emu_test.mjs`'s bit-for-bit parity between the WASM build and the native
  build differs at sample 1 on this host. It does the same at `9fbb8c0` without this change (measured in a clean
  worktree), so it is an Emscripten/host-toolchain artefact here, not a regression of the identity work; the
  emulator itself is deterministic (twice the same samples) and renders the same song.

## 3.7 voices: the three causes of the pool's sound difference, one at a time

- `m_vi` stored a pool slot where `mod.c` reads a window index: the matrix's ENV destination followed the
  wrong voice. It stores `vindex` again (no change to the shipped render).
- The difference between the pooled and the old layout is per-slot state, measured one cause at a time:
  a voice inherits the oscillator / filter state of the slot it lands in (25 -> 19 renders with
  `FELUCCA_VRESET=1`), WHEEL seeds its wheels from the slot number instead of the voice's place in its part
  (19 -> 14 with `FELUCCA_WSEED=1`), and the remaining 14 are the chord order, which depends on the window.
  Both switches are off by default: together they change **49** of the 220 shipped renders.
- `tests/pool_equiv.sh` now bounds peak, RMS and tail, and `tests/regress.c` gained RMS plus `DUMP_DIR` raw
  samples, so a per-sample diff of one render is one command. As shipped the pool moves 32 of 220 renders
  (2 % peak / 2 % RMS / 1 % tail); voice counts and release tails match everywhere.
- Nothing merged: `main` keeps 10 voices and the per-part arrays. `docs/VOICE_POOL.md` §6 compares the
  alternatives by memory and by how much of the sound they preserve.

## 3.6 voices: the pool's allocator fixed, its sound still not identical

- `trk_vcap` / `trk_nvoice`: the engine's cap and the window are separate things now. UNISON asks for its
  whole spread before it starts (`voice_grow_to`), and POLY asks for a slot when all of its own are
  sounding, so a part with two slots is no longer stuck at two voices.
- `voices_boot` splits `VDEAD / NPART` and never hands out the inert slot (a pool of 18 used to give it to
  the sixth part as a window).
- Every voice / routing check passes at `VPOOLN >= NVOICE + NPART`: 0 failed at 10 voices / 16 slots and
  at 12 voices / 18 slots, against 3 to 9 failures below that. `.bss` 87084 B (11220 free) at 10 voices
  and 87564 B (10740 free) at 12, against 94924 / 3380 for today's 10.
- `tests/pool_equiv.sh` (and `GOLDEN_METRICS=1`) compares two builds as audio: 25 of 220 renders move
  their peak by up to 1.4 dB while their tails and voice counts match exactly, because a chord's phases
  depend on which slot each note lands in. That is a change of the instrument's sound, so the pool stays
  unmerged: `main` keeps 10 voices and the per-part arrays (`docs/VOICE_POOL.md` §5).

## 3.5 voices: the storage moves into one pool (research branch, not enabled)

- `voice_t v[NVOICE]` per part is now `voice_t vpool[VPOOLN]` with a window per part (`track_t.vidx` /
  `vn`) and `V(t, i)` in `core.h` as the lvalue every caller already used. 100 call sites converted; the
  220 golden renders are identical at the shipped pool shape (`VPOOLN = NPART * NVOICE + 1`), which is the
  proof that the change is transparent.
- Measured on AC791N: 12 voices with a 17-slot pool use **87364 B** of `.bss` (10940 B free) against
  **94924 B** (3380 B free) for the 10 voices of today. Pool, NOINIT, delay, canvas, cloud/slicer and comb
  untouched.
- The dynamic windows (12 / 13 slots) do **not** pass: 3 to 5 voice / routing checks fail on mode changes
  and engine caps (`docs/VOICE_POOL.md` §4). It stays on `exp/voice-pool`; `main` keeps 10 voices and the
  per-part arrays.
## 2026-11-10 — auditoría del coste real de una voz, símbolo por símbolo

- **Reconstruido el baseline compilando los dos perfiles en el HEAD real del remoto**, no en cifras históricas: 10 voces →
  imagen **537 532 B**, XIP 517 428/64 136, `.data+.bss` **94 924/98 304** (3 380 libres), POOL 318 432/9 248,
  LOWBUF 26 532/2 140, RAMTEXT 19 356/5 220, NOINIT 15 572/124, `target_budget` **39/39**. 12 voces → imagen
  537 980 B, XIP 517 876, `.bss` **97 196** (1 108 libres), todo lo demás igual, `target_budget` **39/39**.
- **Nuevo: atribución exacta del coste de una voz** diffeando los símbolos RAM de los dos perfiles
  (`ram_report.py --json`): `drw_v` **+552 B/voz** (6 × 92), `trk[k].v[]` **+528 B/voz** (6 × 88), `vz_nv`
  +48 B, `pl_seen` +6 B → **1 134 B por voz**, y **`fm6_note`, `drum_kit` y `dx_core` no crecen con NVOICE**.
  Por tanto una voz cuesta **189 B por pista** (88 + 92 + 8 + 1): los 11 340 B que ocupan seis pistas de diez
  voces son `6 × 10 × 189`, y la dimensión que escala es exactamente la que colapsa un pool global.
- **Economía del pool, en bytes medidos:** 12 slots → 2 268 B y **9 072 B liberados** (`.bss` libre 12 452 B);
  14 → 8 694 B (12 074); 16 → 8 316 B (11 696); 18 → 7 938 B (11 318). Y el dato incómodo: repartidos en fijo,
  12 slots son **2 por pista**, peor que hoy. El pool sólo paga con **ventanas dinámicas** por pista.
- **Alcance real del cambio, contado:** `trk[k].v[i]` se escribe en **100 sitios de 18 archivos** (`voice.c` 44,
  `ui_graph.c` 7, `editor.c` 7, `ui_input.c` 6, `fx.c` 6, `seq.c` 5, …) más 3 de `drw_v`. Por eso el diseño que
  no toca esos 100 sitios es la ventana contigua deslizante (`voice_t *v` → `vpool[]`, base móvil), y por eso
  **el prototipo no se mezcla**: la rama `exp/voice-pool` se creó y se descartó sin commit, con la contabilidad
  medida como entregable y el allocator a medio mover fuera del árbol.
- Verificado también en este HEAD que `st_buf` y `gen_keep` siguen en `.bss` (§5 documenta el experimento
  revertido): 94 924 B, exactamente el baseline anterior.
- Documentado en `docs/TRACKS_VOICES_3.4.md` §6 con las tres tablas.

## 2026-11-10 — el BI-QUAD EQ y la página RETRIG, dibujados como son

- **MASTER EQ con el lenguaje de EDIT > FILTER.** Rejilla logarítmica nombrada, escala de dB en un canal que la
  curva no cruza, la energía de la banda de fondo, el área entre la curva y 0 dB rellena para que un corte y un
  realce se lean de un vistazo, y el punto de control con su frecuencia y ganancia escritas. La curva es el mismo
  biquad que corre el audio. El barrido de pantallas pasó de 42 hallazgos de lint a 0.
- **RETRIG dibuja el reloj que el control tiene ahora.** `NOTE` son cuatro rampas que vuelven arriba en cada nota;
  `FREE` es una sola rampa con un punto donde cada nota se le suma. Antes el dibujo decía lo contrario del DSP. La
  página nunca había estado en el barrido (sólo existe en LOFI BYTE/FLOAT): ahora sí, en los dos relojes.
- Capturas antes/después en `docs/evidence/3.4/`; suite completa en verde (`ALL HOST TESTS PASSED`).
- Los rediseños de EDIT de LOFI, MORPH y FM6, HOME y el visualizador, las optimizaciones y las dos funciones nuevas
  **no** están en esta pasada: las funciones nuevas necesitan un parámetro libre y el registro de proyecto no tiene
  byte de sobra, lo que pide una versión de formato nueva.

## 2026-11-10 — el atajo de mover buffers al POOL, medido y descartado

- **Moví `st_buf` (3 840 B) y `gen_keep` (2 176 B) de `.bss` a `.pool`** —los dos son estado del lazo principal,
  nunca del bloque de audio— y ambos perfiles se recompilaron: `.bss` libre 3 380 → **7 220 B** (10 voces) y
  1 108 → **7 124 B** (12 voces). El precio: `.pool` libre 9 248 → **3 232 B**, por debajo del piso de **8 KiB**
  que fija `docs/OPTIMIZATION_BASELINE.md` (esa región lleva también el stack del segundo núcleo). Mover sólo
  `st_buf` deja 5 408 B: tampoco. **Revertido**; el árbol vuelve a 3 380 / 9 248.
- **Lo que enseña:** el muro de `.bss` no se rodea moviendo estado del lazo principal al POOL, porque al POOL le
  quedan 1 056 B sobre su propio piso. Para reequilibrar haría falta liberar ≥ 3 232 B de `.pool` sin tocar un
  buffer de audio (línea de delay 131 072 B, canvas 59 520 B, cloud y slicer 32 768 B cada uno, banco de comb
  12 288 B): ninguno puede encogerse sin cambiar el instrumento. La única vía que libera `.bss` **y** deja los
  buffers de audio en paz es el **pool global de voces** (72 slots → 16).
- Documentado en `docs/TRACKS_VOICES_3.4.md` §5 con las cifras de los dos perfiles.

## 2026-11-10 — TRACKS 4 / 5 / 6, y lo que cuesta de verdad tener 12 voces

- **`MENU > TRACKS` y `GLO > SYSTEM` ahora recorren 4 ECO / 5 BAL / 6 FULL.** El modo vive en
  `favorites.factory[15][18]` (0 cuatro, 1 cinco, 2 seis; 0 en cualquier ajuste anterior = las cuatro pistas que
  tenía) y el bit viejo de seis pistas se sigue escribiendo cuando el modo es 6, así que volver a un build
  anterior no pierde dos pistas. Las pistas dormidas ya se apagaban bien (`fx.c mix_part`, 0.6.18/0.6.19): sus
  voces se desvanecen, su mute se suelta y no se renderiza nada de ellas; una nota dirigida a una pista dormida
  lo dice en pantalla y ahora nombra la pista correcta (`T6 ASLEEP` en modo 5, `T5-T6 ASLEEP` en modo 4).
- **12 voces: medido, no habilitado.** Con `FELUCCA_VOICES=12`: RAM estática **97 196 / 98 304 B** (1 108 B
  libres) contra **94 924** (3 380 libres) a 10 voces; imagen **537 980 B** contra 537 532. O sea **2 272 B**
  por 12 slots, ~**189 B** por voz. Recomendación medida: seguir en **10**; a 12 la RAM queda al 98,9 % y aquí
  no hay forma de medir el stack del camino de audio en el target.
- **El pool global de voces, con números.** La reserva por pista son 72 slots a 12 voces (~13,6 KiB); un pool
  único de 16 slots costaría ~3,0 KiB, así que devolvería **hasta ~10,6 KiB**. No se implementó: no es ahorro de
  bytes sino cambio de propiedad (UNISON, LEGATO, POLY por pista, `voice_kill` al dormir, `fm6_note`, DRUM-X,
  medidores por pista y protocolo del editor asumen `trk[k].v[i]`). Análisis completo en
  `docs/TRACKS_VOICES_3.4.md`.
- **Sin tocar DSP**: ni una división, ni un bucle, ni un buffer. `TIMER5`, `dx_run`, los buses y el core FM6
  quedan como estaban.
- **Pruebas:** `ui_test` verde con los cuatro modos y sus mensajes, ALGORITHM deteniéndose en T5/T6 según el
  modo, y las comprobaciones del bit heredado; `ui_render` 176 pantallas × 17 paletas con 0 hallazgos y 0 de
  2 648 808 desviaciones de alineación; `regress` **220 golden, 0 cambiados**; `target_budget` **39/39**;
  compilación AC791N de los dos perfiles.

## 2026-11-10 — la suite completa, la firma por ssh y el build reproducible, medidos

- **Suite completa: `ALL HOST TESTS PASSED`.** `NO_PACKAGE=1 ./tests/run_tests.sh` sobre `c8715e4`, de punta a
  punta: **4 822 comprobaciones `ok`, 0 `FAIL`, 0 grupos con fallos**. Dos bloques se saltaron por diseño y lo
  dicen en el log: el emulador de navegador (no hay `emcc` en esta máquina) y el manifiesto de build
  (`NO_PACKAGE=1` no produce artefactos). Clase de la medición: host x86_64, runners nativos.
- **Firma desde un host aparte, por ssh.** Se levantó un `sshd` propio en `127.0.0.1:2222` con `ForceCommand`
  limitado a `sign-stdin`, la clave `0400` en su propio directorio y una copia del tool: el proceso de build
  nunca tuvo la clave. `fm1_sign.py sign --request … --signer-cmd "ssh -p 2222 …"` devolvió un manifiesto
  **idéntico byte a byte** al publicado: misma firma `20842d8f…`, mismo `sha256 81c80e36…`, y `verify` lo
  acepta. Es la misma máquina y el mismo uid, así que prueba el procedimiento y el contrato del firmante, **no**
  una separación física ni un HSM: eso queda declarado, no insinuado.
- **El guard de custodia rechaza de verdad.** El primer intento en ese host fue `refused: the private key is
  inside the repository (release.pem): key material in the checkout is treated as leaked` — la copia del tool
  considera repositorio a su propio padre, y la clave estaba dentro. Se movió la clave fuera de ese árbol y
  volvió a firmar. No fue un obstáculo a rodear: fue la comprobación haciendo su trabajo.
- **El paquete publicado es reproducible.** Un `git worktree` en `61ae47b` y `./build.sh --release 2.1.1`
  producen `81c80e365bf9ecec…c563`: el **mismo SHA-256** que `release/felucca-2.1.1.fwsc`, con el mismo
  registro de build (`build_sha256 83cebf6d…`). Los bytes no dependen de cuándo se compilen.
- **Sigue abierto:** leer el estado de las ejecuciones de Actions. No hay token `gh` en esta máquina y la API
  pública está limitada por IP; lo verificado es el resultado (Release con los bytes correctos, sitio sirviendo
  2.1.1), no el color de los runs. No se reporta como verde lo que no se pudo leer.

## 2026-11-10 — EDIT: LOFI estrena panel propio, MORPH dice dónde está

- **LOFI EDIT ya no es el panel genérico.** Sus dos páginas (`CHIP` y `TONE`) caían en el "being" que comparten
  casi todos los motores (WAVE 1 / WAVE 2 / MIXER), que no distingue un chip de un órgano. Ahora dibujan lo que
  este motor es, y sólo con los parámetros que el DSP lee: la traza real de la salida, el conversor y la forma
  de onda **por su nombre** (tomados de los propios descriptores EDIT del motor, así que una forma nueva no
  obliga a tocar la UI), el ancho de pulso sobre su periodo, la escalera del crusher encendida hasta donde llega
  `CRUSH`, y una línea de tiempo de `T.CLK` donde **`NOTE` reinicia la fase en cada nota** y **`FREE` la deja
  correr** mientras la nota se suma. Es el mismo dato que ya sonaba: no se añadió ni un parámetro.
- **MORPH: la posición, visible.** La curva ya era la mezcla real, pero el panel no decía cuánto del recorrido
  A→B estaba hecho. El borde inferior del panel es ahora ese recorrido, con la marca en el punto del knob.
- **Medido, no supuesto.** `tests/ui_render.c` incorpora `edit_lofi` y `edit_lofi_free`, así que el barrido
  cubre las dos páginas y las dos posiciones de `T.CLK`: **176 pantallas × 17 paletas, 0 hallazgos** de lint, 0
  tintas sobre su propio color, 0 píxeles fuera de gris/neutro, y **0 de 2 648 608** mediciones de alineación
  desviadas. `regress`: **220 golden renders, 0 cambiados**. `ui_test` verde. Imagen AC791N **537 328 B**
  (+828 B de código), RAM **94 924 B sin cambio**, POOL libre **9 248 B**, `target_budget` **39/39**.
- **Lo que sigue pendiente, dicho:** el rediseño de maquetación de **FM6** y del resto de los motores, y la
  matriz de trazabilidad completa. Se ven a Preview; el release instalable sigue siendo el **2.1.1** firmado.

## 2026-11-10 — v2.1.1 firmado y publicado: la clave ya no está donde se compila

- **`release/felucca-2.1.1.fwsc` (610 959 B, SHA-256 `81c80e36…c563`, identidad FM-1_982) está firmado** con
  `qg1-release-1` y es lo que el canal estable instala. Compilado aquí con el toolchain AC791N desde el commit
  `61ae47b`; `verify` lo acepta contra la clave fijada en `tools/fm1_keys.json`.
- **La firma se hizo sin la clave junto al build.** `fm1_sign.py request` congela el paquete en un documento
  `qg1-sign-request-1` (SHA-256, tamaño, hashes de `felucca.bin` y `loader/ota.bin`, registro de build, versión,
  `min_firmware`, id de clave y fecha) sin tocar ninguna clave. `fm1_sign.py sign --signer-cmd` entrega ese JSON
  canónico a un comando que sí tiene la clave —en esta pasada, `sign-stdin` sobre el mismo host— y **verifica la
  firma contra la clave fijada antes de escribir el manifiesto**: un firmante con otra clave, basura en stdout o
  un exit distinto de cero no producen archivo. `--key` y `--signer-cmd` son mutuamente excluyentes.
- **CI puede pedir una firma pero no firmar.** `emulator.yml` genera `build/sign-request.json` y lo sube como
  artefacto `sign-request-<sha>`; **no** hay secreto de firma en Actions. `signed-release.yml` (nuevo) verifica en
  cada push a `main` que cada `.fwsc` de `release/` verifique contra su manifiesto y la clave fijada, que no haya
  un `.fwsc` sin manifiesto, que `SHA256SUMS` describa exactamente el directorio, y corre las pruebas de custodia.
- **El sitio y el CI eligen el release más nuevo por número de versión**, no por el primer nombre del directorio:
  con dos versiones firmadas, `felucca-2.0.0` ordena antes que `felucca-2.1.1` y la selección anterior habría
  dejado el canal estable en 2.0.0. La misma selección alimenta el worktree del emulador, la portada y el chequeo
  del deploy.
- **`tests/sign_request_test.py` (nuevo, 27 comprobaciones):** las dos vías (`--key` y `--signer-cmd`) escriben el
  mismo manifiesto byte a byte —la firma Ed25519 es determinista— y se rechazan los caminos equivocados: salida que
  no es una firma, comando que falla, clave distinta a la fijada, sin pin ni `--expect-pub`, las dos banderas,
  ninguna, un paquete que no es el congelado, una versión que el documento no lleva, un manifiesto editado después
  de firmar, y `sign-stdin` con la clave dentro del checkout.
- **El copy del portal ya no dice 2.0.0**: describía la versión anterior en la portada que instala 2.1.1. Ahora
  nombra el release firmado más reciente y el panel de versión lo concreta desde `versions.json`.
- **Medido aquí, en esta pasada:** build AC791N real de 2.1.1, firma y `verify`, `sign_test` (12),
  `key_custody_test`, `sign_request_test` (27), `test_site` sobre los dos canales (0 FAILED), `test_i18n`,
  `test_installer_i18n`, y los dos que la entrada de 2.1.1 daba por abiertos: `drumx_test` termina en «all checks
  ok» y `six_track_live_test` en «all passed» (61 de 61 segundos contra el golden). La suite completa
  (`tests/run_tests.sh`) no se corrió entera en esta pasada.
- **Lo que no cambia:** nada se probó en una FM-1 física; la compatibilidad con `0x856014` y `0xB36014` sigue sin
  medirse en hardware, y el loader no verifica firmas (esto es autenticidad del paquete, no arranque seguro).
## 2026-11-10 — LOFI `T.CLK` audible, y la suite entera en verde

- **LOFI RETRIG no hacía nada, y ahora sí.** El parámetro existía y llegaba al DSP, pero el único punto que lo
  aplicaba era el re-disparo de una voz **sonando**: en POLY cada nota es una voz nueva que ya nace en cero, así
  que el interruptor pedía un reinicio que ya había ocurrido. Pasa a ser **`T.CLK`**, el reloj de la fórmula:
  `NOTE` (por defecto, y lo que esperan los presets de fábrica) arranca cada nota desde el principio; `FREE` deja
  la fórmula corriendo sobre el reloj de la pista y la nota se suma donde está, así dos notas seguidas son dos
  lecturas distintas de la misma fórmula en marcha. Medido en `tests/lofi_tclk_test.c`, en audio: piso del arnés
  0,134 de la señal, `FREE` separa dos notas en 1,140 (8,5× el piso), el nivel se mantiene a 0,6 dB, sobrevive a
  un cambio de sonido y no hace clic en el ataque.
- **`drumx` PUNCH, verde por la razón correcta.** La transferencia se medía con el limitador que dejaba el bloque
  anterior y con el fader que traía el preset. El par se mide ahora desde un kit limpio, con el nivel fijado, la
  matriz de modulación en cero y un golpe de calentamiento descartado: ataque +3,4 dB, cuerpo ×1,77, cola ×0,99.
- **`live` (segundo a segundo), verde.** Los 61 hashes eran de antes de que un sonido llevara su propio nivel;
  regenerados con el procedimiento que el propio test documenta (`SIX_LIVE_ACCEPT=1`).
- **La suite entera pasa**: `NO_PACKAGE=1 sh tests/run_tests.sh` termina en `ALL HOST TESTS PASSED`.
- Los rediseños de EDIT (LOFI, MORPH, FM6), las gráficas unificadas, BI-QUAD EQ, HOME y el visualizador **no**
  entran en esta pasada y quedan declarados como pendientes.

## 2026-11-10 — 2.1.1, comprobado por donde se usa: la página publicada y el DSP

Esta pasada no añade pantallas: comprueba con instrumentos lo que ya había en el árbol, y arregla lo que un
instrumento encontró mal.

- **El instalador publicado, abierto como lo abre un visitante.** La página responde 200 y no deja ni un mensaje
  de consola; sus tres peticiones (página, `.fwsc`, `.manifest.json`) responden 200; ofrece `v2.0.0-2500153`,
  610 959 B, «Ready to flash», con `SHA-256 verificado ✓ c423b17c…` y `Ed25519 qg1-release-1`; enseña el plan
  antes de escribir (143 sectores, `0x04000-0x92FFF`, boot `0x00000-0x03FFF` intacto, factory `0xFF000`), e
  **INSTALL está deshabilitado hasta marcar las cuatro confirmaciones y se habilita al marcarlas**. No había
  defecto de implementación que desbloquear: el botón lo cierra la evidencia y nada más. Registro y clase de la
  medición: `docs/VALIDATION_MATRIX.md` §22.
- **`tests/fx_probe.c`, un arnés para lo que no se podía medir.** El PHASER mide **23,6 dB de peine** entre
  200 Hz y 6 kHz y **0 de salto** al cambiar DEPTH; M-WIDE da r = **1,000** a 0 (mono), **−0,003** a 64 y
  **−0,59 con +3,8 dB** a 127; el EQ (`LOW SH −24`, `PEAK +12`) es monótono y llega a **+11,7 dB a 665 Hz**.
  La primera versión del arnés decía «1,7 dB, débil» porque medía un tono fijo mientras barría el LFO: estaba
  mal el instrumento, no el phaser, y por eso el DSP no se tocó.
- **DRUM-X PUNCH era el que estaba mal.** Pasaba **+0,4 dB** porque el kit sintético del test cargaba a nivel
  120; con el kit a 100 (como suena) el PUNCH da **+4,2 dB**. `tests/drumx_test.c` lo fija y el golden live
  (61 s) se regeneró a propósito.
- **Un medidor, no dos.** El MIXER dibuja ahora con `gw_plasma_bar()`, la función de HOME, en vez de una barra
  propia: los seis tracks se leen con la misma escala y los mismos segmentos en las dos pantallas.
- **El barrido de UI, a cero.** 174 pantallas × 17 paletas: **0 hallazgos** del lint de maquetación, 0 tintas
  sobre su propio color, 0 píxeles fuera de gris/neutro en GREY y MONO, y **2 645 631 mediciones** de alineación
  en 73 puntos con **0 desviaciones de 1 px**. Los 220 renders golden siguen idénticos y `target_budget`
  informa **39 ok, ninguna fuera**.
- **La firma sigue siendo la que bloquea.** `~/.fm1-keys/` no existe en esta máquina: no hay clave privada
  `qg1-release-1` que usar, y no se genera una nueva porque eso movería la identidad de confianza de la página.
  Lo que se instala sigue siendo **2.0.0**, firmado.

## 2026-11-10 — v2.0.0 publicado: firma real, release real, dos canales

- **La firma Ed25519 vuelve y es la que decide.** `release/felucca-2.0.0.fwsc` (610 959 B, SHA-256
  `c423b17c…9bd2`) está firmado con la clave existente `qg1-release-1` (pública `70b6036f…fcd40`, fijada en
  `tools/fm1_keys.json` y en la página). El instalador verifica el SHA-256 del paquete, la firma contra esa
  clave y los hashes de `felucca.bin` y `loader/ota.bin` antes de habilitar INSTALL; sin las tres, sigue
  deshabilitado y dice cuál falló. La entrada de abajo («sin firma en el repo») queda como historia.
- **La firma nombra el firmware, no el archivo.** `images` son la imagen de la app y el loader —los hashes que
  todo registro de build usa—, y el paquete se cubre con `sha256`. Con el nombre del `.fwsc` dentro de `images`
  el sitio rechazaba una firma válida, porque ese nombre cambia con la etiqueta de la versión.
- **El manifiesto firmado lleva su registro de build** (`qg1-build-1`): commit, versión y hashes de los
  artefactos viajan dentro de la firma, así la página que instala dice qué commit escribe sin fiarse de otro
  archivo.
- **Dos canales, sin ambigüedad.** `/` es estable: el release firmado y un emulador compilado desde el mismo
  commit que produjo el paquete. `/preview/` es `main` como está ahora, con su emulador y su commit, y el
  instalador de ese canal se niega a habilitar INSTALL porque el paquete no está firmado.
- **Sin requisito manual de M-Flasher** (ya no aparece ni en el instalador, ni en `/plan/`, ni en las cadenas
  en/es/zh): el loader que la unidad de fábrica prepara lee el chip (`0x856014` / `0xB36014`) y rechaza
  cualquier otro antes del primer borrado.
- **Release publicado:** tag `v2.0.0` → `896797e`, con el paquete, su manifiesto y `SHA256SUMS` descargados de
  vuelta y verificados. Compilado aquí con el toolchain AC791N (`./build.sh --release 2.0.0`).
- Suites: `sign_test`, `key_custody_test`, `install_test`, `gate_test`, `mflasher_test`, `mflasher_accept_test`
  (14), `test_accept` (20), `test_state` (65), `test_update_mode` (12), `test_verify_only` (150), `test_web`,
  `test_sign` (12), `test_backup`, `test_site`, `test_i18n`, `test_installer_i18n`, `tests/ldr_test` (28) y
  `tests/flash_id_test`. **Ninguna unidad física fue tocada**: los dos chips están permitidos por código, no
  medidos en hardware.
## 2026-11-10 — 2.1.1: el árbol, medido; la instalación, la que está firmada

- **La versión es `v2.1.1`** en el firmware, el wasm, el sitio y esta documentación. Lo que se instala sigue
  siendo el paquete **firmado**, hoy `2.0.0`: firmar un 2.1.1 necesita la clave privada `qg1-release-1`, que
  no está en el repositorio ni en CI. Queda como bloqueo declarado, no como tarea hecha.
- **La rotación de los planetas de HOME, medida en el navegador.** Emulador servido en local y abierto en el
  navegador cloud: dos capturas separadas 2,6 s, **868 de 57 600 píxeles cambian**; en una ventana de 300 ms
  cambian **576**, más de lo que mueve una órbita de 7–13 s por vuelta, así que gira la superficie de cada
  cuerpo y no sólo la órbita. Capturas, recorte ×4 y diferencia en `docs/evidence/3.3/`.
- **El audio de LATCH, medido.** `tests/perform_test.c` gana `test_latch_audio()`: con LATCH ON el filtro
  sigue filtrando después de soltar la tecla; el segundo toque devuelve la señal seca bit a bit; con LATCH OFF
  vuelve seca; REPEAT 1/16 sigue sonando tras soltar. Audio renderizado, no una variable booleana.
- **Cinco aserciones que quedaron viejas, corregidas.** El commit que hizo que un sonido lleve su propio nivel
  (`preset_level.c`) dejó a `ui_test` y `editor_test` exigiendo el contrato anterior ("un load no toca
  `LEVEL`"). Ahora comprueban el nuevo — `LEVEL` es el del sonido, medido — y `editor_test` verifica además
  que el load **empuja** ese nivel. Los dos binarios pasan enteros.
- **Lo que sigue abierto:** `drumx` (PUNCH) y `live` (segundo a segundo) fallan en `main` antes de esta
  pasada, por el mismo cambio de nivel entrando en todo render; no se tocan aquí. La matriz Control →
  Parámetro → Destino DSP → Efecto audible y las mejoras musicales baratas siguen pendientes. Nada de esto se
  probó en hardware.

## 2026-11-10 — el instalador no pide ningún informe de M-Flasher

- **Fuera el informe de M-Flasher** del flujo visible: se quitaron la caja de "registro + dump", la guía de
  M-Flasher del panel de verificación, sus cadenas en en/es/zh, `web/fm1record.js` y su test. La identificación
  del chip la hace la unidad: el loader del paquete lee el chip antes de borrar nada (`ldr_flash_known`) y una
  unidad que ya corre QG responde 0x12 en su propio enlace. Las comprobaciones de seguridad siguen en pie
  (modelo, geometría, allowlist de partes, rango de escritura `[0x4000, 0x93000)`, datos de fábrica).
- **El plan antes de escribir** es su propia página (`/plan/`): lee el paquete que sirve el sitio, lo hashea
  contra su SHA-256 publicado y muestra, con el dump del usuario, los sectores exactos que se van a escribir.
  El lanzador de Windows abre esa página primero.
- **`tests/fixtures/felucca-v1.9.0.fwsc`** vuelve al repo (610806 B, sha256 `80ad995c…`): el CHANGELOG lo
  prometía, tres tests lo necesitaban y sin él dos suites no corrían.
- Créditos: Pocket GammaWave queda a nombre de **Frederic Valente (@fvalente)** y se suma
  **David Konsumer (@konsumer)** por la primera instalación en stock, el chequeo de chip del loader, la
  reanudación y los releases por tag.

## 2026-11-10 — releases de GitHub, sin firma en el repo

- **Un solo canal.** El sitio sirve el build de `main` y el instalador lo puede instalar: comprueba el SHA-256 y
  el tamaño publicados antes de escribir, y el loader comprueba el chip flash antes de borrar nada.
- **Los releases son releases de GitHub**, compilados por CI desde un tag (`release.yml`, `docs/RELEASE.es.md`).
  Ya no hay `release/` en el repo, ni firma Ed25519, ni registro de procedencia que el instalador exija.
- Se quitaron `tools/fm1_sign.py`, `tools/fm1_keys.json`, `web/fm1provenance.js` y sus tests. El paquete
  v1.9.0 queda como fixture de tests en `tests/fixtures/`.

## 2026-11-09 — 2.0: el emulador público pasa a ser este árbol, y los planetas giran

La queja era concreta: el emulador publicado mostraba un firmware más viejo que el repositorio. La causa no era
caché ni un workflow roto, era la arquitectura del sitio: se publicaba *un* release — paquete y emulador del
commit firmado — porque el instalador ata un paquete al emulador que su registro nombra. Con `main` por delante
del release, la única forma de no romper esa atadura era servir páginas viejas. Ahora son dos canales.

- **`/` sirve este árbol.** El emulador, el paquete y el registro de build salen del mismo commit; el pie de la
  página dice versión, commit, fecha de compilación, SHA-256 del wasm y tamaño, y la página se niega a arrancar
  si el archivo descargado no coincide con el hash que lleva escrito. Un manifest más nuevo la recarga una vez,
  así que una página cacheada no puede seguir sirviendo un motor viejo.
- **`/release/` conserva el release firmado (`v1.9.0`).** Ese canal se construye desde el commit del release
  — portal, instalador, paquete y emulador juntos — en vez de emparejar un paquete firmado con un emulador más
  nuevo. Es el único paquete que el instalador acepta, y sigue siéndolo: el 2.0 necesita la firma del custodio.
- **El paquete de la portada es el de `main` y no está firmado**, así que el instalador lo publica y se niega a
  escribirlo. Eso es fail-closed, no un ajuste pendiente, y la portada lo dice en la tarjeta de versión, en la
  nota de versiones y en el primer párrafo de Instalación, con enlace al canal firmado.
- **Las teclas de SCL dejan de ser pastillas.** La pasada 3.2 les puso una esquina de 6 px sobre una tecla de
  29 px; vuelven a 3 px, con el mismo bisel, el mismo contorno de 1 px y la misma fila iluminada.
- **Los planetas de HOME giran sobre su eje.** Antes el sistema orbitaba y cada cuerpo era una imagen fija dando
  vueltas; ahora cada planeta lleva dos rasgos de superficie en sus propias longitudes que barren la cara
  iluminada — `x = r·sin(fase)` los comprime contra el limbo y `cos(fase)` los esconde por detrás — con su
  propia velocidad de giro (2,5 s a 4,7 s por vuelta) y una inclinación de brillo que cae hacia el terminador.
  Una nota enciende el planeta: `pl_flash` sube la cara iluminada y su limbo durante los primeros fotogramas y
  decae más rápido que la excitación, así que el golpe se lee como golpe y el rescoldo como rescoldo. Coste:
  un par de senos y hasta cuatro píxeles por planeta y fotograma, sin tabla nueva y sin memoria nueva.
- **La versión es `v2.0.0`.** El firmware, el wasm, el sitio, el Web Studio y la documentación dicen lo mismo; el
  paquete firmado que se puede instalar sigue diciendo `v1.9.0` porque es el que está firmado, y ambas cosas se
  ven a la vez en la portada.

Verificación de esta pasada en `docs/VALIDATION_MATRIX.md` §35: build AC791N y wasm de este árbol, suite de host,
emulador de Node, las dos páginas leídas del sitio público por HTTPS y el navegador sobre la URL real. Nada de
esto es una prueba en hardware: ninguna FM-1 física ha sido escrita.

## 2026-11-09 — el ecosistema al día: instalador, Web Studio, capturas y documentación

No cambia el firmware, así que el paquete firmado `v1.9.0` sigue describiendo este árbol. Lo que cambia es lo que
el repositorio, el portal, la página del instalador y el Web Studio dicen, y cada afirmación de aquí tiene su
prueba en `docs/VALIDATION_MATRIX.md` §32.

- **La página del instalador pierde el preview del navegador.** La sección `QUANTUM GAMMAWAVE — LIVE BROWSER DSP
  PREVIEW` se va entera: rótulo, gráfico, textos, botones, sus reglas CSS, su bloque de script (~14,5 KB) y las
  claves `previewTitle/synPlay/synDice/synStop` con sus frases en los dos diccionarios. No queda contenedor vacío:
  debajo del gate está el gate. `web/test_web.mjs` pasa a exigir la ausencia en vez de comprobar una opción de la
  demo borrada.
- **El Web Studio enseña menos y dice la verdad.** Los diez botones de vista del LCD son un `<select>` con las
  mismas diez vistas más `FULLSCREEN` (GEN y VIZ siguen pasando página al reelegirse, y el clic en el lienzo
  sincroniza el selector); los pasos 2, 3 y 4 del deck están plegados en `<details>` con un resumen de una línea y
  el atajo de hardware dentro. La página lleva `data-version="{{VERSION}}"`, que `web/make_site.py` exige y rellena
  como el token del portal, así que el dispositivo simulado reporta `v1.9.0` y no el `v1.0` que decía. `PALETTE`
  pasa a `STUDIO THEME` (repinta la página, no la paleta del aparato) y la barra de marca deja de anunciar
  `WEB STUDIO 2.0`, que no es una versión publicada.
- **Banner nuevo, dibujado por código.** `tools/generate_banner.py` compone 1600×560 desde la paleta QUANTUM
  decodificada de `ui_palettes.h`: campo de estrellas, planeta anillado con dos lunas y órbitas, wordmark Michroma,
  cinco fichas y la versión leída de `felucca.c`. La ficha de presets decía **207** y ahora dice **200**: las tablas
  del firmware tienen 200 sonidos navegables más dos kits DRUM retirados que siguen en los índices 10 y 11
  (`tests/preset_library_test.py`).
- **Capturas regeneradas.** `tools/generate_screenshots.py` lee `FELUCCA_VERSION` de `firmware/src/felucca.c`
  (decía v1.8.6) y renderiza la UI actual; se añaden `26_boot_splash`, `27_delay_bars`, `28_reverb_reso`,
  `29_reverb_cloud` y `30_punch_in_latch`, y las 25 anteriores se vuelven a renderizar.
- **El portal deja de anunciar 1.8.9.** La tarjeta decía «Qué cambia en v1.8.9» en un sitio que ya servía 1.9.0;
  ahora describe 1.9.0 (paquete firmado, los cuatro gates del instalador, el informe de diagnóstico, Emscripten
  6.0.11 y la pasada 3.2) y dice sin rodeos que no se ha instalado nada en una FM-1 física. El enlace de estado
  apunta a la matriz de validación, y `web/test_i18n.py` pasa con las entradas EN y ZH nuevas.
- **README puesto al día.** La sección de build actual nombraba 1.8.9; ahora es la de 1.9.0, con lo que ese build
  hace y lo que no reclama. El changelog deja de saltar de v1.7 a v1.9. El enlace a `firmware/versions.json` es
  absoluto, porque ese archivo sólo existe en el sitio publicado. La cifra de la biblioteca se comprueba contra su
  test (200). Las dos capturas de interfaz son nuevas, a un solo viewport (432 × 690).

## v1.9.0 — el paquete firmado y el instalador que decide con lo que lee

- **La instalación deja de estar bloqueada por software.** El paquete v1.9.0 está firmado con la clave anclada
  `qg1-release-1`, su registro nombra el commit del build con `dirty: []`, y dos builds dan los mismos bytes.
  El instalador identifica el NOR por `0x12`, rechaza cualquier pieza fuera de `0x856014`/`0xB36014`, exige que
  la geometría coincida con la pieza identificada, verifica la firma contra la clave pública anclada y, tras
  escribir, exige que la unidad reporte el producto QG. Lo que sigue pendiente no es código: es una lectura de
  la unidad del dueño, un dump de 1 MiB propio y la ruta de recuperación ensayada
  (`docs/FORENSIC_FM1_VIABILITY.md`).
- **Verify Only exporta su propio informe.** La página del instalador descarga un `qg1-diagnostic-1` JSON con
  lo que leyó, lo que no pudo leer y por qué: sin claves, sin tokens y sin bytes del paquete. Los ocho estados
  (`DIAGNOSTIC READY` … `INSTALL READY`, `INSTALL BLOCKED`) se derivan de las filas del gate, así que ninguno
  puede mostrarse por encima de su evidencia (`web/fm1diag.js`, 108 comprobaciones en `test_verify_only.mjs`).
- **El release se publica solo, y sólo si se verifica.** el job `publish` de `.github/workflows/emulator.yml` comprueba firma,
  hashes, identidad, versión y que nada del firmware haya cambiado desde el commit del registro; crea el tag
  `v1.9.0` en ese commit y publica el paquete, su manifiesto y el registro. Después vuelve a descargar los
  assets publicados y los verifica otra vez.
- **Pages sirve el paquete firmado.** El workflow del emulador prefiere `release/*.fwsc` cuando existe: un
  paquete construido en CI no lleva firma (la clave privada no está en CI) y el instalador rechaza bytes sin
  firmar, así que publicar el de CI sería publicar algo que no se puede instalar.
- **Emscripten queda en 6.0.11 en todas partes**, que es la versión que los workflows fijan; el wasm de este
  release se construyó con ella desde el mismo commit que el firmware.

- **3.2 — el LATCH por defecto, DEPTH en el knob 2, y todo lo que se ve, probado en el emulador.** El LATCH de
  PUNCH-IN no fallaba en el motor: sus 15 teclas, el límite de tres simultáneos, el NOTE OFF que lo libera, los seis
  mutes y el cambio de banco ya pasaban una matriz nueva y completa (`tests/ui_test.c`). El defecto era alcanzarlo:
  el modo venía en OFF y su interruptor sólo estaba en MENU y en un combo oculto cuyo footer desaparecía justo con
  el LATCH apagado. Ahora `prefs_update()` lo enciende una vez por revisión de ajustes (`prefs_rev = 3`) y la tarjeta
  del mapa dice `OCT+ TO SWITCH`. DEPTH pasa a KNOB 2 y CRUSH a KNOB 4 (los slots del motor no cambian). Los
  porcentajes de FX se alinean a la derecha contra su medidor y ya no tocan el borde al llegar a 100 %. SCL deja el
  dígito de grado (las teclas blancas dicen sólo la nota) y sus teclas se suavizan con esquina mezclada y un solo
  píxel de luz. El Delay dibuja sus siete barras siempre visibles (3 px de mínimo, 7 de ancho, puntas redondeadas,
  subida instantánea con 3 px de rebote y bajada de 1 px por frame, sin trabajo extra en audio). RESO y CLOUD dejan
  de ser la misma partícula: RESO es un resolutor con cuatro cuerdas y puntos de energía recorriéndolas; CLOUD es un
  enjambre de tres profundidades que respira y hace un "puff" en cada nota. HOME pasa a cuatro planetas (rocoso con
  cráteres, bandas, océano, anillado) con órbitas más grandes, anillo de nota y excitación por nota. El Boot Splash
  se rehace como identidad de 16 bits: cielo con tres nebulosas elípticas y cuarenta estrellas en tres capas,
  `QUANTUM`/`GAMMAWAVE` en alfabeto de bloque propio, planeta anillado con dos lunas, riel de dieciséis bloques,
  BOOT/% y hand-over a HOME, 3.1 s en total (antes 2.3 s) sin retrasar audio, MIDI ni init. Medido: `ui_render`
  con 174 pantallas × 17 paletas, 0 hallazgos de lint y 0 desalineaciones; la línea de tiempo del splash son 775
  pasos de 4 ms, 176 cambian píxeles; el emulador se compiló (wasm 1 086 293 B en la captura, 1 079 658 B tras fusionar la
  rama de instalador/optimización) y la secuencia se capturó dentro del emulador. El build AC791N compila (imagen 535 204 B) con `.data+.bss` 94 936 B de 98 304 y 9 248 B de spare en el pool.
- **3.1 — la pasada visual que el dueño pidió, medida y probada.** HOME: el sistema solar se redibuja con
  planetas de 2/3/4 px (órbitas 14/23/32, radios verticales 7/11/13), estrellas fijas precalculadas con
  parpadeo lento, dos bandas de nebulosa por tabla, sombreado por fases con terminador, atmósfera, anillo sólo
  en el exterior, orden por profundidad y estelas cortas; el recorte es un `sys_box` local (no toca el camino
  caliente de `gfx.c`) y el pool de partículas baja a 28 con vidas 8–9, gravedad menor y un clip exacto del
  rect (un cuerpo de 2 px se trunca en el borde, la estela sólo se pinta si sus dos extremos están dentro).
  Visualizador: el tope del espectro se queda en su caja (`clamp(178-ph,153,178)`: antes flotaba hasta 27 px
  fuera) y los medidores de envío se clampan a su altura. FX 1: DIST/CHORUS/DELAY/REVERB se muestran en 0–100 %
  (`(val*100+63)/127`) sin cambiar el rango MIDI interno 0–127 ni los presets. SCL: teclado rediseñado (teclas
  29×60, negras 18×38, franjas de escala en el color del tema, grado y nota abajo, barra de nota sonando) para
  que deje de leerse como teclas naranjas/celestes enteras y sea coherente en los nueve temas. Punch-in FX: las
  seis teclas de mute dicen **M1–M6** (la asignación MIDI no se movió; `FX_BLACK_W` 8 → 10 para que la M quepa)
  y el pie anuncia `OCT+ LATCH`. **FX LATCH ahora se cambia desde el mapa**: con FX armado, OCT+ alterna
  `PREF_LATCH` y `perf_latch_on`, limpia los latches, guarda el ajuste y lo dice ("FX LATCH ON/OFF"); con LATCH
  en OFF, OCT- sigue siendo la octava y OCT+ deja de serlo mientras el mapa está abierto; con LATCH en ON,
  OCT- sigue siendo ALL OFF. Dos defectos de fondo corregidos en el camino: `oct_taps()` no registraba la
  pulsación de OCT± con el mapa FX abierto y LATCH en OFF (el tap moría antes de `layer_oct`), y el primer
  OCT- tras abrir el mapa se lo comía la máscara del LATCH (una pulsación que empezaba en el mapa y se soltaba
  con el mapa ya cerrado, como el CANCEL de un dado, se quedaba sin efecto). Master FX: fuera la banda
  `UN: 100% / CPU / PK`, VU a y 80/87 con un solo rótulo `OUT`/`CLIP` en (108,66), sin solaparse. REVERB: la
  animación lee el retorno real del bus (`fx_vu[5]`) con una envolvente balística (sube al instante, baja 7/8
  hacia el nivel actual por frame dibujado: sin oscilar bajo un retorno constante, sin división y llegando a 0)
  y la fase mueve los anillos; nada por muestra. **Medido en host x86 (cc -O1), no en el AC791N** (tres
  pasadas de cada árbol, mediana): HOME redibujo completo 151,1–151,6 → 164,1–164,6 µs (+8,6 %), frame sin
  `force` 44,1–44,9 → 45,1–45,2 µs (+1,6 %), visualizador 79,4–79,5 → 83,0–83,2 µs/frame HIGH (+4,6 %);
  `.data` 58 016 B en ambos y `.bss` 5 010 016 → 5 010 048 B (+32 B, `size` de `build/ui_render`, que incluye
  el harness host y no es el presupuesto del linker). Verificación: `tests/ui_test` 868 comprobaciones, 0 fallos (incluye el nuevo
  `FX + OCT+ toggles LATCH` y el de la reverb, que exige que los anillos sigan al retorno y decaigan hasta 0);
  barrido de render 173 pantallas × 17 paletas, **0 hallazgos de lint**, 2 643 516 medidas de alineación y
  **0 desviaciones de 1 px**; `tests/particle_bounds_test.py` gana las guardas geométricas del sistema solar
  (órbita + planeta + 2 px dentro de la caja, sistema centrado). **Build AC791N de este mismo árbol** (clase
  `[HOST]`: toolchain JieLi `20260730.1` verificada por sha256 y SDK `AC79NN_SDK_V1.2.1_2023-12-13`): imagen
  531 992 B, `.data+.bss` 95 912 B de 98 304 (2 392 B libres), pool spare 9 248 B, paquete 610 806 B `FM-1_982`,
  sha256 `80ad995c…`, idéntico byte a byte en dos builds limpios; `memory_map_test`, `target_budget`,
  `test_pkg_bounds` y `build_manifest --verify --require-clean` en verde. Esos bytes son **los mismos que el
  paquete firmado de `release/`**: un rebuild independiente en otra máquina reproduce el artefacto publicado, y
  el `.data+.bss` de 95 912 B sustituye a los 96 088 B de la entrega anterior (la pasada visual no costó RAM).
  También se construyó el emulador aquí (Emscripten 6.0.11, wasm 1 073 325 B, sha256 `3d039f75…`, registrado en
  el manifiesto al mismo commit) y la suite host completa pasó con el paquete presente. El paquete publicado —tag
  `v1.9.0`, el Release y la copia de Pages, sha256 `80ad995c…`— es exactamente el que sale de este árbol, así que
  un segundo build independiente confirma que el artefacto publicado es este firmware. Nada de esto es una
  validación de hardware.
- **El primer backup de una unidad stock ya no necesita abrir el equipo, y el instalador perdió su último atajo:**
  M-Flasher 0.9.1 (`9da825b`, MIT) lee la flash entera por el modo ROM del chip, con el loader en RAM y el
  CRC32 calculado por el silicio; su acción de backup sólo lee (`src/ops.rs`), y ahora `tools/fm1_mflasher.py`
  convierte su pareja `.bin`/`.json` en un archivo `qg1-backup-1` que `fm1_restore.py` valida, exigiendo dos
  backups idénticos o el CRC32 del chip, y decodificando la identidad del `app.bin` con la clave `0x980F`
  (`tools/fm1_image.py`). Ese decodificador corrige una premisa falsa: el dump stock público **no** contiene
  `FM-1_` en claro, así que buscar la cadena en los bytes fallaba justo en el caso que importa. Además se
  retiraron `--jedec`, `--allow-unidentified-chip` y `FM1_LAB` de `fm1_install.py`, `fm1_preflight.py` y
  `fm1_gate.py`: el preflight lee ahora `0x12` como el instalador y decide con la misma función, y no queda
  ninguna variable de entorno ni bandera que aporte un chip o salte la firma. Probado con `mflasher_test.py`
  (32 comprobaciones, incluido el dump real importado y restaurado) y el resto de suites del gate. **Sigue sin
  habilitarse la instalación**: la clave de producción no está fijada y ningún requisito físico se demostró aquí.**
- **HOME: los planetas existen, giran con el reloj y contestan a lo que suena.** El cuerpo de cada planeta se
  dibujaba con un `span` que se recorría una sola vez y nunca volvía a subir: en la primera fila caía a 0 y
  todas las demás filas lo heredaban, así que el planeta quedaba de 1 px y lo único visible era el disco de
  atmósfera — de ahí los discos grises planos. Ahora el ancho de cada fila se recalcula por fila (raíz entera
  de `r² - dy²`, sin libm), y el planeta se ve: terminador real orientado a la estrella (`lx`/`ly`), tres zonas
  tonales en la cara iluminada, limbo oscuro de 1 px, anillo de atmósfera, especular y, en el más externo, el
  anillo pasando por detrás. Tamaños 3/5/6 → **4/6/7** px con órbitas 15/25/34 → 16/26/34 y una velocidad
  moderadamente mayor (7/5/4 unidades de 16 bits por ms: 9,4 s / 13,1 s / 16,4 s por vuelta). El movimiento es
  una función de `fm1_ms`, no un acumulador por frame, así que no depende del frame rate ni de la energía del
  audio y nunca salta. Los tres anillos salen de una tabla de 24 puntos (`gw_ring_tab`) en vez de 144 llamadas
  a `sine_i` por frame, y los planetas se pintan ordenados por su `y` cuando pasan por delante de la estrella,
  de modo que dos cuerpos que se cruzan no se leen como una mancha de dos colores. La reacción musical sigue
  por parejas de pistas (1/4 → planeta 0, 2/5 → 1, 3/6 → 2): una nota suma excitación (aditiva: un acorde o
  una nota rápida apilan, no reinician), que decae y mueve radio, calor, la estela sobre la órbita, chispas y
  el destello de la estrella; ahora además calienta el propio anillo de la órbita. Coste medido en el host
  (`build/host/ui_render`, cc -O1): HOME completo 154,7 → 173,1 µs (+12 %) y un frame sin `force` 44,3 → 49,8 µs,
  para un 45 % más de píxeles de cuerpo (220 → 319) y bastante más detalle. **Verificado en host**, no en el
  AC791N (aquí no hay toolchain pi32v2).
- **FX > DLY: las barras son las repeticiones del delay, no un vúmetro.** El motor no publicaba ninguna señal de
  eco, así que la página deducía "qué repetición suena" del nivel del envío y hacía botar las barras con el
  retorno: eso es un medidor de volumen. `fx_buses` mide ahora, dentro de su propio bucle y sin asignaciones,
  el pico que la línea devuelve en cada ventana de un tiempo de delay (`fx_dly_rep[0..5]`, `fx_dly_gen`,
  `fx_dly_frac`): un golpe en el envío arma el tren, la repetición n aparece en la ventana n, cae con FEEDBK y
  la ventana se cierra al terminar la sexta, con un decaimiento por bloque que llega a 0. La página (FX > DLY)
  dibuja seis ranuras fijas y una barra por repetición que existió, con una sola tonalidad y la repetición que
  suena marcada; sin ecos no hay barras. El audio no cambia: 220 renders dorados, **0 cambiados**. Nuevo
  `tests/dly_echo_test.c` (en `run_tests.sh`): silencio → nada; la repetición 1 llega exactamente un TIME
  después; FEEDBK 0 deja una sola pasada; FEEDBK 60 da cuatro repeticiones que caen ~42 % cada una; MIX 0 no
  mide nada; el doble de MIX duplica los picos (la barra sigue al eco, no al nivel); PING-PONG igual; y tras
  4 s de silencio todo vuelve a 0 (nada queda encendido). **Probado en host.**
- **Punch-in FX: un teclado, dos grises, en los 17 temas.** Dos tokens nuevos derivados de cada paleta —
  `T_KEYW` (66 % del fondo al texto) y `T_KEYB` (26 %) — sustituyen a la tecla blanca de keycap y a los agujeros
  `T_BG` de las negras: las blancas son gris claro, las negras gris oscuro, con borde propio, canto iluminado y
  borde inferior, y se distinguen entre sí en QUANTUM, CYBERPUNK, NORDIC, GREY, MONO y el resto. Una tecla
  pulsada toma el acento con la tinta encima y una línea de tope; en GREY/MONO, donde el acento es el propio
  texto, la pulsada va al gris oscuro con etiqueta clara, que es la única forma de que "pulsada" se distinga de
  "blanca" en un tema sin color. `tests/theme_test.c` fija ahora el contraste etiqueta/gris claro, etiqueta/gris
  oscuro y gris claro contra gris oscuro en cada paleta. **Verificado en host en las 17 paletas.**
- **GLO > MONITOR: N/A cuando no hay medida, y las filas que faltaban.** `load_diag.valid` se activa la primera
  vez que un bloque mide de verdad su tiempo, así que en el host y en el emulador WASM —donde `fm1_ticks()` no
  avanza dentro del render— la carga dice **N/A** en vez de un 0 % que parecía una medición. La fila de buses
  sigue ahí (con "N/A TK" cuando nadie la ha escrito) y junto a ella el aviso de TRIP cuando un bus se reinicia
  fuera de rango. Dos filas nuevas con datos que cambian solos: **SEQ** (estado, paso y compás reales de la pista
  seleccionada, `seq_src`/`bar_n`) y **FX RET** (los tres retornos medidos, `fx_vu[3..5]`, con "–" cuando un bus
  nunca ha devuelto nada) más LATE/SHED. Todo se lee de contadores que el audio ya escribe: ni una consulta
  costosa ni un cálculo dentro del hilo de audio.
- **GREY y MONO quedan grises de verdad (el resto del camino).** La saturación por canal de `ux_mix` ya entró en
  `73bd12c`; lo que faltaba era a quién le preguntan los gráficos de HOME: miraban `settings.palette` en vez de
  `ux.bw` (el MONO que se está dibujando), así que una paleta MONO guardada seguía dibujando en color. Ahora
  `bg_col`, `gw_embers` y el mini-knob leen `ux.mono || ux.bw`, y `tests/ui_render.c` cuenta **0** píxeles
  tintados en GREY y MONO sobre las 173 pantallas.
- **`ui_render` puede mirar una pantalla a ojo:** `QG_DUMP=<pantalla>[,<paleta>]` dibuja una sola pantalla con el
  `draw()` real y escribe un PPM (y `QG_DUMP_FX` pone un tren de ecos medido en FX > DLY). Así se revisaron el
  teclado nuevo, la página del delay y el monitor, pixel a pixel, en las 17 paletas.

- **CI en verde con las causas reales, no con el workflow apagado:** los tres workflows llevaban rojos
  desde `d32a600` (antes de este trabajo) y el patrón común era un fallo enmascarado por
  `continue-on-error` o por un paso que nunca llegaba a ejecutarse. Causas encontradas y corregidas, cada
  una reproducida en local sobre el commit que fallaba: (1) el job `host` instalaba Pillow, numpy y
  fonttools pero `sign_test.py`, `key_custody_test.py` e `install_test.py` importan `cryptography`, así
  que moría con `ModuleNotFoundError` en el primer `keygen` — se instala la dependencia, no se salta la
  prueba; (2) el registro de build marcaba el árbol como sucio porque la caché del toolchain restauraba
  en `.jieli` **dentro** del checkout y el SDK se clonaba en `$GITHUB_WORKSPACE/fw-AC79_AIoT_SDK`:
  `--require-clean` rechazaba un build limpio, y `dirty()` ahora ignora `$AC79_SDK` y `$JIELI_TOOLCHAIN`
  cuando apuntan dentro del árbol (el manifiesto ya los identifica por hash); (3) el `env:` de nivel de
  workflow usaba `runner.temp`, que no existe en ese contexto, y GitHub rechazaba el fichero con **cero
  jobs**; (4) el job de paridad WASM nunca generaba `build/gen` (`web/emu/build.sh` se negaba con "run
  ./build.sh once first") ni creaba `build/host` antes de compilar el binario nativo, y clonaba el SDK
  sin reintentos; (5) `web/make_site.py` rechaza cualquier etiqueta que no sea la versión del registro
  más su commit, y el workflow pasaba `1.9.0-<sha>-preview`, de modo que el sitio nunca se construía y el
  fallo aparecía un paso después como "no package in firmware/packages"; (6) el paso de reserva
  comprobaba `build/site/index.html`, una ruta que nadie escribe antes de que se ejecute, así que corría
  (y fallaba) en todos los builds. Además los dos gates de manifiesto y el de paridad publican ahora su
  veredicto como anotación `::error::` pública: antes el único rastro era "exit code 1" y el log exige
  permisos de administración. Estado verificado en `c70867f`: `host`, `AC791N build + memory budgets +
  manifest`, `FELUCCA_FLASH=0` y `WASM builds and matches the native render` en verde, y
  `emulator`/`deploy` en verde desde `d2fce1a` (sus filtros de rutas no se disparan con cambios de
  workflow). El build real AC791N sigue dando `FM-1_982`, 611163 B, y la paridad WASM es bit a bit con
  2,3 % de tiempo real. **Ninguna unidad física, ninguna clave real, ningún release publicado.**
- **El SVF cuesta 26 instrucciones menos, con el mismo sonido:** los dos estados del filtro
  (`tsvf_lp`, en el camino caliente de toda voz ANALOG, NOISE y DRUM-X) se limitaban con dos
  comparaciones por muestra; ahora hay una sola comparación sin signo por límite (`clamp_sym`, el
  mismo `clamp(v, -150000, 150000)` reescrito como `(unsigned)v + k > 2k` y el signo sólo cuando se
  sale). En la desensamblación pi32v2 el bucle por muestra de `analog_render` baja de 1250 a 1228
  bytes, la función de 609 a 603 instrucciones y su coste de 603 a 577 (**-4,3 %**, otra vez bajo su
  presupuesto de 581). El audio es idéntico: 220 renders dorados, 0 cambiados. `tests/clamp_sym_test.c`
  fija la identidad sobre todo el int32 (los extremos donde `v + k` desbordaría, 5 000 001 valores
  densos, 20 000 000 pseudoaleatorios); ASan y UBSan limpios. Medición estática, no del AC791N.
- **La carga de seis pistas, con una mano encima:** `tests/six_track_live_test.c` sostiene las diez
  voces y, mientras suenan, mueve corte, pan, nivel, un envío y el drive en cinco períodos distintos
  (para que coincidan en el mismo bloque), pasa una nota MIDI de un tono a otro 5 168 veces, cambia de
  preset 60 veces y sostiene el punch-in (REPEAT 1/16 y LPF) en 16 507 bloques. Mide media, p50, p95,
  p99, p99.9, peor bloque y deadline, con bloques tardíos, clips, muestras fuera de 16 bits, bloques en
  silencio, el mayor salto entre muestras y la posición de memoria del render; y hashea el audio una vez
  por segundo contra `tests/six_live_golden.txt` (61 hashes), así que un cambio en lo que suena falla
  aunque el tiempo no cambie. Una hora de audio (4 961 250 bloques) dos veces da el mismo hash
  `177b30fd8c1a114d` y p50 10,0 / 9,9 µs: nada deriva. `SIX_LIVE_SECS=n` alarga la corrida. Sólo host.
- **Procedencia del build, obligatoria antes de escribir:** cada build escribe `build/manifest.json`
  (`qg1-build-1`: versión compilada, commit, archivos sin commitear, identidad, SHA-256 y tamaño de cada
  artefacto, knobs `FELUCCA_*`, hashes del toolchain y del SDK, y el emulador del mismo árbol). El
  transporte ya no acepta un paquete sin ese registro: `requireProof()` exige un veredicto de
  `verifyProvenance()` (bytes y tamaño exactos, árbol limpio, commit completo, identidad y versión bien
  formadas, emulador del mismo commit) **antes de `find()`**, y el manifiesto de release firmado debe
  nombrar el registro (`build_sha256`, digest canónico) o se rechaza — así la firma cubre el registro y
  no sólo el paquete. `web/make_site.py` se niega a publicar un paquete que su registro no describe
  (otro commit, árbol sucio, otros bytes) y `fm1_sign.py` gana `--build-manifest` en `sign`/`verify`.
  Pruebas: `web/test_provenance.mjs`, `tests/manifest_test.py`, más casos en `web/test_web.mjs` y
  `tests/sign_test.py`; todos en `tests/run_tests.sh`. **Ninguna unidad física.**
- **Compuerta criptográfica en el transporte de actualización:** `install()`, `write()` y `resume()` de
  `web/fm1ota.js` ya no confían en quien los llama. `requireProof()` corre antes de `find()` (un rechazo
  cuesta cero frames) y liga los bytes (SHA-256 y tamaño exactos), el manifiesto Ed25519 con su clave
  pinneada, la identidad que el propio paquete declara (`productOf`), la política de chip y el piso de
  versión; `admit()` consume una autorización por transferencia, revalida el JEDEC y los bytes tal como
  llegan en ese momento, y sirve una copia privada de la imagen verificada, así que un buffer tocado
  después de validar no llega al equipo. `write()` verifica por su cuenta —es una entrada pública— y la
  unidad que responde el handshake debe declarar la identidad autorizada antes del `UPGRADE`. El retorno
  al V15 oficial queda rechazado (no existe manifiesto firmado para esos bytes). `node web/test_web.mjs`:
  18 casos negativos con `frames = 0`, `upgrades = 0`, `served = 0`, más instalación positiva con clave
  efímera, mutación, swap, reuso y unidad equivocada. Detalle en `docs/TRANSPORT_GATE.md` y fila 9 de
  `docs/SECURITY_AUDIT.md`. **No se conectó ni se escribió ninguna unidad física.**
- **Custodia de la clave de firma, impuesta por las herramientas:** `fm1_sign.py keygen` rechaza
  cualquier destino dentro del checkout (resuelto, con `..` y symlinks) y crea el archivo `0600` con
  `O_EXCL`; `sign` rechaza una clave privada guardada en el repositorio y un `--keyid` cuya clave
  pinneada no es la del par que firma; `pin` exige `--replace` para mover un id a otra clave y valida
  la clave (32 B crudos, 64 hex o PEM Ed25519, y nombra un punto EC de 65 B como lo que es); un
  `fm1_keys.json` corrupto rechaza todo en vez de leerse con tolerancia; `check [--require-key]`
  reporta el estado (hoy: ninguna clave pinneada, todo paquete se rechaza). `tests/key_custody_test.py`
  corre generación, pin, rotación y revocación con una clave efímera, y `tests/sign_test.py` entra en
  `run_tests.sh` y en el grupo `security`. Procedimiento, roles y checklist en
  `docs/SIGNING_CUSTODY.md`. **No se generó ninguna clave real y no se tocó ninguna unidad.**
- **Identificación física (0x12) en la aplicación:** el NOR contesta con el JEDEC que leyó su propio driver
  SPI, la geometría que ese id implica, un estado y un token de sesión; dos lecturas con el mismo nonce deben
  coincidir y traer tokens distintos. Sin respuesta, JEDEC desconocido, CRC malo, geometría que no cuadra o
  token repetido ⇒ el instalador no envía ningún frame que pueda escribir. El loader queda fuera con
  `FELUCCA_LOADER=1`, byte a byte igual. `tests/ota_test.c` cubre los estados 1/2/4/5/6/7 y `tests/chip_test.py`
  el host. **No probado en hardware.**
- **Lectura solo-lectura (0x13) y backup de dos pasadas:** hasta 256 B por petición, validación de rango,
  CRC, sesión y longitud; `tools/fm1_backup.py` lee 1 MiB dos veces, compara byte a byte, calcula SHA-256 por
  pasada y escribe un manifiesto `qg1-backup-1` que distingue completo, parcial, inconsistente y no
  verificable. El test comprueba que solo sale 0x13 (nunca 0x30 ni UPGRADE). **Sin restauración: RECOVERY no
  verificado.**
- **Autenticidad Ed25519:** manifiesto firmado (`qg1-release-1`) con hash, producto, versión, min_firmware y
  key id; verificación en Python y con WebCrypto, obligatoria antes de instalar. `tools/fm1_keys.json` no tiene
  ninguna clave publicada: **todo paquete se rechaza**, que es el estado correcto hasta que exista custodio.
- **Recuperación de una instalación cortada:** el loader no puede identificar su NOR (su código no cambia), así
  que un FM-1 en modo update se rechaza con exit 7 salvo override de laboratorio. Limitación declarada, no
  relajada.
- **Presupuesto:** app 558 200 B, paquete `ad2f3d41…`, RAM `.data+.bss` 96 452/98 304 (spare 1 852), RAM code
  22 432/24 576.

## Flash: the UCUN UC25HQ80 (0xB36014) is a write part, with its evidence on the page

- The gate, the web installer and the evidence record now accept both documented 1 MiB parts and
  refuse anything else. `docs/JEDEC_COMPATIBILITY.md` holds the case: UCUN's datasheet (same geometry,
  `02`/`20`/`D8`/`05`/`06`/`9F`, WIP/WEL, 3 ms page and 10 ms erase maxima), the firmware's polled
  `WIP` waits that make those maxima irrelevant, and a third-party FM-1 with this id that installed a
  full image and then kept settings and a project across a complete power cycle (Felucca #320).
- The boot area stays out of scope on every part, so the `bootloader` column stays `False` for `0xB36014`.
- `tests/gate_test.py`, `tests/flash_evidence_test.py`, `tests/preflight_test.py` and
  `web/test_web.mjs` cover both ids, the unknown id and the blank answer, and fail if the three policy
  copies drift apart.

## Gate de transporte: el chip lo identifica el transporte, la versión es obligatoria

- `requireProof()` ya no acepta `evidence`: el chip se identifica con dos `0x12` en el enlace real
  (`identify()`), antes de cualquier escritura, y `#serve()` sólo sirve el enlace autorizado.
- La versión instalada pasa a ser obligatoria: sin ella un downgrade no puede descartarse, así que
  `install()`/`resume()` la exigen (`version`) y la página recuerda la que instaló.
- El paquete se admite antes de abrir el puerto: un proof usado o de otro producto cuesta cero frames.

## v1.8.7 — the master EQ no longer diverges at 20 Hz (2026-11-04)

- **Causa raíz medida, no supuesta:** el filtro biquad maestro guardaba sus estados en enteros con la misma
  escala que la señal. Un shelf o un peak a 20 Hz tiene sus polos a ~1e-6 del círculo unitario, así que el
  truncamiento por muestra del lazo de realimentación se integraba: con **entrada DC 1000 la salida era
  1 971 664** (debían ser 16 000, +24 dB) y una senoide de 20 Hz a ±24 dB trepaba hasta el clamp. Los
  coeficientes eran correctos (su respuesta calculada da +24,1 dB a 1 Hz y +12,0 dB en la esquina), y aplicar
  redondeo simétrico en `bq_mul` **no cambiaba nada** (mismas cifras antes y después).
- **Corrección:** los estados del biquad pasan a `int64_t` con 10 bits fraccionarios extra (`BQ_S`), de modo que
  la realimentación conserva precisión y el polo no se convierte en integrador. El lazo se unificó para ambos
  canales y los umbrales de estado del silencio se escalaron con `BQ_S`. Sin cambios de coeficientes, de
  topología audible ni de polifonía.
- **Prueba nueva por la ruta de audio:** `tests/eq_response_test.c` inyecta senoides por `qg_biquad_process()`
  y compara RMS (no coeficientes). 11/11 en verde: shelf +24 dB en su esquina +12,09 dB, −24 dB −11,94 dB,
  high shelf 20 kHz +12,06 dB, peak +24 dB 20 Hz +24,01 dB, peak +12 dB en el índice 64 +12,00 dB, 1 kHz
  plano +0,84 dB, 20 s de senoide sin llegar al clamp. Queda documentado un límite real: un corte de −24 dB a
  20 Hz solo alcanza −17,6 dB porque los coeficientes Q25 no resuelven esa profundidad (a 650 Hz llega a
  −23,99 dB); el test lo verifica con ese límite explícito.
- **Presupuesto:** app 558 876 B, paquete 610 898 B, identidad `FM-1_982`; RAM `.data+.bss` 96 452/98 304,
  RAM code 22 528/24 576 (margen 2 048 B, el mínimo exigido por el gate). Sin voces nuevas ni cambios de
  calidad.

## v1.8.9 — the master EQ's DC bias and its 20 Hz cut (2026-11-04)

- **The DC bias was truncation, not the coefficients:** the loop floored every product toward -inf, and
  with the pole 2e-6 from the unit circle that constant error is multiplied by 1/(1+a1+a2) ~ 493 000.
  With a DC input of 1000 the measured gain was 13,492 (22.60 dB) against the 15,956 (24.06 dB) the
  coefficients describe: -1.46 dB, stable from 1 s to 40 s.
- **Fix:** `bq_rnd()` rounds each of the five loop products to nearest (all five: without the
  feedforward ones the 20 Hz cut fell back to -14.8 dB) and the states carry 11 fractional bits
  (`BQ_S` 10 -> 11). Measured: DC 1000 -> 16,384 (+0.23 dB), DC 8000 -> 127,224 (-0.03 dB), stable
  from 1 s to 40 s. The quietest DC (100) sits at -4.38 dB because there the signal's own
  quantisation dominates, not the loop; the test pins that as its own bound.
- **The -24 dB cut at 20 Hz is a cut now:** the numerator is three Q25 coefficients whose rounding
  error, with a denominator of a few LSB, ate half the depth (-17.61 dB measured). The four setters
  share `qg_biquad_commit()`, which corrects the numerator's sum to the exact value it divided by a0:
  -24.14 dB today, and the 20 Hz notch went from -18.7 to -45.30 dB.
- **Budget, measured:** app 558,688 B, package 610,898 B, identity FM-1_982; RAM `.data+.bss`
  96,452/98,304 (spare 1,852), pool 311,240 (spare 9,248), RAM code 22,500/24,576 (**2,076 B spare,
  +28 B over v1.8.8** despite the rounding, from folding the four setters into one commit path and two
  unsigned window compares in `.hot_text`).
- **Real restore implemented, hardware not tested:** `fm1_restore.py --write --programmer
  file:PATH|flashrom:DEVICE` erases, programs and reads back sector by sector (4 KiB / 256 B), then
  reads the whole part back and compares; exit 8 when it does not come back identical, 2 when the
  programmer fails. MIDI still has no write frame and `--write` without `--programmer` still exits 5.
  Tested against a part simulated on a file, a bit that does not program, a flashrom that lies and one
  that fails; **neither backend has run on hardware**.
- **Signing and chip support unchanged:** `fm1_keys.json` still has no production key (every package
  is refused, the correct state until a custodian exists); `0x856014` writable, `0xB36014` read-only.
- **Restore keeps the boot area by default:** `--write` starts at 0x4000; the first 16 KiB (loader
  and boot header) are unit-specific and need `--include-head`, which `flashrom` requires because it
  writes the whole part.
- **OTA reply buffer fixed (found by the merged suite):** `ota_send_big()` writes 6 header bytes,
  n body bytes and one checksum byte, but `ota_qbuf` and `ota_rbuf` were declared `6 + n`. The
  checksum landed one byte past the array: ASan reports a global-buffer-overflow on the 0x12 reply
  (n = 24) and on a bare part it corrupted the neighbouring global silently. Both buffers now carry
  the extra byte; `build/host/asan/ota_test` and the 0x13 path pass.
- **Not verified:** hardware, physical restore, audio on a real unit.

## Target build for the AC791N, portable cache barriers, rebuilt WASM
(branch `freebuff/6ae847e`, merged into v1.8.9; its own text, kept whole)

No DSP, voice count, effect, parameter, layout or project format changed. This pass produced the
**target build** the previous one could not, and fixed what compiling it (and running the whole host
suite) exposed.

- **The firmware is now built for the AC791N.** JieLi's pi32v2 toolchain
  (`jieli-linux-toolchains-20260730.1`, sha256 of the archive
  `a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd`) plus the pinned SDK
  `AC79NN_SDK_V1.2.1_2023-12-13` (its three files match `SDK_SHA256` in `tools/build.py`) produce
  `build/felucca.bin` 556 856 B, `build/loader/ota.bin` 7 999 B and the package
  `build/felucca.fwsc` 611 155 B (identity `FM-1_982`). Measurements and headroom:
  [`docs/MEMORY_BUDGET.md`](docs/MEMORY_BUDGET.md).
- **Cache barriers are portable, so the host tests that needed them compile.** `csync` / `ssync`
  were inline asm at every call site (`fm1_usb.h`, `fm1_uart.h`, `fm1_irq.h`, `fm1_sys.h`,
  `fm1_flash.h`, `fm1_multicore.h`): an x86 or wasm build assembled them verbatim and failed. They
  are now `FM1_CSYNC()` / `FM1_SSYNC()` in `fm1_cc.h`, the chip's instruction on pi32v2 (same
  expansion as before) and a compiler barrier with the same ordering on a host. `persistence_test`,
  `backup_test` and `fm6bank_test` compile and pass — they were reported as "pre-existing failures"
  before, and the cause was this.
- **`FELUCCA_FLASH=0` builds again.** `flash_identify()` referenced the SFC entry points outside
  `#if FELUCCA_FLASH`, and the TAKES reel module (`gen_reel_flash.c`) and its two call sites were
  unguarded: a documented option in `BUILDING.md` did not compile. Fixed in `project.c`,
  `gen_reel_flash.c` and `main.c`; the storage-less build now passes the same checks
  (526 688 B image, 84 356 B `.data+.bss`).
- **The loader's power-cut test no longer depends on how much the two apps differ.** `ldr_test`
  cut at fixed byte counts (up to `0x80000`); once the old and the new app shared enough sectors,
  fewer bytes were programmed, the "power cut" never happened and the test failed on its own
  assumption. The cut points are now derived from the bytes the install actually programs, and the
  counter ignores the post-commit record cleanup (cutting there is not a mid-write loss).
- **The browser emulator builds and matches the firmware again.** Its simulated part needed
  `fl_status_ram` once the identification existed; the stub is guarded by `HOSTSIM_FLASH_ID` so a
  test that drives its own part keeps its own answer. WASM rebuilt at `v1.8.8`:
  `build/emu/manifest.json` carries the version, the commit and the wasm sha256.

## Integrated hardening — flash identification, OTA bounds, core-1 self-test, verified backup CLI
(branch `freebuff/6ae847e`, merged into v1.8.9; its own text, kept whole)

Hardening pass over the write paths and the second core. No DSP, voice count, effect, layout or
parameter changed; no physical FM-1 was used and no target build was produced in this session
(the JieLi toolchain and the AC79 SDK are not installed here). Full evidence, including what could
**not** be run, is in [`docs/VALIDATION_MATRIX.md`](docs/VALIDATION_MATRIX.md).

- **Flash identification is now evidence, not a single reading.** `flash_ok` used to be one `0x9F`
  read against an allowlist. It now requires two independent reads that agree, a documented 1 MiB
  part at its documented capacity, and a status register that is readable, idle and **not
  block-protected** (a protected part ignores erase/program while every call returns success). Any
  other result leaves the firmware RAM-only instead of writing to a part it cannot identify.
  Policy: `firmware/hal/fm1_flash_id.h`; application: `flash_identify()` in `project.c`;
  test: `tests/flash_id_test.c` (11 cases).
- **The update path refuses packages that point outside the part.** `ota_ufw` now requires every
  `flash.bin` / `ota.bin` range to satisfy `off <= 1 MiB && len <= 1 MiB - off` (the only form that
  cannot wrap) and `flash.bin` to cover the app window, before anything is erased. A reply is only
  accepted when it echoes the flags byte the request was sent with.
- **The update loader can no longer hang for ever.** A 20-minute session deadline (`ldr_core.c`)
  turns a host that goes quiet without unplugging into a failed write with the record intact, so the
  next power-on resumes instead of looping.
- **Boot cleanup covers a half-written loader.** `ota_boot_cleanup` samples the head, the first body
  page and the record instead of four bytes, so a staging area whose first word reads `0xFFFFFFFF`
  is still cleared.
- **The second core must prove itself before it is used.** `fm1_mc_start` used to trust `alive`; the
  documented failure of some units is a wrong 32-bit load under load. Eight fixed jobs with answers
  computed on core 0 now run through the shared mailbox at boot
  (`firmware/hal/fm1_mc_selftest.h`); one wrong word and the core is held for good, `CORES 1 (BAD
  READ)` is reported and core 0 renders every part (`tests/mc_selftest_test.c`).
- **The installer can no longer write to an unrelated port.** `pair_output` returned the only output
  on the bus whatever its name; it now requires the names to match (ignoring Windows' numeric
  suffix) and otherwise reports "FM-1 not found".
- **New `tools/fm1_backup.py`.** Two independent reads compared byte for byte, SHA-256 per read, per
  object and for the file, a manifest (device identity, date, method, capacity, passes, result) and
  an explicit classification (complete / partial / inconsistent / unverifiable). Restore validates
  manifest, file hash, every object hash and CRC, the device model and the object geometry before
  the first write, reads every object back and compares it, and reports a selective restore as
  partial. Archives are written `0600` outside the repository; a destination inside a git work tree
  is refused before the device is opened. It moves **musical objects**, not raw flash, and the
  manifest says so.
- **README navigation** is the required two-row table: `WEB SITE` first and double width, `WEB
  EMULATOR`, `WEB STUDIO`, `WEBAPP / EDITOR`, `USB INSTALLER`, `FLASH FM-1`; second row `LICENSE`,
  `GPL-3.0-ONLY`. `tests/readme_nav_test.py` enforces the names, the order, the widths and that every
  destination resolves.
- **Documentation:** `SECURITY_AUDIT.md`, `INSTALLATION_SAFETY.md`, `BACKUP_RESTORE.md`,
  `RECOVERY_GUIDE.md`, `EMULATOR_FIDELITY.md`, `AC791N_SECOND_CORE.md`, `MEMORY_BUDGET.md`,
  `RELEASE_PROCESS.md`, `VALIDATION_MATRIX.md`.
- **Still open, and the reason the verdict does not move:** no cryptographic signature on packages
  (no key material in this tree), no anti-rollback (vendor bootloader, not modifiable), the
  single-bank write window, and no raw-flash backup path. Installation remains **NO-GO for the main
  unit**; the target build, the WASM rebuild and every physical check are unverified.

## v1.8.8 — one master-EQ delay state had the wrong scale (2026-11-04)

- **The bug, measured:** `master_eq_update()` guarded the four delay states against a state a stable
  filter cannot reach, but `s2_r` was compared with `BQ_LIM / 2` **without the `BQ_S` shift** -- 1024x
  below the other three. `master_eq_update()` runs on every block, so with the EQ active an ordinary
  right-channel state looked impossible and the right delay line was cleared again and again: the left
  channel kept its EQ and the right lost it, block after block. A new test walks the four states one by
  one, and it fails on the old code at exactly two assertions (`s2_r = BQ_LIM/2+1` and
  `s2_r = BQ_STATE_HI`), both of which pass now.
- **The fix:** one predicate, `eq_state_bad()`, over `BQ_STATE_HI = (BQ_LIM << BQ_S) / 2`, used for all
  four states, so a scale cannot drift on one of them again. No coefficient, no topology and no
  polyphony change; the image is 16 B smaller (558 860 B).
- **DC gain, separated and documented:** the coefficient's own DC gain for the +24 dB, 20 Hz shelf is
  (b0+b1+b2)/(1+a1+a2) = 15.9559 (24.06 dB). The audio path returns 13.492 (22.60 dB) for a DC input of
  1000, at 1 s and at 20 s alike: it is a settled truncation bias, not settling, because the pole sits
  2.0e-6 from the unit circle and the per-sample truncation is multiplied by ~493 000. At the corner the
  shelf is +12.09 dB against the +12 dB a shelf gives there. `tests/eq_dc_test.c` now measures the three
  numbers (coefficients, processing, bias) and `tests/eq_state_test.c` covers each state.
- **Recovery, honestly:** `tools/fm1_restore.py` validates a raw backup end to end (both passes,
  byte-identical, full 1 MiB, hash, chip, geometry, read-only frame log) and can read the unit again and
  list the ranges that differ. It **refuses to write**: the loader protocol serves a signed package, and
  no raw 1 MiB write has ever been demonstrated on an FM-1 here. `docs/RECOVERY.md` says what a real
  recovery needs.
- **Browser:** `web/emu/browser_test.mjs` loads the built page in Chromium, checks the WASM the page
  received against the manifest's SHA-256, the version and the commit, that the console and the network
  stay clean, that a held key reaches the device, and that the same AudioWorklet renders sound offline
  (peak 0.120 on a held C4). It is a separate group (`tests/groups.sh browser`) because it needs
  Playwright's Chromium.

## Unreleased — six-track load, target link and installer trust

- **Six-track load test:** `tests/six_track_test.c` (new, `bash tests/groups.sh six`) runs six tracks with ten
  sustained voices (3+2+2+1+1+1) and the master EQ for a minute of 32-frame blocks: 82 687 blocks, p50 8.6 µs,
  p99.9 32.4 µs (all blocks, not a sample) against a 725.0 µs deadline, 0 clipped samples, 0.077 % exact zeros.
  Silence is now judged per block (0 silent blocks, leanest block 60 of 64 non-zero samples) instead of by a
  global ratio. `voices_busy()` and `voice_kills` are checked in every block; they prove voice-state
  continuity, not that each of the six tracks is independently audible. Host wall clock only: target timing is
  NOT TESTED.
- **CI 218 (fixed):** the slower splash made the emulator smoke test stop inside the splash (it rendered 2.2 s and the
  splash is now 2.3 s), so HOME never appeared and a key was silent. The test waits 2.5 s; the fail-closed control
  that refused to publish without a target package was correct and is unchanged.
- **Target build of the branch:** 610 897 B package, app 556 152 B, sha256 `a56cdcb9…`, RAM spare 2 572 B, pool spare
  9 248 B, RAM code spare 2 112 B, DSP budgets and package bounds green. Reproducible: same hash twice.
- **Preflight de instalación (solo lectura):** `tools/fm1_preflight.py` comprueba hash obligatorio, tamaño,
  identidad, modelo y modo del dispositivo, y devuelve NO-GO mientras el chip físico no esté identificado
  (exit 7). Nunca envía UPGRADE; `tests/preflight_test.py` lo verifica caso por caso dentro del grupo
  `security`.
- **Cortes por página en el loader:** `tests/ldr_test.c` corta la energía en cada página que el loader
  realmente programa (32 estados con los paquetes de esta rama) y exige reanudación exacta. Los presupuestos
  que no cortaban se informan como omitidos.
- **Documentación:** `docs/FM1_INSTALL_SAFETY.md` reúne alternativas de identificación del chip, el diseño de
  backup bloqueado, la máquina de estados, los niveles de recuperación y el criterio NO-GO.
- **La identificación del chip pasa a ser obligatoria en las dos rutas de instalación:** `tools/fm1_gate.py`
  es la única política y la CLI y la web la invocan justo antes de `UPGRADE`. Sin el JEDEC de esa unidad
  (lector externo, `--jedec`) se rechaza con exit 7 y no se envía nada; el override de laboratorio exige
  `FM1_LAB=1` y no existe en la web. `preflight_test.py` y `test_web.mjs` cuentan los frames del
  dispositivo falso para demostrarlo.
- **CI:** el workflow ejecuta ahora `web/test_web.mjs` y `tests/preflight_test.py` en el job de
  target, para que una ruta que se salte la puerta rompa el build.
- **Propuesta de protocolo:** `docs/JEDEC_BACKUP_SIGNING_PROPOSAL.md` define los comandos `0x12`
  (JEDEC/geometría) y `0x13` (lectura para backup) y la firma Ed25519, sin implementarlos.
- **Bootsplash:** `SPL_WRITE` 800 → 1100 ms and `SPL_HOLD` 900 → 1200 ms, a slower reveal and hold.
- **Installer trust (documented, not changed):** the WebMIDI identity reply carries `model_version` and no
  SPI NOR JEDEC id, so the installer cannot verify the physical flash before writing; the 1 MiB allowlist is
  enforced by the firmware when it mounts flash. Recorded as BLOCKED in `docs/PERF_SIX_TRACK_FM1.md` rather
  than papered over with a UI warning.

## v1.8.6 — FX particles, splash, note visualizer, Punch-In keyboard and Master EQ (2026-11-04)

- **FX particles:** the first FX section was redrawn at `ui.frame / 16` (about 3.75 fps); it now advances on
  every UI frame while drift is on, like the other sections, without extra allocations or reduced particle counts.
- **Splash:** the boot sequence now has stars, parallax, sparks, logo reveal and a sweep, with the same bounded
  timeline contract and no blank/black frames.
- **Note visualizer:** particles are driven by the voices actually playing (`active`, `gate`, `note`, `vel`), with
  fixed storage and bounded work for chords, polyphony and note release.
- **Punch-In keyboard:** white keys are 12 px with a 2 px gap and black keys 8 px wide, 58 px high and 6 px
  apart, preserving MIDI/F3–G5 mapping and vertical labels. `BKPOS` now follows the engine's own order, so
  A#3 and A#4 sit on the boundary of the white key they follow instead of the next one (the old table drew
  three black keys in a row and left a gap).
- **Master Biquad EQ:** real 20 Hz–20 kHz frequency mapping (`EQ_HZ`), logarithmic graphics axis, exact
  `10^(dB/40)` gain tables, Q24 internal coefficients and 64-bit response evaluation. The +24/−24 dB extremes,
  shelves, peak/notch and 480 parameter corners are tested; the graph no longer folds or drops at 20 Hz.
  The sweep also proves the poles stay inside the unit circle at 20 Hz and 20 kHz (+24 dB, both Q ends) and
  that one impulse decays back to silence.
- **Target link:** the EQ's coefficient normalisation divided 64-bit values, and pi32v2 links no
  `__divdi3`/`__udivdi3`; `qg_biquad_set_*` now normalise through a local 64/32 restoring division, so
  `tools/build.py` links again. The rebuilt package is 610.897 B (581.564 B app), SHA-256 `7f7a9ee8…`.
- **Verification:** `ui` (0 lint), `mod`, `presets`, `security`, `target`, `emu`, `sync`, web and package tests;
  target app/package rebuilt from this tree. No physical FM-1 was flashed.

## 2026-11-04 — Integration, target package and public deployment (`0f52f48`)

- **Both histories are integrated.** `origin/main` (`36d3eac`, including PR #60–#62) was merged into
  `work/v18-integration` in `9ff38e3` without force-push and without dropping either side. Conflicts in
  `ui_layer.c`, `web/editor.html`, `web/site/index.html`, `web/test_site.py` and the two captures were
  resolved against the Quantum/two-band tree; the duplicated `layer_fx`/`fx_card` calls and the duplicated
  `pf`/`PF_LOW` blocks were removed.
- **The target package is current.** JieLi `20260730.1` and the AC79 SDK files produced
  `v1.7-QG-0f52f48`, **610.897 B**, `FM-1_982`, SHA-256 `eb7d14a6…308e7b2c`. It includes the two-band
  Punch-in FX, LATCH, the key assignments and the three-head buffer engine; the old `1fae6cc` package is
  no longer offered as current.
- **GitHub Pages published the integrated tree.** The run from `0f52f48` replaced the manifest commit
  `8c101d13…` with `0f52f48011ce`; `/emulator/`, `/emulator/manifest.json`, `/webapp/editor/`,
  `/webapp/installer/` and `/firmware/versions.json` all return 200.
- **Public Chromium passed:** 1440 px and 390 px without overflow, Web Site first, seven Web Studio tabs
  visible, emulator at **60,1 FPS** with WASM `c3258d73…` (1.030.002 B), installer listing
  `v1.7-QG-0f52f48` with the downloaded package's SHA-256 verified, and no console errors.
- **Documentation is aligned with the public build** in `docs/V1.8_STATUS.md`; the temporary `ci-diag`
  diagnostics were removed from the Pages workflow. This remains `v1.7-QG`, not a v1.8.5 release, and no
  physical FM-1 was flashed.

## 2026-11-04 — Target build, real package and a deploy that survives a missing toolchain

- **The firmware is compiled for the AC791N again.** JieLi's linux toolchain (`20260730.1`) and the three AC79 SDK
  files were fetched here; `tools/build.py` produces `felucca-v1.7-QG-7768f59.fwsc`, **610.897 B**, `FM-1_982`,
  SHA-256 `eb7d14a6…308e7b2c`, from a tree that includes the two-band Punch-in FX, the LATCH fixes and the three
  buffer heads. It replaces the `1fae6cc` package, which predated those changes.
- **The site build no longer needs a package committed to the tree.** The target build produces the
  package; if the toolchain did not run, the workflow falls back to one already built for the target.
- **The Pages deploy no longer depends on the target compiler.** `build/site` used to be produced only inside the
  toolchain-gated step, so a failed download left nothing to upload and Pages never moved (that is why `/emulator/`
  answered 404 while the portal served a build from hours earlier). The site falls back to a target-built
  package when the compiler did not run, and a check fails loudly if there is still no `build/site/index.html`.
- **`rev_room` baseline 218 → 243** after measuring with the compiler JieLi ships today: `fx.c` is byte-identical to
  `ab3da86`, so the delta is the toolchain, not a render loop that grew.

# Changelog — QUANTUM GAMMAWAVE (QG-1)

All notable changes to the firmware, the Web Studio, the emulator and the installer pages.
The device is an M-VAVE FM-1 (AC791N); no entry here was tested by flashing hardware.

## 2026-11-04 — Portal access hierarchy, Quantum identity and a synced emulator

- **The portal names the browser build `Web Emulator`** (never `WASM`) and drops the leftover `#emu` hash
  redirect that replaced the portal with the emulator.
- **New `#accesos` section:** `Web Site` first and clearly the largest, with its URL beside the name, then
  `Web Emulator`, `Web Studio` and the USB installer. `web/test_site.py` now enforces the order, rejects the
  old naming and the Solar Heat wording, and fails on any internal link that has no file in the build.
- **Quantum identity on the portal and in the Web Studio:** both use the firmware's `QUANTUM` palette
  (`0x0022 0x10a5 0xef9f 0x3eff 0xfd44`). Every id, tab, handler and protocol frame of the Web Studio is
  unchanged; `web/test_web.mjs` still passes.
- **Web Studio simulator fixed:** `?mock=1` now connects at boot (it only set a flag before), the SIMULATE
  button keeps a coherent label and is idempotent, and `Link.close()` is idempotent, so a deliberate
  disconnect no longer logs `Error: closed`. Chromium 1440/390: seven tabs, no console errors, no 404.
- **Emulator manifest names the current commit** after a rebuild, and `build/site/emulator` ships that same
  `index.html` + `felucca.wasm` + `manifest.json`.
- **Installer:** the published package matches `firmware/versions.json` (`v1.7-QG-0f52f48`, `FM-1_982`,
  610.897 B, SHA-256 `eb7d14a6…308e7b2c`). It was compiled for the AC791N from the integrated tree and
  includes the Punch-in FX / LATCH UI fixes; the older `1fae6cc` package is retained only as history.

## 2026-11-04 — Web portal, Web Studio 2.0 and consonant intervals

- **Official portal at `/`.** `web/site/index.html` is a real Solar Heat / Quantum landing page: current
  build screenshots, firmware features, M-VAVE FM-1 compatibility, installation warnings, dynamic
  `firmware/versions.json` metadata and direct links to the emulator, installer and Web Studio. The Pages
  workflow now uploads `build/site`, keeps `/webapp/installer/` and `/webapp/editor/`, and serves the
  emulator at `/emulator/`. `/?emu=1` and `/#emu` redirect to it. `web/make_site.py` no longer emits a
  redirect page, and `web/test_site.py` checks ids, assets, routes and the published package hash.
- **Web Studio 2.0 surface.** The editor keeps every id, tab, protocol frame and DSP mapping; a new CSS
  layer unifies the charcoal/amber/cyan identity, the brand bar, panels, controls, focus and mobile
  overflow. Chromium 131 local: 1440 px and 390 px, seven tabs, no horizontal overflow, no console or
  failed-request errors. `web/test_web.mjs` still passes. Portal sections are visible without the scroll
  observer; its entrance animation is now progressive, and `web/test_site.py` rejects the old hidden state.
- **Consonant random intervals (JIANT V10.38, adapted).** `param_semi_pick()` and `param_semi_snap()` live
  in `params.c`; `gen.c` keeps or snaps `F_SEMI` parameters instead of adding a random detune, the sound
  dice picks a musical interval, and `ui_layer.c` excludes interval destinations from random macros.
  `tests/semi_test.c` checks 4.096 picks and the snap floors (including the 5/7 tie).
- **WASM rebuilt after the interval change:** **1.030.693 B**, SHA-256
  `5b8475aaeda9e090f022fe8879d6cb3861845838367b454718ef0d482c2fd76a`; native comparison bit for bit
  (88.200 frames), 23,5 ms/s (2,4 % of real time). No target build, physical FM-1 or online deployment
  is claimed.
- **Version decision:** **v1.8.5 is not published.** `FELUCCA_VERSION` stays `v1.7-QG`; the build is
  published and verified in software, but physical hardware, signature, rollback and independent recovery
  remain open. See `docs/V1.8_STATUS.md`.

## 2026-11-04 — PUNCH-IN FX keyboard and EQ safety pass

- **PUNCH-IN FX redrawn as a keyboard.** The ten effect cards, the separate mute row and the four
  black-effect cards are gone. The sixteen white keys and eleven black keys use the full panel height;
  each key carries its abbreviated function vertically. Bank (`B1/B2`), latch, capture state, target
  (`ALL/SYN/DRM`) and the selected track share two compact header lines. No note names are drawn.
  Every label is read from `perf_key()`; bank 2 is `REV FRZ TAPE GLCH RTRG`. Black keys 1..6 remain
  latched mutes T1..T6; G#4/A#4/C#5/D#5/F#5 remain VIB/TGT/GATE/RTRG/DIVE.
  Host UI render: **172 screens × 17 palettes, 0 lint findings, 2.647.728 alignment measurements, 0
  misalignments**. Capture: `artifacts/punch-in-fx-new.png` (240×240, rendered from the firmware code).
- **Master EQ hardening.** The setters clamp FREQ/GAIN/Q, reject coefficients outside the stable
  triangle or a sane feed-forward range, clear the delay line when coefficients change, force a
  poisoned state to zero after 32 silent blocks, and remove DC before the EQ (a +24 dB low shelf
  otherwise amplifies an offset into the leveler). `tests/eq_sweep_test.c` sweeps 480 mode/frequency/
  gain/Q corners: worst tail after silence **0**, nonzero tails **0**, bad coefficients **0**; a
  poisoned state clears; extreme low shelf → bypass → peak passes audio; 4.000 rapid changes remain
  bounded.
- **WASM rebuilt** after these changes: **1.030.207 B**, SHA-256
  `8da41752486e9f8aceade1d221e33198b415a0ae8a3944cf4a5af6f47c8600c5`; native comparison **bit for bit**
  (88.200 frames), 23,4 ms/s (2,3 % of real time). No target, browser or hardware validation is claimed.

## v1.8-QG — correcciones de audio, UI y EQ (NO PUBLICADA)

- **Low Shelf: ruido de fondo corregido y demostrado.** La causa era doble: los productos Q14 se
  desbordaban a 32 bits cuando el shelf +24 dB a 125 Hz llevaba el estado a ~16× la entrada, y la
  cuantización Q14 podía dejar `|a1|` fuera del triángulo estable. Ahora el biquad multiplica en 64 bits,
  limita estados, mete los coeficientes en el triángulo estable y vacía el residuo de ciclo límite sólo en
  bloques mudos por debajo de −24 dB. Prueba directa (cuadrada fuerte + 4.096 ceros): cola **0**; el
  filtro anterior dejaba **21.844**.
- **Phaser: intensidad y pops.** Seis etapas all-pass con realimentación 0,366 y mezcla húmeda completa
  (antes cuatro etapas y retorno fijo). DEPTH y la ganancia rampan por muestra; al apagar DEPTH se
  desvanece a seco y se reinician los estados. Medido: notch de 20 a 97.807 (más de 18 dB), primer
  sample al activar −5.526 contra seco −5.525 (1 LSB), DEPTH 0 bit-exacto. UBSan detectó que el sexto
  estado escribía fuera de `ph_s[4]`; el array se amplió a `ph_s[6]`.
- **Vúmetros y partículas.** El VU de FX es ahora una barra de 7 px con peak cap; se quitó la barra de
  envío duplicada. Las 18 partículas de fondo aceleraron de 21–45 a 55–109 unidades/frame sin añadir
  primitivas ni allocaciones. La gráfica del MASTER EQ añade guías logarítmicas, etiquetas 125/2k/20k y
  mantiene la línea 0 dB y la curva de los coeficientes reales.
- **UBSan/ASan limpios en las rutas ejercitadas.** Se corrigieron desplazamientos a la izquierda de
  valores negativos en `perform.c`, `fx.c`, `slicer.c`, `eng_analog.c`, `eng_lofi.c`, `eng_phase.c`, y
  desbordes en `fm6_core.c` (suma de fase y producto de la onda). `mod_test` y `perform_test` con
  ASan+UBSan terminan sin hallazgos.
- **12 voces: experimento medido, no adoptado.** Sobre las mismas fuentes, la canción pesada costó
  22,9 / 24,1 / 24,3 ms por segundo con 10/11/12 voces (12 = **+6,1 %**), y el estado estático pasó de
  294.792 a 295.864 / 296.936 B (+2.144 B con 12). No hay medición AC791N; se mantiene `NVOICE 10`.

- **Phaser y M-WIDE reparados y medidos.** El Phaser usa cuatro etapas all-pass estables, Q15 con `a`
  entre 0,20 y 0,95, feedback bajo y bypass con DEPTH 0. M-WIDE conserva el centro a 64 y abre el estéreo
  hacia 127 sin inversión de fase ni pérdida anormal; los tests host pasan.
- **HPF/LPF dejan de actuar como mute.** `PF_LOW` subió a `28 << 8` (~480 Hz); las pruebas conservan
  1 kHz en la banda de paso y siguen atenuando los extremos. No se quitó el filtrado.
- **EQ real y recuperado en la UI.** `fx_master_eq_db()` usa los mismos biquads del audio para PEAK,
  NOTCH y shelves; la respuesta se calcula sin división de 64 bits para el target. La gráfica vuelve a
  dibujar una curva, no una imagen decorativa.
- **Vúmetros de señal real.** Los sends y retornos de chorus/delay/reverb publican peaks reales; la prueba
  comprueba señal y caída a cero. No se usa el fader como medición.
- **Punch-in FX:** máximo de tres efectos simultáneos. Las seis capturas (REPEAT, REV, FRZ, TAP, GLC, RTR)
  ahora usan **tres cabezas de lectura independientes sobre una sola captura de 32 KB**: las 15 parejas y
  las 20 ternas se probaron con audio distinto, además de tres FX sobre las seis pistas. El cuarto efecto
  se rechaza sin apagar los activos; los mutes T1–T6 son independientes. La pantalla muestra `CAP %`,
  `n/3` y una barra de captura/lectura por tarjeta. Un único estado `LATCH:NO / LATCH:YES`.
- **Visualizador y startup:** 18 partículas y 24 partículas de arranque, sin allocaciones dinámicas ni
  esperas añadidas.
- **Emulador sincronizado por hash.** `tools/package_emu.py` genera `build/emu/manifest.json` y fija en la
  página el SHA-256 y el tamaño del WASM; la carga usa `?v=<sha256>`, `cache: "no-store"` y verificación
  con `crypto.subtle`. `tests/emu_sync.py` falla si página, manifiesto y binario no coinciden.
- **Build identificable en el emulador:** la página y `manifest.json` muestran versión, commit y SHA-256 del
  WASM; la carga es `?v=<sha256>` con `no-store` y verificación `crypto.subtle`. El workflow publica desde
  `main` o la rama autorizada `perf/security-hardening`.
- **UBSan real corregido:** `eng_analog.c` desplazaba a la izquierda un valor negativo
  (`(pitch16 - 60*16) << 2`); ahora usa `* 4`. `fuzz_proj` pasa 5.000 iteraciones sin ASan/UBSan.
- **Etiquetas del emulador visibles y validadas en Chromium:** el `fill` SVG con `var()` se sustituyó por la
  clase `.panel-label`. La página carga el WASM `b9a52ca…`, corre a 60,6 FPS y Web Studio/instalador cargan
  sin errores de página.
- **Estado:** cambios locales en `perf/security-hardening`, HEAD `c020b7e`; **sin commit ni push**.
  Target app 552660 B, paquete 610897 B; `perf_block` 372/522. `FELUCCA_VERSION` sigue en `v1.7-QG`.
  Browser real, FM-1 físico, ALSA y FPS en navegador siguen **NO VERIFICADOS**. No es una release.

## v1.7-QG — PUNCH-IN FX emulator sync

- **The emulator now shows the PUNCH-IN FX redesign.** The page requests `felucca.wasm?v=@VERSION@` so a
  rebuilt WASM is not hidden by a cached copy; `build/emu` was regenerated at `v1.7-QG` (1019205 B).
- **WASM label fix.** The 18 px effect pads draw their names directly with `cv_text_on`; the measured
  `cv_text_r` path rendered the pads but not their labels in the WebAssembly build. The LED and the
  top highlight stay; no DSP, voice count or parameter changed.
- **Effect audit recorded.** `docs/PUNCH_IN_FX_AUDIT.md` separates the 15 existing pads and the four
  black-key effects from Reverse/Glitch/Freeze/Tape Stop, which remain unimplemented. `DIVE` is a
  two-octave pitch drop, not a tape stop; `RND` and `ST8`/`ST16` are not relabelled as Glitch.
- Final target at this HEAD: app **549220 B**, package **610897 B**, identity `FM-1_982`; app SHA-256
  `616db4d6ed191e171721c7cdc1f7ea9fde80705ef1911f0dad51c1918c206f8d`, package SHA-256
  `106b5f41bd8f93fa22142e9c9669375dc784f7e65a19de2b8183d7e19acedab4`. `perf_block` 569; no new DSP.
  UI 172 × 17 with 0 lint and 0 alignment findings; installation remains **NO VERIFICADO EN HARDWARE —
  NO APTO PARA INSTALACIÓN REAL**.

## v1.7-QG — HOME encoders, mini-knobs, splash and installation safety audit

- **HOME encoder mapping fixed.** `bg_knob()` now reads `home_param()` while HOME is open instead of
  the last page's columns. KNOB 1–4 each edit their own parameter and their own mini-knob; engine
  pages keep `page_desc()`. A new host test covers every available engine and all four encoders.
- **Mini-knobs redesigned.** Concentric rim, recessed body and a 270-degree radial needle, in the
  Solar Heat/Quantum palette; the four controls are evenly spaced in the lower-right bay.
- **Splash polished.** A second electron, twelve solar ticks with longer flares and a progress-driven
  wave phase join the existing wordmark, waves and spectral horizon. No bootloader, flash or audio
  path changed; it remains a function of boot progress with no stored bitmap.
- **Hardware specification correction.** The repository now records 578 KB SDK SRAM versus the 96 KiB
  firmware budget, 14 buttons, 7 encoders plus MASTER, the ST7789-class panel and the unresolved
  320/360 MHz clock reports. `0x856014` and `0xB36014` are the two documented 1 MiB IDs in an explicit
  allowlist; unknown IDs are rejected. Neither variant has been validated on hardware.
- **Loader completion handshake hardened.** The loader now requires exactly `success\0` for the final
  `0xF0000000` ACK and returns `-14` without clearing the update record on a missing or wrong reply.
  Host tests cover a lost final ACK; neither installer reports `done` without the reboot/identity
  confirmation.
- Final target at this HEAD: app **549028 B**, package **610897 B**, `FM-1_982`, package SHA-256
  `932d37a0d5c5d74124713555f678804589ba42b8e129bce03e1d0762ac36be8d`. The app slot is 581564 B,
  not the app size; `548708` and `549012` were earlier baselines. UI 172 × 17 with 0 lint;
  `perf_block 569/522`. Installation verdict: **NO VERIFICADO EN HARDWARE — NO APTO PARA
  INSTALACIÓN REAL** until physical identity, backup and USB_KEY recovery are demonstrated; see
  `FM1_INSTALLATION_SAFETY_AUDIT.md`.

## v1.7-QG — final audit and PUNCH-IN FX visual pass

- PUNCH-IN FX: rounded pads, a tighter 5-column grid and a quiet `PUNCH-IN FX · A#4 TGT · G5 CLR`
  section line, in the existing Solar Heat/Quantum palette. Drawing only; no parameter or DSP change.
- The buffered Reverse/Glitch/Freeze/Tape Stop bank was measured at `perf_block` 662/522 (+27 %) and
  below the RAMTEXT margin, so it was **discarded**. No budget was relaxed; F#5 keeps the existing
  DIVE and is not claimed as a verified tape stop.
- Installer: a failed USB `success` ACK now reports `lost` instead of marking the step finished.
- Previous target (before the HOME/splash pass): app 548708 B, package 610876 B, `FM-1_982`, SHA-256
- Final target: app 548708 B, package 610876 B, `FM-1_982`, SHA-256
  `38eb7655e903a5244663c7625db87e46317591d905e862d0ccb4795534d7351d`. Final status is
  **NO VERIFICADO EN HARDWARE**; see `docs/AUDIT_FINAL_FM1.md`.

## v1.7-QG — visual system: scope and splash

- The full-screen visualizer was recomposed instead of recoloured: a single header with the six live track
  levels, a 144 px zero-crossing waveform on the left, its real X/Y phase trajectory in a framed inset on
  the right, and the 16-band spectrum with peak caps on its own floor. The old overlapping reticle, the
  full-width trace through the bars and the crowded cloud of motes are gone. The phase plot and the
  music-reactive meters remain; the six ambient motes are deliberate and slow, not a random field.
- The boot splash is now a real title screen: the seven block letters of QUANTUM in the same glyphs as the
  QG logo, an accent Q, a solar core with a corona, two orbits and a travelling electron, a progress
  hairline under the wordmark, three written waves and a spectral horizon with its playhead. It stays
  inside the existing 240x124 canvas, the palette and the mono rule; no stored bitmap was added.
- UI passes 172 screens x 17 palettes with 0 lint and 0 alignment findings. The splash timeline still has
  425 steps of 4 ms; 198 change the screen and 227 identical rewrites are skipped. No DSP, persistent
  layout, bootloader or flash path changed. Target app grew 748 B to 548852 B; package size is unchanged
  at 610876 B (`FM-1_982`, SHA-256 `b0d4d9c56690f6224dadcefbf06690b3e33ef0d03c819e3778eb7d85dd9c6a8b`).

## v1.7-QG — performance / security hardening

- `graph_home_viz()` measured the 16 spectrum bands twice per frame because the screen is drawn in two
  88-pixel bands, so peak caps fell two rows per frame. It now measures and updates the decay once and
  draws the same bars in both passes. A native HOME->HOME render was added to `ui_render` (172 screens,
  0 lint findings, 0 alignment errors).
- Seven left shifts of negative fixed-point samples became equivalent multiplications in the DC blocker,
  DRUM-X output and DRUM output/IMAGE paths. This removes undefined behaviour without changing sound:
  all 220 golden renders are unchanged. The six-part 10-voice stress, `audio_test` and `reverb_test`
  pass under the repository's ASan/UBSan policy.
- `tests/groups.sh` gains `security` and `stress`; `docs/PERF_SECURITY_AUDIT.md` records the host and
  target measurements, the remaining single-bank/no-signature risks, and what was not tested on metal.

## v1.7-QG — flash I/O, loader bounds and FX/MASTER UI

- Flash: SPI timeout errors now propagate to erase/program/read callers instead of being discarded;
  cache and SFC waits are bounded, and a failed I/O operation cannot report success. The loader's app
  window is asserted at `[0x4000,0x93000)`; VM, autosave, FM6 bank and USR are rejected at compile time.
- Loader: the stock record at `0xE8F00` is invalidated in place (80 zero bytes), preserving the rest of
  the VM-backup sector; `fvb_boot()` no longer reads an uninitialised sequence when no home bank exists.
- UI: the first FX page gains real per-send peak meters; MASTER gains a live CLIP transfer curve, PUNCH
  envelope, DUCK gain, REL and output VU. UI lint/alignment pass 172 screens x 17 palettes; FX host
  draw cost 125.3 -> 123.9 units. The full gallery was regenerated from the firmware renderer.
- Tests: target build, loader (normal and ASan/UBSan), web, security, presets and UI renderer pass.
  No physical FM-1 verification; the single-bank/no-signature risks remain.

## v1.7-QG — Splash identity

- The splash monogram was a square `Q` (a box with a diagonal) that read as a placeholder. It is now a round
  Q ring with its tail, drawn with the same orbit/knob language as the rest of QG, in the same palette and
  mono rule. `ui_render` still reports 0 lint findings and 0 alignment errors over 172 screens x 17 palettes.

## v1.7-QG — CI budget fix (target compiler checks)

- `mod_begin` and `mod_voice` are back inside their target budgets, measured with the JieLi compiler:
  `mod_begin` 274/166 (+65 %) -> **35/166**, `mod_voice` 36/30 (+20 %) -> **19/30**. The per-slot work is
  out of the loop (`mod_block_slot`, `mod_scan`), `mod_tsrc` / `mod_voice_add` are no longer inlined there,
  and the per-voice sources are resolved once. No budget line was changed; `mod_test` passes and all 220
  golden renders are byte-identical.

- `mod_voice` resolves the per-voice sources (ENV, VEL, KEY, RAND, PAT) once, before the slot loop, and
  indexes them there. 1.6's PAT added a source test to that loop, whose weighted cost had a 30 budget;
  the loop now does one load per slot instead of re-testing the source, with identical renders (220 goldens,
  0 changed).
- `tests/mod_test.c` still asserted `MS_N == 15`; PAT was appended in 1.6 (15, `MS_N` 16). The test now
  checks both NOISE 14 and PAT 15, and the matrix suite passes again on `main`.

## v1.7-QG — Splash dust

- The boot splash gains deterministic particles behind the three written waves, in the saved palette and mono
  rule, with no extra frames of drawing (197 changing frames of 425). `ui_render` now derives the timeline
  length from `SPL_WRITE + SPL_HOLD` instead of a hardcoded 425.

## v1.7-QG — UI Evolution & Hardware Validation (2026-11-03)

- UI audit kept at 0 lint findings and 0 alignment errors across 171 screens x 17 palettes.
- `tools/budget_report.py`: static RAM for 8/10/12 voices, library composition and host CPU baselines, with
  the host/target distinction built into the output.
- `docs/HARDWARE_SIMULATION.md`: what the host stubs and the wasm shim model, and what they do not.
- `docs/FEATURES_1.7.md`: every requested feature classified as already present, deferred with a reason, or
  blocked by the frozen project format. No persistent parameter was added.
- **MIDI Learn (session only):** CC 119 arms the parameter id of the track's engine, the next CC binds to it,
  CC 118 clears every binding. RAM only, so projects, user presets and settings keep their layout; the MIDI
  control test covers the range, the clear and an out-of-range arm.
- `tests/stress_run.sh`: project, persistence, robustness and MIDI paths under ASan/UBSan for N runs, then
  two full golden passes, all against the simulated flash.
- `tools/hw_model.py`: block deadlines, half-buffer latency, DMA and LCD bandwidth, SRAM share per voice
  count and the cycle ceiling at 160/240 MHz — arithmetic from documented constants, never a measurement.
- **UBSan findings fixed:** ANALOG shifted negative samples left twice (`y << 1`, `voice_amp(...) << 1`);
  both are now `* 2`, the same DSP with defined behaviour (goldens unchanged). The FUN5 persistence test
  wrote 27+ globals into a 27-entry header; it now stops at the old format's size.
- Version 1.7-QG across firmware, Web Studio, installer strings and documentation.

## v1.6-QG — EVOLVE / MOTION, Touch & 200 Sounds (2026-11-03)

### Library — exactly 200 factory sounds
- Retired from browsing the two least distinct DRUM kits, **HEARTH KIT** and **CYBER DRUM**. They stay in
  the table as legacy entries at their original indices 10 and 11, so a project, favorite or user preset
  made before 1.6 loads the exact kit it saved (their golden hashes are byte-identical to v1.5). DRUM's
  ten browsable sounds keep indices 0..9 and the library is exactly 200 (`core.h engine_library`).
- Added 16 original sounds (not variations): QUANTUM BLOOM, ABYSSAL DRONE, PRISM KEYS, GLASS BELL,
  AURORA PLUCK, OCEANIC PAD, EMBER LEAD (ANALOG); EVOLVING GLASS (PHASE); TAPE DRONE (LOFI);
  CATHEDRAL REED, TIDAL ORGAN (WHEEL); BREATH CHOIR (FORMANT); SOLAR WIND (NOISE);
  EVOLVE BELL, VELVET EP, AURORA FM (FM6).
- Per engine: ANALOG 56, PHASE 25, LOFI 25, FORMANT 20, WHEEL 18, DRUM 10, NOISE 17, FM6 29 = 200.
- `tests/preset_library_test.py` (run by `tests/run_tests.sh`) checks the total, each engine's share,
  DRUM's ten preserved indices, the retired names and every entry's ranges.

### EVOLVE / MOTION
- New LFO wave **EVO** (`P_LWAVE = 6`, appended to `N_LWAVE`): a smooth random walk with one new point per
  cycle, eased with an integer smoothstep. Reuses RATE, PHASE, FADE, the LFO destinations and the matrix.
- Negative RATE is already the musical division on the transport's grid (4BAR..1/32); positive RATE is the
  free rate. Both are used by the demo sounds. No new parameter, no format change.
- Demo sounds: QUANTUM BLOOM, ABYSSAL DRONE, OCEANIC PAD, EVOLVING GLASS, TAPE DRONE, SOLAR WIND,
  CATHEDRAL REED, TIDAL ORGAN, EVOLVE BELL, AURORA FM.

### Touch — velocity and aftertouch, honestly
- Channel aftertouch (MIDI `0xD0`) continues to feed `MS_AT`, per track.
- New per-voice source **PAT** (`MS_PAT`, appended): polyphonic aftertouch (MIDI `0xA0`) from an external
  controller, routed to the matching held key. The FM-1's 27 key switches have no pressure sensor, so the
  device's own keys never fake per-key pressure.
- Sensitivity and curve: the matrix AMT per destination (CUT, SHP, AMP, PITCH, VIB, E1..E8, CHO, DLY, REV,
  RATE); engines keep their own velocity response. Adding stored curve/sensitivity parameters would have
  grown `P_COUNT` and broken the frozen project layout, so it was deliberately not done.
- Presets demonstrating the routes: PRISM KEYS (VEL→CUT, AT→CUT), GLASS BELL (VEL→CUT, PAT→VIB),
  AURORA PLUCK (VEL→CUT, AT→REV), BREATH CHOIR (AT→E1 vowel), VELVET EP (VEL→CUT, AT→VIB),
  EMBER LEAD (VEL→CUT, AT→VIB).

### UI
- HOME and EDIT WAVE 1 rotary knobs redrawn as real round knobs: filled body, 270-degree ring, value arc,
  needle and centre dot; the offset orbit ring and the bakelite artefact are gone.
- Boot splash redrawn as one SURF card in the UI's language: theme rule, QG monogram with a dotted orbit,
  three waves written across it, the 16-segment rail and its playhead. The old octagonal frame's dark bands
  (read as four black lines) are gone. It is still shapes only, in the saved palette, and still written by
  the same 4 ms timeline.
- No change to the device bootloader, flash layout, or recovery code.

### Verification (see `docs/PROJECT_STATUS.md`, `docs/POLYPHONY_1.6.md`, `docs/TOUCH_1.6.md`)
- **VERIFIED on the host:** 200-sound library test plus the two legacy DRUM indices; UI lint/alignment
  (0 findings) and PNG renders; 220 golden renders and the health suite; installer, Web Studio and backup tests; sanitizer/fuzz runs when
  the local compiler supports them.
- **SIMULATED:** the emulator page and its installer strings, from the same firmware sources.
- **NOT VERIFIED:** anything on a physical FM-1; a 12-voice build on hardware; a rebuilt `felucca.wasm`
  when Emscripten is unavailable in the build environment; package signing (design only, not enabled).
