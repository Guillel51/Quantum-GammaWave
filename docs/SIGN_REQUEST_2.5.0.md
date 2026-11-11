# v2.5.0: qué se publicó y qué queda por firmar

Estado: **v2.5.0 publicado por el modelo SHA-256.** El manifiesto Ed25519 queda como registro histórico;
desde v2.6.0 el camino activo de instalación es SHA-256 + tamaño, y Ed25519 solo documenta el pasado.

Este documento no contiene ninguna clave y no debe contenerla. La clave privada vive fuera de este
repositorio (`docs/SIGNING_CUSTODY.md`) y la herramienta se niega a usarla si está dentro del checkout.

## 1. El paquete que realmente se publicó

El Release `v2.5.0` (publicado 2026-11-11T03:46:55Z) lleva estos bytes. Se descargaron del Release y se
volvieron a hashear aquí; las cuatro comprobaciones coinciden.

| Campo | Valor |
| --- | --- |
| Producto | `FM-1` |
| Versión | `v2.5.0` |
| Identidad | `FM-1_965` |
| Commit | `b5450ace78b03452809bc89e7c7ca3c77377542b` |
| Paquete | `felucca.fwsc`, **610959 B**, `d4f65dd1959beca64dd138da69dd26002926428240f7817bd324b4387edd7421` |
| `felucca.bin` | 537936 B, `4fefc6f1c20124ffcf82ba3ea0c2cc258ae8bb939074984a927e896b45bf0cdd` |
| `felucca.wasm` | 1095007 B, `3e9ded0cca2970aa3934b2deef3d27b2710995e541af9bc153361e9afc00a218` |
| `loader/ota.bin` | 7803 B, `3ca785b93edaabe9fe383c777a0a9507b0d32e10977574731af65982639c33e1` |
| Registro de build | `manifest.json` del Release, `a78c9308789d134e77afdc28771517df3a1db006f0e3a45e0f6213378673d39d` |

Presupuestos medidos por el build:

```
.ram_text: 1193 insns, no calls
image 537936 B; RAM .data+.bss 87648 B of 98304; pool 305096 B, spare 15392 B
XIP code 377396 B, constants 140528 B
RAM code 19264 B of 24576; calls reach 83116 B past the code's end
register access: hal/ only (src/, loader/ clean)
```

`spare 15392 B` cumple el mínimo de 8 KiB de `AGENTS.md` §8.

## 2. Los archivos que lo acompañan

- `release/sign-request-v2.5.0.json` — solicitud `qg1-sign-request-1` generada desde **esos** bytes, no
  desde otros. Nombra el commit `b5450ac` y el SHA-256 `d4f65dd1…7421`, que es el del paquete publicado.
- `release/build-manifest-v2.5.0.json` — el registro de build de ese paquete, igual al `manifest.json`
  del Release.
- `release/SHA256SUMS` — describe exactamente el contenido de `release/`; el workflow *signed release*
  rechaza el directorio si no lo hace.

Una solicitud anterior nombraba el build de `c6db91f` (`9752a1a6…7942`), que nunca se publicó: se
reemplazó por esta, que sí corresponde al paquete que la gente descarga.

## 3. Si alguna vez hay que firmar v2.5.0 con Ed25519

Solo como ejercicio histórico: el paquete ya no depende de esa firma, y una firma nueva no cambiaría los
bytes publicados.

```bash
# 1. obtener los bytes exactos
curl -LO https://github.com/Guillel51/Quantum-GammaWave/releases/download/v2.5.0/felucca.fwsc
sha256sum felucca.fwsc     # d4f65dd1…7421, 610959 B; si no coincide, abortar

# 2. firmar desde la solicitud, con la clave fuera del repositorio
python3 tools/fm1_sign.py sign \
  --request release/sign-request-v2.5.0.json \
  --key ~/.fm1-keys/qg1-release.pem \
  --keyid qg1-release-1 \
  --out release/felucca-v2.5.0.fwsc.manifest.json

# 3. verificar contra la clave pública fijada
python3 tools/fm1_sign.py verify felucca.fwsc \
  --manifest release/felucca-v2.5.0.fwsc.manifest.json --product FM-1
```

`--signer-cmd` hace lo mismo con la clave en un host aislado (`docs/SIGNING_CUSTODY.md` §4.1). El
manifiesto firmado se entrega solo, por el canal acordado; nunca la clave ni su medio.

## 4. Por qué la firma no se puede hacer aquí

La clave privada de `qg1-release-1` no existe en este entorno ni en el repositorio: `~/.fm1-keys/` está
vacío y `tools/fm1_keys.json` contiene solo la clave pública. No se genera otra clave ni se sustituye la
fijada: una firma con otra clave no verificaría y no valdría nada.
