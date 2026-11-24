# Presupuestos de RAM del FM-1 (v1.11.2)

Clase de cada cifra: **build AC791N real**, `build/felucca.map` (linker de pi32v2), leído por
`tools/mem_budget.py`. Es memoria del firmware, no una simulación; no mide pilas ni el uso en ejecución.

## Las cinco ventanas (firmware/app.ld)

| ventana | qué guarda | usado | tamaño | libre | piso que vigila la suite |
|---|---|---|---|---|---|
| LOWBUF | staging del lazo principal | 31 648 B | 32 768 | 1 120 | 1 024 |
| RAM | `.data+.bss`, el presupuesto de 96 KiB | 92 652 B | 98 304 | 5 652 | 4 096 |
| POOL | líneas de FX, canvas y pilas del core 1 | 312 124 B | 327 680 | 15 556 | 4 096 |
| RAMTEXT | código que corre con la flash ocupada | 3 792 B | 24 576 | 20 784 | 1 024 |
| NOINIT | datos que sobreviven al reinicio | 15 572 B | 15 696 | **124** | 0 |

## Por qué NOINIT era el riesgo

La flash está a 928 B (imagen 582 684 de 583 612) y se ve en cada build; **NOINIT tenía 124 B y ninguna regla**.
`.noinit` está en `0x01C7C000..0x01C7CD50`, pegado a `_guard0` y a las pilas de usuario y de excepción: un
buffer que creciera de más enlazaba bien —el linker no revisaba esa ventana— y después pisaba memoria viva.
Desde v1.5.12 hay un `ASSERT`, igual que el de LOWBUF desde 1.4.5.

## Cómo se vigila

```bash
./build.sh                      # escribe build/felucca.map
python3 tools/mem_budget.py     # tabla + veredicto (--show la imprime siempre)
```

`tests/run_tests.sh` lo corre cuando el mapa existe. Los pisos viven en `tests/mem_budget.json`: bajarlos es una
decisión explícita con su medición, no un efecto colateral de añadir una función.

## Lo que sigue sin medirse

- Las **pilas**: su uso real sólo se ve en la unidad, pintándolas y leyendo la marca de agua tras un rato de uso.
- El **uso del pool en ejecución**: `build.sh` imprime la holgura al enlazar, no hay medición en vivo.
