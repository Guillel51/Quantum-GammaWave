# FASE 7/9 — Auditoría musical y herramientas de performance

Versión de este árbol: **v2.3.3** (`firmware/src/felucca.c`). Clases de evidencia según AGENTS §4: `[HOST]`
nativo x86-64, `[TARGET]` build AC791N, `[STATIC]` lectura de código. **No hubo unidad FM-1 física**: nada de
este documento es una medición de hardware.

Este documento es la auditoría previa que la fase pide antes de agregar nada. **No hay cambios de firmware en
esta pasada**: lo que sigue es lo que existe hoy, con su prueba, y el punto donde la fase choca con un límite
del formato.

---

## 1. Lo que ya existe (y está probado)

| Área | Qué hay realmente | Evidencia |
|---|---|---|
| **ARP** | 14 modos de dirección (`AM_UP`, `DN`, `UPDN`, `DNUP`, `CONV`, `DIVG`, `PINKY`, `THUMB`, `PEDAL`, `ALBT`, `RND`, `WALK`, `UP8`, `CHORD`, `REPEAT`, `TRNS`), con `arp_step_index()` compartido entre el secuenciador y la pantalla (la ruta que marca el gráfico es la que suena). Ocho parámetros: `P_AMODE`, `P_ARATE`, `P_AOCT`, `P_AGATE`, `P_ASWING`, `P_APROB`, `P_AHOLD`, `P_AORDER`. | `seq.c:702` `arp_step`, `seq.c:606` `arp_step_index`; `[HOST]` `motion_test` ok (modos y REPEAT), `etapa7_audit_test` ok (todo índice de todo modo < len para len 2..64) |
| **ARP swing / gate / prob / latch** | Swing por paso alternado (`sw = P_ASWING*period/250`, aplicado con aritmética sin signo: la pasada impar resta), gate `period*P_AGATE/128`, probabilidad `rng&127 <= P_APROB`, latch por `P_AHOLD` con `arp_phys` contando cada key-down. | `seq.c:719-745`; `[HOST]` los dos tests de ARP |
| **Sequencer** | Velocity, gate (`P_SGATE`), chance por paso (`step_chance`), ratchets (`step_ratchet`), ties, swing por track (`P_SDIV`/`P_SSWING`), micro-timing por REC Q, count-in, grabación en tiempo real con overdub. | `seq.c` (`rec_note`, `step_samples`, `seq_src`); `[HOST]` `seq_tools_test` ok, `motion_test` ok |
| **SEQ TOOLS** | DUP / HALF / ROT / REV / CLEAR, EUCLID, MIRROR, THIN, CRESC — sin escribir fuera de LEN ni mover gates ni chances. | `[HOST]` `seq_tools_test` ok |
| **SCL / CHORD** | Escalas (4 tablas consistentes, 5..12 notas por máscara), raíz, cuantización, voicings e inversiones. | `[HOST]` `chord_scale_test` ok; `etapa7_audit_test` ok (coherencia de tablas, HIJAZ/HUNGMIN exactos) |
| **Teclado / controles** | 10 motores × 91 pantallas en cada estado calculado. | `[HOST]` `control_map_test` ok: **24 663 comprobaciones, 0 fallidas** |
| **MIDI** | Parser TRS/USB, CC→parámetro (`MIDI_CC_MAP`), CC de motor por engine, clock externo, después de tocar, RND. | `[HOST]` `midi_uart_test` ok, `mod_test` (matriz de modulación con CC1/CC11) en la suite |

Ninguno de esos seis frentes está roto: las cuatro pruebas que tocan ARP, secuenciador, escalas/acordes,
controles y MIDI **pasan** en este árbol.

> Nota de honestidad sobre dos pruebas: `mod_test` y `song_chain_test` **no se compilan con el comando de una
> línea** que usé para barrer (`-lm` y un solo objeto): la suite los arma con objetos extra (`build/mod_demo`) y
> flags propios. No están fallando; están sin ejecutar por mí en esta pasada. La cifra que cito de `mod_test`
> viene de la suite, no de mi barrido.

## 2. El hallazgo que frena la fase: el espacio de parámetros está lleno

`core.h` lo dice en el propio `enum`:

```
P_LSHP,   /* (v1.12.5) ... Con él P_COUNT llega a 127, el último id que la modulación de 7 bits puede
           * nombrar: el espacio de parámetros está LLENO, y el próximo necesita un formato nuevo de
           * almacenamiento, no un hueco */
P_E0 ... P_E7, P_COUNT
```

Y la modulación guarda cada destino en **7 bits** (`MOTION`), igual que el mapa de parámetros del proyecto.

Consecuencia directa para esta fase: **casi todo lo que se pide agregar es un parámetro por track** —accent
del ARP, dirección del secuenciador, variaciones, voicing por acorde, dive/curve del glide, macros— y no hay
id libre. Meterlo exige:

1. subir `P_COUNT` por encima de 127 y **cambiar el ancho del id de la modulación** (7 bits → más), y
2. un **formato de proyecto v3** con migración, porque los mapas de los stores actuales van por conteo.

Eso no es «agregar una función»: es un cambio de formato que hay que decidir sabiendo que los proyectos
guardados por los usuarios se leen con el mapa viejo. Ninguno de los dos se puede hacer de costado ni
silenciosamente (AGENTS §5: cada estado con su evidencia y su migración en el mismo commit).

**Lo que sí tiene margen:** los ajustes **globales** (`song.g[]`, `G_*`), que terminan en `G_MWIDTH` y siguen
antes de `G_COUNT`. Un parámetro global nuevo no toca el espacio por track ni el ancho del id de modulación:
se agrega al final y el proyecto lo lee por conteo.

## 3. Qué se puede construir sin romper nada, y en qué orden

**Vía A — sin formato nuevo (recomendada para 2.3.x).** Todo lo que sea *comportamiento* o *ajuste global*:

- **ARP**: acentos por posición (el acento ya existe por paso; el ARP puede heredarlo), retrigger al cambiar
  de acorde, y latch que sobreviva a un cambio de preset.
- **Secuenciador**: mejoras de edición que no piden id nuevo (copiar/pegar pasos, extensión de LEN con datos,
  variaciones como rotación determinista sobre los SEQ TOOLS que ya existen).
- **SCL/Chord**: memoria de acordes y progresiones sobre los 64 pads, usando `P_SLPAT`/`P_SLCR` que ya están.
- **Performance/MIDI**: un ajuste global de **transición entre presets** (glide de parámetros al cargar) y otro
  de **ancho de banda de CC**; ambos como `G_*` nuevos al final.
- **Ideas nuevas con los controles físicos**: usando los 7 encoders y el potenciómetro MASTER, sin parámetros
  nuevos — p. ej. un modo «performance» de los encoders que reasigna sus 4 destinos por gesto (ya hay
  `perform.c` y `pfx.c` para colgarlo).

**Vía B — formato v3.** Dirección del secuenciador, variaciones por paso, macros por track, voicings por
acorde y morphing por parámetro, todo con su id propio. Es la vía que hace falta para la fase completa, y
necesita: ensanchar el id de modulación, escribir el migrador v2→v3, y un test que lea un proyecto viejo y
demuestre que suena igual.

## 4. Estado

- Sin cambios de firmware en esta pasada; el árbol queda en `v2.3.3` (2 920 B de flash libres, 4 976 B de RAM,
  `P_COUNT` 127/127).
- Las pruebas de las seis áreas están en verde `[HOST]` (ver §1).
- Falta una decisión de producto —**Vía A ahora, o Vía B con formato v3**— porque la Vía B invalida los
  proyectos guardados con el formato actual y eso lo decide el dueño, no un agente.
