# RECONCILIACIÓN DE PROCEDENCIA FWSC Y PAQUETE DE RELEASE (QG-1)

## 1. Identificación y Comparativa de los Dos Archivos `.fwsc`

En el entorno de desarrollo y compilación coexisten dos artefactos empaquetados (`.fwsc`) de exactamente 610.806 bytes:

| Archivo | SHA-256 | Commit de Origen | Rol / Destino |
|---|---|---|---|
| `release/felucca-v1.9.0.fwsc` | `80ad995caf9147f28fcbe3b135e3ab8da3e4509bdfed6d4766fee09c53eafed2` | `7eb9636f58f442ccbfb8440dec0aa61e55203889` | **Paquete Oficial Firmado para Release e Instalador** |
| `build/felucca.fwsc` | `985f7441f0799e402a81aa1ae35bcaec3e3b30bef1f9eeda643169913185f6e8` | `786a9cbc0a7ef1d149c90a4dde8203553877aa14` | Artefacto de compilación local (`build.sh`) |

## 2. Explicación Técnica de la Diferencia de Hashes
1. **Procedencia del binario release (`80ad995c...`):**
   - Compilado en entorno de producción certificado con el SDK referenciado en `tools/build.py` (`SDK_SHA256`: `uboot.boot` = `4e3b4c22...`, `eq_cfg_hw.bin` = `41167491...`).
   - Imagen resultante: `felucca.bin` de 531.992 bytes.
   - Firmado con la clave de release privada `qg1-release-1`.
   - **Clave pública anclada:** `70b6036fd33da3297a41da2141c5d135b3efce2d815d55911fc4f9b1d07fcd40` en `tools/fm1_keys.json` e indexada en el instalador web.
   - **Verificación criptográfica:** Aprobada al 100% mediante WebCrypto Ed25519 en `web/fm1ota.js`.
2. **Procedencia del binario local `build/felucca.fwsc` (`985f7441...`):**
   - Compilado con el SDK de JieLi clonado de Gitee (`fw-AC79_AIoT_SDK` commit `0617f4cc`), cuyos blobs auxiliares difieren del SDK 1.2.1 pinned (advertido por `build.sh`: *warning: SDK uboot.boot differs from AC79NN_SDK_V1.2.1*).
   - Imagen resultante: `felucca.bin` de 532.120 bytes.
   - Este archivo es un producto intermedio en el directorio `build/` (ignorado en git), utilizado para auditar compilación target y presupuestos de memoria.

## 3. Paquete Destinado al Instalador Web
El paquete que distribuye GitHub Pages y que consume el instalador web (`web/index_pkg.html`) es estricta e inequívocamente:
- **`release/felucca-v1.9.0.fwsc`**
- **SHA-256:** `80ad995caf9147f28fcbe3b135e3ab8da3e4509bdfed6d4766fee09c53eafed2`
- **Manifiesto:** `release/felucca-v1.9.0.manifest.json` (Firma Ed25519 `a0a8688f...` verificada con clave `qg1-release-1`).
