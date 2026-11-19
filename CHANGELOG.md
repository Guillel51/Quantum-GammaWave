## v1.5.10 — El canvas dibuja de a dos píxeles, y el presupuesto medido

- **`cv_rect` cambia de stores.** Es la primitiva más usada del canvas (cada tarjeta, pozo, barra y banda), y
  escribía un píxel de 16 bits por vez. El canvas está alineado a 4 bytes (`cv_buf`), así que a partir de un
  índice par cada fila se escribe de a dos píxeles en un solo store de 32 bits, con el primer píxel suelto si la
  fila arranca impar y el último si termina impar: **los mismos bytes, la mitad de los stores**. Una
  intersección que el recorte dejó vacía ahora vuelve antes de recorrer la fila.
- **Medición, con su clase.** En host x86-64 (el barrido completo de UI: 3056 frames, 3 corridas por binario)
  el cambio queda dentro del ruido: 38.37 s de CPU de usuario de media antes, 38.15 s después (−0.6 %, spread
  ±0.8 %), y los **3056 frames son byte a byte idénticos**. En el AC791N no está medido: es un cambio de
  instrucciones, no de píxeles, y sin unidad física no hay número que dar. No se presenta como una mejora
  medida en el dispositivo.
- **Revisión de recursos (medido, no estimado).** La imagen AC791N queda en **582 684 B** de 583 612 B (**928 B**
  libres en el slot; +68 B de código respecto de v1.5.9, el precio del cambio de stores) y `.data+.bss` sigue en
  **91 648 B** de 98 304 B (6 656 B libres): la RAM no cambia. El presupuesto de flash es el recurso más ajustado
  del proyecto y este cambio no lo empeora de forma significativa, pero tampoco lo mejora. Perfil de host con `-pg` sobre `eng_bench` (3000 bloques × 10
  motores): `fx_buses` 11 %, `cv_blit` 8.7 %, `ux_mix` 7 %, `cv_rect` 7 %, `rev_room` 5 %. El DSP, el robo de
  voces, la UI y el transporte no cambian de comportamiento en esta versión.
- **Pendientes anteriores cerrados.** La instalación desde V16 (v1.5.9) sigue verificada: `test_v16_path`
  vuelve a pasar dentro de la suite completa.
- **Sin cambios en:** motores, presets, FX, MIDI, SEQ/ARP, polifonía, backup, instalador y gates.

