/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT (0.6.91) the song generator's knowledge, as constant tables (gen.c reads them; included by it). Everything a
 * genre knows is written here, nothing is computed: a genre is a row, its drums a map of priorities (after Mutable
 * Instruments' Grids: a level a step a lane, a hit where the level clears the density), its bass one of ~50 archetypes
 * written against the kick (funk, post punk, jazz, reggae, house, techno, hip hop ..), its chords a progression with a
 * quality a chord, its arp and lead a way of speaking, its sounds a recipe a role, its kit a kit, its space a reverb and
 * a delay. The research behind the tables: reports/ and research_notes/ (practitioner conventions, Grids, Marbles, the
 * FiloBass corpus, Witek on syncopation, Attack's beat dissections). */

/* ---------------------------------------------------------------------------------------------------- roles --- */
enum { GP_DRUMS, GP_BASS, GP_CHORDS, GP_ARP, GP_LEAD, GP_N };   /* the parts the GEN screen weighs */
#define GA_AUTO 255u                                    /* an amount left to the genre */

/* ------------------------------------------------------------------------------------------------ drum maps --- */
/* a lane's 16 steps as levels: '.' 0, '1'..'9' 26 a step (9 = 234), 'X' 255. A step hits when its level > 255 - density;
 * '9' and 'X' are anchors (accented, never dropped by a variation); '1'..'3' ghosts. Lanes: BD SD CP CH OH TM RS CB */
enum { DM_HOUSE, DM_DEEP, DM_MICRO, DM_TECHHOUSE, DM_TECHNO, DM_DETROIT, DM_DUBTECH, DM_HYPNO, DM_HARD, DM_ELECTRO,
       DM_EBM, DM_ITALO, DM_SYNTHPOP, DM_DARKWAVE, DM_POSTPUNK, DM_TRIBAL, DM_RBOX, DM_KLING, DM_MOTORIK, DM_INDIE,
       DM_SHOEGAZE, DM_FUNK, DM_DISCO, DM_BOOMBAP, DM_TRAP, DM_UKG, DM_DUBSTEP, DM_BREAKS, DM_DNB, DM_ONEDROP,
       DM_STEPPERS, DM_ROCKERS, DM_JAZZ, DM_BOSSA, DM_AFRO, DM_SPARSE, DM_IDM, DM_JOYDIV, DM_NEWORDER,
       DM_TRAP_ATL, DM_UK_DRILL, DM_PHONK, DM_RAGE, DM_NONE };
static const char *const DRUM_MAP[][8] = {
    [DM_HOUSE]     = {"X...8...X...8...", "", "....9.......9...", "7.3.7.3.7.3.7.3.", "..8...8...8...8.", "", ".......2......3.", ""},
    [DM_DEEP]      = {"X...8...X...8..2", "", "....7.......7...", ".37..37..37..37.", "..8...8...8...8.", "", "...2......4.....", ""},
    [DM_MICRO]     = {"X...8...X...8...", "", "", "..6...5.6.5...6.", "", "", "....6.......6...", ".2..2....3...2.."},
    [DM_TECHHOUSE] = {"X...8...X...8...", "", "....9.......9...", "7373737373737373", "..8...8...8...8.", "", "...5..5....5..5.", ""},
    [DM_TECHNO]    = {"X...9...X...9...", "", "....6.......6...", "..8...8...8...8.", "", "", "......5.....5..2", ""},
    [DM_DETROIT]   = {"X...9...X...9...", "", "....7.......7...", "6686668666866686", "..8...8...8...8.", "", "", ""},
    [DM_DUBTECH]   = {"X...8...X...8...", "", "", "..5...5...5...5.", "", "", "...3......6.....", ""},
    [DM_HYPNO]     = {"X...9...X...9...", "", "", "5555555555555555", "", "..6..6....6..6..", ".4....4.4....4..", ""},
    [DM_HARD]      = {"X...X...X...X...", "", "....7.......7...", "8484848484848484", "..7...7...7...7.", "", "..3..3....3..3..", "......3.......3."},
    [DM_ELECTRO]   = {"X.....8...8..7..", "....X.......X...", "....5.......5...", "7474747474747474", "", "", "", "6..6..6...6.6..."},
    [DM_EBM]       = {"X.6.X.6.X.6.X.6.", "....X.......X...", "....4.......4...", "..7...7...7...7.", "", "", "", ""},
    [DM_ITALO]     = {"X...8...X...8...", "....9.......9...", "", "7.7.7.7.7.7.7.7.", "..7...7...7...7.", "..............44", "", ""},
    [DM_SYNTHPOP]  = {"X.......X.6.....", "....X.......X...", "....4.......4...", "7.6.7.6.7.6.7.6.", "", "", "", ""},
    [DM_DARKWAVE]  = {"X...X...X...X...", "....X.......X..3", "....6.......6...", "..8...8...8...8.", "", "", "", ""},
    [DM_POSTPUNK]  = {"X...8...X...8...", "....X.......X...", "", "7.8.7.8.7.8.7.8.", "", "", ".....3.......3..", ""},
    [DM_TRIBAL]    = {"X.......X.......", "....X.......X...", "", "...3...3...3...3", "", "7..7..7.7..7..7.", "", ""},
    [DM_RBOX]      = {"X.......8.8.....", "....8.......8...", "", "7474747474747474", "", "", "", ""},
    [DM_KLING]     = {"X.....8.X.......", "....8.......8...", "", "6.6.6.6.6.6.6.6.", "", "", "..........5...5.", "5..5..5...5.5..."},
    [DM_MOTORIK]   = {"X.6.6...8.6.6...", "....8.......8...", "", "7.5.7.5.7.5.7.5.", "..............3.", "", "", ""},
    [DM_INDIE]     = {"X.....7.8.......", "....X.......X..3", "", "7.5.7.5.7.5.7.5.", "..............4.", "", "", ""},
    [DM_SHOEGAZE]  = {"X.....6.8.......", "....8.......8...", "", "5.4.5.4.5.4.5.4.", "6...............", "", "", ""},
    [DM_FUNK]      = {"X..6..6.8.6.....", "...3X.3..3.3X.3.", "", "7474757474757474", "..............4.", "", "", ""},
    [DM_DISCO]     = {"X...9...X...9...", "....9.......9...", "....6.......6...", "8585858585858585", "..8...8...8...8.", "", "", "2.2.2.2.2.2.2.2."},
    [DM_BOOMBAP]   = {"X......6..8.8...", "....X....3..X...", "....3.......3...", "7.5.7.5.7.5.7.5.", "..............3.", "", "", ""},
    [DM_TRAP]      = {"X......8..8.....", "........X.......", "........6.......", "7.7.7.7.7.7.7.7.", "......4.........", "", "", ""},
    [DM_UKG]       = {"X......6..9.....", "....9.......9..3", "", "..7...7...7...7.", "......4.......5.", "", ".3..3.3...3..3..", ""},
    [DM_DUBSTEP]   = {"X.........8.....", "........X.......", "........4.......", "6.6.6.6.6.6.6.6.", "", "", "", ""},
    [DM_BREAKS]    = {"X.6.......8.....", "....X..3.3..X..3", "", "7.6.7.6.7.6.7.6.", "......5.......5.", "", "", ""},
    [DM_DNB]       = {"X.........8.....", "....X.......X..3", "", "6.5.6.5.6.5.6.5.", "", "", ".......3........", ""},
    [DM_ONEDROP]   = {"........X.......", "", "", "6.4.6.4.6.4.6.4.", "..............4.", "", "........X.......", ""},
    [DM_STEPPERS]  = {"X...X...X...X...", "", "", "..6...6...6...6.", "", "", "........8.......", ""},
    [DM_ROCKERS]   = {"X.......X.......", "....X.......X...", "", "6.4.6.4.6.4.6.4.", "", "", "", ""},
    [DM_JAZZ]      = {"3...3...3...3...", "...2...2.2...2..", "", "....6.......6...", "7...7..77...7..7", "", "", ""},
    [DM_BOSSA]     = {"X..6X..6X..6X..6", "", "", "5.5.5.5.5.5.5.5.", "", "", "6..6..6...6..6..", ""},
    [DM_AFRO]      = {"X6......X6..4...", "....X...3...X.3.", "", "6.567.567.567.56", "......4.........", "......5.......5.", "", ".5..5.5..5..5.5."},
    [DM_SPARSE]    = {"7...............", "", "", "", "", "", "......3.....4...", "..........3....."},
    [DM_IDM]       = {"", "", "", "", "", "", "", ""},   /* (procedural: euclids against each other) */
    /* (0.6.97) post punk after Joy Division (She's Lost Control: the kick syncopated against a flat sixteenth hat, the
     * snare on 2 and 4, a tom at the bar's end) and New Order (Blue Monday, Ceremony: four on the floor with the
     * snare and a clap, driven sixteenths, open hats off the beat, toms into the one) */
    [DM_JOYDIV]    = {"X..X..X.X..X..X.", "....X.......X...", "", "7777777777777777", "", "..........4...5.", "", ""},
    [DM_NEWORDER]  = {"X...X...X...X...", "....X.......X...", "....5.......5...", "7676767676767676", "..6...6...6...6.", "..............44", "", ""},
    /* Trap & Drill variants: Atlanta 808 bounce, UK Drill 3-3-2 tresillo hats & displaced snare, Dark Phonk cowbell, Rage */
    [DM_TRAP_ATL]  = {"X.....7...9...6.", "........X....4..", "........9.......", "8684868886848888", "..7.......6.....", "", "...5........5...", ""},
    [DM_UK_DRILL]  = {"X.....6...9..7..", "........X..7....", "........8.......", "9..8..9.8..8..9.", "....6.........6.", "", "..5..5...5..5...", ""},
    [DM_PHONK]     = {"X...6..8X.....8.", "....8.......X...", "........X.......", "8585858885858888", "..6...6...6...6.", "", "", "9..7..9.7.8.7..."},
    [DM_RAGE]      = {"X..7..8...9...7.", "........X.......", "........X....5..", "8.8.888.8.8.8888", "....8.......8...", "..............67", "", ""},
    [DM_NONE]      = {"", "", "", "", "", "", "", ""},
};
/* (0.6.97) a throw's own hats and percussion: a hat pattern, an open hat pattern and a percussion line from these pools
 * now and then instead of the map's, so a genre's throws differ in their top end */
static const char *const HAT_POOL[8] = {"7.7.7.7.7.7.7.7.", "7373737373737373", "..8...8...8...8.", "7.7.7.7.7.7.77.7",
                                        "7.37.37.37.37.3.", "8.5.8.5.8.5.8.55", "7..7..7.7..7..7.", "6666666666666666"};
static const char *const OH_POOL[6] = {"", "..7...7...7...7.", "......7.......7.", "..............7.", "..7.......7.....", "...7...7...7...7"};
static const char *const RS_POOL[6] = {"", "...5......5.....", ".4....4.4....4..", "......5.....5...", "..5...5...5...5.", "...3..3....3..3."};
static const char *const CB_POOL[4] = {"", "5..5..5...5.5...", "......4.......4.", "2.2.2.2.2.2.2.2."};
static const char *const TM_POOL[4] = {"", "..............44", "......5.......5.", "..6..6....6..6.."};
/* the snare's ghost positions, the best first (the sixteenth before 2 and 4, then the "a"s, the "e"s, the "&"s) */
static const uint8_t GHOST_RANK[12] = {3, 7, 11, 15, 13, 5, 9, 1, 14, 6, 10, 2};
/* the hats' accent cells (percent of the hit's velocity on the four sixteenths of a beat) */
enum { HC_FLAT, HC_HOUSE, HC_FUNK, HC_GARAGE, HC_MACHINE };
static const uint8_t HAT_CELL[5][4] = {{100, 100, 100, 100}, {100, 55, 80, 55}, {90, 50, 70, 65}, {110, 40, 60, 90}, {100, 70, 100, 70}};
/* fills on the phrase's last bar */
enum { FL_NONE, FL_ROLL, FL_TOM, FL_DROP, FL_HAT, FL_LINEAR, FL_KICK16, FL_DUB, FL_BREAK };
/* the kits: offsets on the factory kit's {PITCH PMOD DECAY NOISE COLOR} a lane (both patches), then the DRUM track's
 * TONE DECY NOIS DRV */
enum { KIT_DEF, KIT_909, KIT_808, KIT_CLICK, KIT_INDUS, KIT_SOFT, KIT_DUB, KIT_BREAKS, KIT_N };
typedef struct { int8_t d[8][5]; int8_t tone, decy, nois, drv; } gkit_t;
static const gkit_t KIT[KIT_N] = {
    [KIT_DEF]    = {{{0}}, 0, 0, 0, 0},
    [KIT_909]    = {{{-2, 25, -6, 0, 25}, {0, 0, 6, 10, 10}, {0, 0, 4, 0, 0}, {4, 0, -4, 0, 6}, {4, 0, 0, 0, 6}, {0, 10, 0, 0, 10}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}}, 6, 0, 0, 6},
    [KIT_808]    = {{{-4, -20, 22, -4, -10}, {-4, -10, -8, 10, -20}, {-2, 0, 0, 0, -10}, {-6, 0, -6, 0, -20}, {-6, 0, -4, 0, -20}, {-4, -10, 10, 0, -10}, {0, 0, 0, 0, -10}, {0, 0, 4, 0, 0}}, -10, 6, 0, 0},
    [KIT_CLICK]  = {{{2, 10, -16, 0, 0}, {6, 0, -20, 20, 10}, {4, 0, -10, 0, 10}, {6, 0, -10, 0, 10}, {6, 0, -16, 0, 10}, {4, 0, -16, 0, 0}, {6, 0, -6, 0, 10}, {4, 0, -10, 0, 10}}, 10, -16, -6, 0},
    [KIT_INDUS]  = {{{-2, 30, 6, 12, 40}, {0, 20, 10, 20, 30}, {0, 0, 6, 10, 20}, {0, 0, 8, 10, 20}, {0, 0, 10, 10, 20}, {0, 20, 8, 10, 30}, {0, 10, 4, 10, 20}, {0, 0, 0, 10, 20}}, 12, 6, 10, 40},
    [KIT_SOFT]   = {{{0, -10, 6, 6, -25}, {-2, 0, 8, 14, -18}, {-2, 0, 6, 0, -20}, {-4, 0, 2, 0, -30}, {-4, 0, 6, 0, -30}, {-2, -10, 6, 6, -20}, {-2, 0, 4, 0, -20}, {-2, 0, 4, 0, -20}}, -20, 4, 4, 0},
    [KIT_DUB]    = {{{-6, -10, 18, 0, -20}, {-6, 0, 12, 8, -10}, {-2, 0, 6, 0, -10}, {-2, 0, 0, 0, -20}, {-2, 0, 6, 0, -20}, {-4, 0, 10, 0, -10}, {-4, 0, 10, 0, 0}, {0, 0, 6, 0, -10}}, -16, 8, 0, 0},
    [KIT_BREAKS] = {{{2, 6, -4, 8, 10}, {2, 6, 2, 16, 20}, {0, 0, 2, 4, 10}, {0, 0, 4, 6, 0}, {0, 0, 8, 6, 0}, {2, 6, 0, 8, 10}, {0, 0, 0, 4, 10}, {0, 0, 0, 0, 0}}, 4, 0, 6, 10},
};

/* ----------------------------------------------------------------------------------------------- bass archetypes --- */
/* 16 steps a bar, a character a step: '.' rest, '=' the note before held on, 'R' root, 'r' root soft, 'x' a dead
 * note (the root, very soft and short), '2' '3' '5' '6' '7' '9' the chord's degrees (in the scale), 'L' the fifth
 * below, '8' the octave, 'A' an approach a semitone under the next chord's root (the fifth when the chord stays),
 * 'B' from a semitone above, 'N' the next chord's root early, 'S' a scale step towards the next root. The flags, a
 * character a step: 'a' accent, 's' slide into the next note, 'g' ghost (soft), '.' nothing. bars: 1, 2 or 4 */
enum { BA_OFFBEAT, BA_OFFOCT, BA_BOUNCE, BA_DEEPWALK, BA_ROLL16, BA_OCTBNC, BA_DETROIT, BA_EBM, BA_ITALO, BA_ITALO16,
       BA_SYNTHW, BA_PSY, BA_PUMP8, BA_UKG, BA_SPEEDG, BA_JAMERSON, BA_AROUND, BA_ONTHEONE, BA_SLAP, BA_GOODTIME,
       BA_WALK, BA_WALK2, BA_TWOFEEL, BA_BOOGIE, BA_COUNTRY, BA_SKA, BA_TUMBAO, BA_BOSSA, BA_ROCK8, BA_ROOT5,
       BA_PUNK16, BA_DUST, BA_MCCART, BA_HOOK, BA_MOTORIK, BA_ANGULAR, BA_INDIE, BA_ONEDROP, BA_ONEDROP2, BA_STEPPERS,
       BA_ROCKERS, BA_DUB, BA_BOOMBAP, BA_TRAP, BA_DNB, BA_JUNGLE, BA_WOBBLE, BA_PEDAL, BA_FIFTHS, BA_NEOARP,
       BA_SUICIDE, BA_DARK8, BA_SEQ16, BA_ACID, BA_NITZER1, BA_NITZER2, BA_NITZER3, BA_RADIO, BA_HOOK2, BA_BLUEMON,
       BA_CEREMONY, BA_ATL808, BA_UKDRILL, BA_RAGE808, BA_N };
typedef struct { const char *n, *f; uint8_t bars; } barch_t;
static const barch_t BASS_ARCH[BA_N] = {
    [BA_OFFBEAT]  = {"..R...R...R...R.", 0, 1},
    [BA_OFFOCT]   = {"..8...R...8...R.", 0, 1},
    [BA_BOUNCE]   = {"R.8.R.8.R.8.R.8A", "a...............", 1},
    [BA_DEEPWALK] = {"R..R..5..7.8..5R", "a.............s.", 1},
    [BA_ROLL16]   = {"RRRRRRRRRRRRRRRR", "..a...a...a...a.", 1},
    [BA_OCTBNC]   = {"R.8.R.8.R.8.R.85", 0, 1},
    [BA_DETROIT]  = {"R..R..5.7..8....R..R..5.6..5..3.", 0, 2},
    [BA_EBM]      = {"R.8RR.8RR.8RR8R8", "a...a...a...a...", 1},
    [BA_ITALO]    = {"R.8.R.8.R.8.R.8.", 0, 1},
    [BA_ITALO16]  = {"RR88RR88RR88RR88", 0, 1},
    [BA_SYNTHW]   = {"R585R585R585R585", 0, 1},
    [BA_PSY]      = {".RRR.RRR.RRR.RRR", 0, 1},
    [BA_PUMP8]    = {"R=R=R=R=R=R=R=R=", 0, 1},
    [BA_UKG]      = {"R..R..5...R..7.N", "..............s.", 1},
    [BA_SPEEDG]   = {"R.8...8.R.8...58", "..............s.", 1},
    [BA_JAMERSON] = {"R.xR.5.67.x8.5A.", "..g.......g.....", 1},
    [BA_AROUND]   = {"R..x3.R.x5.5..NR", "...g....g.....s.", 1},
    [BA_ONTHEONE] = {"R..R..R......R.x", "a..............g", 1},
    [BA_SLAP]     = {"Rx8xR.x8Rx7x8.xR", "ag.ga.g.ag.g..g.", 1},
    [BA_GOODTIME] = {"R...R.5.6..7.8..N...R.5.6.7.8..8", 0, 2},
    [BA_WALK]     = {"R===3===5===A===", 0, 1},
    [BA_WALK2]    = {"R===2===5===6=A=", 0, 1},
    [BA_TWOFEEL]  = {"R=======5=====A.", 0, 1},
    [BA_BOOGIE]   = {"R.3.5.6.7.6.5.3.R.3.5.6.7.6.5.A.", 0, 2},
    [BA_COUNTRY]  = {"R=======L=======R===L===S===A===", 0, 2},
    [BA_SKA]      = {"R.3.5.6.8.6.5.3.", 0, 1},
    [BA_TUMBAO]   = {"R......5====N====......5====N===", 0, 2},
    [BA_BOSSA]    = {"R=====L.R=====L.", 0, 1},
    [BA_ROCK8]    = {"R.R.R.R.R.R.R.Sx", "a.......a......g", 1},
    [BA_ROOT5]    = {"R.R.5.5.R.R.5.A.", 0, 1},
    [BA_PUNK16]   = {"RRRRRRRRRRRRRRRR", "a...a...a...a...", 1},
    [BA_DUST]     = {"R..R.R........78", 0, 1},
    [BA_MCCART]   = {"3=5=6=5=3=2=R=A=", 0, 1},
    [BA_HOOK]     = {"8.8.9.8.7.8.5.5.", 0, 1},
    [BA_MOTORIK]  = {"R.R.R.R.R.R.5.5.", 0, 1},
    [BA_ANGULAR]  = {"R..R..7..R..5..8", 0, 1},
    [BA_INDIE]    = {"R.R.R.R.R.R.R.R.R.R.R.R.R.R.R.R.R.R.R.R.R.R.R.R.R.R.R.3.5.6.8.A.", 0, 4},
    [BA_ONEDROP]  = {"..R.5..R..R.7.....R.5..R..8.5.3.", 0, 2},
    [BA_ONEDROP2] = {"R.....8.R....5..", 0, 1},
    [BA_STEPPERS] = {"R.xR.5x.R.xR.5.x", "..g...g...g....g", 1},
    [BA_ROCKERS]  = {"R.R...5.R.R.7...", 0, 1},
    [BA_DUB]      = {"R=====....5R===.", "..........s.....", 1},
    [BA_BOOMBAP]  = {"R==...R=5=.....A", 0, 1},
    [BA_TRAP]     = {"R=========R=====R=====5=7=====R=", "......................s.......s.", 2},
    [BA_DNB]      = {"R=====..R==...7=", "..............s.", 1},
    [BA_JUNGLE]   = {"R..R..R...R.R...", 0, 1},
    [BA_WOBBLE]   = {"R=========R===5=", "..............s.", 1},
    [BA_PEDAL]    = {"R===============================================================", 0, 4},
    [BA_FIFTHS]   = {"R=======L=======", 0, 1},
    [BA_NEOARP]   = {"R==5==8===5===R=", 0, 1},
    [BA_SUICIDE]  = {"R.R.5.R.R.R.7.5.", "a.......a.......", 1},
    [BA_DARK8]    = {"R.R.R.R.R.R.R.R.", "a...a...a...a...", 1},
    [BA_SEQ16]    = {"R858R858R858R858", 0, 1},
    [BA_ACID]     = {0, 0, 1},                          /* (procedural: the 303's rules) */
    /* (0.6.95) EBM after Nitzer Ebb: relentless sixteenths on the root, the octave and the minor third thrown in, a
     * rest where the body hits, accents on the beat (Join in the Chant, Murderous, Let Your Body Learn) */
    [BA_NITZER1]  = {"R.R8R.R8R.R3R8R8", "a...a...a...a...", 1},
    [BA_NITZER2]  = {"RR8RRR8RRR8RRR3RRR8RRR8RRR8RR2R8", "a...a...a...a...a...a...a...a...", 2},
    [BA_NITZER3]  = {"R.RR.R8.R.RR.R3.", "a.....a.a.....a.", 1},
    /* (0.6.95) post rock after Radiohead: long roots, a walk up to the next chord at the phrase's end */
    [BA_RADIO]    = {"R===============R=======5===7===", 0, 2},
    /* (0.6.97) post punk after Peter Hook: the bass sings high on the neck, eighths over the root, fifth, octave and
     * seventh (Transmission, Love Will Tear Us Apart), Blue Monday's sequenced sixteenth octaves, Ceremony's run */
    [BA_HOOK2]    = {"R.5.8.7.R.5.8.5.R.5.8.7.8.7.5.3.", "a.......a.......a.......a.......", 2},
    [BA_BLUEMON]  = {"R8R8R8R8R8R8R8R8R8R8R8R8R8R85858", "a...a...a...a...a...a...a...a...", 2},
    [BA_CEREMONY] = {"R.R.R.3.5.5.5.3.R.R.R.3.5.5.7.8.", "a.......a.......a.......a.......", 2},
    /* Trap & Drill 808 slide archetypes: Atlanta booming 808 with octave/5th slides, UK Drill high octave glides, Rage */
    [BA_ATL808]   = {"R=====..R===8=5=R=====..R=8=7=R=", "a...........s.s.a.........s.s...", 2},
    [BA_UKDRILL]  = {"R=========..8=5=R=======..8=9=8=", "a...........s.s.a.........s.s.s.", 2},
    [BA_RAGE808]  = {"R==R==8.R==5==2=R==R==8.R==8=52=", "a.....s.a.....s.a.....s.a....ss.", 2},
};
enum { KM_DOUBLE, KM_ANSWER, KM_SUSTAIN, KM_INTERLOCK };   /* the bass against the kick */

/* ------------------------------------------------------------------------------------------------- harmony --- */
/* a chord's quality: its notes as steps of the scale from its root (a stack), POWER in semitones */
enum { Q_TRI, Q_7, Q_9, Q_ADD9, Q_SUS2, Q_SUS4, Q_6, Q_QUART, Q_POWER, Q_11, Q_7NO5, Q_N };
static const int8_t QSTACK[Q_N][4] = {{0, 2, 4, 127}, {0, 2, 4, 6}, {0, 2, 6, 8}, {0, 2, 4, 8}, {0, 1, 4, 127}, {0, 3, 4, 127},
                                      {0, 2, 4, 5}, {0, 3, 6, 9}, {0, 127, 127, 127}, {0, 2, 6, 10}, {0, 2, 6, 127}};
#define CQ_DOM 0x80u                                    /* the quality's bit 7: the third raised (a dominant in minor) */
typedef struct { uint8_t n; int8_t deg[4]; uint8_t q[4]; } gprog_t;
enum { PR_ONE, PR_I_VII, PR_I_VI, PR_I_IV, PR_I_V, PR_AEOL, PR_CYCLE, PR_POP, PR_AXIS, PR_50S, PR_JAZZ, PR_ANDAL,
       PR_MINOR4, PR_I_II, PR_I_III, PR_PLAGAL, PR_IV_I, PR_DESC, PR_SUS, PR_NEOCL, PR_TRANCE, PR_DORIAN, PR_RADIO,
       PR_PYRAMID, PR_N };
static const gprog_t PROG[PR_N] = {
    [PR_ONE]    = {1, {0}, {Q_7}},
    [PR_I_VII]  = {2, {0, 6}, {Q_TRI, Q_TRI}},
    [PR_I_VI]   = {2, {0, 5}, {Q_7, Q_7}},
    [PR_I_IV]   = {2, {0, 3}, {Q_7, Q_7}},
    [PR_I_V]    = {2, {0, 4}, {Q_TRI, Q_TRI}},
    [PR_AEOL]   = {4, {0, 5, 2, 6}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI}},
    [PR_CYCLE]  = {4, {0, 3, 4, 0}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI}},
    [PR_POP]    = {4, {0, 4, 5, 3}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI}},
    [PR_AXIS]   = {4, {5, 3, 0, 4}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI}},
    [PR_50S]    = {4, {0, 5, 3, 4}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI}},
    [PR_JAZZ]   = {4, {1, 4, 0, 0}, {Q_7, Q_7, Q_9, Q_6}},
    [PR_ANDAL]  = {4, {0, 6, 5, 4}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI | CQ_DOM}},
    [PR_MINOR4] = {4, {0, 3, 6, 2}, {Q_7, Q_7, Q_TRI, Q_TRI}},
    [PR_I_II]   = {2, {0, 1}, {Q_9, Q_9}},
    [PR_I_III]  = {2, {0, 2}, {Q_7, Q_7}},
    [PR_PLAGAL] = {4, {0, 3, 0, 4}, {Q_7, Q_7, Q_7, Q_7NO5}},
    [PR_IV_I]   = {2, {3, 0}, {Q_7, Q_7}},
    [PR_DESC]   = {4, {0, 6, 5, 6}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI}},
    [PR_SUS]    = {2, {0, 4}, {Q_SUS4, Q_SUS2}},
    [PR_NEOCL]  = {4, {0, 4, 5, 3}, {Q_7, Q_7, Q_7, Q_ADD9}},
    [PR_TRANCE] = {4, {0, 5, 6, 3}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI}},
    [PR_DORIAN] = {2, {0, 3}, {Q_7, Q_7}},
    [PR_RADIO]  = {4, {0, 5, 6, 3}, {Q_ADD9, Q_7, Q_SUS2, Q_ADD9}},   /* (0.6.95) i add9, VI7, VII sus2, iv add9 */
    [PR_PYRAMID] = {4, {0, 2, 5, 4}, {Q_7, Q_TRI, Q_7, Q_SUS4}},
};
/* how the chords are voiced: PAD led to the nearest voicing in C3..G5 as written; PLAIN the same but triads whatever
 * the progression says; RICH sevenths and ninths added; SUS suspensions; QUART fourths; POWER root fifth octave;
 * PLANE the same shape moved in parallel (techno, impressionism) */
enum { VM_PAD, VM_PLAIN, VM_RICH, VM_SUS, VM_QUART, VM_POWER, VM_PLANE };
/* how the chord track plays: held, swelling, stabbed on offbeats, pulsed, dubbed (a stab or two a bar through the
 * delay), skanked (reggae's offbeat eighths), comped (jazz, syncopated), power (a push on the bar), a drone, sparse,
 * broken sixteenths (UKG), trance (gated sixteenths) */
enum { CS_HOLD, CS_SWELL, CS_STAB, CS_PULSE, CS_DUB, CS_SKANK, CS_COMP, CS_POWER, CS_DRONE, CS_SPARSE, CS_BROKEN,
       CS_TRANCE };
/* the arp: an order over the chord's notes (indices, +4 an octave up, -1 the end), its rate in steps, its gate */
enum { AP_UP, AP_UPDOWN, AP_DOWN, AP_ALBERTI, AP_GLASS, AP_PULSE, AP_SEQ, AP_WALK, AP_CYCLE3, AP_CYCLE5, AP_TRANCE,
       AP_BROKEN, AP_OCTAVES, AP_N };
typedef struct { int8_t ord[8]; uint8_t rate, gate; uint16_t mask; } garp_t;
static const garp_t ARP[AP_N] = {
    [AP_UP]      = {{0, 1, 2, 3, 4, 5, 6, 7}, 1, 50, 0xFFFF},
    [AP_UPDOWN]  = {{0, 1, 2, 3, 2, 1, -1}, 2, 60, 0xFFFF},
    [AP_DOWN]    = {{3, 2, 1, 0, 7, 6, 5, 4}, 1, 50, 0xFFFF},
    [AP_ALBERTI] = {{0, 3, 1, 3, -1}, 1, 70, 0xFFFF},
    [AP_GLASS]   = {{0, 1, 2, 0, 1, 2, 3, 2}, 1, 75, 0xFFFF},
    [AP_PULSE]   = {{0, 0, 0, 0, 0, 0, 0, 2}, 1, 40, 0xFFFF},
    [AP_SEQ]     = {{0, 3, 1, 3, -1}, 1, 45, 0xFFFF},
    [AP_WALK]    = {{0, 2, 1, 3, 2, 0, 3, 1}, 1, 55, 0xFFFF},
    [AP_CYCLE3]  = {{0, 2, 5, -1}, 1, 55, 0xFFFF},
    [AP_CYCLE5]  = {{0, 1, 2, 3, 6, -1}, 1, 55, 0xFFFF},
    [AP_TRANCE]  = {{0, 4, 1, 5, 2, 6, 3, 7}, 1, 35, 0xDB6D},   /* (a trance gate: x.xx.xx.xx.xx.xx) */
    [AP_BROKEN]  = {{0, 2, 1, 3, -1}, 2, 60, 0xB6DB},
    [AP_OCTAVES] = {{0, 4, 0, 4, 1, 5, 0, 4}, 1, 45, 0xFFFF},
};
/* the lead: a motif in phrases, an ostinato cell on an euclid, sparse long chord tones, Part's scale line, a machine's
 * figure, a two-bar hook (call and answer), none */
enum { LD_MOTIF, LD_OSTINATO, LD_SPARSE, LD_TINTI, LD_MECH, LD_HOOK, LD_NONE };

/* ------------------------------------------------------------------------------------------ sounds and mix --- */
#define EG_WH 7u                                        /* WHEEL (the others: song_dice.c) */
/* a recipe: three engines it may take (the first the likeliest), words of the factory presets that fit (any engine),
 * then what the role asks of the sound: ADSR ranges, the filter (type: LP HP BP, 255 the engine's own), its cutoff and
 * resonance, the envelope on it, a slow LFO on it, drive and chorus, glide, the voice mode */
typedef struct {
    uint8_t eng[3];
    const char *w[3];
    uint8_t atk[2], dec[2], sus[2], rel[2];
    uint8_t ftype, cut[2], res[2], envf[2], lfo[2], lfo_rate[2], dist[2], chor[2], glide, voice;
} grecipe_t;
enum { SR_SUB, SR_ACID, SR_REESE, SR_FMBASS, SR_ROCKBASS, SR_HOOK, SR_WOW, SR_PAD, SR_STRINGS, SR_CHOIR, SR_ORGAN,
       SR_EP, SR_STAB, SR_DUBCHORD, SR_PLUCK, SR_BELL, SR_LEAD, SR_VOXLEAD, SR_CHIP, SR_KEYS, SR_DRONE, SR_NOISE,
       SR_TEXTURE, SR_HOOVER, SR_WALL, SR_GTR, SR_SINEKEY, SR_EBMBASS, SR_808GLIDE, SR_N };
static const grecipe_t RECIPE[SR_N] = {
    [SR_SUB]      = {{EG_A, EG_A, EG_LO}, {"INGOT SUB", "SUB BASS", "TRI BASS"}, {0, 1}, {55, 75}, {90, 120}, {15, 35}, 0, {30, 48}, {0, 10}, {0, 10}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, 0, V_LEGATO},
    [SR_ACID]     = {{EG_A, EG_A, EG_PH}, {"ACID", "FERAL 303", "SQR BASS"}, {0, 2}, {38, 60}, {0, 30}, {20, 40}, 0, {40, 64}, {55, 90}, {40, 62}, {0, 0}, {0, 0}, {12, 36}, {0, 0}, 30, V_LEGATO},
    [SR_REESE]    = {{EG_A, EG_A, EG_PH}, {"WORM REESE", "REESE", "GRAVE GROWL"}, {2, 8}, {70, 90}, {100, 120}, {30, 45}, 0, {40, 62}, {15, 35}, {0, 10}, {0, 0}, {0, 0}, {10, 30}, {20, 40}, 20, V_LEGATO},
    [SR_FMBASS]   = {{EG_FM, EG_PH, EG_LO}, {"REACTOR SUB", "FM BASS", "HARD BASS"}, {0, 2}, {45, 70}, {40, 90}, {18, 35}, 0, {50, 80}, {10, 30}, {10, 30}, {0, 0}, {0, 0}, {0, 15}, {0, 0}, 0, V_LEGATO},
    [SR_ROCKBASS] = {{EG_A, EG_A, EG_PH}, {"SQR BASS", "REED BASS", "FAT BASS"}, {0, 3}, {50, 75}, {70, 110}, {20, 40}, 0, {50, 76}, {10, 30}, {10, 25}, {0, 0}, {0, 0}, {15, 40}, {0, 0}, 0, V_LEGATO},
    [SR_HOOK]     = {{EG_A, EG_A, EG_PH}, {"SQR BASS", "SYN BASS", "FAT BASS"}, {0, 2}, {60, 85}, {90, 115}, {30, 50}, 0, {60, 86}, {10, 24}, {5, 15}, {0, 0}, {0, 0}, {5, 20}, {35, 65}, 0, V_LEGATO},
    [SR_WOW]      = {{EG_VO, EG_A, EG_PH}, {"WRAITH GROW", "WOW BASS", "TALK BOX"}, {0, 2}, {60, 75}, {60, 80}, {25, 35}, 0, {70, 100}, {10, 30}, {10, 30}, {0, 0}, {0, 0}, {10, 20}, {0, 0}, 0, V_LEGATO},
    [SR_PAD]      = {{EG_A, EG_FM, EG_PH}, {"WITCH PAD", "JUNO PAD", "CHERENKOV"}, {55, 100}, {80, 100}, {100, 120}, {80, 115}, 0, {44, 68}, {10, 25}, {5, 20}, {8, 20}, {4, 16}, {0, 0}, {25, 60}, 0, V_POLY},
    [SR_STRINGS]  = {{EG_A, EG_PH, EG_FM}, {"STRINGS", "FM STRINGS", "VOX PAD"}, {40, 80}, {80, 100}, {105, 125}, {70, 100}, 0, {50, 76}, {8, 20}, {0, 12}, {6, 16}, {4, 14}, {0, 0}, {40, 70}, 0, V_POLY},
    [SR_CHOIR]    = {{EG_VO, EG_VO, EG_FM}, {"BLACK CHOIR", "CHOIR", "DEAD SAINT"}, {60, 100}, {90, 100}, {105, 120}, {85, 115}, 0, {60, 90}, {0, 10}, {0, 8}, {6, 12}, {3, 10}, {0, 0}, {50, 80}, 0, V_POLY},
    [SR_ORGAN]    = {{EG_WH, EG_WH, EG_PH}, {"DOOM CATH", "DUB ORGAN", "BLOOD ORGAN"}, {0, 2}, {64, 64}, {127, 127}, {25, 45}, 0, {60, 90}, {0, 10}, {0, 0}, {0, 0}, {0, 0}, {0, 15}, {0, 20}, 0, V_POLY},
    [SR_EP]       = {{EG_FM, EG_PH, EG_FM}, {"TINE EP", "DX KEYS", "CZ PIANO"}, {0, 2}, {70, 90}, {20, 50}, {50, 75}, 0, {60, 90}, {0, 15}, {5, 20}, {0, 0}, {0, 0}, {0, 0}, {10, 30}, 0, V_POLY},
    [SR_STAB]     = {{EG_A, EG_PH, EG_FM}, {"PLASMA CORE", "DUB STAB", "DIGI STAB"}, {0, 6}, {55, 72}, {30, 70}, {25, 45}, 0, {50, 78}, {15, 40}, {15, 40}, {0, 0}, {0, 0}, {0, 15}, {15, 40}, 0, V_POLY},
    [SR_DUBCHORD] = {{EG_A, EG_PH, EG_FM}, {"DUB STAB", "DUB ORGAN", "PWM STR"}, {0, 8}, {50, 70}, {20, 60}, {30, 50}, 2, {46, 66}, {25, 45}, {10, 30}, {0, 0}, {0, 0}, {0, 0}, {20, 50}, 0, V_POLY},
    [SR_PLUCK]    = {{EG_A, EG_FM, EG_PH}, {"ARP PLUCK", "FM PLUCK", "PLUCK"}, {0, 3}, {40, 66}, {0, 20}, {35, 60}, 0, {58, 86}, {20, 50}, {30, 55}, {0, 0}, {0, 0}, {0, 0}, {0, 20}, 0, V_POLY},
    [SR_BELL]     = {{EG_FM, EG_PH, EG_A}, {"CRYPT BELL", "SOFT BELL", "ICE BELL"}, {0, 2}, {80, 100}, {0, 10}, {70, 100}, 0, {70, 100}, {0, 20}, {0, 10}, {0, 0}, {0, 0}, {0, 0}, {0, 20}, 0, V_POLY},
    [SR_LEAD]     = {{EG_A, EG_PH, EG_FM}, {"HEARTH LEAD", "PLASMA CORE", "MOOG LEAD"}, {2, 20}, {50, 85}, {30, 90}, {30, 70}, 0, {56, 84}, {15, 40}, {15, 40}, {0, 14}, {6, 24}, {0, 25}, {0, 30}, 15, V_LEGATO},
    [SR_VOXLEAD]  = {{EG_VO, EG_A, EG_FM}, {"GHOUL VOX", "VOX LEAD", "TALK BOX"}, {4, 20}, {55, 85}, {40, 95}, {35, 70}, 0, {60, 90}, {10, 30}, {10, 25}, {0, 10}, {6, 20}, {0, 10}, {10, 35}, 20, V_LEGATO},
    [SR_CHIP]     = {{EG_LO, EG_LO, EG_PH}, {"DATAMOSH", "GB ARP", "PULSE LD"}, {0, 4}, {45, 75}, {25, 80}, {20, 50}, 0, {70, 110}, {10, 30}, {10, 30}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, 0, V_LEGATO},
    [SR_KEYS]     = {{EG_A, EG_FM, EG_PH}, {"GLASS KEYS", "RING KEYS", "SOFT KEYS"}, {0, 4}, {65, 85}, {20, 50}, {45, 70}, 0, {60, 90}, {5, 20}, {5, 20}, {0, 0}, {0, 0}, {0, 0}, {10, 30}, 0, V_POLY},
    [SR_DRONE]    = {{EG_A, EG_FM, EG_PH}, {"CHERENKOV", "DARK PAD", "ASH CLOUD"}, {90, 120}, {90, 100}, {120, 127}, {100, 125}, 0, {36, 60}, {15, 35}, {0, 10}, {10, 24}, {2, 8}, {0, 0}, {30, 60}, 0, V_POLY},
    [SR_NOISE]    = {{EG_NO, EG_NO, EG_LO}, {"GEIGER HISS", "SEA", "TAPE AIR"}, {80, 120}, {90, 100}, {110, 127}, {80, 120}, 255, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, 0, V_POLY},
    [SR_TEXTURE]  = {{EG_NO, EG_LO, EG_VO}, {"ASH CLOUD", "HISS PAD", "CRACKLE"}, {70, 110}, {90, 100}, {110, 127}, {90, 120}, 2, {40, 70}, {20, 50}, {0, 10}, {10, 30}, {2, 10}, {0, 0}, {20, 50}, 0, V_POLY},
    [SR_HOOVER]   = {{EG_A, EG_A, EG_PH}, {"HOOVER", "RAVE", "PWM STR"}, {4, 12}, {60, 90}, {60, 110}, {35, 60}, 0, {60, 90}, {10, 25}, {10, 30}, {0, 0}, {0, 0}, {15, 35}, {40, 60}, 0, V_POLY},
    [SR_WALL]     = {{EG_A, EG_A, EG_PH}, {"DARK PAD", "JUNO PAD", "STRINGS"}, {30, 60}, {90, 100}, {110, 127}, {80, 110}, 0, {50, 70}, {0, 10}, {0, 10}, {6, 14}, {3, 9}, {30, 55}, {40, 70}, 0, V_POLY},
    [SR_GTR]      = {{EG_PH, EG_A, EG_FM}, {"HARP", "DIGI STAB", "PLUCK"}, {2, 10}, {70, 90}, {60, 100}, {50, 80}, 2, {56, 80}, {15, 35}, {5, 20}, {0, 0}, {0, 0}, {5, 25}, {20, 40}, 0, V_POLY},
    [SR_SINEKEY]  = {{EG_A, EG_FM, EG_PH}, {"GLASS KEYS", "WOOD", "SINE KEY"}, {0, 4}, {70, 90}, {30, 60}, {50, 80}, 0, {60, 90}, {0, 15}, {0, 15}, {0, 0}, {0, 0}, {0, 0}, {0, 20}, 0, V_POLY},
    /* (0.6.95) the EBM bass: a square, short and punchy, the filter's envelope on it, driven */
    [SR_EBMBASS]  = {{EG_A, EG_A, EG_PH}, {"SYN BASS", "HARD BASS", "SQR BASS"}, {0, 1}, {34, 50}, {10, 40}, {12, 25}, 0, {58, 80}, {15, 35}, {25, 45}, {0, 0}, {0, 0}, {18, 40}, {0, 0}, 0, V_LEGATO},
    /* Trap / UK Drill 808 sub with portamento glide and warm saturation */
    [SR_808GLIDE] = {{EG_A, EG_FM, EG_LO}, {"INGOT SUB", "REACTOR SUB", "SUB BASS"}, {0, 1}, {65, 90}, {80, 115}, {25, 45}, 0, {36, 56}, {8, 22}, {12, 28}, {0, 0}, {0, 0}, {15, 38}, {0, 0}, 38, V_LEGATO},
};
/* (0.6.95) a throw its own: the neighbours a throw may take instead (the drum map's family, the chords' play, a role's
 * recipe), so two throws of a genre are not the same piece in another key */
static const uint8_t DM_ALT[DM_NONE][2] = {
    [DM_HOUSE] = {DM_DEEP, DM_TECHHOUSE}, [DM_DEEP] = {DM_HOUSE, DM_MICRO}, [DM_MICRO] = {DM_DEEP, DM_DUBTECH},
    [DM_TECHHOUSE] = {DM_HOUSE, DM_TECHNO}, [DM_TECHNO] = {DM_DETROIT, DM_HYPNO}, [DM_DETROIT] = {DM_TECHNO, DM_HOUSE},
    [DM_DUBTECH] = {DM_MICRO, DM_HYPNO}, [DM_HYPNO] = {DM_TECHNO, DM_DUBTECH}, [DM_HARD] = {DM_TECHNO, DM_EBM},
    [DM_ELECTRO] = {DM_RBOX, DM_KLING}, [DM_EBM] = {DM_HARD, DM_DARKWAVE}, [DM_ITALO] = {DM_DISCO, DM_SYNTHPOP},
    [DM_SYNTHPOP] = {DM_ITALO, DM_RBOX}, [DM_DARKWAVE] = {DM_POSTPUNK, DM_EBM}, [DM_POSTPUNK] = {DM_JOYDIV, DM_NEWORDER},
    [DM_JOYDIV] = {DM_POSTPUNK, DM_NEWORDER}, [DM_NEWORDER] = {DM_POSTPUNK, DM_JOYDIV},
    [DM_TRIBAL] = {DM_POSTPUNK, DM_AFRO}, [DM_RBOX] = {DM_SYNTHPOP, DM_KLING}, [DM_KLING] = {DM_RBOX, DM_MOTORIK},
    [DM_MOTORIK] = {DM_KLING, DM_INDIE}, [DM_INDIE] = {DM_MOTORIK, DM_SHOEGAZE}, [DM_SHOEGAZE] = {DM_INDIE, DM_SPARSE},
    [DM_FUNK] = {DM_AFRO, DM_BOOMBAP}, [DM_DISCO] = {DM_ITALO, DM_HOUSE}, [DM_BOOMBAP] = {DM_FUNK, DM_TRAP},
    [DM_TRAP] = {DM_TRAP_ATL, DM_UK_DRILL}, [DM_UKG] = {DM_BREAKS, DM_HOUSE}, [DM_DUBSTEP] = {DM_TRAP, DM_DNB},
    [DM_BREAKS] = {DM_UKG, DM_DNB}, [DM_DNB] = {DM_BREAKS, DM_DUBSTEP}, [DM_ONEDROP] = {DM_STEPPERS, DM_ROCKERS},
    [DM_STEPPERS] = {DM_ONEDROP, DM_ROCKERS}, [DM_ROCKERS] = {DM_ONEDROP, DM_STEPPERS}, [DM_JAZZ] = {DM_BOSSA, DM_SPARSE},
    [DM_BOSSA] = {DM_JAZZ, DM_AFRO}, [DM_AFRO] = {DM_FUNK, DM_TRIBAL}, [DM_SPARSE] = {DM_JAZZ, DM_IDM}, [DM_IDM] = {DM_SPARSE, DM_BREAKS},
    [DM_TRAP_ATL] = {DM_TRAP_ATL, DM_TRAP}, [DM_UK_DRILL] = {DM_UK_DRILL, DM_UK_DRILL},
    [DM_PHONK] = {DM_PHONK, DM_TRAP_ATL}, [DM_RAGE] = {DM_RAGE, DM_TRAP_ATL},
};
static const uint8_t CS_ALT[CS_TRANCE + 1][2] = {
    [CS_HOLD] = {CS_SWELL, CS_PULSE}, [CS_SWELL] = {CS_HOLD, CS_PULSE}, [CS_STAB] = {CS_PULSE, CS_DUB}, [CS_PULSE] = {CS_STAB, CS_HOLD},
    [CS_DUB] = {CS_STAB, CS_SPARSE}, [CS_SKANK] = {CS_DUB, CS_STAB}, [CS_COMP] = {CS_STAB, CS_BROKEN}, [CS_POWER] = {CS_PULSE, CS_HOLD},
    [CS_DRONE] = {CS_HOLD, CS_SWELL}, [CS_SPARSE] = {CS_HOLD, CS_DUB}, [CS_BROKEN] = {CS_COMP, CS_STAB}, [CS_TRANCE] = {CS_PULSE, CS_STAB},
};
static const uint8_t SR_ALT[SR_N][2] = {
    [SR_SUB] = {SR_808GLIDE, SR_REESE}, [SR_ACID] = {SR_SUB, SR_FMBASS}, [SR_REESE] = {SR_FMBASS, SR_808GLIDE}, [SR_FMBASS] = {SR_SUB, SR_ROCKBASS},
    [SR_ROCKBASS] = {SR_FMBASS, SR_HOOK}, [SR_HOOK] = {SR_ROCKBASS, SR_FMBASS}, [SR_WOW] = {SR_FMBASS, SR_REESE}, [SR_PAD] = {SR_STRINGS, SR_CHOIR},
    [SR_STRINGS] = {SR_PAD, SR_ORGAN}, [SR_CHOIR] = {SR_PAD, SR_STRINGS}, [SR_ORGAN] = {SR_EP, SR_KEYS}, [SR_EP] = {SR_KEYS, SR_ORGAN},
    [SR_STAB] = {SR_DUBCHORD, SR_HOOVER}, [SR_DUBCHORD] = {SR_STAB, SR_PAD}, [SR_PLUCK] = {SR_BELL, SR_KEYS}, [SR_BELL] = {SR_PLUCK, SR_SINEKEY},
    [SR_LEAD] = {SR_VOXLEAD, SR_HOOVER}, [SR_VOXLEAD] = {SR_LEAD, SR_CHOIR}, [SR_CHIP] = {SR_PLUCK, SR_LEAD}, [SR_KEYS] = {SR_EP, SR_SINEKEY},
    [SR_DRONE] = {SR_PAD, SR_TEXTURE}, [SR_NOISE] = {SR_TEXTURE, SR_DRONE}, [SR_TEXTURE] = {SR_NOISE, SR_DRONE}, [SR_HOOVER] = {SR_STAB, SR_LEAD},
    [SR_WALL] = {SR_STRINGS, SR_PAD}, [SR_GTR] = {SR_PLUCK, SR_KEYS}, [SR_SINEKEY] = {SR_KEYS, SR_BELL}, [SR_EBMBASS] = {SR_ROCKBASS, SR_ACID},
    [SR_808GLIDE] = {SR_808GLIDE, SR_SUB},
};
/* the names the song generator never picks (the cheerful, the odd), whatever the role */
static const char *const GEN_NEVER[] = {"ARCADE", "BYTEBEAT", "GOSPEL", "ROCK DRIVE", "METAL", "BYTE VAR", "FLOAT FM",
                                        "FLOAT ARP", "BYTE TONE"};

/* -------------------------------------------------------------------------------------------------- forms --- */
enum { FM_LOOP, FM_PARTS, FM_CLUB, FM_ARC };            /* one loop; A B A C; intro build drop break; a slow arc */

/* -------------------------------------------------------------------------------------------------- genres --- */
#define SC_BLUE 12u                                     /* (the other scales: song_dice.c) */
typedef struct {
    const char *name;
    uint8_t bpm_lo, bpm_hi, swing;                      /* swing: G_SWING, 0..100 */
    uint8_t scale[4];
    uint8_t drum, drum_pct, kit, ghosts, hats, fills;   /* the map, how often drums play, the kit, snare ghosts a bar
                                                         * at full amount, the hats' cell, the fill */
    int8_t  sd_micro;                                   /* the snare laid back (sixteenths of a step) */
    uint8_t bass[3], bass_lo, kick_mode, bass_gate;     /* three archetypes, the root's lowest note, the kick rule, GATE */
    uint8_t prog[3], cbars, pads, voicing;              /* three progressions, bars a chord, the chords' play, voicing */
    uint8_t arp[2], lead;
    uint8_t amt[GP_N];                                  /* the parts' default amounts (0: the part sleeps) */
    uint8_t snd[GP_N];                                  /* the kit (GP_DRUMS) and a recipe a role */
    uint8_t tex;                                        /* the sixth track's recipe (TRACKS 6) */
    uint8_t rtype, dtime, wet, dtype;                   /* reverb type, delay time (N_DTIME), 0..100 wet, delay type bit 0
                                                         * ANALOG / DIGITAL, bit 1 PING-PONG */
    uint8_t form;
    uint8_t human;                                      /* 0 a machine .. 100 a band: HUMAN and TIMING */
} ggenre_t;
#define AMT(d, b, c, a, l) {d, b, c, a, l}
static const ggenre_t GENRE[] = {
    /* ---- signature Pocket GammaWave / Wraith & Trap / Drill genres */
    {"GAMMAWAVE", 118, 134, 4, {SC_MIN, SC_PHRY, SC_HARM, SC_DOR}, DM_EBM, 100, KIT_INDUS, 1, HC_MACHINE, FL_ROLL, 0,
     {BA_SYNTHW, BA_NITZER1, BA_ACID}, 33, KM_DOUBLE, 50, {PR_ONE, PR_I_VI, PR_MINOR4}, 2, CS_HOLD, VM_RICH,
     {AP_SEQ, AP_OCTAVES}, LD_MOTIF, AMT(95, 95, 75, 65, 55), {KIT_INDUS, SR_REESE, SR_CHOIR, SR_PLUCK, SR_LEAD}, SR_DRONE,
     2, 2, 60, 2, FM_CLUB, 10},
    {"WRAITHWAVE", 104, 126, 6, {SC_PHRY, SC_HARM, SC_MIN, SC_DOR}, DM_DARKWAVE, 100, KIT_808, 1, HC_FLAT, FL_TOM, 1,
     {BA_DARK8, BA_WOBBLE, BA_DUB}, 31, KM_SUSTAIN, 65, {PR_I_VI, PR_MINOR4, PR_ONE}, 2, CS_SWELL, VM_RICH,
     {AP_CYCLE3, AP_GLASS}, LD_HOOK, AMT(85, 95, 85, 55, 50), {KIT_808, SR_SUB, SR_CHOIR, SR_BELL, SR_VOXLEAD}, SR_TEXTURE,
     4, 4, 75, 2, FM_ARC, 20},
    {"TRAP ATL", 136, 150, 0, {SC_PHRY, SC_MIN, SC_HARM, SC_MIN}, DM_TRAP_ATL, 100, KIT_808, 1, HC_MACHINE, FL_HAT, 0,
     {BA_ATL808, BA_TRAP, BA_RAGE808}, 29, KM_SUSTAIN, 110, {PR_I_VI, PR_ONE, PR_MINOR4}, 2, CS_SPARSE, VM_RICH,
     {AP_BROKEN, AP_CYCLE3}, LD_OSTINATO, AMT(95, 100, 50, 55, 45), {KIT_808, SR_808GLIDE, SR_PAD, SR_BELL, SR_KEYS}, SR_CHOIR,
     2, 1, 55, 2, FM_PARTS, 5},
    {"UK DRILL", 140, 145, 8, {SC_MIN, SC_PHRY, SC_HARM, SC_MIN}, DM_UK_DRILL, 100, KIT_808, 2, HC_GARAGE, FL_HAT, 0,
     {BA_UKDRILL, BA_ATL808, BA_TRAP}, 29, KM_SUSTAIN, 115, {PR_I_VI, PR_MINOR4, PR_ANDAL}, 2, CS_SWELL, VM_RICH,
     {AP_BROKEN, AP_ALBERTI}, LD_SPARSE, AMT(95, 100, 65, 45, 40), {KIT_808, SR_808GLIDE, SR_CHOIR, SR_KEYS, SR_STRINGS}, SR_DRONE,
     2, 2, 60, 2, FM_CLUB, 10},
    {"DARK TRAP", 130, 148, 4, {SC_PHRY, SC_HARM, SC_MIN, SC_PHRY}, DM_PHONK, 100, KIT_808, 1, HC_MACHINE, FL_HAT, 0,
     {BA_ATL808, BA_DARK8, BA_WOBBLE}, 29, KM_DOUBLE, 105, {PR_ONE, PR_I_II, PR_I_VI}, 2, CS_HOLD, VM_SUS,
     {AP_CYCLE3, AP_OCTAVES}, LD_HOOK, AMT(95, 100, 60, 50, 55), {KIT_808, SR_808GLIDE, SR_CHOIR, SR_BELL, SR_VOXLEAD}, SR_TEXTURE,
     4, 2, 65, 2, FM_PARTS, 10},
    {"RAGE TRAP", 138, 156, 0, {SC_MIN, SC_PHRY, SC_MIN, SC_HARM}, DM_RAGE, 100, KIT_INDUS, 0, HC_MACHINE, FL_HAT, 0,
     {BA_RAGE808, BA_ATL808, BA_SYNTHW}, 31, KM_DOUBLE, 95, {PR_I_VI, PR_TRANCE, PR_ONE}, 2, CS_STAB, VM_POWER,
     {AP_TRANCE, AP_OCTAVES}, LD_OSTINATO, AMT(100, 100, 75, 75, 65), {KIT_INDUS, SR_REESE, SR_HOOVER, SR_PLUCK, SR_LEAD}, SR_NOISE,
     2, 1, 45, 3, FM_CLUB, 0},
    /* ---- the club: house */
    {"HOUSE", 122, 127, 8, {SC_MIN, SC_DOR, SC_MAJ, SC_MIN}, DM_HOUSE, 100, KIT_909, 0, HC_HOUSE, FL_ROLL, 0,
     {BA_OFFBEAT, BA_BOUNCE, BA_OFFOCT}, 36, KM_ANSWER, 45, {PR_I_IV, PR_I_VI, PR_MINOR4}, 1, CS_STAB, VM_RICH,
     {AP_BROKEN, AP_UP}, LD_HOOK, AMT(80, 70, 60, 30, 40), {KIT_909, SR_FMBASS, SR_STAB, SR_PLUCK, SR_KEYS}, SR_NOISE,
     0, 1, 40, 0, FM_CLUB, 10},
    {"DEEP HOUSE", 118, 124, 20, {SC_MIN, SC_DOR, SC_MIN, SC_MPEN}, DM_DEEP, 100, KIT_808, 0, HC_HOUSE, FL_DROP, 1,
     {BA_DEEPWALK, BA_OFFBEAT, BA_UKG}, 36, KM_ANSWER, 50, {PR_I_IV, PR_JAZZ, PR_MINOR4}, 2, CS_DUB, VM_RICH,
     {AP_BROKEN, AP_UPDOWN}, LD_SPARSE, AMT(70, 75, 70, 30, 30), {KIT_808, SR_SUB, SR_EP, SR_KEYS, SR_SINEKEY}, SR_TEXTURE,
     0, 1, 55, 0, FM_CLUB, 20},
    {"MICRO HOUSE", 122, 128, 10, {SC_MIN, SC_DOR, SC_MIN, SC_PHRY}, DM_MICRO, 100, KIT_CLICK, 0, HC_FLAT, FL_DROP, 0,
     {BA_OFFBEAT, BA_DEEPWALK, BA_ROLL16}, 36, KM_ANSWER, 40, {PR_ONE, PR_I_VII, PR_I_IV}, 2, CS_SPARSE, VM_QUART,
     {AP_CYCLE3, AP_WALK}, LD_SPARSE, AMT(70, 65, 40, 45, 20), {KIT_CLICK, SR_FMBASS, SR_PLUCK, SR_PLUCK, SR_CHIP}, SR_TEXTURE,
     0, 1, 45, 0, FM_CLUB, 0},
    {"TECH HOUSE", 124, 128, 10, {SC_MIN, SC_PHRY, SC_DOR, SC_MIN}, DM_TECHHOUSE, 100, KIT_909, 0, HC_HOUSE, FL_ROLL, 0,
     {BA_OFFBEAT, BA_OFFOCT, BA_BOUNCE}, 36, KM_ANSWER, 40, {PR_ONE, PR_I_VII, PR_I_VI}, 2, CS_STAB, VM_SUS,
     {AP_BROKEN, AP_SEQ}, LD_SPARSE, AMT(85, 80, 40, 30, 25), {KIT_909, SR_FMBASS, SR_STAB, SR_PLUCK, SR_LEAD}, SR_NOISE,
     0, 1, 35, 0, FM_CLUB, 0},
    {"ACID HOUSE", 122, 130, 8, {SC_MIN, SC_PHRY, SC_DOR, SC_MIN}, DM_HOUSE, 100, KIT_909, 0, HC_HOUSE, FL_ROLL, 0,
     {BA_ACID, BA_ACID, BA_BOUNCE}, 36, KM_SUSTAIN, 60, {PR_ONE, PR_I_VII, PR_I_IV}, 2, CS_STAB, VM_SUS,
     {AP_UP, AP_SEQ}, LD_NONE, AMT(85, 90, 30, 20, 0), {KIT_909, SR_ACID, SR_STAB, SR_PLUCK, SR_LEAD}, SR_NOISE,
     0, 1, 35, 0, FM_CLUB, 0},
    /* ---- the club: techno */
    {"TECHNO", 128, 136, 4, {SC_MIN, SC_PHRY, SC_MIN, SC_MIN}, DM_TECHNO, 100, KIT_909, 0, HC_MACHINE, FL_DROP, 0,
     {BA_ROLL16, BA_OCTBNC, BA_OFFBEAT}, 33, KM_SUSTAIN, 45, {PR_ONE, PR_ONE, PR_I_VII}, 4, CS_STAB, VM_PLANE,
     {AP_PULSE, AP_SEQ}, LD_OSTINATO, AMT(90, 80, 35, 35, 25), {KIT_909, SR_REESE, SR_STAB, SR_PLUCK, SR_LEAD}, SR_NOISE,
     0, 2, 35, 0, FM_CLUB, 0},
    {"DETROIT", 124, 132, 8, {SC_MIN, SC_DOR, SC_MIN, SC_MAJ}, DM_DETROIT, 100, KIT_909, 0, HC_HOUSE, FL_ROLL, 0,
     {BA_DETROIT, BA_OCTBNC, BA_SYNTHW}, 36, KM_SUSTAIN, 50, {PR_I_VI, PR_MINOR4, PR_I_IV}, 1, CS_STAB, VM_RICH,
     {AP_UPDOWN, AP_BROKEN}, LD_MOTIF, AMT(85, 80, 60, 45, 45), {KIT_909, SR_FMBASS, SR_STRINGS, SR_PLUCK, SR_LEAD}, SR_PAD,
     0, 1, 45, 0, FM_CLUB, 5},
    {"DUB TECHNO", 116, 124, 6, {SC_MIN, SC_DOR, SC_MIN, SC_MPEN}, DM_DUBTECH, 100, KIT_DUB, 0, HC_FLAT, FL_DUB, 0,
     {BA_DUB, BA_PEDAL, BA_OFFBEAT}, 31, KM_SUSTAIN, 70, {PR_ONE, PR_I_IV, PR_I_VI}, 4, CS_DUB, VM_RICH,
     {AP_PULSE, AP_CYCLE3}, LD_SPARSE, AMT(60, 60, 75, 20, 15), {KIT_DUB, SR_SUB, SR_DUBCHORD, SR_PLUCK, SR_SINEKEY}, SR_TEXTURE,
     4, 4, 85, 0, FM_ARC, 0},
    {"HARD TECHNO", 138, 150, 0, {SC_PHRY, SC_MIN, SC_HARM, SC_PHRY}, DM_HARD, 100, KIT_INDUS, 0, HC_MACHINE, FL_DROP, 0,
     {BA_ROLL16, BA_PSY, BA_OCTBNC}, 33, KM_SUSTAIN, 40, {PR_ONE, PR_I_VII, PR_ONE}, 4, CS_STAB, VM_POWER,
     {AP_PULSE, AP_OCTAVES}, LD_OSTINATO, AMT(100, 85, 30, 30, 25), {KIT_INDUS, SR_REESE, SR_STAB, SR_PLUCK, SR_HOOVER}, SR_NOISE,
     1, 2, 25, 0, FM_CLUB, 0},
    {"HYPNOTIC", 128, 134, 2, {SC_PHRY, SC_MIN, SC_DOR, SC_HARM}, DM_HYPNO, 100, KIT_CLICK, 0, HC_FLAT, FL_NONE, 0,
     {BA_ROLL16, BA_PSY, BA_OFFBEAT}, 33, KM_SUSTAIN, 40, {PR_ONE, PR_ONE, PR_I_II}, 4, CS_DUB, VM_PLANE,
     {AP_CYCLE5, AP_CYCLE3}, LD_OSTINATO, AMT(90, 80, 30, 50, 30), {KIT_CLICK, SR_FMBASS, SR_STAB, SR_PLUCK, SR_PLUCK}, SR_TEXTURE,
     0, 4, 50, 0, FM_ARC, 0},
    {"TRANCE", 134, 140, 0, {SC_MIN, SC_MIN, SC_HARM, SC_DOR}, DM_TECHHOUSE, 100, KIT_909, 0, HC_FLAT, FL_ROLL, 0,
     {BA_PSY, BA_PUMP8, BA_OFFBEAT}, 36, KM_ANSWER, 45, {PR_TRANCE, PR_AEOL, PR_MINOR4}, 2, CS_TRANCE, VM_RICH,
     {AP_TRANCE, AP_UP}, LD_MOTIF, AMT(85, 85, 70, 70, 50), {KIT_909, SR_REESE, SR_STRINGS, SR_PLUCK, SR_LEAD}, SR_NOISE,
     2, 4, 55, 3, FM_CLUB, 0},
    /* ---- electro, EBM, italo, synth pop, the wave */
    {"ELECTRO", 112, 128, 2, {SC_MIN, SC_PHRY, SC_MIN, SC_MPEN}, DM_ELECTRO, 100, KIT_808, 0, HC_MACHINE, FL_KICK16, 0,
     {BA_SYNTHW, BA_ITALO16, BA_ANGULAR}, 36, KM_DOUBLE, 40, {PR_I_VII, PR_ONE, PR_MINOR4}, 2, CS_STAB, VM_SUS,
     {AP_SEQ, AP_OCTAVES}, LD_MECH, AMT(90, 85, 40, 50, 45), {KIT_808, SR_FMBASS, SR_STAB, SR_CHIP, SR_LEAD}, SR_NOISE,
     1, 2, 35, 1, FM_PARTS, 0},
    {"EBM", 118, 132, 0, {SC_PHRY, SC_MIN, SC_PHRY, SC_MIN}, DM_EBM, 100, KIT_INDUS, 0, HC_MACHINE, FL_NONE, 0,
     {BA_NITZER1, BA_NITZER2, BA_NITZER3}, 33, KM_DOUBLE, 45, {PR_ONE, PR_I_VII, PR_I_II}, 4, CS_STAB, VM_POWER,
     {AP_SEQ, AP_PULSE}, LD_SPARSE, AMT(95, 100, 30, 25, 30), {KIT_INDUS, SR_EBMBASS, SR_STAB, SR_PLUCK, SR_VOXLEAD}, SR_NOISE,
     1, 1, 25, 0, FM_PARTS, 0},
    {"ITALO", 114, 124, 0, {SC_MIN, SC_MIN, SC_MAJ, SC_DOR}, DM_ITALO, 100, KIT_808, 0, HC_MACHINE, FL_TOM, 0,
     {BA_ITALO, BA_ITALO16, BA_SYNTHW}, 36, KM_DOUBLE, 40, {PR_AEOL, PR_50S, PR_POP}, 1, CS_STAB, VM_PLAIN,
     {AP_OCTAVES, AP_UP}, LD_HOOK, AMT(90, 90, 60, 60, 60), {KIT_808, SR_FMBASS, SR_STAB, SR_PLUCK, SR_LEAD}, SR_PAD,
     0, 1, 40, 2, FM_PARTS, 0},
    {"SYNTH POP", 112, 128, 0, {SC_MIN, SC_MAJ, SC_DOR, SC_MIN}, DM_SYNTHPOP, 100, KIT_808, 0, HC_MACHINE, FL_TOM, 0,
     {BA_ITALO, BA_HOOK, BA_ROCK8}, 36, KM_DOUBLE, 50, {PR_AEOL, PR_POP, PR_AXIS}, 1, CS_HOLD, VM_PLAIN,
     {AP_SEQ, AP_UPDOWN}, LD_HOOK, AMT(85, 85, 65, 55, 60), {KIT_808, SR_FMBASS, SR_PAD, SR_PLUCK, SR_LEAD}, SR_STRINGS,
     0, 1, 45, 2, FM_PARTS, 5},
    {"DARKWAVE", 124, 136, 0, {SC_MIN, SC_PHRY, SC_DOR, SC_MIN}, DM_DARKWAVE, 100, KIT_808, 0, HC_MACHINE, FL_NONE, 0,
     {BA_DARK8, BA_PUNK16, BA_OCTBNC}, 31, KM_DOUBLE, 40, {PR_ONE, PR_I_VI, PR_I_VII}, 4, CS_HOLD, VM_PLAIN,
     {AP_PULSE, AP_SEQ}, LD_SPARSE, AMT(90, 95, 60, 25, 40), {KIT_808, SR_ROCKBASS, SR_PAD, SR_PLUCK, SR_LEAD}, SR_TEXTURE,
     0, 1, 50, 0, FM_PARTS, 0},
    {"POST PUNK", 118, 148, 0, {SC_MIN, SC_DOR, SC_MIN, SC_MAJ}, DM_POSTPUNK, 100, KIT_SOFT, 1, HC_FLAT, FL_TOM, 1,
     {BA_HOOK2, BA_BLUEMON, BA_CEREMONY}, 40, KM_DOUBLE, 70, {PR_AEOL, PR_I_VI, PR_POP}, 2, CS_PULSE, VM_PLAIN,
     {AP_OCTAVES, AP_UPDOWN}, LD_HOOK, AMT(95, 100, 55, 45, 50), {KIT_SOFT, SR_HOOK, SR_STRINGS, SR_PLUCK, SR_LEAD}, SR_CHOIR,
     1, 2, 45, 0, FM_PARTS, 35},   /* (0.6.97: after Joy Division and New Order: Hook's bass, Morris's drums, Sumner's
                                    * pulsed chords, a sequenced octave arp, strings and choir behind) */
    {"TRIBAL PUNK", 140, 165, 0, {SC_MIN, SC_PHRY, SC_MIN, SC_DOR}, DM_TRIBAL, 100, KIT_SOFT, 0, HC_FLAT, FL_TOM, 1,
     {BA_ANGULAR, BA_ROCK8, BA_HOOK}, 36, KM_SUSTAIN, 60, {PR_ONE, PR_I_VII, PR_I_VI}, 4, CS_POWER, VM_POWER,
     {AP_PULSE, AP_UP}, LD_OSTINATO, AMT(95, 95, 40, 10, 45), {KIT_SOFT, SR_ROCKBASS, SR_GTR, SR_PLUCK, SR_VOXLEAD}, SR_NOISE,
     0, 1, 55, 0, FM_PARTS, 40},
    {"MINIMAL SYNTH", 104, 132, 0, {SC_MIN, SC_DOR, SC_MIN, SC_PHRY}, DM_RBOX, 100, KIT_SOFT, 0, HC_MACHINE, FL_NONE, 0,
     {BA_SUICIDE, BA_SEQ16, BA_DARK8}, 33, KM_DOUBLE, 40, {PR_ONE, PR_ONE, PR_I_IV}, 4, CS_DRONE, VM_POWER,
     {AP_PULSE, AP_SEQ}, LD_SPARSE, AMT(85, 95, 55, 20, 45), {KIT_SOFT, SR_ROCKBASS, SR_ORGAN, SR_CHIP, SR_VOXLEAD}, SR_NOISE,
     1, 2, 50, 0, FM_LOOP, 0},
    {"MAN MACHINE", 100, 124, 0, {SC_MIN, SC_MAJ, SC_DOR, SC_MIN}, DM_KLING, 100, KIT_CLICK, 0, HC_MACHINE, FL_NONE, 0,
     {BA_SEQ16, BA_SYNTHW, BA_MOTORIK}, 36, KM_DOUBLE, 45, {PR_ONE, PR_I_IV, PR_CYCLE}, 4, CS_HOLD, VM_PLAIN,
     {AP_SEQ, AP_OCTAVES}, LD_MECH, AMT(85, 90, 50, 60, 60), {KIT_CLICK, SR_FMBASS, SR_PAD, SR_PLUCK, SR_LEAD}, SR_VOXLEAD,
     0, 0, 15, 1, FM_PARTS, 0},
    /* ---- kraut, indie, shoegaze, post rock */
    {"MOTORIK", 118, 140, 0, {SC_MAJ, SC_MIX, SC_DOR, SC_MIN}, DM_MOTORIK, 100, KIT_SOFT, 0, HC_FLAT, FL_NONE, 0,
     {BA_MOTORIK, BA_ROCK8, BA_PUMP8}, 33, KM_DOUBLE, 60, {PR_ONE, PR_I_IV, PR_I_V}, 4, CS_PULSE, VM_PLAIN,
     {AP_UPDOWN, AP_GLASS}, LD_TINTI, AMT(90, 95, 45, 50, 35), {KIT_SOFT, SR_ROCKBASS, SR_GTR, SR_PLUCK, SR_LEAD}, SR_DRONE,
     0, 1, 45, 0, FM_ARC, 30},
    {"INDIE", 110, 140, 0, {SC_MAJ, SC_MIN, SC_MIX, SC_MAJ}, DM_INDIE, 100, KIT_BREAKS, 1, HC_FLAT, FL_TOM, 1,
     {BA_INDIE, BA_ROOT5, BA_MCCART}, 36, KM_DOUBLE, 70, {PR_POP, PR_AXIS, PR_50S}, 1, CS_POWER, VM_PLAIN,
     {AP_UPDOWN, AP_ALBERTI}, LD_HOOK, AMT(85, 90, 70, 20, 55), {KIT_BREAKS, SR_ROCKBASS, SR_GTR, SR_PLUCK, SR_KEYS}, SR_STRINGS,
     0, 1, 45, 0, FM_PARTS, 50},
    {"SHOEGAZE", 100, 130, 0, {SC_MAJ, SC_LYD, SC_MIX, SC_MIN}, DM_SHOEGAZE, 100, KIT_SOFT, 0, HC_FLAT, FL_ROLL, 1,
     {BA_ROCK8, BA_PUMP8, BA_ROOT5}, 36, KM_DOUBLE, 90, {PR_POP, PR_I_IV, PR_I_II}, 2, CS_SWELL, VM_RICH,
     {AP_UPDOWN, AP_GLASS}, LD_SPARSE, AMT(75, 85, 100, 30, 40), {KIT_SOFT, SR_ROCKBASS, SR_WALL, SR_PLUCK, SR_VOXLEAD}, SR_DRONE,
     2, 0, 90, 0, FM_ARC, 40},
    {"POST ROCK", 84, 118, 0, {SC_MIN, SC_MAJ, SC_DOR, SC_MIN}, DM_INDIE, 85, KIT_BREAKS, 2, HC_FLAT, FL_TOM, 1,
     {BA_RADIO, BA_PUMP8, BA_FIFTHS}, 36, KM_SUSTAIN, 100, {PR_RADIO, PR_PYRAMID, PR_I_II}, 2, CS_SWELL, VM_PAD,
     {AP_GLASS, AP_UPDOWN}, LD_MOTIF, AMT(60, 75, 80, 75, 45), {KIT_BREAKS, SR_ROCKBASS, SR_EP, SR_GTR, SR_VOXLEAD}, SR_STRINGS,
     4, 0, 70, 0, FM_ARC, 45},   /* (0.6.95: after Radiohead: add9 / sus chords, a clean arpeggio, long roots) */
    /* ---- funk, disco, hip hop, UK */
    {"FUNK", 96, 112, 14, {SC_DOR, SC_MIX, SC_MPEN, SC_BLUE}, DM_FUNK, 100, KIT_BREAKS, 4, HC_FUNK, FL_LINEAR, 1,
     {BA_JAMERSON, BA_SLAP, BA_AROUND}, 33, KM_INTERLOCK, 40, {PR_DORIAN, PR_ONE, PR_I_IV}, 2, CS_COMP, VM_RICH,
     {AP_BROKEN, AP_WALK}, LD_HOOK, AMT(90, 100, 55, 20, 45), {KIT_BREAKS, SR_ROCKBASS, SR_EP, SR_PLUCK, SR_LEAD}, SR_ORGAN,
     0, 1, 30, 0, FM_PARTS, 60},
    {"DISCO", 114, 122, 8, {SC_MIN, SC_DOR, SC_MAJ, SC_MIN}, DM_DISCO, 100, KIT_BREAKS, 1, HC_HOUSE, FL_ROLL, 0,
     {BA_ITALO, BA_GOODTIME, BA_BOUNCE}, 36, KM_DOUBLE, 40, {PR_DORIAN, PR_I_IV, PR_JAZZ}, 2, CS_COMP, VM_RICH,
     {AP_UP, AP_BROKEN}, LD_HOOK, AMT(90, 100, 65, 35, 45), {KIT_BREAKS, SR_ROCKBASS, SR_STRINGS, SR_PLUCK, SR_LEAD}, SR_STRINGS,
     0, 1, 40, 0, FM_PARTS, 30},
    {"BOOM BAP", 86, 96, 16, {SC_MIN, SC_DOR, SC_MPEN, SC_MIN}, DM_BOOMBAP, 100, KIT_SOFT, 1, HC_FUNK, FL_NONE, 2,
     {BA_BOOMBAP, BA_JUNGLE, BA_DUB}, 33, KM_DOUBLE, 65, {PR_I_IV, PR_MINOR4, PR_I_VI}, 2, CS_SPARSE, VM_RICH,
     {AP_BROKEN, AP_WALK}, LD_SPARSE, AMT(90, 85, 60, 20, 35), {KIT_SOFT, SR_SUB, SR_EP, SR_KEYS, SR_BELL}, SR_TEXTURE,
     0, 1, 40, 0, FM_LOOP, 50},
    {"TRAP", 136, 146, 0, {SC_MIN, SC_PHRY, SC_HARM, SC_MIN}, DM_TRAP, 100, KIT_808, 0, HC_FLAT, FL_HAT, 0,
     {BA_TRAP, BA_WOBBLE, BA_DUB}, 29, KM_SUSTAIN, 110, {PR_I_VI, PR_ONE, PR_MINOR4}, 2, CS_SPARSE, VM_RICH,
     {AP_BROKEN, AP_UPDOWN}, LD_SPARSE, AMT(90, 100, 45, 40, 35), {KIT_808, SR_SUB, SR_PAD, SR_BELL, SR_BELL}, SR_CHOIR,
     2, 1, 55, 0, FM_PARTS, 0},
    {"UK GARAGE", 128, 134, 24, {SC_MIN, SC_DOR, SC_MIN, SC_MAJ}, DM_UKG, 100, KIT_909, 2, HC_GARAGE, FL_KICK16, 1,
     {BA_UKG, BA_SPEEDG, BA_DEEPWALK}, 33, KM_INTERLOCK, 45, {PR_I_IV, PR_JAZZ, PR_MINOR4}, 1, CS_BROKEN, VM_RICH,
     {AP_BROKEN, AP_SEQ}, LD_HOOK, AMT(90, 90, 60, 35, 45), {KIT_909, SR_REESE, SR_EP, SR_PLUCK, SR_VOXLEAD}, SR_TEXTURE,
     0, 1, 40, 0, FM_CLUB, 10},
    {"DUBSTEP", 138, 142, 0, {SC_MIN, SC_PHRY, SC_HARM, SC_MIN}, DM_DUBSTEP, 100, KIT_808, 1, HC_FLAT, FL_BREAK, 0,
     {BA_WOBBLE, BA_DUB, BA_TRAP}, 29, KM_SUSTAIN, 110, {PR_ONE, PR_I_VI, PR_MINOR4}, 2, CS_SPARSE, VM_SUS,
     {AP_PULSE, AP_BROKEN}, LD_SPARSE, AMT(85, 100, 40, 20, 30), {KIT_808, SR_REESE, SR_PAD, SR_PLUCK, SR_BELL}, SR_NOISE,
     2, 1, 55, 1, FM_CLUB, 0},
    {"BREAKS", 126, 140, 6, {SC_MIN, SC_DOR, SC_PHRY, SC_MIN}, DM_BREAKS, 100, KIT_BREAKS, 3, HC_FUNK, FL_LINEAR, 1,
     {BA_ACID, BA_JUNGLE, BA_AROUND}, 33, KM_INTERLOCK, 50, {PR_ONE, PR_I_IV, PR_I_VI}, 2, CS_STAB, VM_RICH,
     {AP_UPDOWN, AP_BROKEN}, LD_OSTINATO, AMT(95, 90, 45, 30, 40), {KIT_BREAKS, SR_ACID, SR_STAB, SR_PLUCK, SR_LEAD}, SR_NOISE,
     1, 1, 40, 0, FM_PARTS, 20},
    {"DRUM & BASS", 170, 176, 0, {SC_MIN, SC_DOR, SC_MIN, SC_MPEN}, DM_DNB, 100, KIT_BREAKS, 2, HC_FLAT, FL_LINEAR, 0,
     {BA_DNB, BA_JUNGLE, BA_WOBBLE}, 29, KM_DOUBLE, 90, {PR_ONE, PR_I_VI, PR_MINOR4}, 2, CS_SPARSE, VM_RICH,
     {AP_BROKEN, AP_CYCLE3}, LD_SPARSE, AMT(95, 95, 50, 20, 30), {KIT_BREAKS, SR_REESE, SR_PAD, SR_PLUCK, SR_LEAD}, SR_NOISE,
     2, 4, 50, 1, FM_CLUB, 0},
    /* ---- dub, reggae */
    {"DUB", 70, 80, 20, {SC_MIN, SC_MAJ, SC_DOR, SC_MIN}, DM_ONEDROP, 100, KIT_DUB, 1, HC_FLAT, FL_DUB, 1,
     {BA_ONEDROP, BA_DUB, BA_ONEDROP2}, 33, KM_SUSTAIN, 50, {PR_I_IV, PR_I_VI, PR_ONE}, 2, CS_SKANK, VM_PLAIN,
     {AP_PULSE, AP_BROKEN}, LD_SPARSE, AMT(80, 100, 60, 10, 25), {KIT_DUB, SR_SUB, SR_DUBCHORD, SR_ORGAN, SR_ORGAN}, SR_TEXTURE,
     1, 4, 80, 0, FM_ARC, 40},
    {"STEPPERS", 76, 88, 16, {SC_MIN, SC_MIN, SC_DOR, SC_MAJ}, DM_STEPPERS, 100, KIT_DUB, 1, HC_FLAT, FL_DUB, 1,
     {BA_STEPPERS, BA_ROCKERS, BA_ONEDROP2}, 33, KM_DOUBLE, 45, {PR_I_VII, PR_I_IV, PR_ONE}, 2, CS_SKANK, VM_PLAIN,
     {AP_PULSE, AP_UP}, LD_SPARSE, AMT(90, 100, 65, 15, 30), {KIT_DUB, SR_SUB, SR_ORGAN, SR_ORGAN, SR_ORGAN}, SR_TEXTURE,
     1, 4, 70, 0, FM_PARTS, 40},
    /* ---- jazz, latin */
    {"JAZZ", 110, 150, 40, {SC_MAJ, SC_DOR, SC_MIN, SC_MIX}, DM_JAZZ, 90, KIT_BREAKS, 2, HC_FLAT, FL_NONE, 1,
     {BA_WALK, BA_WALK2, BA_TWOFEEL}, 33, KM_SUSTAIN, 95, {PR_JAZZ, PR_PLAGAL, PR_I_III}, 1, CS_COMP, VM_RICH,
     {AP_WALK, AP_UPDOWN}, LD_MOTIF, AMT(60, 95, 75, 20, 55), {KIT_BREAKS, SR_ROCKBASS, SR_EP, SR_EP, SR_VOXLEAD}, SR_ORGAN,
     0, 1, 40, 0, FM_LOOP, 70},
    {"BOSSA", 120, 136, 6, {SC_MAJ, SC_LYD, SC_DOR, SC_MIN}, DM_BOSSA, 100, KIT_SOFT, 0, HC_FLAT, FL_NONE, 0,
     {BA_BOSSA, BA_TUMBAO, BA_TWOFEEL}, 33, KM_DOUBLE, 80, {PR_JAZZ, PR_I_II, PR_PLAGAL}, 2, CS_COMP, VM_RICH,
     {AP_ALBERTI, AP_UPDOWN}, LD_MOTIF, AMT(75, 90, 70, 30, 50), {KIT_SOFT, SR_ROCKBASS, SR_GTR, SR_EP, SR_SINEKEY}, SR_STRINGS,
     0, 1, 40, 0, FM_LOOP, 50},
    {"AFROBEAT", 112, 126, 10, {SC_DOR, SC_MIX, SC_MPEN, SC_MIN}, DM_AFRO, 100, KIT_BREAKS, 3, HC_FUNK, FL_LINEAR, 1,
     {BA_AROUND, BA_SKA, BA_ANGULAR}, 33, KM_INTERLOCK, 45, {PR_DORIAN, PR_ONE, PR_I_IV}, 2, CS_COMP, VM_RICH,
     {AP_BROKEN, AP_CYCLE3}, LD_HOOK, AMT(100, 100, 55, 40, 45), {KIT_BREAKS, SR_ROCKBASS, SR_ORGAN, SR_GTR, SR_LEAD}, SR_ORGAN,
     0, 1, 30, 0, FM_ARC, 50},
    /* ---- the night, the room */
    {"AMBIENT", 60, 90, 0, {SC_LYD, SC_MAJ, SC_MIN, SC_DOR}, DM_SPARSE, 25, KIT_SOFT, 0, HC_FLAT, FL_NONE, 0,
     {BA_PEDAL, BA_FIFTHS, BA_NEOARP}, 31, KM_SUSTAIN, 120, {PR_I_II, PR_I_IV, PR_SUS}, 4, CS_SWELL, VM_RICH,
     {AP_GLASS, AP_UPDOWN}, LD_TINTI, AMT(20, 50, 100, 40, 30), {KIT_SOFT, SR_SUB, SR_PAD, SR_BELL, SR_SINEKEY}, SR_DRONE,
     4, 0, 95, 0, FM_ARC, 20},
    {"NEO CLASSIC", 70, 112, 0, {SC_MIN, SC_DOR, SC_MAJ, SC_MEL}, DM_NONE, 0, KIT_SOFT, 0, HC_FLAT, FL_NONE, 0,
     {BA_NEOARP, BA_PEDAL, BA_FIFTHS}, 36, KM_SUSTAIN, 100, {PR_NEOCL, PR_CYCLE, PR_AEOL}, 1, CS_HOLD, VM_RICH,
     {AP_GLASS, AP_ALBERTI}, LD_TINTI, AMT(0, 60, 70, 90, 55), {KIT_SOFT, SR_SINEKEY, SR_STRINGS, SR_EP, SR_BELL}, SR_CHOIR,
     0, 1, 70, 0, FM_ARC, 30},
    {"IMPRESSION", 60, 92, 0, {SC_LYD, SC_MAJ, SC_LYD, SC_MPEN}, DM_SPARSE, 25, KIT_SOFT, 0, HC_FLAT, FL_NONE, 0,
     {BA_FIFTHS, BA_PEDAL, BA_NEOARP}, 36, KM_SUSTAIN, 110, {PR_I_II, PR_I_III, PR_SUS}, 2, CS_SWELL, VM_PLANE,
     {AP_UPDOWN, AP_GLASS}, LD_TINTI, AMT(15, 50, 85, 75, 40), {KIT_SOFT, SR_SINEKEY, SR_PAD, SR_EP, SR_BELL}, SR_CHOIR,
     4, 0, 90, 0, FM_ARC, 30},
    {"SOUNDTRACK", 64, 92, 0, {SC_MIN, SC_DOR, SC_MEL, SC_MPEN}, DM_SPARSE, 40, KIT_DUB, 0, HC_FLAT, FL_NONE, 0,
     {BA_PEDAL, BA_FIFTHS, BA_DUB}, 29, KM_SUSTAIN, 120, {PR_MINOR4, PR_I_VI, PR_ANDAL}, 2, CS_SWELL, VM_RICH,
     {AP_ALBERTI, AP_GLASS}, LD_SPARSE, AMT(30, 60, 100, 30, 35), {KIT_DUB, SR_SUB, SR_STRINGS, SR_BELL, SR_CHOIR}, SR_DRONE,
     4, 0, 90, 0, FM_ARC, 20},
    {"IDM", 86, 140, 10, {SC_PHRY, SC_HARM, SC_MIN, SC_DOR}, DM_IDM, 100, KIT_CLICK, 2, HC_FLAT, FL_KICK16, 0,
     {BA_JUNGLE, BA_ANGULAR, BA_DUST}, 33, KM_INTERLOCK, 50, {PR_MINOR4, PR_I_VI, PR_I_II}, 2, CS_SPARSE, VM_QUART,
     {AP_WALK, AP_CYCLE5}, LD_MOTIF, AMT(85, 70, 50, 60, 45), {KIT_CLICK, SR_FMBASS, SR_EP, SR_BELL, SR_CHIP}, SR_TEXTURE,
     4, 5, 60, 1, FM_PARTS, 10},
    {"BROKEN", 80, 100, 14, {SC_DOR, SC_MIN, SC_MEL, SC_DOR}, DM_BREAKS, 90, KIT_SOFT, 3, HC_FUNK, FL_LINEAR, 2,
     {BA_AROUND, BA_DUST, BA_JUNGLE}, 33, KM_INTERLOCK, 55, {PR_I_IV, PR_JAZZ, PR_MINOR4}, 2, CS_COMP, VM_QUART,
     {AP_WALK, AP_BROKEN}, LD_MOTIF, AMT(80, 85, 60, 30, 45), {KIT_SOFT, SR_FMBASS, SR_EP, SR_KEYS, SR_VOXLEAD}, SR_TEXTURE,
     4, 4, 65, 0, FM_PARTS, 50},
};
#define NGENRE (sizeof GENRE / sizeof GENRE[0])
