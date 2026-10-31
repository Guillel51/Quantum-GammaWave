# Aplicar el cambio de workflow a mano (permiso `workflows`)

El cambio de `.github/workflows/emulator.yml` de las etapas 2 y 3 **no está en ningún commit** de esta
rama: el token con el que se sube el trabajo es una GitHub App **sin permiso `workflows`**, y GitHub
rechaza el push entero con

```
! [remote rejected] cline/13ey37k1 -> cline/13ey37k1 (refusing to allow a GitHub App to create or
  update workflow `.github/workflows/emulator.yml` without `workflows` permission)
```

El cambio vive en tres sitios: el árbol de trabajo de esta rama, `docs/ci-stage2.patch` (80 líneas,
verificado con `git apply --check --reverse`) y las tres secciones de abajo, para hacerlo a mano.

## Opción A — el parche (recomendada)

```sh
git checkout cline/13ey37k1
git apply docs/ci-stage2.patch          # sin --check: aplica y deja el archivo modificado
git diff --stat .github/workflows/emulator.yml    # 1 file changed, ~+30/-5
git add .github/workflows/emulator.yml
git commit -m "ci: the DSP budget and the .fwsc reader run on pull requests; toolchain pin; Pages preflight"
git push
```

Si ya tienes el árbol modificado (como en esta sesión), el parche no aplica: comprueba con
`git diff --stat .github/workflows/emulator.yml` que el cambio ya está y salta al commit.

## Opción B — a mano en GitHub (Edit file)

Tres cambios, todos en `.github/workflows/emulator.yml`.

**1. El disparo por rutas** (para que editar el presupuesto dispare el job que lo aplica):

```yaml
on:
  push:
    paths: ["firmware/**", "web/**", "tools/**", "tests/target_budget.txt", "tests/target_budget.py", "build.sh",
            ".github/workflows/emulator.yml"]
  pull_request:
    paths: ["firmware/**", "web/**", "tools/**", "tests/target_budget.txt", "tests/target_budget.py", "build.sh",
            ".github/workflows/emulator.yml"]
  workflow_dispatch:
```

**2. La toolchain, también en PRs, con el hash fijable** (quita los dos `if: github.ref == ...` y añade
`env:`):

```yaml
      - name: JieLi toolchain and SDK files (cached)
        uses: actions/cache@v4
        with:
          path: |
            ~/.jieli
            ~/fw-AC79_AIoT_SDK/cpu/wl82/tools
          key: jieli-ac79-v1
      - name: JieLi toolchain and SDK files
        id: tc
        continue-on-error: true
        env:
          JIELI_TOOLCHAIN_SHA256: ${{ vars.JIELI_TOOLCHAIN_SHA256 }}
        run: |
          ... (el cuerpo existente no cambia) ...
```

**3. Un solo paso con las dos comprobaciones que necesitan el compilador del objetivo**, justo después
del paso "the package, the installer and the editor" y antes de `upload-pages-artifact`:

```yaml
      - name: target compiler checks (DSP budget of the render loops, the .fwsc reader on the package)
        if: steps.tc.outcome == 'success'
        env:
          AC79_SDK: /home/runner/fw-AC79_AIoT_SDK
          JIELI_TOOLCHAIN: /home/runner/.jieli/toolchain
        run: |
          if [ ! -s build/felucca.dis ] || [ ! -s build/felucca.fwsc ]; then python3 tools/build.py; fi
          python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt
          node web/test_pkg_bounds.mjs build/felucca.fwsc
```

Y en el job `deploy`, antes de `actions/deploy-pages@v4`, el preflight de Pages:

```yaml
      - name: "Pages is enabled (Settings > Pages > Build and deployment > Source: GitHub Actions)"
        env:
          GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
        run: |
          if ! gh api "repos/${{ github.repository }}/pages" > /tmp/pages.json 2>/dev/null; then
            echo "::error::GitHub Pages is not enabled for ${{ github.repository }}: deploy-pages would fail with HTTP 404. Enable it once, Settings > Pages > Build and deployment > Source: GitHub Actions (no workflow change is needed)."
            exit 1
          fi
          echo "Pages: $(tr -d '\n' < /tmp/pages.json)"
```

## Los dos ajustes que solo puede hacer el dueño del repositorio

Comprobado desde aquí, con el token de la sesión:

* `GET /repos/Guillel51/Quantum-GammaWave` → `"has_pages": false` (y `admin: false`), y
  `GET .../pages` → **404**: Pages no está habilitado. `POST .../pages` no está permitido con este token.
* `GET .../actions/variables` → **403** `Resource not accessible by integration`: tampoco se puede crear
  la variable.

1. **Pages**: <https://github.com/Guillel51/Quantum-GammaWave/settings/pages> → *Build and deployment* →
   *Source*: **GitHub Actions**. No hace falta tocar el workflow para esto.
2. **Variable** (fija la toolchain): <https://github.com/Guillel51/Quantum-GammaWave/settings/variables/actions>
   → *New repository variable* → nombre `JIELI_TOOLCHAIN_SHA256`, valor
   `a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd`
   (el archivo que sirve hoy `pkgman.jieliapp.com/s/linux-toolchain`, `jieli-linux-toolchains-20260730.1`).
   Con la variable puesta, `tools/get_toolchain.sh` **rechaza** cualquier otro archivo antes de
   descomprimir; sin ella imprime el hash que vio y sigue.

## Cómo comprobar que quedó bien

```sh
gh api repos/Guillel51/Quantum-GammaWave/pages | head -5        # 200 y "build_type": "workflow"
gh run list --workflow=emulator --limit 3                       # el job "deploy" pasa el preflight
gh run view --log --job=build | grep 'target: DRUM path'        # la comprobación corrió
```

En un PR, el job `build` del workflow del emulador debe mostrar el paso
"target compiler checks (DSP budget of the render loops, the .fwsc reader on the package)" y fallar si
un bucle de render crece más del 10 % o si el lector del paquete empeora.
