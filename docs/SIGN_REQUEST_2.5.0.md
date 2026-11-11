# Firmar el paquete v2.5.0 — instrucciones para el custodio de `qg1-release-1`

Estado: **solicitud preparada, paquete construido, firma pendiente.** Nada de esto se publica hasta que la
firma verifique contra la clave pública fijada.

Este documento no contiene ninguna clave y no debe contenerla. La clave privada vive fuera de este
repositorio (`docs/SIGNING_CUSTODY.md`) y la herramienta se niega a usarla si está dentro del checkout.

## 1. Qué se firma exactamente

| Campo | Valor |
| --- | --- |
| Producto | `FM-1` |
| Versión | `v2.5.0` |
| Identidad | `FM-1_965` |
| Commit | `c6db91f01e69aa2292d15e70c5d23f27e9e39ebf` |
| Key ID | `qg1-release-1` |
| `min_firmware` | `1.0.0` |
| Paquete | `felucca.fwsc`, **610959 B** |
| SHA-256 del paquete | `9752a1a6498aa4f090a234fc8a6d38a5c5b6926539301c8f9036e02808f07942` |
| `felucca.bin` | 541492 B, `3cc922a6c7fcdb174d31dcdcf4a3f2f150b00de7385cc2bef8ed9498e8010bb4` |
| `loader/ota.bin` | 7803 B, `3ca785b93edaabe9fe383c777a0a9507b0d32e10977574731af65982639c33e1` |
| Emulador (mismo commit) | `v2.5.0`, WASM 1104688 B, `c1bb42fa1bff1b2d08053e713b90b60cf1f15a18c010a6346479c783460b54da` |
| Registro de build | SHA-256 `377809685507aaa1ae2b139535434bf4866762d49537e2133db32c2a2f96caa8` |

La solicitud congelada es `release/sign-request-v2.5.0.json`; el registro de build que la acompaña está
también suelto en `release/build-manifest-v2.5.0.json`. Una solicitud sin registro de build se rechaza: no
hay release sin ese registro.

Presupuestos del build, tal como los midió `target.yml` (run `42957699477`):

```
.ram_text: 1193 insns, no calls
image 541492 B; RAM .data+.bss 87696 B of 98304; pool 305096 B, spare 15392 B
XIP code 380944 B, constants 140516 B
RAM code 19284 B of 24576; calls reach 79568 B past the code's end
register access: hal/ only (src/, loader/ clean)
```

`spare 15392 B` cumple el mínimo de 8 KiB que exige `AGENTS.md` §8.

## 2. Obtener los bytes exactos

Dos caminos; los dos terminan en el mismo SHA-256, que es lo que ancla la firma.

**a) Reconstruir desde el commit.** El build es reproducible: `target.yml` reconstruye en el mismo job,
compara con `cmp` y solo pasa si los bytes son idénticos. Comprobado también para el emulador: reconstruir
`web/emu/build.sh` sobre este árbol da exactamente 1104688 B y `c1bb42fa…54da`.

```bash
git fetch origin c6db91f01e69aa2292d15e70c5d23f27e9e39ebf
git checkout c6db91f01e69aa2292d15e70c5d23f27e9e39ebf
python3 -c "import sys; sys.path.insert(0,'tools'); import build; build.generate()"
./build.sh                       # exige JIELI_TOOLCHAIN y AC79_SDK
sha256sum build/felucca.fwsc     # debe dar 9752a1a6…7942
```

**b) Descargar el artefacto del run que lo produjo.** Run `42957699477` (workflow *target build*, push a
`main` de `c6db91f`), artefacto `ac791n-c6db91f01e69aa2292d15e70c5d23f27e9e39ebf`, retención 14 días.

En cualquiera de los dos casos: si el SHA-256 no es `9752a1a6…7942`, **abortar**. No es el paquete que esta
solicitud describe.

## 3. Firmar

Con la clave fuera del repositorio:

```bash
python3 tools/fm1_sign.py sign \
  --request release/sign-request-v2.5.0.json \
  --key ~/.fm1-keys/qg1-release.pem \
  --keyid qg1-release-1 \
  --out release/felucca-v2.5.0.fwsc.manifest.json
```

Si la clave está en un host aislado, `--signer-cmd` hace lo mismo sin que la clave llegue a este proceso
(`docs/SIGNING_CUSTODY.md` §4.1).

## 4. Verificar antes de entregar

```bash
python3 tools/fm1_sign.py verify build/felucca.fwsc \
  --manifest release/felucca-v2.5.0.fwsc.manifest.json --product FM-1
```

Tiene que imprimir `verified: FM-1 version 2.5.0, key qg1-release-1, sha256 9752a1a6…7942`.
Cualquier otra salida —otra clave, otra versión, otro hash— significa que no se publica.

## 5. Entregar

Solo el manifiesto firmado, `felucca-v2.5.0.fwsc.manifest.json`, por el canal acordado. Nunca la clave, el
medio cifrado ni su contraseña, y nunca el contenido de la clave en un chat.

## 6. Lo que ocurre después

1. El paquete y su manifiesto entran juntos en `release/` (el workflow *signed release* rechaza un `.fwsc`
   sin manifiesto, y rechaza `release/` si `SHA256SUMS` no lo describe).
2. Se regenera `release/SHA256SUMS` y se ejecuta *signed release*: verifica la firma, el hash y el tamaño
   contra la clave pública fijada, y los hashes de las imágenes que el manifiesto nombra.
3. Solo con eso en verde se crea el tag `v2.5.0` y se publica el Release con esos bytes exactos.

## 7. Por qué la firma no se puede hacer aquí

La clave privada de `qg1-release-1` no existe en este entorno ni en el repositorio: `~/.fm1-keys/` está
vacío y `tools/fm1_keys.json` contiene solo la clave pública. No se genera otra clave ni se sustituye la
fijada: una firma con otra clave no verificaría y no valdría nada.
