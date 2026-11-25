# Fase 5 — auditoría de partida (motores, presets, modulación, FX)

Clase de cada cifra: **[HOST x86-64, fuentes reales]** para los presets y la biblioteca; **[TARGET AC791N]**
para memoria e imagen. Nada aquí es una medición de hardware.

## El release

`make_release --check`: *ok — v2.5.0: release/, site/, el instalador, el editor y ambos SHA256SUMS describen un
paquete*. `coherence_check`: *ok*. El set incluye todo lo de esta conversación (grabación con largo real, ocho
voicings, el par de deshacer en RAMTEXT, la consola FX con movimiento y el repintado al cambiar de skin).

## Los 293 presets, medidos ([HOST], `tests/preset_audit_test.c`)

| motor | presets | silencio | recortados | nombres repetidos | duplicados exactos | fuera de rango | DC > 2 % |
|---|---|---|---|---|---|---|---|
| ANALOG | 73 | 0 | 0 | 0 | 0 | 0 | 1 (1,05 %) |
| PHASE | 34 | 0 | 0 | 0 | 0 | 0 | 3 |
| LOFI | 29 | 0 | 0 | 0 | 0 | 0 | 2 |
| VOICE | 25 | 0 | 0 | 0 | 0 | 0 | 0 (0,47 %) |
| WHEEL | 22 | 0 | 0 | 0 | 0 | 0 | 0 (0,39 %) |
| DRUM | 18 | 0 | 0 | 0 | 0 | 0 | 0 (1,26 %) |
| NOISE | 22 | 0 | 0 | 0 | 0 | 0 | 0 (0,95 %) |
| FM6 | 37 | 0 | 0 | 0 | 0 | 0 | 0 (0,32 %) |
| WTABLE | 23 | 0 | 0 | 0 | 0 | 0 | 0 (0,44 %) |
| ADDTV | 10 | 0 | 0 | 0 | 0 | 0 | 0 (0,21 %) |

Los cinco con DC por encima del 2 % — **reportados, no fallados** (el auditor los mide y el umbral es informativo):
`PHASE PD 808` (2,1 %), `PHASE ETHNIC PLUCK` (2,2 %), `PHASE SYNC CATHEDRAL` (3,6 %), `LOFI BYTEBEAT` (2,2 %),
`LOFI BYTE VAR` (2,6 %). Son formas asimétricas a propósito: el DC comes headroom, no se oye, y corregirlo
cambia el timbre, así que cada uno pide escucha antes de tocarlo.

## La biblioteca como diseño ([HOST], `tools/preset_audit.py`)

- categorías: PADS 68, KEYS 58, TEXTURES 51, DRUMS 24, BASS 21, BELLS 21, ORGANS 18, LEADS 13, PLUCKS 12, ARPS 7;
- **291 de 293** presets llevan movimiento (`x[]`: LFO, matriz, filtro, dist type, voice);
- **ninguno** sin envío: los cuatro envíos (DIST / CHORUS / DELAY / REVERB) se usan en toda la banca;
- pares a menos de 0,035 de distancia: **0**; el más cercano es 0,045 (`808 SUB` / `TRAP 808`, DRUM).
  La banca no tiene sonidos que un oyente llamaría el mismo.

Conclusión de la auditoría: **no hay duplicados, ni silencios, ni recortes, ni parámetros fuera de rango**. Lo que
queda por hacer en la fase es diseño (A y B), no reparación (C y D).

## Lo que la fase 5 todavía NO tiene hecho

Ordenado por valor por byte gastado, con el archivo donde vive cada cosa:

1. **Destinos de modulación** (`mod.c`, `MD_*` y `param_of_dest`): la lista ya cubre PITCH, CUT, SHP, AMP, PAN,
   los tres envíos + DIST, RATE, VIB, VEL/AT, los ocho E, CLIP, PNCH, los cuatro CHARACTER, RES, ATK/DEC/REL,
   HARM y LSHP. Los huecos que valen algo son **OSC MIX / PW** (el motor que no los alcanza por matriz) y
   **FEEDBK de FM6**: cada uno es un enum, un nombre y una escritura más en `param_of_dest`, y ninguno cambia
   los renders dorados mientras ningún preset lo use (hay que dejarlo apagado en los 293).
2. **Movimiento por motor** (A): los diez motores ya reciben LFO, matriz, VEL/AT y CHARACTER; lo que falta es
   *rango* y *curvas* — p. ej. WTABLE con transición entre frames interpolada por posición fraccionaria y
   ADDTV con un parcial más de control. Los dos tocan DSP caliente: exigen `tests/target_budget.py` y los 311
   renders dorados regenerados **con justificación**, no a ciegas.
3. **Estéreo y buses** (C): el ancho está en `G_MWIDTH`; lo que no existe es un *send post-pan por bus*, que
   pediría un parámetro por pista (formato de proyecto) — se decide con la matriz de compatibilidad, no de paso.
4. **Presets** (D): actualizar sonidos para usar lo nuevo sólo después de (1) y (2), preservando índices y nombres.

Nada de esto se implementó en esta ronda: la fase entra por su auditoría, con las cifras de arriba como punto de
partida medido. Decirlo así es la única forma honesta de dejarlo escrito.
