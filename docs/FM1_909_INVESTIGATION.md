# FM-1_909 — Investigación de identidad desconocida

## Resumen

El dispositivo físico responde `FM-1_909` al handshake SysEx 0x11. Esta identidad
**no está documentada** en este repositorio y **no es compatible** con el firmware
Quantum GammaWave v2.4.0 (que es `FM-1_965`). La instalación está **bloqueada**.

## Evidencia revisada

### Documentación oficial del repositorio

| Documento | Contenido | FM-1_909 aparecen? |
|-----------|-----------|-------------------|
| `docs/IDENTITY.md` | FM-1_982 (legacy, hasta 2.1.1), FM-1_965 (nueva desde 2.1.2) | NO |
| `docs/JEDEC_COMPATIBILITY.md` | Solo dos JEDEC: 0x856014 (P25Q80H) y 0xB36014 (UC25HQ80) | NO |
| `web/fm1ota.js` | `LEGACY_IDS = ["FM-1_982"]`, `isOurLoader` con 965 y 900 | NO |
| `web/fm1verify.js` | Comprobaciones de identidad | NO |
| `tools/fm1_gate.py` | Puerta de instalación | NO |
| `tools/fm1_install.py` | Instalación | NO |
| `tests/install_test.py` | Tests de instalación | NO |
| `tests/ota_test.c` | Tests de protocolo | NO |

FM-1_909 no aparece en ninguna de las fuentes de verdad del proyecto.

### Manifiesto firmado v2.4.0

- `product`: "FM-1"
- `identity`: "FM-1_965"
- `build.identity`: "FM-1_965"
- `min_firmware`: "1.0.0"

El paquete v2.4.0 declara explícitamente ser FM-1_965, no FM-1_909.

## Análisis de compatibilidad

### Lo que SÍ sabemos

1. El dispositivo responde `FM-1_909` al handshake SysEx 0x11.
2. El firmware v2.4.0 espera `FM-1_965` (o `FM-1_982` como legacy).
3. El instalador verifica la identidad del dispositivo antes de escribir.
4. Un dispositivo con identidad no reconocida es rechazado por el instalador.

### Lo que NO sabemos

1. ¿Qué representa realmente FM-1_909?
   - ¿Firmware alternativo no QG-1?
   - ¿Variante de hardware no documentada?
   - ¿Modo boot/recovery diferente?
   - ¿Respuesta de un estado no estándar del dispositivo?

2. ¿Existe relación entre FM-1_909 y FM-1_965?
   - No hay evidencia verificable de que sean la misma familia de firmware.

3. ¿Qué JEDEC tiene el chip flash del dispositivo?
   - No se ha verificado con 0x12.
   - Los únicos JEDEC documentados como compatibles son 0x856014 y 0xB36014.

4. ¿Qué loader tiene el dispositivo?
   - No se ha verificado con 0x12.
   - Los loaders reconocidos son ota-FM-1_965 y ota-FM-1_900.

## Conclusión

**A. ¿Qué significa FM-1_909?**
No se puede determinar con la evidencia disponible. Es una identidad no documentada
que no corresponde a ninguna variante conocida de Quantum GammaWave.

**B. ¿Qué evidencia respalda esa conclusión?**
- FM-1_909 no aparece en IDENTITY.md, JEDEC_COMPATIBILITY.md, ni en ningún archivo
  del repositorio que defina identidades soportadas.
- El firmware v2.4.0 declara ser FM-1_965.
- No hay pruebas, dumps, ni documentación técnica que relacione 909 con 965 o 982.

**C. ¿Es compatible con el firmware FM-1_965?**
NO DEMOSTRADO. Sin evidencia de compatibilidad, no se puede asumir compatibilidad.
Instalar un firmware sobre un dispositivo con identidad desconocida es un riesgo
no aceptable.

**D. ¿Qué prueba adicional hace falta?**
Para determinar si es seguro proceder, se necesitaría:
1. Verificar el loader del dispositivo con 0x12 (¿ota-FM-1_965 o distinto?).
2. Verificar el JEDEC del chip flash (0x856014, 0xB36014, o algo distinto).
3. Confirmar si FM-1_909 está documentado en el firmware hub externo.
4. Determinar si es un modo de operación no estándar (boot, recovery, etc.).
5. Si es un firmware alternativo, identificar qué firmware es y si es compatible.

**E. ¿Puede autorizarse la instalación?**
**NO — INSTALACIÓN BLOQUEADA**

Razones:
1. FM-1_909 no está documentado en IDENTITY.md.
2. No está en LEGACY_IDS del instalador.
3. No se ha verificado el loader ni el chip.
4. No hay evidencia de compatibilidad con el paquete v2.4.0.
5. Instalar en un dispositivo con identidad desconocida es un riesgo no aceptable.

## Acciones requeridas antes de desbloquear

1. **Verificar identidad real**: Determinar qué representa FM-1_909 mediante las
   pruebas listadas arriba.
2. **Documentar**: Si 909 es una variante soportada, documentarla en IDENTITY.md
   con la evidencia de compatibilidad. Si no es soportada, documentar por qué no.
3. **No modificar controles**: No agregar FM-1_909 a LEGACY_IDS ni modificar
   STOCK_DONOR_VERSIONS sin la evidencia requerida.
4. **No instalar**: Mantener la instalación bloqueada hasta resolver el bloqueo.

## Riesgo de instalar sin verificación

Instalar QG-1 v2.4.0 sobre un dispositivo que responde FM-1_909 sin entender qué
es ese dispositivo conlleva riesgos:
- Podría ser un firmware alternativo que no es compatible con QG-1.
- Podría ser hardware con diferente mapa de memoria o requisitos no soportados.
- Podría ser un modo de operación donde la instalación causa daño.
- No hay rollback garantizado si el dispositivo no es compatible.

Ninguno de estos riesgos está mitigado con la evidencia actual.
