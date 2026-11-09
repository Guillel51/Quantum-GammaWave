#!/usr/bin/env python3
"""
Generate real firmware UI screenshots and composite assets for Quantum GammaWave (QG-1).
"""
import os
import re
import subprocess
from PIL import Image


def firmware_version():
    """FELUCCA_VERSION from firmware/src/felucca.c: the screenshots state the version the firmware carries, not
    a copy typed into this script (it said v1.8.6 while the tree shipped v1.9.0)."""
    src = open("firmware/src/felucca.c", encoding="utf-8").read()
    m = re.search(r'#define\s+FELUCCA_VERSION\s+"([^"]+)"', src)
    if not m:
        raise SystemExit("generate_screenshots: FELUCCA_VERSION not found in firmware/src/felucca.c")
    return m.group(1)

def shrink(im):
    """The same pixels in fewer bytes: a palette image when the render has at most 256 colours and the
    round trip is byte-identical, the RGB image otherwise. Returns (image, note)."""
    cols = im.getcolors(maxcolors=257)
    if cols is None or len(cols) > 256:
        return im, ""
    try:
        pal = im.quantize(colors=len(cols))
    except Exception:
        return im, ""
    if pal.convert("RGB").tobytes() != im.tobytes():
        return im, ""
    return pal, ", palette"


def generate_screens():
    os.makedirs("assets/screenshots", exist_ok=True)
    os.makedirs("assets/ui-art/art", exist_ok=True)

    with open("tests/ui_render.c", "r") as f:
        src = f.read()

    src = src.replace("int main(int argc, char **argv)", "int ui_render_main(int argc, char **argv)")

    custom_main = """
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    state();

    /* Base palette: QUANTUM (13) */
    pal(UI_QUANTUM_INDEX);

    /* 01. HOME screen */
    setup(S_HOME);
    pal(UI_QUANTUM_INDEX);
    draw(S_HOME);
    write_ppm("assets/screenshots", "quantum", "01_home");

    /* 02. EDIT Track (ANALOG) with Wraith ribbon */
    setup(S_EDIT_ANALOG);
    pal(UI_QUANTUM_INDEX);
    draw(S_EDIT_ANALOG);
    write_ppm("assets/screenshots", "quantum", "02_edit_analog");

    /* 03. FM6 Operator Algorithms */
    setup(S_FM6_ALG1);
    pal(UI_QUANTUM_INDEX);
    draw(S_FM6_ALG1);
    write_ppm("assets/screenshots", "quantum", "03_fm6_algorithm");

    /* 04. Phrases / Track Map */
    setup(S_PHRASES);
    pal(UI_QUANTUM_INDEX);
    draw(S_PHRASES);
    write_ppm("assets/screenshots", "quantum", "04_track_map");

    /* 05. 16-step Sequencer */
    setup(S_STEP);
    pal(UI_QUANTUM_INDEX);
    draw(S_STEP);
    write_ppm("assets/screenshots", "quantum", "05_sequencer");

    /* 06. Dynamic Piano Roll (playing) */
    setup(S_ROLL_PLAYING);
    pal(UI_QUANTUM_INDEX);
    draw(S_ROLL_PLAYING);
    write_ppm("assets/screenshots", "quantum", "06_seq_roll");

    /* 07. DRUM-X synthesized pads matrix */
    setup(S_DRUM);
    pal(UI_QUANTUM_INDEX);
    draw(S_DRUM);
    write_ppm("assets/screenshots", "quantum", "07_drum_x");

    /* 08. DRUM-X Morph & Pad Hit */
    setup(S_DRUMX_HIT);
    pal(UI_QUANTUM_INDEX);
    draw(S_DRUMX_HIT);
    write_ppm("assets/screenshots", "quantum", "08_drum_morph");

    /* 09. Hearth Macros Layer */
    setup(S_MACRO_LAYER);
    pal(UI_QUANTUM_INDEX);
    draw(S_MACRO_LAYER);
    write_ppm("assets/screenshots", "quantum", "09_macro_layer");

    /* 10. Motion Sequencer (MSEQ) */
    setup(S_MSEQ);
    pal(UI_QUANTUM_INDEX);
    draw(S_MSEQ);
    write_ppm("assets/screenshots", "quantum", "10_motion_seq");

    /* 11. Playable Punch-In FX Performance Grid */
    setup(S_FX_HELD);
    pal(UI_QUANTUM_INDEX);
    perf_latched = PF_BIT(PF_LPF) | PF_BIT(PF_R16) | PF_BIT(PF_GATE);
    perf_held = 0;
    draw(S_FX_HELD);
    write_ppm("assets/screenshots", "quantum", "11_punch_in_fx");

    /* 12. FX send rack (the first FX page), with a real track peak for the meters */
    setup(S_FX);
    TSEL->peak = 12000;
    pal(UI_QUANTUM_INDEX);
    draw(S_FX);
    write_ppm("assets/screenshots", "quantum", "12_fx_rack");

    /* 13. 6-Track Master Mixer */
    setup(S_MIXER);
    pal(UI_QUANTUM_INDEX);
    draw(S_MIXER);
    write_ppm("assets/screenshots", "quantum", "13_mixer");

    /* 14. Song Arranger / Chains */
    setup(S_SONG);
    pal(UI_QUANTUM_INDEX);
    draw(S_SONG);
    write_ppm("assets/screenshots", "quantum", "14_song_chains");

    /* 15. Scale and Chord Harmonic Selector */
    setup(S_SCL);
    pal(UI_QUANTUM_INDEX);
    draw(S_SCL);
    write_ppm("assets/screenshots", "quantum", "15_scales_chords");

    /* 16. Dynamic Modern ARP Visualization */
    setup(S_ARP);
    pal(UI_QUANTUM_INDEX);
    draw(S_ARP);
    write_ppm("assets/screenshots", "quantum", "16_modern_arp");

    /* 17. Motion Parameter Automation & Locks */
    setup(S_MOTION_LOCKS);
    pal(UI_QUANTUM_INDEX);
    draw(S_MOTION_LOCKS);
    write_ppm("assets/screenshots", "quantum", "17_motion_locks");

    /* 18. Full-Screen Audio Visualizer (Oscilloscope & Lissajous), actually running.
     * The song is playing, the six VU meters have signal and ui.frame sits mid-animation, so the capture shows the
     * visualizer working -- spectrum bars up, phosphor trace, motes spread over their orbits -- instead of parked at
     * its idle frame with every meter empty. */
    setup(S_HOME);
    pal(UI_QUANTUM_INDEX);
    ui.home = 1;
    ui.home_viz = 1;
    song.playing = 1;
    trk_live = NTRK;
    {
        static const int32_t pk[6] = {21000, 9000, 15500, 27000, 7400, 12800};
        uint32_t k;
        for (k = 0; k < NTRK; k++)
            trk[k].peak = pk[k];
    }
    ui.frame = 173;
    ui_draw();
    write_ppm("assets/screenshots", "quantum", "18_fullscreen_visualizer");
    ui.home_viz = 0;

    /* 19. Project Manager & Flash Storage */
    setup(S_PROJECT);
    pal(UI_QUANTUM_INDEX);
    draw(S_PROJECT);
    write_ppm("assets/screenshots", "quantum", "19_project_slots");

    /* 20. Harmonic Performance Mode */
    setup(S_FX_HARM);
    pal(UI_QUANTUM_INDEX);
    draw(S_FX_HARM);
    write_ppm("assets/screenshots", "quantum", "20_harmonic_performance");

    /* 21. Parametric Master EQ Biquad Page */
    setup(S_MASTER);
    pal(UI_QUANTUM_INDEX);
    song.g[G_EQMODE] = 1;
    song.g[G_EQFREQ] = 80;
    song.g[G_EQGAIN] = 15;
    song.g[G_EQQ] = 64;
    ui.home = 0;
    for (uint32_t i = 0; i < NPAGES; i++) {
        if (!strcmp(PAGES[i].title, "MASTER EQ")) { ui.page = (uint8_t)i; break; }
    }
    ui_draw();
    write_ppm("assets/screenshots", "quantum", "21_master_eq");

    /* 25. MASTER bus: clip curve, punch, duck, output VU */
    setup(S_MASTER);
    pal(UI_QUANTUM_INDEX);
    draw(S_MASTER);
    write_ppm("assets/screenshots", "quantum", "25_master_bus");

    /* 22. Transient 1.5s Categorized Preset Overlay */
    setup(S_HOME);
    pal(UI_QUANTUM_INDEX);
    ui.preset_pop_ms = fm1_ms;
    ui_draw();
    write_ppm("assets/screenshots", "quantum", "22_transient_presets");
    ui.preset_pop_ms = 0;

    /* 23. GLO 2 System Telemetry & Hardware Monitor */
    setup(S_GLOBAL);
    pal(UI_QUANTUM_INDEX);
    ui.home = 0;
    for (uint32_t i = 0; i < NPAGES; i++) {
        if (!strcmp(PAGES[i].title, "GLOBAL 2")) { ui.page = (uint8_t)i; break; }
    }
    ui_draw();
    write_ppm("assets/screenshots", "quantum", "23_glo2_telemetry");

    /* 24. ABOUT page */
    setup(S_ABOUT);
    pal(UI_QUANTUM_INDEX);
    draw(S_ABOUT);
    write_ppm("assets/screenshots", "quantum", "24_about_credits");

    /* Themes */
    setup(S_HOME); pal(UI_SOLARIS_INDEX); draw(S_HOME);
    write_ppm("assets/screenshots", "solaris", "theme_solaris_home");

    setup(S_HOME); pal(UI_CYBER_INDEX); draw(S_HOME);
    write_ppm("assets/screenshots", "cyberpunk", "theme_cyberpunk_home");

    setup(S_HOME); pal(UI_NORDIC_INDEX); draw(S_HOME);
    write_ppm("assets/screenshots", "nordic", "theme_nordic_home");

    setup(S_HOME); pal(UI_GAMMA_INDEX); draw(S_HOME);
    write_ppm("assets/screenshots", "gamma", "theme_gamma_home");

    /* 3.2: the screens the pass changed, captured from the same drawing code the firmware links */
    setup(S_SPLASH); pal(UI_QUANTUM_INDEX); draw(S_SPLASH);
    write_ppm("assets/screenshots", "quantum", "26_boot_splash");

    setup(S_DLY); pal(UI_QUANTUM_INDEX); draw(S_DLY);
    write_ppm("assets/screenshots", "quantum", "27_delay_bars");

    setup(S_REVERB_RESO); pal(UI_QUANTUM_INDEX); draw(S_REVERB_RESO);
    write_ppm("assets/screenshots", "quantum", "28_reverb_reso");

    setup(S_REVERB_CLOUD); pal(UI_QUANTUM_INDEX); draw(S_REVERB_CLOUD);
    write_ppm("assets/screenshots", "quantum", "29_reverb_cloud");

    setup(S_FX_HELD); pal(UI_QUANTUM_INDEX); draw(S_FX_HELD);
    write_ppm("assets/screenshots", "quantum", "30_punch_in_latch");

    return 0;
}
"""
    with open("/tmp/gen_screens.c", "w") as f:
        f.write(src + "\n" + custom_main)

    os.makedirs("assets/screenshots/ppm", exist_ok=True)
    version = firmware_version()
    print(f"screens: FELUCCA_VERSION {version}")
    subprocess.run(["gcc", "-O1", "-w", f'-DFELUCCA_VERSION="{version}"',
                    "-Ibuild/gen", "-Ifirmware/src", "-Itests",
                    "-o", "/tmp/gen_screens", "/tmp/gen_screens.c", "-lm"], check=True)
    subprocess.run(["/tmp/gen_screens"], check=True)

    # Convert all PPM to PNG, as small as the pixels allow. A palette PNG is only used when the render really has
    # 256 colours or fewer AND decoding it back is byte-identical to the source, so "smaller without losing quality"
    # is checked rather than assumed; optimize=True applies either way.
    for fname in sorted(os.listdir("assets/screenshots/ppm")):
        if fname.endswith(".ppm"):
            base = fname[:-4]
            ppm_path = os.path.join("assets/screenshots/ppm", fname)
            png_path = os.path.join("assets/screenshots", base + ".png")
            im = Image.open(ppm_path).convert("RGB")
            out, note = shrink(im)
            out.save(png_path, optimize=True)
            os.remove(ppm_path)
            print(f"Generated {png_path} ({os.path.getsize(png_path)} B{note})")
    os.rmdir("assets/screenshots/ppm")

    # Synchronize assets/ui-art/art/ with actual rendered screens
    art_map = [
        ("01_home.png", "quantum_01_home.png"),
        ("02_track.png", "quantum_02_edit_analog.png"),
        ("03_fm_algorithm.png", "quantum_03_fm6_algorithm.png"),
        ("04_track_map.png", "quantum_04_track_map.png"),
        ("05_sequencer.png", "quantum_05_sequencer.png"),
        ("06_seq_performance.png", "quantum_06_seq_roll.png"),
        ("07_drum_x.png", "quantum_07_drum_x.png"),
        ("08_drum_morph.png", "quantum_08_drum_morph.png"),
        ("09_macro.png", "quantum_09_macro_layer.png"),
        ("10_macro_map.png", "quantum_10_motion_seq.png"),
        ("11_punch_in_fx.png", "quantum_11_punch_in_fx.png"),
        ("12_fx_rack.png", "quantum_12_fx_rack.png"),
        ("13_mixer.png", "quantum_13_mixer.png"),
        ("14_song.png", "quantum_14_song_chains.png"),
        ("15_chord.png", "quantum_15_scales_chords.png"),
        ("16_arp.png", "quantum_16_modern_arp.png"),
        ("17_automation.png", "quantum_17_motion_locks.png"),
        ("18_visualizer.png", "quantum_18_fullscreen_visualizer.png"),
        ("19_save_project.png", "quantum_19_project_slots.png"),
        ("20_performance.png", "quantum_20_harmonic_performance.png"),
    ]
    for dst, src in art_map:
        src_path = os.path.join("assets/screenshots", src)
        dst_path = os.path.join("assets/ui-art/art", dst)
        if os.path.exists(src_path):
            im = Image.open(src_path).convert("RGB")
            out, _ = shrink(im)
            out.save(dst_path, optimize=True)
            print(f"Updated {dst_path} from {src}")

if __name__ == "__main__":
    generate_screens()
