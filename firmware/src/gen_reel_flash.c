/* SPDX-License-Identifier: GPL-3.0-only
 * (1.0-GW TAKES v2) the TAKES reel and its SET kept over power-off: OBJ_REEL (0xDA000 / 0xDB000, storage.c), packed
 * (gen_reel.c reel_pack). Written as the autosave is (project.c): only when it changed and has stayed so for 10 s with
 * the transport stopped and nothing sounding (an erase stops the audio for its ~40 ms), at most once a minute; read
 * at power-on. Main loop only. Included by felucca.c after project.c */
#define RS_IDLE_MS 10000u
#define RS_GAP_MS 60000u
static struct { uint32_t saved, seen, t, last, poll; uint8_t wrote, err; } reel_fs __attribute__((section(".pool")));
static void reel_save_poll(void)
{
    uint32_t sig, n;
    if (!flash_ok || fm1_ms - reel_fs.poll < 250u)
        return;
    reel_fs.poll = fm1_ms;
    sig = reel_sig();
    if (sig != reel_fs.seen || !autosave_quiet()) {
        reel_fs.seen = sig;
        reel_fs.t = fm1_ms;
        return;
    }
    if (sig == reel_fs.saved || fm1_ms - reel_fs.t < RS_IDLE_MS || (reel_fs.wrote && fm1_ms - reel_fs.last < RS_GAP_MS))
        return;
    reel_fs.last = fm1_ms;
    reel_fs.wrote = 1;
    n = reel_pack(st_buf, ST_PAYLOAD_MAX);              /* (st_save copies from st_buf onto itself: the driver's RAM) */
    reel_fs.err = st_save(OBJ_REEL, st_buf, n) != 0;
    if (!reel_fs.err)
        reel_fs.saved = sig;
}
static void reel_boot(void)
{
    int n;
    if (flash_ok && (n = st_load(OBJ_REEL, st_buf, ST_PAYLOAD_MAX)) > 0)
        (void)reel_unpack(st_buf, (uint32_t)n);
    reel_fs.saved = reel_fs.seen = reel_sig();
    reel_fs.t = fm1_ms;
}
