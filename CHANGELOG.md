## v1.5.11 — El banco de presets, medido y nivelado

- **La medición.** `tests/preset_levels` renderiza los 285 presets de fábrica uno por uno (C4 a
  velocidad 100, ataque, cuerpo y el primer tramo del release, por el mix real) y da RMS y pico en dBFS.
  Antes de esta versión: mediana **−18,5 dB**, **28 presets fuera de ±3 dB**, dispersión **25,8 dB**, y los
  extremos eran `CRYSTAL PLUCK` a −36,8 dB y `QUANTUM BEAT` a −11,0 dB.
- **El problema no era el fader.** La tabla `firmware/src/preset_level.c` (generada por
  `tools/gen_preset_levels.py` a partir de esa misma medición) ya pone el nivel al cargar cada sonido, y
  **25 presets seguían fuera de alcance con el fader a tope**: su contenido era demasiado bajo, no su nivel.
- **Seis sonidos corregidos en su patch, no en el número:** `CRYSTAL PLUCK` y `METAL CLUSTER` (ADDTV) ganan
  cuerpo y SUB — el cluster tenía el suboscilador mudo —, `AURORA FM` baja el envío de reverb/delay que lo
  ahogaba, `ABYSSAL DRONE` baja la reverb de 96 a 72, y `STATIC HAZE` / `GEIGER CNT` (NOISE) recuperan densidad
  y dejan de esconderse en su propia reverb.
- **Después, medido con la misma herramienta:** **19 presets** fuera de ±3 dB, dispersión **12,6 dB**, el más
  fuerte `HEARTH KIT` a −16,9 dB (antes −11,0). La tabla de niveles se regeneró desde la nueva medición, así
  que cada preset carga al nivel que le corresponde con el sonido nuevo.
- **Lo que queda fuera a propósito:** `AURORA FM`, `STATIC HAZE`, `GEIGER CNT`, `ARCADE`, `ABYSSAL DRONE`,
  `CRYSTAL PLUCK`, `GLASS BELL` y `POLAR AURORA` siguen 4–11 dB por debajo de la mediana. Son texturas, colchones
  y drones: su RMS bajo es su carácter (un geiger contando chispas, una aurora que se abre en 30 ms de ataque) y
  subirlos más habría sido cambiar el sonido, no nivelarlo. Está documentado en
  `docs/PRESET_BALANCE.md` con la tabla completa antes/después.
- **Sin tocar:** motores, algoritmos, polifonía, FX, routing, UI y gates. Sólo datos de preset y la tabla de
  niveles. Goldens de audio regenerados por el cambio audible (63 renders) y verificados por la suite: 0 fallos
  de salud, 0 de voces, 0 presupuestos de CPU excedidos.

