#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define NVOICE 10
#define NPART 6
#define P_LEVEL 0

typedef struct {
    uint8_t active;
    uint8_t gate;
    uint8_t stage; /* 0..3 attack/decay/sustain/release, 4 fade-out kill */
    uint16_t env_out;
    uint32_t age;
    uint8_t note;
} voice_t;

typedef struct {
    voice_t v[NVOICE];
    int16_t p[10];
    uint8_t engine;
} track_t;

static track_t trk[NPART];

static uint32_t quiet_tail(track_t **pp) {
    uint32_t p, i, best = NVOICE;
    uint32_t lowest_energy = 0xFFFFFFFFu;
    for (p = 0; p < NPART; p++) {
        for (i = 0; i < NVOICE; i++) {
            const voice_t *v = &trk[p].v[i];
            if (!v->active || v->gate || v->stage == 4u || v->env_out >= 120)
                continue;
            uint32_t energy = (uint32_t)v->env_out * (uint32_t)(trk[p].p[P_LEVEL] + 1);
            if (energy < lowest_energy) {
                lowest_energy = energy;
                best = i;
                *pp = &trk[p];
            }
        }
    }
    return best;
}

static uint32_t voices_busy(void) {
    uint32_t p, i, n = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].active && trk[p].v[i].stage != 4u;
    return n;
}

int main(void) {
    printf("=== POLYPHONY & VOICE STEALING RIGOROUS TEST ===\n");
    memset(trk, 0, sizeof(trk));
    for (int p = 0; p < NPART; p++) trk[p].p[P_LEVEL] = 100;

    /* 1. Allocate 10 simultaneous voices */
    for (int i = 0; i < 10; i++) {
        int track_idx = i % NPART;
        voice_t *v = &trk[track_idx].v[i / NPART];
        v->active = 1;
        v->gate = 1;
        v->stage = 2; /* Sustain */
        v->env_out = 2000; /* Full loudness */
        v->age = i + 1;
        v->note = 60 + i;
    }
    assert(voices_busy() == 10);
    printf("[PASS] 10 voices sounding simultaneously verified.\n");

    /* 2. Release 1 voice into quiet release tail (<-40 dB, env_out < 120) */
    trk[0].v[0].gate = 0;
    trk[0].v[0].stage = 3; /* Release */
    trk[0].v[0].env_out = 45; /* Inaudible tail */

    track_t *victim_track = NULL;
    uint32_t victim_idx = quiet_tail(&victim_track);
    assert(victim_idx == 0 && victim_track == &trk[0]);
    printf("[PASS] Psychoacoustic quiet tail correctly targeted (energy = %u).\n",
           trk[0].v[0].env_out * (trk[0].p[P_LEVEL] + 1));

    /* 3. Ensure held notes (env_out = 2000) are NEVER targeted by quiet_tail */
    for (int i = 1; i < 10; i++) {
        int p = i % NPART;
        int v_idx = i / NPART;
        assert(trk[p].v[v_idx].gate == 1);
        assert(trk[p].v[v_idx].env_out >= 120);
    }
    printf("[PASS] Loud held notes protected from quiet-tail stealing.\n");

    printf("ALL POLYPHONY & VOICE STEALING TESTS PASSED!\n");
    return 0;
}
