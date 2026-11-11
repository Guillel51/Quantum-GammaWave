# Cómo hacer un release

Un release es la **versión firmada** que está en `release/`, publicada en GitHub con sus artefactos.
El workflow `release.yml` no recompila: verifica la firma del paquete que ya está en el árbol y publica
esos bytes exactos. Así el release y el sitio no pueden separarse.

## 1. Firmar (una vez por versión, con la clave fuera del repo)

La clave privada vive fuera del checkout, en `~/.fm1-keys/qg1-release.pem` (modo `0600`); nunca entra
al repositorio. `tools/fm1_sign.py sign` se niega a leer una clave de dentro del árbol.

Desde 2.1.1 la firma se hace en dos pasos, y el build puede estar en otra máquina (CI incluida) sin
que la clave se acerque:

```sh
# 1. con el toolchain y el SDK presentes: firmware + paquete + registro de build
JIELI_TOOLCHAIN=$HOME/.jieli/toolchain AC79_SDK=$HOME/fw-AC79_AIoT_SDK ./build.sh --release 2.1.1

# 2. congelar lo que se va a firmar: hashes, tamaño, imágenes, registro de build, versión y fecha.
#    Esto no toca la clave y es lo único que CI puede producir.
python3 tools/fm1_sign.py request build/felucca-2.1.1.fwsc \
  --version 2.1.1 --min 1.0.0 --keyid qg1-release-1 \
  --build-manifest build/manifest.json --out build/sign-request.json

# 3. firmar donde está la clave: en este host, o a través de un firmante externo que la tenga
python3 tools/fm1_sign.py sign --request build/sign-request.json \
  --signer-cmd "ssh signer-host python3 tools/fm1_sign.py sign-stdin --key /secure/qg1-release.pem" \
  --out build/felucca-2.1.1.fwsc.manifest.json

# 4. lo que se publica: los bytes firmados y su manifiesto
cp build/felucca-2.1.1.fwsc build/felucca-2.1.1.fwsc.manifest.json release/
(cd release && sha256sum felucca-*.fwsc felucca-*.fwsc.manifest.json > SHA256SUMS)
python3 tools/fm1_sign.py verify release/felucca-2.1.1.fwsc \
  --manifest release/felucca-2.1.1.fwsc.manifest.json     # tiene que decir "verified"
git add -f release/ && git commit -m "release: 2.1.1, signed by qg1-release-1"
```

`sign` verifica la firma que devuelve el firmante contra la clave fijada antes de escribir nada: un
firmante con otra clave, o que devuelve basura, no produce manifiesto. El nombre de la técnica y los
ejemplos (host offline, `ssh`, tarjeta/HSM) están en [`SIGNING_CUSTODY.md`](SIGNING_CUSTODY.md#41-signing-where-the-key-lives).

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

1. deduce la versión del nombre (`v2.1.1` o `release/v2.1.1` → `2.1.1`);
2. verifica `release/felucca-2.1.1.fwsc` contra su manifiesto con `tools/fm1_sign.py verify`, y que el
   manifiesto nombre esa versión y sus imágenes;
3. crea el GitHub Release con el `.fwsc`, su `.manifest.json` y `SHA256SUMS`;
4. avisa a [mutant.cloud](https://mutant.cloud).

Si la firma no verifica, el workflow falla y no publica nada. En cada push a `main` que toque `release/`,
`signed-release.yml` vuelve a verificar todos los paquetes del directorio contra la clave fijada, exige que
ninguno esté sin manifiesto y comprueba que `SHA256SUMS` describa exactamente lo que hay: un paquete
editado, un manifiesto de otra clave o un `.fwsc` suelto paran el cambio antes del deploy.

CI puede, en cambio, **pedir** una firma: en cada build publica `build/sign-request.json` como artefacto
(`sign-request-<sha>`), que es lo que un custodio firma en su máquina. La clave no está en el runner, ni
en un secreto de Actions, ni en este repositorio.

## 3. El sitio

GitHub Pages compila `main` en cada push, pero **lo que se instala es el release firmado**: `web/make_site.py`
busca en `release/` el manifiesto **firmado más nuevo por versión** cuyo SHA-256 sea exactamente el de los
bytes de al lado y, si lo encuentra, publica *esos* bytes como paquete instalable —con el registro de build
que la firma lleva dentro, así la página dice qué commit va a escribir— aunque el build de `main` sea otro.
`release/` puede tener varias versiones firmadas: la anterior sigue publicada mientras se corta la nueva, y
la que gana se elige por número de versión, no por nombre de archivo.

En la portada, el emulador también es el del release: se compila en un worktree del commit que produjo el
paquete firmado, así "probar" y "instalar" son los mismos bytes. Lo que sigue a `main` —emulador y paquete
sin firmar— vive en `/preview/`, marcado como no instalable. `main` cambia todo el tiempo; una firma no puede
seguirlo, y un instalador que se negara a instalar por eso sería el problema, no la protección.

El instalador verifica el SHA-256 del paquete y la firma Ed25519 contra la clave fijada en
`tools/fm1_keys.json`, y contrasta los hashes de `felucca.bin` y `loader/ota.bin` con el registro de build
que sirve la página. Si algo no cuadra, INSTALL queda deshabilitado y la página dice cuál de las tres
comprobaciones falló. Cuando `main` tiene firmware nuevo sin firmar, el sitio sigue instalando la última
versión firmada: para publicar esa nueva hay que firmarla (sección 1).

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
