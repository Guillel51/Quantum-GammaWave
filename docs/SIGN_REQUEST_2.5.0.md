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
| Commit | `51d109f1a3a4d0131e57e9d728759941bfbbd806` |
| Key ID | `qg1-release-1` |
| `min_firmware` | `1.0.0` |
| Paquete | `felucca.fwsc`, **610959 B** |
| SHA-256 del paquete | `b99a03fe7daffe6afc059a028335176b8cbf7be16e3a2da6a867ec08eb9649a4` |
| `felucca.bin` | 538200 B, `132362aa21311a8e6b52d66f42fcaf253a665a8092a6055432745acfe22b62c6` |
| `loader/ota.bin` | 7803 B, `3ca785b93edaabe9fe383c777a0a9507b0d32e10977574731af65982639c33e1` |
| Emulador (del mismo commit) | `v2.5.0`, WASM 1093893 B, `9c5328b9468646cfccb5f05981cf11a33898752767f2b9cbd5d3f06c9bfe0c20` |

La solicitud congelada es `release/sign-request-v2.5.0.json`; el registro de build que la acompaña es
`release/build-manifest-v2.5.0.json`. Una solicitud sin registro de build se rechaza: no hay release sin
ese registro.

## 2. Obtener los bytes exactos

Dos caminos; los dos terminan en el mismo SHA-256, que es lo que ancla la firma.

**a) Reconstruir desde el commit.** Es reproducible: `target.yml` reconstruye, compara con `cmp` y solo
pasa si los bytes son idénticos.

```bash
git fetch origin 51d109f1a3a4d0131e57e9d728759941bfbbd806
git checkout 51d109f1a3a4d0131e57e9d728759941bfbbd806
python3 -c "import sys; sys.path.insert(0,'tools'); import build; build.generate()"
./build.sh                       # exige JIELI_TOOLCHAIN y AC79_SDK
sha256sum build/felucca.fwsc     # debe dar b99a03fe…9649a4
```

**b) Descargar el artefacto del run que lo produjo.** Run `42957308038` (workflow *target build*),
artefacto `ac791n-51d109f1a3a4d0131e57e9d728759941bfbbd806`, retención 14 días. Es el mismo build que
generó el manifiesto de §1.

En cualquiera de los dos casos: si el SHA-256 no es `b99a03fe…9649a4`, **abortar**. No es el paquete que
esta solicitud describe.

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

Tiene que imprimir `verified: FM-1 version 2.5.0, key qg1-release-1, sha256 b99a03fe…9649a4`.
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
