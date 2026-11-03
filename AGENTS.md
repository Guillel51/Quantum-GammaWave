# AGENTS.md — Instructions & Architecture Directives for Quantum GammaWave (QG-1)

Welcome to **QUANTUM GAMMAWAVE (QG-1)**. This repository houses the custom open-source firmware, browser-based WebAssembly emulator, and Web Studio workstation for the **M-VAVE FM-1 multitrack FM synthesizer & groovebox**.

---

## 1. Project & Hardware Identity
- **Product Name**: QUANTUM GAMMAWAVE (QG-1).
- **Target Hardware**: M-VAVE FM-1 (JieLi AC791N / WL82, dual-core `pi32v2`, 32-bit little-endian, simple-precision FPU; 578 KB SRAM according to the SDK, with the firmware using the `0x01C00000–0x01C7FFFF` window and a 96 KiB `.data+.bss` budget; 1 MiB SPI NOR with XIP at `0x02000000`; documented P25Q80H JEDEC `0x856014`, while the newer `0xB36014` variant is unvalidated and currently not accepted by the firmware; 240×240 ST7789-class RGB565 LCD; 14 buttons; 7 rotary encoders plus the MASTER analog pot; 27 micro-switches in 2 rows as keys). The clock is not fixed by this repository: 320 MHz and 360 MHz have been reported, and must be measured on the actual unit.
- **Not a guitar pedal**: The M-VAVE FM-1 is a multitrack groovebox / synthesizer desktop instrument.
- **Lineage**: Major evolution/fork of Pocket GammaWave (GW-1) by Leo Kuroshita / Hügelton Instruments, built on JIANT FM and Felucca.
- **Licensing**: GPL-3.0-only. All original copyright notices, credits and licenses in `LICENSES/` must strictly remain intact.

---

## 2. Safety & Operational Constraints
- **NO PHYSICAL FLASHING**: You must never flash real hardware directly. Never trigger blind firmware updates or vendor flash commands that could permanently brick an FM-1.
- **NO RELEASES PUBLISHING**: Do not publish GitHub Releases.
- **ZERO TELEMETRY**: No hidden tracking, analytics, phone-home network calls, or proprietary spyware.
- **PRESERVE EXISTING USER DATA**: Project formats (`FUN1` through `FUNC`), custom presets, flash object structures (`OBJ_REEL`, `OBJ_BANK`, `OBJ_PROJ`), and SysEx backup routines must remain 100% backward-compatible.

---

## 3. Toolchain & Environment

### Python Requirements
- Python 3.10+ (tested on 3.12/3.13).
- Required Python modules for asset/header generation:
  ```bash
  pip install Pillow fonttools numpy
  ```

### Asset & Table Generation
Whenever firmware headers or tables are needed:
```bash
python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()"
```
This generates in `build/gen/`:
- `felucca_tables.h` (DSP sine/wavetable lookup tables)
- `felucca_fm6.h` (FM6 DX7 factory and default voices)
- `ui_fonts.h`, `ui_icons.h`, `ui_keycaps.h`, `ui_palettes.h`, `ui_shapes.h`

### Firmware Compilation (Target JieLi toolchain)
- Requires `JIELI_TOOLCHAIN` (pi32v2 clang toolchain) and `AC79_SDK` (or Docker with `JIELI_DOCKER=1`).
- Building the `.fwsc` firmware package:
  ```bash
  ./build.sh
  ```

### Browser Emulator (WebAssembly)
- Requires `emcc` (Emscripten v3.1.50+ or v6.0.11):
  ```bash
  sh web/emu/build.sh
  ```
- Local dev test server:
  ```bash
  python3 tools/emu_dev.py
  ```

---

## 4. Test Suites & Verification

Always run the available test suites before committing:
```bash
# 1. Regenerate tables/headers
python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()"

# 2. Host and installer tests
python3 tests/install_test.py

# 3. Web Studio & protocol tests
node web/test_web.mjs
node web/test_backup.mjs
node web/test_pkg_bounds.mjs
```

Note: Running `./tests/run_tests.sh` runs extensive native C test runners and ASan checks. If running without a built `.fwsc` package, use:
```bash
NO_PACKAGE=1 ./tests/run_tests.sh
```

---

## 5. DSP & Firmware Guidelines
- **Dual-Core Safety**: Core 0 renders master buses, UI, display and tracks 1–3; Core 1 renders tracks 4–6. Keep inter-core mailbox structures strictly synchronized and non-blocking.
- **Memory Footprint**: The SDK reports 578 KB SRAM, but this firmware's linker budget is 96 KiB for `.data+.bss`; the full SRAM window is not available to the application. Headroom must remain $\ge 8\text{ KiB}$ in `POOL_END - pool_top`.
- **Render Budget**: Target DSP render cost per block (32 samples @ 44.1 kHz $\approx 725\ \mu\text{s}$) must never exceed static budgets checked by `tests/target_budget.py`.
- **Fixed-Point Math**: Prefer Q12/Q15/Q31 arithmetic over floating-point on target hardware where feasible. Avoid unvetted runtime memory allocations (`malloc` is forbidden in realtime audio paths).
