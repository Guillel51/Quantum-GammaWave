# Remaster de presets — lo que hay, lo que se midió y lo que queda

**Estado:** auditoría hecha, biblioteca **no modificada**. Este documento dice por qué, con números, y
deja la lista concreta de lo que sí merece un cambio y lo que no.

---

## 1. La biblioteca real

**267 presets de fábrica en 9 motores** (`build/host/preset_audit_test --dump` →
`tools/preset_audit.py`). Reparto por categoría:

| Categoría | n | | Motor | n |
|---|---|---|---|---|
| PADS | 57 | | ANALOG | 68 |
| KEYS | 56 | | FM6 | 37 |
| TEXTURES | 49 | | PHASE | 32 |
| DRUMS | 24 | | LOFI | 29 |
| BASS | 18 | | VOICE | 25 |
| ORGANS | 18 | | WHEEL | 22 |
| BELLS | 15 | | NOISE | 22 |
| LEADS | 12 | | DRUM | 18 |
| PLUCKS | 11 | | WTABLE | 14 |
| ARPS | 7 | | | |

## 2. Lo que la medida dice que está bien

Del audit de sonido (`tests/preset_audit_test.c`, 267 presets por el camino real de carga y render):

- **0 silenciados** — ningún preset de fábrica deja el motor mudo.
- **0 al rojo** — ninguno vive pegado al recortador suave.
- **0 nombres repetidos**, **0 duplicados exactos** de parámetros, **0 parámetros fuera de rango**.
- **265 de 267 llevan movimiento** (`x[]`: LFO, matriz, filtro, tipo de distorsión, voz). Los dos que no
  son `HEARTH KIT` y `CYBER DRUM`, y está bien: son kits de batería, no pads.
- **Ningún preset se entrega seco**: los 267 llevan al menos un envío. La mediana del envío de reverb es
  51/127, delay 30, chorus 30, distorsión 11.

Es decir: **la biblioteca no está «desactualizada» ni rota**. Inventar variaciones mínimas para inflar el
número sería exactamente lo que la fase prohíbe.

## 3. Lo que sí está mal: el equilibrio

La tabla completa está en `docs/QG_EVOLUTION_PHASE_5_ENGINES.md` §3. Lo que hay que arreglar, ordenado:

### 3.1 La familia NOISE entera (12 dB baja)

Mediana **−29,7 dBFS** contra −17,9 de ANALOG, −16,0 de WTABLE y −15,2 de DRUM. Cambiar de un pad
ANALOG a un pad NOISE baja el volumen una barbaridad. No es carácter: es un salto de familia.

### 3.2 Sospechosos por tipo de sonido (mismo rol, nivel muy distinto)

| Preset | Motor | Medido | vs su motor |
|---|---|---|---|
| `STEEL DRUM` | FM6 | −29,1 dBFS | −9,5 |
| `DEEP MARIMBA` | FM6 | −26,2 | −6,6 |
| `MARIMBA` | FM6 | −33,1 | −13,5 |
| `CHROME PLUCK` | WTABLE | −27,5 | −11,5 |
| `ANGEL AIR` | VOICE | −28,5 | −11,5 |
| `VOX DRONE` | VOICE | −25,6 | −8,7 |
| `TIN HAT` | DRUM | −27,3 | −12,2 |
| `NEON CLAP` | DRUM | −21,5 | −6,4 |

Un `MARIMBA` 13,5 dB por debajo de sus vecinos de FM6 es un preset que se pierde en la mezcla.

### 3.3 Los que **no** hay que tocar aunque el número asuste

`GEIGER CNT` (−52,0), `RISER` (−43,3), `SOLAR WIND` (−45,8), `STATIC HAZE` (−42,3): son clics
espaciados o barridos que empiezan callados. El RMS de una ventana los hunde sin que estén mal, y
subirlos les quitaría el carácter. Igual `SOLAR WIND`/`DUST HAZE`, que son texturas de fondo a propósito.

## 4. Lo aplicado (revisión posterior)

La medición se convirtió en cambios cuando hubo flash y una prueba que midiera el nivel de cada preset tal
como carga. **52 presets llevan ahora su propio `P_LEVEL`**, en el `x[]` que ya existía: el array mide lo
mismo con tres entradas que con cuatro, así que el cambio cuesta **0 B de flash** (imagen 580 404 B antes y
después).

| | antes | después |
|---|---|---|
| Mediana de la familia NOISE (a nivel propio) | −29,7 dBFS | **−21,8 dBFS** |
| Rango NOISE | −52,0 … −15,2 | **−40,8 … −17,4** |
| Desviados ≥6 dB de su motor, fuera de NOISE | 30 | 0 (nivelados a su familia) |
| Presets al rojo tras el cambio | 0 | **0** |

Método: `engine_param_test --table` mide cada preset a un nivel fijo de 100 y `--table-own` al nivel que
carga de verdad; `--cal` mide qué hace el nivel (sube ~0,6 dB por unidad hasta 105 y se aplana por encima,
por el recortador suave del master: de 110 a 127 apenas hay 0,4 dB). Con esa curva se invirtió el objetivo
(−18 dBFS para NOISE, la mediana de su motor para los desviados), con dos topes: pico ≤ −2 dBFS y nivel
≤ 127. Los 12 presets que necesitaban sitio y ya usaban las seis entradas de `x[]` **se han dejado como
estaban**: quitarles un parámetro para poner el nivel habría empeorado el sonido.

**El techo es real.** `RISER` (−31,3), `GEIGER CNT` (−40,8), `SOLAR WIND` (−32,8), `STATIC HAZE` (−31,4) y
`DUST HAZE` (−27,5) siguen lejos: están ya a nivel 127 y son sonidos de eventos espaciados, cuyo RMS no
sube aunque suba el nivel. Sacarlos de ahí exige cambiar el diseño del sonido (densidad, banda, decay), no
su volumen, y eso no se ha hecho: cambiaría el carácter.

## 5. Por qué no se tocó más

1. **Un preset es un sonido que el dueño ya conoce.** «Evita cambios cosméticos» es explícito, y subir
   el nivel de un preset no es cosmético: cambia cómo suena en una canción ya hecha.
2. **Cuesta flash.** El nivel de un preset se guarda en su `x[]` (una lista de pares id/valor que se
   aplica al cargar). Añadir una entrada a un preset puede hacer crecer el array del motor: hay **220 B**
   de margen en la región de la aplicación. Un remaster completo de la biblioteca no cabe sin liberar
   flash primero.
3. **No hay oídos aquí.** El nivel se mide; el timbre no. Los ocho sospechosos del §3.2 necesitan
   escucharse antes de tocarlos.

## 5. Plan concreto (lo que haría, en este orden)

1. **NOISE: subir el nivel de la familia** (una entrada por preset, o el parámetro de nivel del motor si
   lo tiene) hasta que su mediana quede a ±2 dB de ANALOG. Es el cambio con más efecto por byte.
2. **Los ocho del §3.2**, uno a uno, escuchados y nivelados contra su familia.
3. **Nada más.** Ni renombrar, ni recolorear, ni reordenar: la auditoría no ha encontrado ningún defecto
   estructural que lo justifique.

## 6. Cómo se comprueba hoy

```bash
cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/preset_audit_test tests/preset_audit_test.c -lm
build/host/preset_audit_test                       # estructura, silencio, nivel, duplicados
cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/engine_param_test tests/engine_param_test.c -lm
build/host/engine_param_test                       # 216 mandos EDIT: ¿mueve el sonido?
build/host/engine_param_test --levels              # RMS de los 267, por motor
```

Los dos están en `tests/run_tests.sh`. El primero ya estaba; el segundo es nuevo en esta fase.

**No verificado:** el timbre. Ningún preset se ha escuchado en el FM-1.
