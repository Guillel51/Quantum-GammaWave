# Cómo hacer un release

Un release es la **versión firmada** que está en `release/`, publicada en GitHub con sus artefactos.
El workflow `release.yml` no recompila: verifica la firma del paquete que ya está en el árbol y publica
esos bytes exactos. Así el release y el sitio no pueden separarse.

## 1. Firmar (una vez por versión, con la clave fuera del repo)

La clave privada vive fuera del checkout, en `~/.fm1-keys/qg1-release.pem` (modo `0600`); nunca entra
al repositorio. `tools/fm1_sign.py sign` se niega a leer una clave de dentro del árbol.

```sh
# con el toolchain y el SDK presentes: firmware + paquete + registro de build
JIELI_TOOLCHAIN=$HOME/.jieli/toolchain AC79_SDK=$HOME/fw-AC79_AIoT_SDK ./build.sh --release 2.0.0

# la firma cubre el paquete, su SHA-256, el tamaño, la versión y los hashes de las imágenes
python3 tools/fm1_sign.py sign build/felucca-2.0.0.fwsc \
  --key ~/.fm1-keys/qg1-release.pem --keyid qg1-release-1 \
  --version 2.0.0 --min 1.0.0 --build-manifest build/manifest.json

# lo que se publica: los bytes firmados y su manifiesto
cp build/felucca-2.0.0.fwsc build/felucca-2.0.0.fwsc.manifest.json release/
(cd release && sha256sum felucca-2.0.0.fwsc > SHA256SUMS)
python3 tools/fm1_sign.py verify release/felucca-2.0.0.fwsc \
  --manifest release/felucca-2.0.0.fwsc.manifest.json     # tiene que decir "verified"
git add -f release/ && git commit -m "release: 2.0.0, signed by qg1-release-1"
```

El `.fwsc` es reproducible: mientras el firmware no cambie, cualquier commit posterior reconstruye
los mismos bytes y la misma firma sigue valiendo. Si cambia una sola línea del firmware, los hashes
del manifiesto dejan de coincidir con el build del sitio y el instalador **no habilita INSTALL**
hasta que se firme el build nuevo. Eso es lo que se quiere.

## 2. Publicar

Cualquiera de los dos dispara `release.yml`; hacen lo mismo:

```sh
git push origin v2.0.0            # un tag
git push origin release/v2.0.0    # una rama, para entornos que no pueden empujar tags
```

El workflow:

1. deduce la versión del nombre (`v2.0.0` o `release/v2.0.0` → `2.0.0`);
2. verifica `release/felucca-2.0.0.fwsc` contra su manifiesto con `tools/fm1_sign.py verify`, y que el
   manifiesto nombre esa versión y sus imágenes;
3. crea el GitHub Release con el `.fwsc`, su `.manifest.json` y `SHA256SUMS`;
4. avisa a [mutant.cloud](https://mutant.cloud).

Si la firma no verifica, el workflow falla y no publica nada.

## 3. El sitio

GitHub Pages compila `main` y publica el paquete que sale de ese build. `web/make_site.py` busca en
`release/` un manifiesto cuyo SHA-256 sea **exactamente el de los bytes que va a servir**; si lo
encuentra publica la firma al lado del paquete y el instalador la verifica contra la clave fijada en
`tools/fm1_keys.json`. Si no coincide —porque el firmware cambió y todavía no se firmó— el sitio
sigue publicado, pero el instalador dice por qué y deja INSTALL deshabilitado.

## Si algo falla

- **`verify` rechaza el paquete**: los bytes de `release/` no son los que la clave firmó. Volvé a
  firmar el build actual; no edites el manifiesto a mano (la firma cubre cada campo).
- **El workflow falla en `verify`**: el manifiesto y la versión del tag no coinciden, o falta
  `release/felucca-X.Y.Z.fwsc`.
- **El instalador dice que la firma no verifica**: el paquete servido por Pages no es el firmado
  (mirá el SHA-256 que muestra el sitio contra `release/SHA256SUMS`).
- Para reintentar con el mismo número, borrá el tag y volvé a empujar:

  ```sh
  git tag -d v2.0.0 && git push origin :refs/tags/v2.0.0
  git tag v2.0.0 && git push origin v2.0.0
  ```

## Notas

- Un tag que empieza con `v` siempre dispara el workflow: usá otros nombres para tags que no son
  releases.
- `release/SHA256SUMS` cubre el paquete; los hashes de la imagen y del loader viajan dentro del
  manifiesto firmado (`images`), que es lo que el sitio compara con su propio build.
