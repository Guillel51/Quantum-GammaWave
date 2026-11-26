# Fase 4 — polifonía, pistas, multicore y eficiencia general

Estado: **auditoría cerrada, sin cambios de código de audio**. Ningún número de este documento sale de una
unidad FM-1. Lo que es del AC791N son los presupuestos estáticos del desensamblado; el resto es host x86-64.

## 1. Reparto entre núcleos (puntos 1, 2, 6)

`fx.c: mc_split()` no reparte por número de pista. Es un planificador voraz, por bloque, que asigna cada parte
al núcleo con menos trabajo con:

- **ticks medidos por pista** (`mc_tk[i]`, media exponencial en `mix_part_t`: sube rápido, baja lento),
- **coste del motor por voz** (`mc_cost(&trk[i]) * MC_VW`, `MC_VW` = 24 ticks ≈ 1 µs),
- **los buses pre-cobrados al núcleo 1** (`mc_fx_tk`, desde 0.6.75),
- **las partes DRUM como una unidad** (comparten kit, IMAGE y SEND).

Consecuencia: un FM6 a 12 voces en la pista 2 se cobra a quien lo corre de verdad. `AGENTS.md` §8 describe el
reparto 1–3 / 4–6 como intención de diseño; el código lo generaliza a «el que tenga menos», que es lo que evita
el desequilibrio que pide el punto 2. `mix_part_m0` corre la copia de core 0 desde RAM (`.hot_text`), así que el
reparto no paga XIP.

## 2. Esperas y sincronización (punto 3)

`mc_run()`/`mc_wait()` con `mc_lost()` de respaldo: un core 1 que leyó mal, falló o no volvió queda excluido y
sus partes se recuperan en core 0. `fx_bad`/`fx_trip` y `fx_insane()` por bloque impiden que un bloque roto se
propague. Sin locks ni `malloc` en la ruta de audio: el buzón es de escritura y espera acotada por bloque.

## 3. Voces (puntos 4, 5, 7)

- Pool de **18 ranuras compartidas** (`VPOOLN`), **12 por parte** (`NVOICE`), con **presupuesto común**
  (`voices_busy()` vs `voice_cap()`).
- `voice_alloc()`: reutiliza la misma nota, la ranura DRUM, el slot libre más cercano en GLIDE/REUSE y
  round-robin en ROTATE; roba la cola liberada más antigua, luego la nota sostenida más antigua, **nunca la nota
  sostenida más grave** (el bajo). Si la voz a ceder es de la propia parte, se reinicia en su sitio.
- **Un solo barrido y ninguno bajo el presupuesto** (FASE 3): antes se recorrían NPART × NVOICE voces dos veces
  por nota iniciada; hoy el gobernador de carga decide antes de barrer.
- `load_tight` ya **no corta colas** (v1.5.17): una cola en release renderiza igual que una nota nueva, así que
  matarla no ahorraba CPU y sí acortaba el decay. La protección que queda es el cap y el guardián de
  sobrecarga de `audio.c`, que suelta una voz tras dos mitades por encima del 80 %.

## 4. Polifonía efectiva (punto 8) — no se subió

El límite real no es `NVOICE` sino el presupuesto compartido: subir el número por parte sin tocar el pool no da
voces extra, solo cambia a quién le falta. Y el margen estático del AC791N es el que es: `BLOCK_PATH`
34 845 instrucciones con presupuesto 33 437 (**+4 %**). Sin la unidad no hay forma de medir memoria, CPU,
estabilidad y calidad del caso de 12 voces, y la condición de la fase prohíbe habilitarlo solo porque compila.

## 5. Prioridad y animaciones (puntos 9, 10)

- ISR de audio desde RAMTEXT; TIMER5 (10 kHz) sostiene teclado, LEDs y el contador de ms, y solo llama a
  `usb_poll`/`uart_midi_poll` cuando el audio no está renderizando.
- **LOAD LITE / ECO** baja la animación del panel a 20 / 14 fps (FULL sin freno). La base de píxeles de
  3 321 cuadros fija que el freno no cambia ningún dibujo.

## 6. Picos (punto 11)

Preset, apertura de página y carga de proyecto ocurren en el main loop, no en el ISR: `proj_import` valida sobre
`proj_scratch` y publica la sección con `ready` (`sec_stage`); el cambio de motor hace `memset` de la arena de
esa pista. Las páginas de gráfico respetan el freno de LOAD.

## 7. Escenarios obligatorios — [HOST x86-64]

| escenario | prueba | resultado |
|---|---|---|
| seis pistas simultáneas | `six_track_live_test` | pasa |
| todas las voces ocupadas | `poly_stress_test`, `poly_budget_test` | pasa; todas las medidas dentro del presupuesto del pool |
| FM6 y motores pesados | `audio_stress` | pasa |
| efectos y buses activos | `audio_stress`, `regress` | pasa; 316 renders dorados idénticos |
| cambios rápidos de preset | `morph_six_test` (6 transiciones), `morph_depth` | 0 fallidas |
| sostenidas y liberaciones | `poly_voices_test`, `poly_parts_test`, `poly_phase2_test` | pasan, sin voces colgadas |
| grabación y reproducción | `sequencer` y `record` de `run_tests.sh` | pasan |
| animaciones durante carga de audio | barrido `ui_render` (208 × 22) | 0 hallazgos de lint, 0 desalineaciones en 4 234 479 medidas |

## 8. Lo que falta y cómo se desbloquea

No hay contador de ciclos utilizable en host (reloj de 10 ms) ni unidad. La alternativa es la que el propio
firmware ya tiene: `mc_tk[]`, `mc_fx_tk` y la ocupación del pool. Publicarlos como diagnóstico (página de
estado o línea de log) convierte cualquier decisión sobre `NVOICE`, el pool o los pesos de `mc_split` en una
medición del destino, que es lo que exige la fase.
