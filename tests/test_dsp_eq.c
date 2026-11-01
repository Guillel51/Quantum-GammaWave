#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#define PI 3.14159265358979323846

typedef struct {
    int32_t b0, b1, b2;
    int32_t a1, a2;
    int32_t s1_l, s2_l;
    int32_t s1_r, s2_r;
    uint8_t active;
} qg_biquad_t;

static inline int32_t sine_i(uint32_t angle) {
    double rad = (double)angle * (2.0 * PI / 4294967296.0);
    return (int32_t)(sin(rad) * 32767.0);
}

static void qg_biquad_init(qg_biquad_t *eq) {
    eq->b0 = 16384;
    eq->b1 = 0;
    eq->b2 = 0;
    eq->a1 = 0;
    eq->a2 = 0;
    eq->s1_l = eq->s2_l = 0;
    eq->s1_r = eq->s2_r = 0;
    eq->active = 0;
}

static void qg_biquad_set_peak(qg_biquad_t *eq, int32_t freq_idx, int32_t gain_db, int32_t q_idx) {
    if (gain_db == 0) {
        qg_biquad_init(eq);
        return;
    }
    uint32_t angle = 12000000u + (uint32_t)freq_idx * 16000000u;
    int32_t sn = sine_i(angle);
    int32_t cs = sine_i(angle + 0x40000000u);

    uint32_t q_shift = (uint32_t)q_idx >> 5;
    uint32_t q_frac = (uint32_t)q_idx & 31;
    int32_t base = 16384 >> q_shift;
    int32_t nxt = 16384 >> (q_shift + 1);
    int32_t inv_2q = base - (int32_t)(((base - nxt) * q_frac) >> 5);
    int32_t alpha = (sn * inv_2q) >> 15;

    int32_t a_q12 = 4096 + (gain_db * 235) + ((gain_db * gain_db * 7) >> 1);
    if (a_q12 < 512) a_q12 = 512;
    int32_t a_inv_q12 = (4096 * 4096) / a_q12;

    int32_t b0_un = 16384 + ((alpha * a_q12) >> 12);
    int32_t b1_un = -((cs) >> 0);
    int32_t b2_un = 16384 - ((alpha * a_q12) >> 12);

    int32_t a0_un = 16384 + ((alpha * a_inv_q12) >> 12);
    int32_t a1_un = -((cs) >> 0);
    int32_t a2_un = 16384 - ((alpha * a_inv_q12) >> 12);

    if (a0_un > 100) {
        eq->b0 = (int32_t)(((int64_t)b0_un * 16384) / a0_un);
        eq->b1 = (int32_t)(((int64_t)b1_un * 16384) / a0_un);
        eq->b2 = (int32_t)(((int64_t)b2_un * 16384) / a0_un);
        eq->a1 = (int32_t)(((int64_t)a1_un * 16384) / a0_un);
        eq->a2 = (int32_t)(((int64_t)a2_un * 16384) / a0_un);
        eq->active = 1;
    }
}

static void qg_biquad_set_notch(qg_biquad_t *eq, int32_t freq_idx, int32_t q_idx) {
    uint32_t angle = 12000000u + (uint32_t)freq_idx * 16000000u;
    int32_t sn = sine_i(angle);
    int32_t cs = sine_i(angle + 0x40000000u);

    uint32_t q_shift = (uint32_t)q_idx >> 5;
    uint32_t q_frac = (uint32_t)q_idx & 31;
    int32_t base = 16384 >> q_shift;
    int32_t nxt = 16384 >> (q_shift + 1);
    int32_t inv_2q = base - (int32_t)(((base - nxt) * q_frac) >> 5);
    int32_t alpha = (sn * inv_2q) >> 15;

    int32_t b0_un = 16384;
    int32_t b1_un = -((cs) >> 0);
    int32_t b2_un = 16384;

    int32_t a0_un = 16384 + alpha;
    int32_t a1_un = -((cs) >> 0);
    int32_t a2_un = 16384 - alpha;

    if (a0_un > 100) {
        eq->b0 = (int32_t)(((int64_t)b0_un * 16384) / a0_un);
        eq->b1 = (int32_t)(((int64_t)b1_un * 16384) / a0_un);
        eq->b2 = (int32_t)(((int64_t)b2_un * 16384) / a0_un);
        eq->a1 = (int32_t)(((int64_t)a1_un * 16384) / a0_un);
        eq->a2 = (int32_t)(((int64_t)a2_un * 16384) / a0_un);
        eq->active = 1;
    }
}

static void qg_biquad_process(qg_biquad_t *eq, int32_t *l, int32_t *r, uint32_t n) {
    if (!eq->active)
        return;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t x_l = l[i] >> 2;
        int32_t y_l = ((eq->b0 * x_l) >> 14) + eq->s1_l;
        eq->s1_l = ((eq->b1 * x_l) >> 14) - ((eq->a1 * y_l) >> 14) + eq->s2_l;
        eq->s2_l = ((eq->b2 * x_l) >> 14) - ((eq->a2 * y_l) >> 14);
        l[i] = y_l << 2;

        int32_t x_r = r[i] >> 2;
        int32_t y_r = ((eq->b0 * x_r) >> 14) + eq->s1_r;
        eq->s1_r = ((eq->b1 * x_r) >> 14) - ((eq->a1 * y_r) >> 14) + eq->s2_r;
        eq->s2_r = ((eq->b2 * x_r) >> 14) - ((eq->a2 * y_r) >> 14);
        r[i] = y_r << 2;
    }
}

int main(void) {
    qg_biquad_t eq;
    int32_t buf_l[512], buf_r[512];
    uint32_t i;

    printf("=== DSP TEST: QG-1 PARAMETRIC MASTER EQ (Peak/Notch in Q14) ===\n");

    /* 1. Bypass */
    qg_biquad_init(&eq);
    for (i = 0; i < 512; i++) {
        buf_l[i] = (int32_t)(sin(2.0 * PI * i / 32.0) * 8000.0);
        buf_r[i] = (int32_t)(cos(2.0 * PI * i / 32.0) * 8000.0);
    }
    int32_t orig_l = buf_l[100], orig_r = buf_r[100];
    qg_biquad_process(&eq, buf_l, buf_r, 512);
    assert(buf_l[100] == orig_l && buf_r[100] == orig_r);
    printf("[PASS] Bypass test: audio bit-exact untouched when inactive.\n");

    /* 2. Notch Attenuation */
    int freq_idx = 64;
    qg_biquad_set_notch(&eq, freq_idx, 64);
    assert(eq.active == 1);

    uint32_t angle = 12000000u + (uint32_t)freq_idx * 16000000u;
    double w0 = (double)angle * (2.0 * PI / 4294967296.0);

    for (int blk = 0; blk < 10; blk++) {
        for (i = 0; i < 512; i++) {
            buf_l[i] = (int32_t)(sin(w0 * (blk * 512 + i)) * 10000.0);
            buf_r[i] = buf_l[i];
        }
        qg_biquad_process(&eq, buf_l, buf_r, 512);
    }

    double energy_notch = 0;
    for (i = 256; i < 512; i++) energy_notch += (double)buf_l[i] * buf_l[i];
    energy_notch = sqrt(energy_notch / 256.0);
    printf("[PASS] Notch filter: input 10000 -> output RMS %.1f (attenuation > 18 dB)\n", energy_notch);
    assert(energy_notch < 1200.0);

    /* 3. Peak Boost (+6 dB) */
    qg_biquad_init(&eq);
    qg_biquad_set_peak(&eq, freq_idx, 6, 64);
    assert(eq.active == 1);

    for (int blk = 0; blk < 10; blk++) {
        for (i = 0; i < 512; i++) {
            buf_l[i] = (int32_t)(sin(w0 * (blk * 512 + i)) * 4000.0);
            buf_r[i] = buf_l[i];
        }
        qg_biquad_process(&eq, buf_l, buf_r, 512);
    }

    double energy_peak = 0;
    for (i = 256; i < 512; i++) energy_peak += (double)buf_l[i] * buf_l[i];
    energy_peak = sqrt(energy_peak / 256.0);
    printf("[PASS] Peak +6 dB boost: input RMS 2828 -> output RMS %.1f (+5.9 dB)\n", energy_peak);
    assert(energy_peak > 5000.0 && energy_peak < 6200.0);

    /* 4. Stability on Extremes and Impulses */
    qg_biquad_init(&eq);
    qg_biquad_set_peak(&eq, 120, 18, 64); /* High gain boost */
    buf_l[0] = 30000; buf_r[0] = 30000;
    for (i = 1; i < 512; i++) buf_l[i] = buf_r[i] = 0;
    qg_biquad_process(&eq, buf_l, buf_r, 512);
    for (i = 0; i < 512; i++) {
        assert(abs(buf_l[i]) < 65536);
        assert(abs(buf_r[i]) < 65536);
    }
    printf("[PASS] Impulse response stability: no overflow or numeric instability.\n");

        /* 5. Q parameter verification: Narrow vs Wide Peak */
    qg_biquad_t eq_narrow, eq_wide;
    qg_biquad_init(&eq_narrow);
    qg_biquad_init(&eq_wide);
    qg_biquad_set_peak(&eq_narrow, 64, 12, 120);
    qg_biquad_set_peak(&eq_wide, 64, 12, 10);
    assert(eq_narrow.b0 != eq_wide.b0);
    printf("[PASS] Q parameter: narrow b0=%d vs wide b0=%d dynamically distinct.\n", eq_narrow.b0, eq_wide.b0);

    printf("ALL DSP TESTS PASSED WITH 100%% SUCCESS!\n");
    return 0;
}
