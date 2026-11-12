#!/usr/bin/env python3
# qe_spread.py — Quantum GammaWave v2.7.5 — Herramienta de expansión sonora
import sys, os

MOTORS = {
    'ANALOG': {
        'name': 'ANALOG',
        'features': ['DRIVE', 'FILTERS', 'MORPH', 'SYNC', 'RING', 'SAW3', 'TUBE', 'FOLD'],
        'params': ['P_E6 (DRIVE)', 'P_E7 (INT)', 'P_FTYPE (filtro)'],
    },
    'FM6': {
        'name': 'FM6',
        'features': ['RATIO', 'MLVL', 'FB', 'LFO', 'VIB', 'ECUT', 'INS', 'ARB'],
        'params': ['P_E0 (ALG)', 'P_E1 (FB)', 'P_E2 (MLVL)', 'P_E3 (RATIO)', 'P_E4 (MEG)', 'P_E5 (PMOD)', 'P_E6 (DTUN)', 'P_E7 (SLOT)'],
    },
    'DIGITAL': {
        'name': 'DIGITAL',
        'features': ['WHEEL', 'NOISE', 'PHASE'],
        'params': ['P_WHEEL', 'P_NOISE', 'P_PHASE'],
    },
    'LOFI': {
        'name': 'LOFI',
        'features': ['BIT', 'FLT', 'TX'],
        'params': ['P_BIT', 'P_TX', 'P_FLT'],
    },
    'VOICE': {
        'name': 'VOICE',
        'features': ['FORM (8 formantes)', 'VOC (vocoder)', 'RES (resonancia)'],
        'params': ['P_F1-P_F8', 'P_VOC', 'P_RES'],
    },
    'WHEEL': {
        'name': 'WHEEL',
        'features': ['WAVE (8 ondas)', 'HASH', 'LOOP (16 pasos)'],
        'params': ['P_WAVE', 'P_HASH', 'P_LOOP'],
    },
    'NOISE': {
        'name': 'NOISE',
        'features': ['NZZ', 'ZEN', 'ZENMOD'],
        'params': ['P_NZZ', 'P_ZEN', 'P_ZENMOD'],
    },
    'DRUM-X': {
        'name': 'DRUM-X',
        'features': ['DRUM (16 slots)', 'FX'],
        'params': ['P_DRUM', 'P_FX'],
    },
    'PHASE': {
        'name': 'PHASE',
        'features': ['PHX', 'XO (crossover)', 'SPX (espectro)'],
        'params': ['P_PHX', 'P_XO', 'P_SPX'],
    },
}

def main():
    if len(sys.argv) < 2:
        print('QE_spread.py — Herramienta de expansión sonora v2.7.5')
        print('Uso: qe_spread.py <list|struct|template> [motor]')
        return
    cmd = sys.argv[1].lower()
    if cmd == 'list':
        for mid, m in MOTORS.items():
            print(f'  [{mid}] {m["name"]}: {len(m["features"])} funciones, {len(m["params"])} params')
    elif cmd == 'struct':
        if len(sys.argv) < 3:
            print('Uso: qe_spread.py struct <motor>')
            return
        mid = sys.argv[2].upper()
        if mid not in MOTORS:
            print(f'No existe {mid}')
            return
        m = MOTORS[mid]
        print(f'[{mid}] {m["name"]}')
        print(f'  Funciones ({len(m["features"])}):')
        for f in m['features']:
            print(f'    - {f}')
        print(f'  Parametros ({len(m["params"])}):')
        for p in m['params']:
            print(f'    - {p}')

if __name__ == '__main__':
    main()
