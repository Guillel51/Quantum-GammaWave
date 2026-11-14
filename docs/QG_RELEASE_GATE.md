# El gate de `release.yml`: qué exige, por qué se traba y cómo destrabarlo

Fecha: 2026-11-14. Estado medido, no supuesto.

## 1. Qué exige hoy

`.github/workflows/release.yml` abre con un job `gate` que corre antes de todo lo demás:

```yaml
gate:
  runs-on: ubuntu-24.04
  steps:
    - uses: actions/checkout@v4
    - name: a 3.x release needs an explicit approval
      env:
        QG_RELEASE_APPROVED: ${{ vars.QG_RELEASE_APPROVED }}
      run: |
        set -euo pipefail
        V=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c | head -1)
        python3 tools/release_gate.py "$V" "${QG_RELEASE_APPROVED:-false}"
```

`tools/release_gate.py` acepta dos condiciones, y las dos tienen que darse: la versión del firmware tiene
que ser `vX.Y.Z` completa, y la aprobación tiene que ser `true`, `1` o `yes`. Un tag `3.x` sin esa variable
no llega al `build`, y por lo tanto no hay attestation: el paso `attestations: write` vive en el job de
`zvenson/fm1-build`, que depende de `gate`.

## 2. Por qué se traba

La variable no existe en el repositorio, y **el token disponible no puede crearla**:

| Petición | Resultado |
|---|---|
| `GET /repos/Guillel51/Quantum-GammaWave/actions/variables` | `403 Resource not accessible by personal access token` |
| `POST /repos/.../actions/variables` | `403 Resource not accessible by personal access token` |
| `PUT /repos/.../actions/variables/QG_RELEASE_APPROVED` | `404` (no existe; sin permiso de escritura) |

No es un problema del workflow ni del comando: al PAT fine-grained le falta *Repository permissions →
Variables: Read and write*. Como consecuencia, todos los tags desde v3.1.0 se publicaron a mano —paquete,
manifiesto, `SHA256SUMS`, release y sitio verificados— pero sin attestation de CI. Medido con la API de
attestations: el paquete de v2.7.5 (`772f0dae…476f2`) tiene 2 bundles in-toto; el de v3.1.1 (`03a6cd25…5d68`)
devuelve `404`.

## 3. Cómo destrabarlo sin tocar el requisito

El requisito es **una aprobación explícita y registrada antes de construir un tag 3.x**. Nada de lo que
sigue lo rebaja: cambia *dónde* se registra esa aprobación, no *si* existe.

### Opción A — la variable, una vez, desde la interfaz (coste: 20 segundos, cero código)

Settings → Secrets and variables → Actions → *Variables* → **New repository variable**:
`QG_RELEASE_APPROVED` = `true`.

Es la vía mínima: el workflow queda exactamente como está, el gate sigue leyendo la misma variable y la
aprobación queda firmada por quien la creó, con su fecha. Si además se quiere que el permiso no dependa de
una persona, hay que darle al token *Variables: Read and write* y entonces el propio agente puede hacerlo.

### Opción B — un Environment con revisores (aprobación por ejecución, no permanente)

Es la que recomiendo si la intención es que "aprobado" valga **una vez por release** y no para siempre:

```yaml
gate:
  runs-on: ubuntu-24.04
  environment: release          # el environment tiene requerido un revisor humano
  steps:
    - uses: actions/checkout@v4
    - name: a 3.x release needs an explicit approval
      env:
        QG_RELEASE_APPROVED: ${{ vars.QG_RELEASE_APPROVED }}
      run: |
        set -euo pipefail
        V=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c | head -1)
        python3 tools/release_gate.py "$V" "${QG_RELEASE_APPROVED:-false}"
```

Con Settings → Environments → `release` → *Required reviewers*, el job se detiene hasta que un revisor
aprueba esa ejecución concreta, y GitHub deja el registro de quién aprobó y cuándo — mejor evidencia que
una variable booleana que no caduca. La variable `QG_RELEASE_APPROVED` pasa a ser una variable **del
environment** (mismo nombre, mismo valor) y se escribe desde la misma pantalla que los revisores.

Sin `environment:` en el job, esta opción no hace nada; con él, y sin revisores configurados, tampoco. Las
dos mitades se configuran en el repositorio, no en el YAML.

### Opción C — `workflow_dispatch` con confirmación escrita

Añadir un disparo manual con un input obligatorio (`approve: "v3.1.4"`) que el gate compare contra la
versión. Sirve si se quiere publicar sin empujar un tag, pero **no destraba nada hoy**: disparar un workflow
también pide permiso de *Actions: write*, que es el mismo bloque que sufrimos. Queda como idea, no como
solución.

## 4. Lo que no hay que hacer

- No convertir el gate en un `if` que se saltee cuando la variable falta: eso es exactamente el requisito
  que se quiere conservar.
- No mover la attestation a un job que no dependa de `gate`: attestaría un paquete que nadie aprobó.
- No firmar ni atestar a mano desde esta sesión: la attestation de GitHub vale porque la emite el runner con
  `id-token: write` sobre el workflow público; una copia local no prueba procedencia.

## 5. Qué queda pendiente, con nombre y apellido

Una de las dos mitades de la opción A o de la opción B, hecha por quien tiene la cuenta. Después de eso el
camino es automático: poner la variable, re-ejecutar el run del tag, y verificar con
`GET /repos/Guillel51/Quantum-GammaWave/attestations/sha256:<digest>` que devuelve al menos un bundle para
el paquete de ese tag, y que `gh attestation verify` lo firma el workflow de `zvenson/fm1-build`.
