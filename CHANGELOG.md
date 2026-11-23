## v1.8.1 — Una fuente, un valor por bloque: la matriz deja de resolver la misma fuente dos veces

**El defecto, en el código.** `mod_tsrc()` es pura: lee la LFO del bloque anterior, la VEL/KEY/RAND del último
note-on, la rueda, los macros, el transporte y el reloj del ruido, y no escribe nada. Aun así se llamaba **una vez
por slot**: ocho rutas con la misma fuente resolvían la misma fuente ocho veces por bloque y por pista, y cada ruta
de NOISE pagaba su propio `mod_noise`. En `macro_master` el `mulq15` de la LFO se recalculaba además por slot.

**La corrección.** El valor de cada fuente se guarda en el `mod_t` de la parte (17 enteros y una máscara de bits,
72 B por core) y se resuelve una vez por bloque; `macro_master` mezcla la LFO una vez por pista. La misma cuenta,
muchas menos llamadas: **8 slots con la misma fuente = 1 resolución por bloque** (antes 8), y **2 fuentes distintas
= 2** (la prueba lo comprueba con un contador, `-DMOD_TSRC_COUNT`, que no existe en el build que se publica).

**El audio no cambia.** `regress`: 311 renders, 0 cambiados. `mod_test`: las dos comprobaciones nuevas (`CACHE`)
fallan al reponer las llamadas por slot y pasan con la caché.

**Lo que ya existía y se verificó, sin duplicarlo.** La secuencia expresiva completa: probabilidad por paso
(`step_chance`, página CHANCE), ratchets de x1 a x4 con su división y largo (`SF_RATCH`, `ctime`), microtiming por
paso (`ST_MICRO`, ±8/16 de paso, temprano y tarde), acentos por paso y por lane, velocity por paso con humanize,
variaciones reproducibles (MUTATE con semilla, FUGUE) y su edición desde los knobs y el editor SysEx (chance y
ratchet viajan en el registro del preset). No se añadió un segundo secuenciador.

