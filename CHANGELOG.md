## v1.8.2 — Cierre de ciclo: lo que la web decía y el firmware ya no

**Números públicos contra números medidos.** `tests/preset_library_test.py` cuenta **291 sonidos de fábrica** en
diez motores; la portada decía 283, el `fmhub.json` 287 y el README anunciaba `v1.7.0`. Los tres se corrigen a lo
que el código tiene (291 sonidos, versión 1.8.2). Ninguno de esos números se tocó de la tabla de presets: lo que
estaba mal era la copia.

**Un pendiente que ya estaba resuelto.** La desincronización del punto de ENV con el ADSR dibujado se cerró en
v1.12: el punto se coloca con `env_x_of`, que recorre la misma recurrencia que la curva muestra a muestra en vez de
invertir una recta. La matriz lo daba por abierto; ahora dice dónde y cómo se cerró.

**Auditoría de seguridad (sólo lectura, nada debilitado).** `sign_test` (firma válida, alterada, clave desconocida,
downgrade), `key_custody_test` (la clave nunca se sobreescribe, PKCS#8), `install_test` con sus compuertas,
`preflight_test` sobre el paquete real y `test_pkg_bounds`: todos en verde. No hay claves, tokens ni `.env` en el
repositorio; el único `BEGIN PRIVATE KEY` del árbol es una aserción de test sobre una clave que él mismo genera.

**Limpieza.** Sin artefactos de build, `.o`, `.pyc`, `.log` ni respaldos versionados; sin `TODO`/`FIXME` en el
firmware; `build/` y los worktrees `wt/` no están en el repo. Se conservan las auditorías históricas de `docs/`
como registro, marcando cuál es el estado actual.

