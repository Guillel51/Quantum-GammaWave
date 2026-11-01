# Quantum GammaWave (QG-1): Galería Visual e Interfaz Real

Las 20 pantallas de referencia en `assets/ui-art/art/` y la suite extendida en `assets/screenshots/` son **renders reales píxel a píxel a 240 × 240** generados directamente desde el motor gráfico embebido del firmware Quantum GammaWave (QG-1) para el hardware M-VAVE FM-1 (SoC dual-core JieLi AC791N).

El firmware renderiza toda la interfaz proceduralmente mediante dibujo vectorial, fuentes anti-aliasing y primitivas optimizadas en C (`firmware/src/ui*.c`, `firmware/src/gfx.c`), sin almacenar mapas de bits pesados en flash.

## Galería de Pantallas (Paleta Insignia: QUANTUM)

1. **`01_home.png` — HOME**: Analizador de espectro reactivo de 28 barras, sol Hearth y luna orbital, curva de filtro en tiempo real y tira de pistas T1–T6.
2. **`02_track.png` — EDIT 1 (ANALOG)**: Dos osciladores virtuales, corte, resonancia y la cinta espectral Wraith (`FLU`, `WHI`, `VOX`, `CHR`, `BEL`, `SCR`).
3. **`03_fm_algorithm.png` — FM6 ALGORITHM**: Diagramas interactivos de los 32 algoritmos FM de 6 operadores (estilo DX7).
4. **`04_track_map.png` — TRACK MAP / PHRASES**: Distribución de frases musicales y patrones por pista.
5. **`05_sequencer.png` — STEP SEQUENCER**: Secuenciador por pasos con edición de microtiming, velocity y flags.
6. **`06_seq_performance.png` — PIANO ROLL DINÁMICO**: Visualización fluida de notas activas, ligaduras y trayectoria del cabezal de reproducción.
7. **`07_drum_x.png` — DRUM-X SYNTH PADS**: Matriz de 8 pads sintetizados (BD, SD, CP, CH, OH, TM, RS, CB) con modulación en vivo.
8. **`08_drum_morph.png` — DRUM-X MORPH & HIT**: Edición de capas y morfología de pads ante impacto.
9. **`09_macro.png` — HEARTH MACROS**: Capa de modulación rápida de macros Hearth (gravedad, calor, eclipse).
10. **`10_macro_map.png` — MOTION SEQUENCER (MSEQ)**: Automatización continua de parámetros y trayectoria por paso.
11. **`11_punch_in_fx.png` — PUNCH-IN FX**: Matriz de efectos en vivo cuantizados (stutters, repeticiones, filtros, saltos).
12. **`12_fx_rack.png` — FX RACK**: Suite de distorsiones armónicas cálidas (`SOFT`, `TAPE`, `HEARTH`, `FLUX`) y multiefectos.
13. **`13_mixer.png` — MASTER MIXER**: Mezclador global de 6 pistas con vúmetros de pico, balance panorámico y envíos de reverb/delay.
14. **`14_song.png` — SONG ARRANGER**: Encadenamiento de secciones y bloques de patrones para canciones completas.
15. **`15_chord.png` — ESCALAS & ACORDES**: Cuantización por notas de escalas musicales y generador armónico de acordes.
16. **`16_arp.png` — ARPEGIADOR DINÁMICO**: Trayectoria de arpegio de 16 pasos con visualización de modo de dirección y swing.
17. **`17_automation.png` — AUTOMATIZACIÓN & PARAMETER LOCKS**: Parámetros bloqueados en pasos específicos de la secuencia.
18. **`18_visualizer.png` — VISUALIZADOR DE AUDIO FULL-SCREEN**: Osciloscopio con sincronización de cruce por cero y órbitas Lissajous (`HOME -> HOME`).
19. **`19_save_project.png` — PROJECT MANAGER**: Almacenamiento seguro en ranuras de memoria flash física.
20. **`20_performance.png` — MODO PERFORMANCE ARMÓNICO**: Tiras de rendimiento con filtros en vivo y contornos envolventes.

## Renders Adicionales de Nuevas Características (`assets/screenshots/`)

- **`quantum_21_master_eq.png`**: Pantalla del Ecualizador Paramétrico Maestro Biquad Q14 (Direct Form II Transposed: PEAK & NOTCH).
- **`quantum_22_transient_presets.png`**: Panel flotante no bloqueante de selección rápida de presets (1.5 segundos).
- **`quantum_23_glo2_telemetry.png`**: Pantalla de telemetría y diagnóstico en vivo GLO 2 (conteo de voces, reparto de polifonía de 10 voces, carga de doble núcleo).
- **`quantum_24_about_credits.png`**: Pantalla ABOUT / CREDITS con métricas de sistema y código QR hacia el portal oficial.
- **Temas Visuales**: Capturas en temas exclusivos `SOLARIS`, `CYBERPUNK`, `NORDIC` y `GAMMA`.

Las formas vectoriales de soporte están en [assets/ui-shapes/](../ui-shapes/) (SVG).
