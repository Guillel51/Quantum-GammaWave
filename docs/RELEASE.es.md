# Cómo hacer un release

Un release es una versión publicada en GitHub con el paquete `.fwsc` adjunto. Lo hace GitHub Actions
(`.github/workflows/release.yml`) cuando subís un **tag** que empieza con `v`.

## Pasos

1. Asegurate de que `main` tiene todo lo que querés publicar y que los tests de CI están en verde
   (pestaña **Actions** del repo).
2. Elegí el número de versión. Cada parte tiene **un solo dígito**: `1.9.1`, `2.0`, `2.0-rc1`
   (`1.10` no funciona, lo rechaza `tools/build.py`).
3. Creá el tag y subilo. Desde la terminal:

   ```sh
   git checkout main
   git pull
   git tag v1.9.1
   git push origin v1.9.1
   ```

   O desde la web: **Releases → Draft a new release → Choose a tag**, escribí `v1.9.1`,
   **Create new tag on publish**, y **Publish release**. (El workflow agrega los archivos a ese release.)
4. Mirá la pestaña **Actions**: el workflow **release** compila el firmware (unos minutos), crea el
   release con `felucca-1.9.1.fwsc`, el `.bin` de la app y `SHA256SUMS`, y avisa a
   [mutant.cloud](https://mutant.cloud) para que la lista se actualice enseguida.
5. Abrí el release en **Releases**, editá el texto y escribí qué cambió.

## Si algo falla

- Abrí la ejecución en **Actions** y mirá el paso en rojo.
- Para volver a intentarlo con el mismo número, borrá el tag y subilo de nuevo:

  ```sh
  git tag -d v1.9.1
  git push origin :refs/tags/v1.9.1
  git tag v1.9.1
  git push origin v1.9.1
  ```

  (Si el release ya se creó, borralo primero en **Releases**.)
- Un tag que empieza con `v` siempre dispara el workflow: usá otros nombres para tags que no son releases.

## Notas

- El sitio (GitHub Pages) sirve el build de `main`; el instalador comprueba el SHA-256 del paquete antes de escribir.
- `JIELI_TOOLCHAIN_SHA256` (Settings → Secrets and variables → Actions → Variables) fija el hash del toolchain,
  igual que en `target.yml`.
