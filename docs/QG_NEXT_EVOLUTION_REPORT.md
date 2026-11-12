# QG — evolución, personalización y optimización: estado real

**Fecha:** 12 de noviembre de 2026 · **HEAD de partida:** `2e781a7` · **Versión publicada:** v2.8.1

Este documento responde al pedido de "máximo potencial" con una regla: **nada entra si no está medido y probado**.
Donde el pedido describe trabajo que no se hizo, lo dice; donde hay números, dice de qué clase son.

Clases: `[HOST]` · `[SIM]` · `[TARGET]` · `[JEDEC]` · `[PUBLIC]` · `[FÍSICO]` (**ninguna** en todo el proyecto).

---

## 1. Optimización integral: lo medido y lo aplicado

| Punto | Estado | Evidencia |
|---|---|---|
| Perfil del bloque de audio | **hecho** | gprof (sin valgrind/perf: no hay root): `fx_buses` 45 %, `mix_part` 10 %, `mix_block` 10 %, `rev_room` 10 %, `fm6_op_run` 10 % |
| `fx_buses` a una pasada (dos recorridos sobre los tres buses) | **aplicado y medido** | banco nuevo `tests/fx_bench`, A/B intercalado 6 rondas: mediana 127,2 → 126,0 ns/muestra `[HOST]`; 220 renders dorados bit-idénticos |
| Coste por motor y por camino DSP | **medido** | `tests/target_budget.py` sobre el `.dis`: 39 entradas, todas en presupuesto `[TARGET]` (`analog_render` 148/148, `fm6_render` 322/322, `lofi_render` 197/197, `rev_room` 243/243) |
| Memoria | **medida** | imagen 551 720 B; `.data+.bss` 87 696/98 304 B; pool 305 096 B con spare 15 392 B `[TARGET]` |
| Reproducibilidad | **probada** | dos builds del mismo código desde commits distintos dan bytes idénticos |
| Flags del compilador | **evaluado y descartado** | `-O3` es un 40 % más lento en el camino de audio `[HOST]`; el target compila `-Os` por función porque `-O2` no entra en XIP |
| División por muestra del limitador | **descartado con razón** | cambiaría el audio; una tabla o Newton da otros bits, y esta línea de trabajo promete cero regresiones |
| Reverb en reposo | **descartado con razón** | saltar el núcleo con la compuerta cerrada congela el estado y al reabrir aparece una cola vieja |
| Segundo núcleo | **usado y probado** | reparto por coste medido (`mc_split`, partes + buses); `tests/ui_test.c` comprueba que las partes se reparten y que la suma es idéntica a un solo núcleo |
| UI | **medida** | 187 pantallas × 17 paletas: 0 hallazgos de lint, 0 de 2 777 796 medidas de alineación; animación con redibujo por firma |

Lo que **no** se puede medir aquí: tiempo real en silicio, jitter y consumo. Todo el coste de CPU del target es
modelado sobre el `.dis`, no cronometrado en placa.

## 2. Funciones de otros firmwares: investigación y decisión

Propuestas estudiadas (JIANT, Optimist, Melodee, Jangada, X0X, Felucca, SLOOP, Melodee, zp12, oficial M-VAVE),
con la decisión y el motivo. Ninguna se implementó en esta entrega; todas están dimensionadas para entregas
pequeñas.

| Idea | Origen | Utilidad para QG | Coste / riesgo | Decisión |
|---|---|---|---|---|
| Efectos de inserción por pista además de los buses | Optimist | ocuparía el hueco real del modelo de FX: hoy hay 4 envíos y buses globales; insertar por pista multiplicaría el DSP por 6 | alto (6× el coste de cada inserto) | **estudio**, no esta entrega |
| Retornos por bus con su propia mezcla | Optimist / JIANT | QG ya tiene `fx_vu[3..5]` (retornos) medidos; falta exponer el retorno como control | bajo | **candidato inmediato** |
| Condiciones de disparo y probabilidad por paso | JIANT | encaja en `seq.c`; el paso ya tiene longitud, ventana y chance parcial | medio (compat de formato) | **candidato** |
| Escenas / snapshots de performance | JIANT | estado completo del proyecto ya serializable | medio | **candidato** |
| Parameter locks por paso | JIANT / Elektron | requiere un mapa de parámetros lockeables y su espacio en el proyecto | alto (RAM del proyecto) | **estudio** |
| Rasgueos y arpegios (strum con dirección y dispersión) | Melodee | el secuenciador ya puede emitir acordes; el strum es un desplazamiento por nota | bajo-medio | **candidato fuerte**: se apoya en `chord_build` de la fase 2 |
| Matriz de modulación de varios destinos | Jangada | hoy hay LFO con destino por track; una matriz exige más RAM y un editor nuevo | alto | **estudio** |
| Sonidos 303/909 dedicados | X0X | QG ya los aproxima con ANALOG y DRUM-X; un motor propio compite por el pool | alto | **descartado** (duplica lo que hay) |
| Arranque sin tarjeta / sin SD | oficial M-VAVE | el FM-1 no tiene ranura; sin aplicación | — | **no aplica** |

Criterios usados: utilidad musical real, encaje en la arquitectura existente, coste en RAM/flash/CPU, licencia
(GPL-3.0-only compatible), y si duplica algo que QG ya hace.

## 3. Visual, skins y personalización

Lo que existe hoy: 17 paletas generadas (`ui_palettes.h`), estilos FLAT/LINE, modo LARGE, ANIM ON/OFF,
VIS QUALITY, y animación por firma en HOME y visualizers.

Lo que **no** está hecho y el pedido pide explícitamente:

- Temas conceptuales nuevos (Nebula, Plasma, Organic, Minimal, Retro) **no** existen: las 17 paletas son
  combinaciones de color, no diseños.
- Un sistema de personalización por componentes (fondo, partículas, tarjetas, knobs, medidores, intensidades)
  no existe: el estilo global es único (FLAT/LINE + LARGE).
- Los planetas y criaturas de HOME existen y animan por señal (`bg_energy()`, `fx_vu`), pero no hay capa nueva
  de partículas ni cambios de intensidad por parámetro.
- Indicador de páginas homogéneo: **pendiente** (el pie muestra "DLY 1/3", "HOME", etc. por página, sin un
  indicador común por familia).

Elegí **no** añadir paletas de relleno: un tema "Nebula" que sólo cambia tres colores sería exactamente la
estética cosmética que este proyecto rechaza.

## 4. Seguridad y regresiones (repetido sobre el HEAD actual)

| Comprobación | Resultado |
|---|---|
| Suite completa `[HOST]` | `ALL HOST TESTS PASSED`, 0 FAIL (ASan/UBSan y los dos fuzzers incluidos) |
| 220 renders dorados | 0 changed, 0 gone |
| `coherence_check.py` | ok — paquete, manifiesto, sitio, emulador y `SHA256SUMS` concordantes |
| Paquete real: corrupción, truncado, bytes críticos | `test_pkg_faults` en verde |
| Límites del contenedor | `test_pkg_bounds` en verde |
| Installer vs `fm1pkg.js` | `install_test` en verde |
| Verify Only | 153 checks: no sale ni un frame de escritura |
| JEDEC Puya `0x856014` / UCUN `0xB36014` | allowlist, doble lectura, unknowns rechazados `[JEDEC]`/`[SIM]` |
| Flash protegida | `_Static_assert` de la ventana de escritura; `flash evidence` 20 checks, `flash read` 16 checks |
| **Defecto encontrado y corregido en esta entrega** | dos paquetes históricos (`felucca-2.7.3.fwsc`, `felucca-v2.7.3.fwsc`) estaban en `release/` **sin manifiesto**, y el workflow `signed release` los rechazaba en cada push: se generaron sus manifiestos con el propio `tools/fm1_sign.py` y ahora los 20 paquetes del directorio verifican |

## 5. Estado de publicación

| Componente | Estado |
|---|---|
| Firmware | v2.8.1, paquete 610 959 B, `FM-1_965`, sha256 `7a4e98fd…` |
| `main` | `2e781a7` (+ este commit) |
| Release | `v2.8.1` publicada `[PUBLIC]`, digest del asset = `sha256:7a4e98fd…` |
| Emulador | wasm v2.8.1 desde `b9dab40` |
| Instalador / sitio / Web Studio | v2.8.1; `site_check` y `test_site` en verde |
| README / imágenes | **sin renovar** en esta entrega |

## 6. Riesgos y límites

1. **Ninguna evidencia física.** Instalar en una unidad sigue **NO-GO**: falta lectura real de JEDEC, backup
   verificado de esa unidad y recuperación demostrada.
2. El `README`, el banner y las capturas no se actualizaron: pedirlo y no hacerlo se dice acá.
3. Los temas conceptuales, la matriz de modulación, los inserts por pista y los parameter locks quedan como
   propuestas dimensionadas, no como trabajo hecho.
4. El coste de CPU del target es modelado (`.dis`), no cronometrado en placa.
