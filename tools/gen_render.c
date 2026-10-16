/* SPDX-License-Identifier: GPL-3.0-only
 * The GEN screen's pieces heard: firmware/src/gen.c on the host build of the tests, the real engines playing each piece
 * at its tempo (its song, when FORM made parts), written as WAV files, to judge a genre by ear. Each argument is
 * GENRE:SEED[:ENERGY[:FORM[:COMPLEX]]] (the genre's name with '_' for spaces, or its number 1..49; ENERGY and COMPLEX
 * 0..100, default 50; FORM 0 LOOP, 1 PARTS, 2 SONG, default 2); the files are DIR/<genre>_<seed>.wav, SECONDS each.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/gen_render tools/gen_render.c -lm
 *   build/gen_render DIR SECONDS TRAP_ATL:7 UK_DRILL:11:70:2 ... */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int genre_of(const char *s)
{
    char want[32];
    uint32_t i, g;
    if (s[0] >= '0' && s[0] <= '9') return atoi(s);
    for (i = 0; s[i] && i + 1u < sizeof want; i++) want[i] = s[i] == '_' ? ' ' : s[i];
    want[i] = 0;
    for (g = 0; g < NGENRE; g++)
        if (!strcmp(GENRE[g].name, want)) return (int)g + 1;
    return -1;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/gen_render";
    double secs = argc > 2 ? atof(argv[2]) : 30.0;
    int a;
    if (secs <= 0) secs = 30.0;
    for (a = 3; a < argc; a++) {
        char gname[32], path[600], name[40];
        unsigned seed = 1, energy = 50, form = 2, complex = 50;
        int g;
        uint32_t nsamp = 0, want, f = 0, i;
        FILE *wav;
        if (sscanf(argv[a], "%31[^:]:%u:%u:%u:%u", gname, &seed, &energy, &form, &complex) < 2 || (g = genre_of(gname)) < 1 ||
            g > (int)NGENRE) {
            fprintf(stderr, "GENRE:SEED[:ENERGY[:FORM[:COMPLEX]]], not %s\n", argv[a]);
            return 1;
        }
        ui_power_on();
        ui_rec_prefs |= PREF_SIX >> 8; rp_apply();      /* TRACKS 6 */
        song.master_q12 = 4096;
        dice_secs = 0xFF;                               /* (a fresh project: the parts may be written) */
        gp.genre = (uint8_t)g; gp.seed = (uint16_t)(seed % 10000u); gp.energy = (uint8_t)energy; gp.form = (uint8_t)(form % 3u);
        gp.complex = (uint8_t)complex;
        gen_go();
        str_cpy(name, GENRE[g - 1].name, sizeof name);
        for (i = 0; name[i]; i++) if (name[i] == ' ' || name[i] == '&') name[i] = '_';
        snprintf(path, sizeof path, "%s/%s_%04u.wav", dir, name, seed % 10000u);
        if (!(wav = fopen(path, "wb"))) { fprintf(stderr, "cannot write %s\n", path); return 1; }
        wav_hdr(wav, 0);
        if (live.mode) chain_play_ui();                 /* (its parts in ARRANGE: PLAY plays the song) */
        else transport_req = 1;
        want = (uint32_t)(secs * FS);
        while (nsamp < want) {                          /* a frame (1/30 s) of the main loop, then its audio */
            uint32_t to = (uint32_t)((uint64_t)(++f) * FS / 30u);
            ui_input();
            song_poll();
            while (nsamp < to && nsamp < want) {
                int32_t o[2 * CTL];
                int16_t s[2 * CTL];
                mix_block(o, CTL);
                for (i = 0; i < 2u * CTL; i++) s[i] = (int16_t)clamp(o[i], -32768, 32767);
                fwrite(s, 2, 2u * CTL, wav);
                nsamp += CTL;
            }
            host_ticks += 1000000u / 30u; fm1_ms += 1000u / 30u;
        }
        transport_req = 2; ui_input();
        fclose(wav);
        if ((wav = fopen(path, "r+b"))) { wav_hdr(wav, nsamp); fclose(wav); }
        printf("%s  %s, %d BPM\n", path, gp.last, song.g[G_BPM]);
    }
    return 0;
}
