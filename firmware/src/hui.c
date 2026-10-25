/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* The chord machine user interface on the FM-1's 240 x 240 screen: HOME (the chord played, big; the key, the mode,
 * the tempo; the keys as a keyboard; the sound and its effects) and the menus, colour-coded, each a few pages of
 * four cards after JIANT (KNOB 1..4 the four cards): KEY (SCL, grey), SOUND (FX, yellow: the tone and the effects),
 * MIX (GLO: the levels, the tempo, the setup), LOOPER (SEQ) and PRESETS (SAVE: P1..P4); EDIT steps the play style,
 * whose own settings are HOME's knobs.
 *   SELECT    a menu's pages (PRESETS: the slot); on HOME the key
 *   a menu's  tapped: into its menu, on the page left there; tapped again: the next page, round (PRESETS the next
 *   button    slot, LOOPER back HOME); EDIT tapped: the next play style; held: its layer (keys and knobs)
 *   KNOB 1..4 on a menu its four cards; on HOME the style's four (HU_HOME_KNOBS: the filter and the envelope while
 *             it plays the sound, ARP its pattern, LOOPS its loop ..), named along the foot
 *   OCT- / +  the octave; on an armed action card (CLEAR, LOAD, SAVE) OCT+ does it, OCT- lets it be; both:
 *             RANDOMIZE (ALL; the sound on SOUND, the pattern on MODE)
 *   ALGORITHM the layer, PRESETS the sound: on every screen, their list shown while they turn
 *   ENV / LFO SOUND's AMP / WOBBLE page (again: HOME); ARP: the play style before (EDIT the next)
 * Included after ui.c (its sounds, messages and projects) and project.c; main.c runs hui_input, hui_leds and
 * hui_draw every frame. */

/* ------------------------------------------------------------ colours --- */
/* every colour from the palette (tools/gen_ui_palettes.py: the roles and the degrees), so a theme is one row there */
#define HC_GREY T_ROLE(UR_KEY)
#define HC_YELLOW T_ROLE(UR_SOUND)
#define HC_RED T_ROLE(UR_MODE)
#define HC_GREEN T_ROLE(UR_OK)
#define HC_BLUE T_ROLE(UR_LOOP)
#define HC_INK T_INK

/* ------------------------------------------------------------ sounds --- */
typedef struct { const char *name; uint8_t engine, preset, env, level; } hc_sound_t;   /* level: P_LEVEL, 0 = the default */
#define HCE ((uint8_t)ENGI_HC)
static const hc_sound_t HC_SOUNDS[] = {
    {"SINE", HCE, 0, HE_LONG},      {"SAW", HCE, 1, HE_LONG},        {"TRIANGLE", HCE, 2, HE_LONG},
    {"SQUARE", HCE, 3, HE_LONG},    {"E.PIANO", HCE, 4, HE_KEYS},    {"HX7 PIANO", HCE, 5, HE_KEYS},
    {"FM BELL", HCE, 6, HE_KEYS},   {"FM ORGAN", HCE, 7, HE_SUSTAIN}, {"FM BRASS", HCE, 8, HE_SUSTAIN},
    {"STRINGS", 0, 11, HE_LONG},    {"CLARINET", 0, 10, HE_SUSTAIN}, {"CELLOS", 2, 2, HE_LONG},
    {"ACOUSTIC", 9, 2, HE_KEYS},    {"BRASS", 0, 9, HE_SUSTAIN},     {"PIANO", 4, 0, HE_KEYS},
    {"VIBES", 9, 1, HE_KEYS},       {"VIOLINS", 0, 3, HE_LONG},      {"VOX AHH", 5, 0, HE_LONG},
    {"SAX", 4, 3, HE_SUSTAIN},      {"HARP", 9, 8, HE_KEYS},         {"HUMMING", 5, 1, HE_LONG},
    {"SYNTH BASS", 6, 0, HE_SHORT}, {"ARCADE", 11, 2, HE_SHORT},     {"FLUTE", 4, 2, HE_SUSTAIN},
    {"SAW SQUARE", HCE, 9, HE_LONG}, {"JUNO POLY", HCE, 10, HE_LONG}, {"OCEAN PAD", HCE, 11, HE_SWELL},
    {"WOBBLE BASS", HCE, 12, HE_SHORT}, {"BUZZ ORGAN", 7, 4, HE_SUSTAIN}, {"ORGAN", 7, 0, HE_SUSTAIN},
    {"HORNS", 2, 0, HE_SUSTAIN},    {"E GUITAR", 6, 2, HE_KEYS},     {"KALIMBA", 9, 4, HE_KEYS},
    {"SAW BASS", 0, 7, HE_SHORT},   {"SHIMMER", 8, 3, HE_SWELL},     {"GOSPEL ORGAN", 7, 2, HE_SUSTAIN},
    {"PURE SINE", HCE, 13, HE_LONG},
    {"ACOUSTIC GTR", 9, 9, HE_KEYS},                    /* fm1-chord: a plucky guitar for LEAD (PHYS) */
    /* FM6, the six-operator FM with DX7 patches (eng_fm6.c FM6_PRESETS): its own envelopes shape the notes, so ENV,
     * ATTACK and RELEASE do nothing on these. The quieter ones raised (0.5 dB a step) as far as a held chord's peaks
     * stay where the other sounds' are */
    {"DX TINE EP", ENGI_FM6, 0, HE_KEYS},       {"DX BELL", ENGI_FM6, 1, HE_KEYS, 109},  {"DX BASS", ENGI_FM6, 2, HE_SHORT, 108},
    {"DX BRASS", ENGI_FM6, 3, HE_SUSTAIN},      {"DX PAD", ENGI_FM6, 4, HE_LONG, 112},  {"DX MARIMBA", ENGI_FM6, 5, HE_KEYS, 114},
    {"DX ORGAN", ENGI_FM6, 6, HE_SUSTAIN},      {"DX PLUCK", ENGI_FM6, 7, HE_KEYS, 112},
};
#define HC_NSOUNDS (sizeof HC_SOUNDS / sizeof HC_SOUNDS[0])
/* the chord machine's ten scales (CC 103 order) -> Felucca's SCALE values */
static const uint8_t HC_SCALE_V[10] = {1, 2, 7, 11, 5, 6, 12, 3, 4, 9};
static const char *const HC_SCALE_NAME[10] = {"MAJOR", "MINOR", "HARM MIN", "MELOD MIN", "MAJ PEN", "MIN PEN", "BLUES",
                                              "DORIAN", "MIXOLYD", "LYDIAN"};
static const char *const HC_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const char *const HC_DEG_NAME[7] = {"I", "ii", "iii", "IV", "V", "vi", "vii"};
static const char *const HC_DIR_NAME[8] = {"UP", "UP-RIGHT", "RIGHT", "DOWN-RIGHT", "DOWN", "DOWN-LEFT", "LEFT", "UP-LEFT"};
static const char *const HP_SHORT[HP_COUNT] = {"PLAY", "STRUM", "LEAD", "DRONE", "ARP", "REPEAT", "SEQ", "DRUM", "LOOPS", "HIRO",
                                               "EAR", "MIXER"};   /* (the header band) */

/* ------------------------------------------------------------- state --- */
enum { HU_HOME, HU_KEY, HU_SOUND, HU_MODE, HU_PRESET, HU_LOOP, HU_PICK, HU_MIX, HU_N };   /* HU_PICK: a list (hui_pick_*) */
static const uint8_t HU_ROLE[HU_N] = {0, UR_KEY, UR_SOUND, UR_MODE, UR_PRESET, UR_LOOP, 0, UR_LOOP};
#define HU_COL(sc) T_ROLE(HU_ROLE[sc])
/* the play styles a layer can have (EDIT held + a white key, the MODE menu), in the chord machine's order; MIXER
 * is a screen now (GLO), not a style */
static const uint8_t HU_STYLE[] = {HP_PLAY, HP_STRUM, HP_LEAD, HP_DRONE, HP_ARP, HP_REPEAT, HP_SEQ, HP_DRUM, HP_DRUMLOOP,
                                   HP_HIRO, HP_EAR};
#define HU_NSTYLES ((uint32_t)sizeof HU_STYLE)
/* each layer's own colour (its chip, its row): four of the palette's degree colours */
#define HU_LCOL(i) T_DEG((uint32_t)"\x04\x03\x01\x06"[(i) & 3u])

/* ------------------------------------------------------ pages, cards --- */
/* After JIANT: a menu is one chain of pages (in groups, the tabs), a page four cards, KNOB 1..4 the four. A card
 * has a name, a value and a turn (hui_card_*); an action card (CLEAR, LOAD, SAVE) is armed by turning it right and
 * done by OCT+. A play style's own settings are no menu's: they are HOME's knobs (HU_HOME_KNOBS) */
enum { HK_NONE,
       HK_KEY, HK_SCALE, HK_OCT, HK_LAYOUT, HK_BASS, HK_VOICES, HK_VLEAD, HK_JOY,
       HK_FILT, HK_CUT, HK_RES, HK_HP, HK_ENV, HK_ATK, HK_REL, HK_GLIDE,
       HK_REV, HK_REV_AMT, HK_DLY, HK_DLY_AMT, HK_CHO, HK_CHO_AMT, HK_FLG, HK_FLG_AMT, HK_TREM, HK_TREM_AMT,
       HK_LFO, HK_STEREO, HK_DRIVE, HK_TAPE,
       HK_STRUM, HK_APAT, HK_ARATE, HK_ALAYER, HK_SEQLEN, HK_KIT, HK_DLSTYLE, HK_DLVAR, HK_SONG, HK_DIFF,
       HK_SPEED, HK_LEVEL,
       HK_L1, HK_L2, HK_L3, HK_L4, HK_BPM, HK_METRO, HK_SPK, HK_OUT, HK_MIDIIN, HK_THEME,
       HK_BARS, HK_CLEAR, HK_SLOT, HK_LOAD, HK_SAVE, HK_N };
static const char *const HK_NAME[HK_N] = {"",
    "KEY", "SCALE", "OCTAVE", "LAYOUT", "BASS", "VOICES", "V.LEAD", "JOYSTK",
    "WHEEL", "CUTOFF", "RESO", "HI-PASS", "ENV", "ATTACK", "RELEASE", "GLIDE",
    "REVERB", "AMOUNT", "DELAY", "AMOUNT", "CHORUS", "AMOUNT", "FLANGER", "AMOUNT", "TREM", "AMOUNT",
    "VIBRATO", "STEREO", "DRIVE", "TAPE",
    "SPEED", "PATTERN", "RATE", "LAYER", "LENGTH", "KIT", "LOOP", "VARIANT", "SONG", "DIFF",
    "SPEED", "LEVEL",
    "L1", "L2", "L3", "L4", "TEMPO", "METRO", "SPEAKER", "OUTPUT", "MIDI IN", "THEME",
    "BARS", "CLEAR", "SLOT", "LOAD", "SAVE"};
/* the cards that are no one layer's (the key, the sequencer, the games, the mix, the setup, the looper, the
 * presets); the rest are the selected layer's, and a page with one of those names it in the header */
static int hui_card_global(uint32_t k)
{
    return k <= HK_OCT || k == HK_SEQLEN || (k >= HK_SONG && k <= HK_LEVEL) || k >= HK_L1;
}
static int hui_card_action(uint32_t k) { return k == HK_CLEAR || k == HK_LOAD || k == HK_SAVE; }
typedef struct { const char *name; uint8_t group, card[4]; } hu_page_t;
static const hu_page_t HU_PG_KEY[] = {{"KEY", 0, {HK_KEY, HK_SCALE, HK_OCT, HK_LAYOUT}},
                                      {"CHORD", 1, {HK_BASS, HK_VOICES, HK_VLEAD, HK_JOY}}};
static const hu_page_t HU_PG_SOUND[] = {{"FILTER", 0, {HK_FILT, HK_CUT, HK_RES, HK_HP}},
                                        {"AMP", 0, {HK_ENV, HK_ATK, HK_REL, HK_GLIDE}},
                                        {"SPACE", 1, {HK_REV, HK_REV_AMT, HK_DLY, HK_DLY_AMT}},
                                        {"MOTION", 1, {HK_CHO, HK_CHO_AMT, HK_FLG, HK_FLG_AMT}},
                                        {"WOBBLE", 1, {HK_TREM, HK_TREM_AMT, HK_LFO, HK_STEREO}},
                                        {"COLOUR", 1, {HK_DRIVE, HK_TAPE, 0, 0}}};
enum { HU_PG_AMP = 1, HU_PG_WOBBLE = 4 };              /* (the ENV and LFO buttons' pages) */
static const hu_page_t HU_PG_MIX[] = {{"MIX", 0, {HK_L1, HK_L2, HK_L3, HK_L4}},
                                      {"TEMPO", 1, {HK_BPM, HK_METRO, 0, 0}},
                                      {"SETUP", 2, {HK_SPK, HK_OUT, HK_MIDIIN, HK_THEME}}};
static const hu_page_t HU_PG_LOOP[] = {{"LOOPER", 0, {HK_BARS, 0, 0, HK_CLEAR}}};
static const hu_page_t HU_PG_PRESET[] = {{"PRESETS", 0, {HK_SLOT, HK_LOAD, 0, HK_SAVE}}};
/* HOME's four knobs, the style's: the sound's filter and envelope where the style plays the sound, else what the
 * style has (ARP its pattern, LOOPS its loop, HIRO its song ..), the tempo where it runs to the clock */
static const uint8_t HU_HOME_KNOBS[HP_COUNT][4] = {
    [HP_PLAY] = {HK_CUT, HK_RES, HK_ATK, HK_REL},        [HP_STRUM] = {HK_CUT, HK_RES, HK_STRUM, HK_REL},
    [HP_LEAD] = {HK_CUT, HK_RES, HK_ATK, HK_REL},        [HP_DRONE] = {HK_CUT, HK_RES, HK_ATK, HK_REL},
    [HP_ARP] = {HK_APAT, HK_ARATE, HK_ALAYER, HK_CUT},   [HP_REPEAT] = {HK_ARATE, HK_CUT, HK_RES, HK_REL},
    [HP_SEQ] = {HK_SEQLEN, HK_CUT, HK_RES, HK_BPM},      [HP_DRUM] = {HK_KIT, HK_CUT, HK_RES, HK_BPM},
    [HP_DRUMLOOP] = {HK_DLSTYLE, HK_DLVAR, HK_KIT, HK_BPM}, [HP_HIRO] = {HK_SONG, HK_DIFF, HK_SPEED, 0},
    [HP_EAR] = {HK_LEVEL, 0, 0, 0},                      [HP_MIXER] = {HK_L1, HK_L2, HK_L3, HK_L4}};
typedef struct { const char *title; const hu_page_t *pg; uint8_t n; const char *group[3]; } hu_sec_t;
static const hu_sec_t HU_SEC[HU_N] = {
    [HU_KEY] = {"KEY", HU_PG_KEY, 2, {"KEY", "CHORD"}},
    [HU_SOUND] = {"SOUND", HU_PG_SOUND, 6, {"TONE", "EFFECTS"}},
    [HU_PRESET] = {"PRESETS", HU_PG_PRESET, 1, {"PRESETS"}},
    [HU_LOOP] = {"LOOPER", HU_PG_LOOP, 1, {"LOOPER"}},
    [HU_MIX] = {"MIX", HU_PG_MIX, 3, {"MIX", "TEMPO", "SETUP"}}};
static int hui_paged(uint32_t sc) { return sc < HU_N && HU_SEC[sc].title != 0; }
static const char *const HC_OUT_NAME[2] = {"HOT", "LINE"};
static const char *const HC_SPK_NAME[3] = {"FLAT", "LOWCUT", "BASS+"};

static struct {
    uint8_t screen;              /* HU_* */
    uint8_t page[HU_N];          /* the page each menu was left on */
    uint8_t slot;                /* PRESETS: P1..P4 */
    uint8_t armed;               /* the action card OCT+ does (HK_*; 0: none) */
    uint32_t sig[4];             /* drawn-state caches: header, body, strip, footer */
    uint8_t force;
    char msg[20];
    uint8_t msg_t;
    uint32_t home_t0;            /* HOME held (btn_hold) */
    uint32_t rec_t0;             /* REC held: clear the layer */
    uint32_t tap_ms[4];          /* tap tempo: the taps (GLO held, key 16) */
    uint8_t ntap;
    uint8_t midi_in;             /* the MIDI IN setting (chordmachine.c reads it) */
    uint8_t out_line;            /* OUT LEVEL: LINE (-10 dB) */
    uint32_t rnd;
    uint8_t preset_used[4];      /* P1..P4 hold something */
    uint8_t pick_kind;           /* HU_PICK: PK_* */
    uint8_t pick_back;           /* the screen under the list (it goes 1.5 s after the last turn) */
    uint32_t pick_ms;            /* the last turn */
    uint8_t ly;                  /* HL_*: the menu button held as a layer (0: none) */
    uint8_t ly_open;             /* its map shows */
    uint8_t ly_used;             /* a key, a knob or a button went with it: no tap when it is let go */
    uint8_t ly_page;             /* SOUND held: the page of 16 sounds on the keys */
    uint32_t ly_ms;              /* when it went down */
    uint32_t ly_seen;            /* the layer's keys already taken (seq.c kb_layer) */
} hui = {.rnd = 0x9E3779B9u};
static void hui_mode_set(track_t *t, uint32_t play);   /* below: the mode, with the drum engine swap */
static void hui_say(const char *s)
{
    str_cpy(hui.msg, s, sizeof hui.msg);
    hui.msg_t = 70;
}
static uint32_t hui_rand(uint32_t n)
{
    uint32_t r = hui.rnd;
    r ^= r << 13; r ^= r >> 17; r ^= r << 5;
    hui.rnd = r;
    return n ? (r >> 8) % n : 0u;
}

/* the live track's chord machine settings */
static hc_trk_t *hui_c(void) { return &hc.t[song.sel]; }
static void hui_apply(void) { hc_apply(TSEL); }
static int hui_card(uint32_t k, char *b, int32_t *frac);   /* (below) */
/* a knob turned: its name and value in the header bar for a moment ("ATTACK 790ms") */
static void hui_say_value(const char *label, uint32_t k)
{
    char v[20];
    int32_t f;
    hui_card(k, v, &f);
    str_cpy(hui.msg, label, sizeof hui.msg);
    str_cpy(hui.msg + str_len(hui.msg), " ", sizeof hui.msg - str_len(hui.msg));
    str_cpy(hui.msg + str_len(hui.msg), v, sizeof hui.msg - str_len(hui.msg));
    hui.msg_t = 70;
}

/* the effects with an amount: REVERB, DELAY, CHORUS (their sends), FLANGER (its wet), TREMOLO (its depth), each
 * its type card (HK_REV ..) and its AMOUNT card after it. The amount in effect: the one set, else the type's own */
static uint32_t hui_fx_amount(const hc_trk_t *c, uint32_t k)
{
    static const uint8_t REV_DEF[HRV_COUNT] = {0, 50, 65, 60, 60, 90}, CHO_DEF[HCH_COUNT] = {0, 40, 60, 85, 110};
    switch (k) {
    case HK_REV: return c->rev_amt ? c->rev_amt : REV_DEF[c->rev % HRV_COUNT];
    case HK_DLY: return c->dly_amt ? c->dly_amt : 55u;
    case HK_CHO: return c->cho_amt ? c->cho_amt : CHO_DEF[c->cho % HCH_COUNT];
    case HK_FLG: return c->flg_amt ? c->flg_amt : 64u;
    default: return c->trem_amt ? c->trem_amt : 90u;
    }
}
static uint8_t *hui_fx_amount_of(hc_trk_t *c, uint32_t k)
{
    return k == HK_REV ? &c->rev_amt : k == HK_DLY ? &c->dly_amt : k == HK_CHO ? &c->cho_amt :
           k == HK_FLG ? &c->flg_amt : &c->trem_amt;
}
static uint8_t *hui_fx_on(hc_trk_t *c, uint32_t k)
{
    return k == HK_REV ? &c->rev : k == HK_DLY ? &c->dly : k == HK_CHO ? &c->cho : k == HK_FLG ? &c->flg : &c->trem;
}
/* an AMOUNT card turned (an effect that is OFF comes on at its first type) */
static void hui_fx_turn(hc_trk_t *c, uint32_t k, int32_t s)
{
    uint8_t *amt = hui_fx_amount_of(c, k), *on = hui_fx_on(c, k);
    if (!*on)
        *on = 1;
    *amt = (uint8_t)clamp((int32_t)hui_fx_amount(c, k) + s * 4, 1, 127);
}

/* the key (ROOT) and the scale are the chord machine's: global, every track's */
static void hui_key_set(int32_t root)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++)
        trk[i].p[P_ROOT] = (int16_t)((root % 12 + 12) % 12);
}
static uint32_t hui_scale_get(void)                     /* the chord machine scale index of the tracks' SCALE */
{
    uint32_t i;
    for (i = 0; i < 10u; i++)
        if (HC_SCALE_V[i] == TSEL->p[P_SCALE])
            return i;
    return 0;
}
static void hui_scale_set(uint32_t s)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++)
        trk[i].p[P_SCALE] = HC_SCALE_V[s % 10u];
}

/* a sound of the list onto track t: its engine, its preset (ui.c), its envelope; the chord machine settings on top */
static void hc_sound_load(track_t *t, uint32_t idx)
{
    const hc_sound_t *s = &HC_SOUNDS[idx % HC_NSOUNDS];
    hc_trk_t *c = hc_of(t);
    c->sound = (uint8_t)(idx % HC_NSOUNDS);
    c->env = s->env;
    c->atk = c->rel = 0;
    set_engine_of(t, s->engine);
    if (s->preset)
        apply_preset_to(t, s->preset);
    t->p[P_LEVEL] = (int16_t)(s->level ? s->level : s->engine == ENGI_PHYS ? 127 : TP[P_LEVEL].def);   /* the physical models sit
                                                          * ~12 dB under the rest (3 voices): full level, no trim (fx.c) */
    hc_apply(t);
}

/* the chord machine UI takes a track: its chord keys on, its sound loaded */
static void hui_track_init(track_t *t)
{
    hc_trk_t *c = hc_of(t);
    if (t->p[P_CHRD] != CH_HI || !hc_on(t)) {
        hc_sound_load(t, c->sound);
        hc_apply(t);
    }
}

/* ----------------------------------------------------- presets P1..P4 --- */
/* a preset's own state packed into HC_PRESET_BYTES (settings_persist.c hc_presets): magic, the tracks' settings,
 * the key inversions (2 bits a key) and locks, MIDI IN and OUT LEVEL. Saving: the project slot (ui.c project_save:
 * sounds, key, tempo, the Felucca parameters) and this; loading: both, then hc_apply. HCP3 (a byte an inversion)
 * packed 262 bytes into the 256 of the record: the last 6 (the last two locks, MIDI IN, OUT LEVEL) were never
 * stored, and read back as 0 */
#define HCP_MAGIC 0x48435034u                           /* "HCP4": the inversions 2 bits a key */
#define HCP3_MAGIC 0x48435033u
#define HCP_INV_BYTES ((27u + 3u) / 4u)
_Static_assert(4u + NTRK * sizeof(hc_trk_t) + HCP_INV_BYTES + 27u * 3u + 2u <= HC_PRESET_BYTES,
               "a chord machine preset fits its record");
static void hc_preset_pack(uint8_t *b)
{
    uint32_t i, k = 4;
    memset(b, 0, HC_PRESET_BYTES);
    memcpy(b, &(uint32_t){HCP_MAGIC}, 4);
    for (i = 0; i < NTRK; i++, k += sizeof(hc_trk_t))
        memcpy(b + k, &hc.t[i], sizeof(hc_trk_t));
    for (i = 0; i < 27u; i++)
        b[k + i / 4u] |= (uint8_t)((hc.inv[i] % 3u) << (2u * (i % 4u)));
    k += HCP_INV_BYTES;
    for (i = 0; i < 27u; i++) {
        b[k++] = hc.lock[i].on;
        b[k++] = hc.lock[i].q;
        b[k++] = (uint8_t)hc.lock[i].roff;
    }
    b[k++] = hui.midi_in;
    b[k++] = hui.out_line;
}
static int hc_preset_unpack(const uint8_t *b)
{
    uint32_t m, i, k = 4, v3;
    memcpy(&m, b, 4);
    if (m != HCP_MAGIC && m != HCP3_MAGIC)
        return 0;
    v3 = m == HCP3_MAGIC;
#define HCP_AT(j) ((j) < HC_PRESET_BYTES ? b[j] : 0u)  /* (HCP3: past the record, 0) */
    for (i = 0; i < NTRK; i++, k += sizeof(hc_trk_t))
        memcpy(&hc.t[i], b + k, sizeof(hc_trk_t));
    for (i = 0; i < 27u; i++)
        hc.inv[i] = (uint8_t)((v3 ? b[k + i] : b[k + i / 4u] >> (2u * (i % 4u)) & 3u) % 3u);
    k += v3 ? 27u : HCP_INV_BYTES;
    for (i = 0; i < 27u; i++, k += 3u) {
        hc.lock[i].on = (uint8_t)(HCP_AT(k) & 1u);
        hc.lock[i].q = (uint8_t)(HCP_AT(k + 1u) % HQ_COUNT);
        hc.lock[i].roff = (int8_t)HCP_AT(k + 2u);
    }
    hui.midi_in = HCP_AT(k) & 1u;
    hui.out_line = HCP_AT(k + 1u) & 1u;
#undef HCP_AT
    return 1;
}
/* The live state across power-off, as the chord machine keeps its sound, effects, mode, inversions and locks
 * (key, scale, octave and tempo are Felucca's song and reset, as on the chord machine): packed like a preset
 * into hc_live (settings_persist.c, PER6), saved with the settings 3 s after the last change (one flash
 * erase per edit session, not per knob click), restored at boot (main.c) */
static struct { uint32_t at_ms, poll_ms, loop_sig; uint8_t dirty; } hui_live;
static void hui_live_restore(void)
{
    uint32_t i;
    if (hcl_boot_loop)                                   /* the loop as it played, paused: PLAY goes on with it */
        hui_say("LOOP BACK: PLAY");
    if (!hc_preset_unpack(hc_live))
        return;
    for (i = 0; i < NTRK; i++)
        hc_play_set(&trk[i], hc.t[i].play == HP_MIXER ? HP_PLAY : hc.t[i].play);   /* (MIXER: a screen now) */
}
static void hui_live_poll(void)
{
    uint8_t b[HC_PRESET_BYTES];
    if ((uint32_t)(fm1_ms - hui_live.poll_ms) < 500u)
        return;
    hui_live.poll_ms = fm1_ms;
    hc_preset_pack(b);
    if (!hcl_count(HLS_REC) && !hcl_count(HLS_ARMED)) {  /* the loop too, once no take is under way (packed into
                                                          * the record when it is written: settings_persist.c) */
        uint32_t sig = hcl_sig();
        if (sig != hui_live.loop_sig) {
            hui_live.loop_sig = sig;
            hui_live.at_ms = fm1_ms;
            hui_live.dirty = 1;
        }
    }
    if (memcmp(b, hc_live, sizeof b)) {
        memcpy(hc_live, b, sizeof b);
        hui_live.at_ms = fm1_ms;
        hui_live.dirty = 1;
    } else if (hui_live.dirty && (uint32_t)(fm1_ms - hui_live.at_ms) >= 3000u) {
        hui_live.dirty = 0;
        settings_save();
    }
}
static void hui_preset_scan(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        hui.preset_used[i] = (uint8_t)project_used(i);
}
static void hui_preset_save(uint32_t slot)
{
    char b[16] = "SAVED P";
    if (chain_busy() || song.playing) {
        hui_say("STOP TO SAVE");
        return;
    }
    hc_preset_pack(hc_presets[slot & 3u]);
    if (project_save(slot & 3u) != 0) {
        hui_say("SAVE FAILED");
        return;
    }
#if FELUCCA_FLASH
    if (flash_ok) {                                      /* the loop too, as it plays (none: an empty record, so
                                                          * the slot brings no older loop back) */
        uint8_t lp[HCL_PACK_BYTES];
        hcl_pack(lp);
        if (st_save(OBJ_LOOP0 + (slot & 3u), lp, sizeof lp) != 0) {
            hui_say("LOOP NOT SAVED");
            return;
        }
    }
#endif
    settings_save();
    hui_preset_scan();
    b[7] = (char)('1' + (slot & 3u));
    b[8] = 0;
    hui_say(b);
}
static void hui_preset_load(uint32_t slot)
{
    char b[20] = "PRESET P";
    uint32_t i;
    if (!project_used(slot & 3u)) {
        hui_say("EMPTY");
        return;
    }
    project_load(slot & 3u);
    if (!hc_preset_unpack(hc_presets[slot & 3u]))
        for (i = 0; i < NTRK; i++)
            hc_trk_defaults(&hc.t[i]);
    for (i = 0; i < NTRK; i++)                          /* (hc_apply on every track; a drum mode's engine is the
                                                          * project's; MIXER: a screen now) */
        hc_play_set(&trk[i], hc.t[i].play == HP_MIXER ? HP_PLAY : hc.t[i].play);
    b[8] = (char)('1' + (slot & 3u));
    b[9] = 0;
#if FELUCCA_FLASH
    if (flash_ok) {                                      /* its loop, if it has one, in place of the one playing
                                                          * (paused: PLAY); without one the loop stays */
        uint8_t lp[HCL_PACK_BYTES];
        if (st_load(OBJ_LOOP0 + (slot & 3u), lp, sizeof lp) == (int)sizeof lp && hcl_unpack(lp))
            str_cpy(b + 9, " + LOOP", sizeof b - 9u);
    }
#endif
    hui_say(b);
}

/* --------------------------------------------------------- randomize --- */
static void hui_random_sound(void)
{
    hc_trk_t *c = hui_c();
    hc_sound_load(TSEL, hui_rand(HC_NSOUNDS));
    c->env = (uint8_t)hui_rand(HE_COUNT);
    c->rev = (uint8_t)(hui_rand(3) ? hui_rand(HRV_COUNT) : 0);
    c->dly = (uint8_t)(hui_rand(3) == 0 ? hui_rand(HDL_COUNT) : 0);
    c->cho = (uint8_t)(hui_rand(2) ? hui_rand(HCH_COUNT) : 0);
    c->flg = (uint8_t)(hui_rand(4) == 0 ? hui_rand(HFL_COUNT) : 0);
    c->trem = (uint8_t)(hui_rand(4) == 0 ? hui_rand(HTR_COUNT) : 0);
    c->lfo = (uint8_t)(hui_rand(3) == 0 ? hui_rand(HLF_COUNT) : 0);
    c->drive = (uint8_t)(hui_rand(4) == 0 ? hui_rand(HDR_COUNT) : 0);
    c->tape = (uint8_t)(hui_rand(5) == 0 ? hui_rand(HTP_COUNT) : 0);
    c->glide = 0;
    hui_apply();
    hui_say("RANDOMIZED SOUND");
}
static void hui_random_pattern(void)
{
    hc_trk_t *c = hui_c();
    c->arp_pat = (uint8_t)hui_rand(HA_COUNT);
    c->arp_rate = (uint8_t)(HR_1_4 + hui_rand(5));
    c->arp_layer = (uint8_t)hui_rand(3);
    c->strum = (uint8_t)hui_rand(3);
    hui_apply();
    hui_say("RANDOMIZED PATTERN");
}
static void hui_random_all(void)
{
    hc_trk_t *c = hui_c();
    hui_key_set((int32_t)hui_rand(12));
    hui_scale_set(hui_rand(10));
    song.g[G_BPM] = (int16_t)(70 + hui_rand(110));
    c->mode = (uint8_t)hui_rand(HM_COUNT);
    c->bass = (uint8_t)hui_rand(3);
    c->voices = (uint8_t)hui_rand(2);
    hui_random_sound();
    hui_random_pattern();
    hui_mode_set(TSEL, hui_rand(HP_REPEAT + 1u));
    hui_say("RANDOMIZED ALL");
}

/* ------------------------------------------------------- the values --- */
/* the value of a row as text; 1 = the row is an action */
static uint32_t hui_cycle(uint32_t v, int32_t d, uint32_t n) { return (uint32_t)(((int32_t)v + d) % (int32_t)n + (int32_t)n) % n; }
static uint32_t hui_style_index(uint32_t play)
{
    uint32_t i;
    for (i = 0; i < HU_NSTYLES; i++)
        if (HU_STYLE[i] == play)
            return i;
    return 0;
}

static const hu_page_t *hui_page_of(uint32_t sc, uint32_t i) { return &HU_SEC[sc].pg[i < HU_SEC[sc].n ? i : 0u]; }
static const hu_page_t *hui_page(void) { return hui_page_of(hui.screen, hui.page[hui.screen]); }

/* a time parameter's value as Felucca shows it ("790ms") */
static void hui_time(char *b, uint32_t p)
{
    const char *u;
    param_format(&TP[p], TSEL->p[p], b, &u);
    str_cpy(b + str_len(b), u, 16 - str_len(b));
}
/* a value out of its range for a card's gauge (0..1000), or -1: no gauge */
static int32_t hui_frac(int32_t v, int32_t lo, int32_t hi) { return hi > lo ? (int32_t)((v - lo) * 1000 / (hi - lo)) : -1; }

/* a card's value into b ("HALL", "790ms", "65"), its gauge into *frac; 1 = an action card */
static int hui_card(uint32_t k, char *b, int32_t *frac)
{
    const hc_trk_t *c = hui_c();
    b[0] = 0;
    *frac = -1;
    switch (k) {
    case HK_KEY: str_cpy(b, HC_NOTE_NAME[(uint32_t)TSEL->p[P_ROOT] % 12u], 16); break;
    case HK_SCALE: str_cpy(b, HC_SCALE_NAME[hui_scale_get()], 16); break;
    case HK_OCT: str_cpy(b, song.octave > 0 ? "+" : "", 16); fmt_int(b + str_len(b), song.octave); *frac = hui_frac(song.octave, -2, 2); break;
    case HK_LAYOUT: str_cpy(b, HL_NAME[c->layout % 2u], 16); break;
    case HK_BASS: str_cpy(b, HB_NAME[c->bass % 3u], 16); break;
    case HK_VOICES: str_cpy(b, HV_NAME[c->voices % 4u], 16); break;
    case HK_VLEAD: str_cpy(b, c->vlead ? "ON" : "OFF", 16); break;
    case HK_JOY: str_cpy(b, HM_NAME[c->mode % HM_COUNT], 16); break;
    case HK_FILT: str_cpy(b, c->filt ? "ON" : "OFF", 16); break;
    case HK_CUT: {
        const char *u;
        param_format(&(param_desc_t){"CUT", F_CUTOFF, 0, 127, 127, 0, 0}, c->cutoff, b, &u);
        str_cpy(b + str_len(b), u, 16 - str_len(b));
        *frac = hui_frac(c->cutoff, 0, 127);
        break;
    }
    case HK_RES: fmt_int(b, c->res); *frac = hui_frac(c->res, 0, 127); break;
    case HK_HP: str_cpy(b, c->hp ? "ON" : "OFF", 16); break;
    case HK_ENV: str_cpy(b, HE_NAME[c->env % HE_COUNT], 16); break;
    case HK_ATK: hui_time(b, P_ATK); *frac = hui_frac(TSEL->p[P_ATK], TP[P_ATK].min, TP[P_ATK].max); break;
    case HK_REL: hui_time(b, P_REL); *frac = hui_frac(TSEL->p[P_REL], TP[P_REL].min, TP[P_REL].max); break;
    case HK_GLIDE: str_cpy(b, HGL_NAME[c->glide % HGL_COUNT], 16); break;
    case HK_REV: str_cpy(b, HRV_NAME[c->rev % HRV_COUNT], 16); break;
    case HK_DLY: str_cpy(b, HDL_NAME[c->dly % HDL_COUNT], 16); break;
    case HK_CHO: str_cpy(b, HCH_NAME[c->cho % HCH_COUNT], 16); break;
    case HK_FLG: str_cpy(b, HFL_NAME[c->flg % HFL_COUNT], 16); break;
    case HK_TREM: str_cpy(b, HTR_NAME[c->trem % HTR_COUNT], 16); break;
    case HK_REV_AMT: case HK_DLY_AMT: case HK_CHO_AMT: case HK_FLG_AMT: case HK_TREM_AMT:
        if (!*hui_fx_on((hc_trk_t *)c, k - 1u)) {        /* (the type's card is just before its AMOUNT) */
            str_cpy(b, "-", 16);
        } else {
            fmt_int(b, (int32_t)hui_fx_amount(c, k - 1u));
            *frac = hui_frac((int32_t)hui_fx_amount(c, k - 1u), 0, 127);
        }
        break;
    case HK_LFO: str_cpy(b, HLF_NAME[c->lfo % HLF_COUNT], 16); break;
    case HK_STEREO: str_cpy(b, c->stereo ? "ON" : "OFF", 16); break;
    case HK_DRIVE: str_cpy(b, HDR_NAME[c->drive % HDR_COUNT], 16); break;
    case HK_TAPE: str_cpy(b, HTP_NAME[c->tape % HTP_COUNT], 16); break;
    case HK_STRUM: str_cpy(b, HST_NAME[c->strum % 3u], 16); break;
    case HK_APAT: str_cpy(b, HA_NAME[c->arp_pat % HA_COUNT], 16); break;
    case HK_ARATE: str_cpy(b, HR_NAME[c->arp_rate % HR_COUNT], 16); break;
    case HK_ALAYER: str_cpy(b, HAL_NAME[c->arp_layer % 3u], 16); break;
    case HK_SEQLEN: fmt_int(b, hcs.len); *frac = hui_frac(hcs.len, 4, 16); break;
    case HK_KIT: str_cpy(b, N_DRUM_KIT[c->kit % 4u], 16); break;
    case HK_DLSTYLE: str_cpy(b, DL_NAME[c->dl_style % DL_STYLES], 16); break;
    case HK_DLVAR: str_cpy(b, DV_NAME[c->dl_var % DV_VARS], 16); break;
    case HK_SONG: str_cpy(b, HCG_SONG[hcg.song % HG_SONGS].name, 16); break;
    case HK_DIFF: str_cpy(b, HGD_NAME[hcg.diff % HGD_COUNT], 16); break;
    case HK_SPEED: fmt_int(b, hcg.speed); str_cpy(b + str_len(b), "%", 16 - str_len(b)); *frac = hui_frac(hcg.speed, 50, 100); break;
    case HK_LEVEL: fmt_int(b, hcg.level + 1); *frac = hui_frac(hcg.level, 0, HGE_LEVELS - 1); break;
    case HK_L1: case HK_L2: case HK_L3: case HK_L4:
        fmt_int(b, trk[k - HK_L1].p[P_LEVEL]);
        *frac = hui_frac(trk[k - HK_L1].p[P_LEVEL], 0, 127);
        break;
    case HK_BPM: fmt_int(b, song.g[G_BPM]); *frac = hui_frac(song.g[G_BPM], GP[G_BPM].min, GP[G_BPM].max); break;
    case HK_METRO: str_cpy(b, hcl.metro ? "ON" : "OFF", 16); break;
    case HK_SPK: str_cpy(b, HC_SPK_NAME[settings.lowcut % 3u], 16); break;
    case HK_OUT: str_cpy(b, HC_OUT_NAME[hui.out_line & 1u], 16); break;
    case HK_MIDIIN: str_cpy(b, hui.midi_in ? "ON" : "OFF", 16); break;
    case HK_THEME: str_cpy(b, UI_PALETTES[settings.palette % NPALETTES].name, 16); break;
    case HK_BARS: if (hcl.bars) fmt_int(b, hcl.bars); else str_cpy(b, "FREE", 16); break;
    case HK_SLOT: str_cpy(b, "P1", 16); b[1] = (char)('1' + hui.slot % 4u); break;
    case HK_CLEAR: str_cpy(b, "ALL", 16); return 1;
    case HK_LOAD: case HK_SAVE: str_cpy(b, "P1", 16); b[1] = (char)('1' + hui.slot % 4u); return 1;
    default: break;
    }
    return 0;
}
/* a line under a card's value: what a layer's level is doing, what a preset holds */
static void hui_card_sub(uint32_t k, char *b)
{
    b[0] = 0;
    if (k >= HK_L1 && k <= HK_L4 && (trk[k - HK_L1].p[P_MUTE] || hcl.l[k - HK_L1].state != HLS_OFF))
        str_cpy(b, trk[k - HK_L1].p[P_MUTE] ? "MUTE" : HLS_NAME[hcl.l[k - HK_L1].state & 3u], 16);
    else if (k == HK_SLOT && !hui.preset_used[hui.slot % 4u])
        str_cpy(b, "EMPTY", 16);
    else if (k == HK_SLOT && !(project_name(hui.slot % 4u, b) && b[0]))
        str_cpy(b, "SAVED", 16);
}

/* a card turned s detents: its value (a list steps round, a number stops at its ends, a switch goes on to the
 * right and off to the left); an action card is armed to the right, let be to the left */
static void hui_card_turn(uint32_t k, int32_t s)
{
    hc_trk_t *c = hui_c();
    if (hui_card_action(k)) {
        hui.armed = (uint8_t)(s > 0 ? k : 0u);
        return;
    }
    switch (k) {
    case HK_KEY: hui_key_set(TSEL->p[P_ROOT] + s); hc.dirty = 1; break;
    case HK_SCALE: hui_scale_set(hui_cycle(hui_scale_get(), s, 10)); hc.dirty = 1; break;
    case HK_OCT: song.octave = (int8_t)clamp(song.octave + s, -2, 2); break;
    case HK_LAYOUT: c->layout = (uint8_t)hui_cycle(c->layout, s, 2); hc.dirty = 1; break;
    case HK_BASS: c->bass = (uint8_t)hui_cycle(c->bass, s, 3); hc.dirty = 1; break;
    case HK_VOICES: c->voices = (uint8_t)hui_cycle(c->voices, s, 4); hc.dirty = 1; break;
    case HK_VLEAD: c->vlead = (uint8_t)(s > 0); hc.dirty = 1; break;
    case HK_JOY: c->mode = (uint8_t)hui_cycle(c->mode, s, HM_COUNT); hc.dirty = 1; break;
    case HK_FILT: c->filt = (uint8_t)(s > 0); break;
    case HK_CUT: c->cutoff = (uint8_t)clamp(c->cutoff + s * 3, 0, 127); c->filt = 1; break;
    case HK_RES: c->res = (uint8_t)clamp(c->res + s * 3, 0, 127); c->filt = 1; break;
    case HK_HP: c->hp = (uint8_t)(s > 0); break;
    case HK_ENV: c->env = (uint8_t)hui_cycle(c->env, s, HE_COUNT); c->atk = c->rel = 0; break;
    case HK_ATK: c->atk = (uint8_t)clamp((c->atk ? c->atk : TSEL->p[P_ATK]) + s * 2, 1, 127); break;
    case HK_REL: c->rel = (uint8_t)clamp((c->rel ? c->rel : TSEL->p[P_REL]) + s * 2, 1, 127); break;
    case HK_GLIDE: c->glide = (uint8_t)hui_cycle(c->glide, s, HGL_COUNT); break;
    case HK_REV: c->rev = (uint8_t)hui_cycle(c->rev, s, HRV_COUNT); break;
    case HK_DLY: c->dly = (uint8_t)hui_cycle(c->dly, s, HDL_COUNT); break;
    case HK_CHO: c->cho = (uint8_t)hui_cycle(c->cho, s, HCH_COUNT); break;
    case HK_FLG: c->flg = (uint8_t)hui_cycle(c->flg, s, HFL_COUNT); break;
    case HK_TREM: c->trem = (uint8_t)hui_cycle(c->trem, s, HTR_COUNT); break;
    case HK_REV_AMT: case HK_DLY_AMT: case HK_CHO_AMT: case HK_FLG_AMT: case HK_TREM_AMT: hui_fx_turn(c, k - 1u, s); break;
    case HK_LFO: c->lfo = (uint8_t)hui_cycle(c->lfo, s, HLF_COUNT); break;
    case HK_STEREO: c->stereo = (uint8_t)(s > 0); break;
    case HK_DRIVE: c->drive = (uint8_t)hui_cycle(c->drive, s, HDR_COUNT); break;
    case HK_TAPE: c->tape = (uint8_t)hui_cycle(c->tape, s, HTP_COUNT); break;
    case HK_STRUM: c->strum = (uint8_t)hui_cycle(c->strum, s, 3); break;
    case HK_APAT: c->arp_pat = (uint8_t)hui_cycle(c->arp_pat, s, HA_COUNT); break;
    case HK_ARATE: c->arp_rate = (uint8_t)hui_cycle(c->arp_rate, s, HR_COUNT); break;
    case HK_ALAYER: c->arp_layer = (uint8_t)hui_cycle(c->arp_layer, s, 3); break;
    case HK_SEQLEN: hcs.len = (uint8_t)clamp((int32_t)hcs.len + s * 4, 4, 16); if (hcs.cur >= hcs.len) hcs.cur = 0; return;
    case HK_KIT: c->kit = (uint8_t)hui_cycle(c->kit, s, 4); break;
    case HK_DLSTYLE: c->dl_style = (uint8_t)hui_cycle(c->dl_style, s, DL_STYLES); return;
    case HK_DLVAR: c->dl_var = (uint8_t)hui_cycle(c->dl_var, s, DV_VARS); return;
    case HK_SONG: hcg.song = (uint8_t)hui_cycle(hcg.song, s, HG_SONGS); return;
    case HK_DIFF: hcg.diff = (uint8_t)hui_cycle(hcg.diff, s, HGD_COUNT); return;
    case HK_SPEED: hcg.speed = (uint8_t)clamp(hcg.speed + s * 10, 50, 100); return;
    case HK_LEVEL: hcg.level = (uint8_t)clamp(hcg.level + s, 0, HGE_LEVELS - 1); return;
    case HK_L1: case HK_L2: case HK_L3: case HK_L4:
        trk[k - HK_L1].p[P_LEVEL] = (int16_t)clamp(trk[k - HK_L1].p[P_LEVEL] + s * 4, 0, 127);
        return;
    case HK_BPM: song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + s, GP[G_BPM].min, GP[G_BPM].max); break;
    case HK_METRO: hcl.metro = (uint8_t)(s > 0); return;
    case HK_SPK: settings.lowcut = hui_cycle(settings.lowcut, s, 3); fx_lowcut = (uint8_t)settings.lowcut; return;
    case HK_OUT: hui.out_line = (uint8_t)(s > 0); hcfx.line = hui.out_line; return;
    case HK_MIDIIN: hui.midi_in = (uint8_t)(s > 0); return;
    case HK_THEME:                                       /* the palette: the whole screen again; saved with the
                                                          * settings a moment later, as the sound is */
        settings.palette = hui_cycle(settings.palette, s, NPALETTES);
        palette_set(settings.palette);
        hui.force = 1;
        hui_live.dirty = 1;
        hui_live.at_ms = fm1_ms;
        return;
    case HK_BARS: hcl.bars = (uint8_t)clamp(hcl.bars + s, 0, 8); return;
    case HK_SLOT: hui.slot = (uint8_t)clamp((int32_t)hui.slot + s, 0, 3); return;
    default: return;
    }
    hui_apply();
}
/* OCT+ on the armed card: done */
static void hui_card_do(uint32_t k)
{
    hui.armed = 0;
    if (k == HK_CLEAR) {
        hcl_clear_all();
        hui_say("ALL CLEARED");
    } else if (k == HK_LOAD) {
        hui_preset_load(hui.slot);
    } else if (k == HK_SAVE) {
        hui_preset_save(hui.slot);
    }
}

/* the mode of the live track: a drum mode takes the DRUM engine (the sound kept for the way back); the chord
 * modes get the sound back. hc_play_set does the rest (what ran stops; a running sequencer bounces) */
static int hui_drum_mode(uint32_t play) { return play == HP_DRUM || play == HP_DRUMLOOP; }
static void hui_mode_set(track_t *t, uint32_t play)
{
    hc_trk_t *c = hc_of(t);
    uint32_t was = c->play;
    play %= HP_COUNT;
    hc_play_set(t, play);
    if (hui_drum_mode(play) && !hui_drum_mode(was)) {
        c->sound_saved = c->sound;
        set_engine_of(t, ENGI_DRUM);
        hc_apply(t);
    } else if (!hui_drum_mode(play) && hui_drum_mode(was)) {
        if (t->p[P_LEVEL] <= 96)                         /* the drum modes cap the level (hc_apply): back to full */
            t->p[P_LEVEL] = TP[P_LEVEL].def;
        hc_sound_load(t, c->sound_saved);
    }
    if (play == HP_DRUMLOOP)
        hcd_loop_start();
}

/* a layer began to play: it stays the selected one (each layer keeps its own sound and style); ALGORITHM, or REC
 * on it again, goes on to the next */
static void hui_looper_advance(void)
{
    hcl.advance = 0;
    hui_say("LAYER ");
    hui.msg[6] = (char)('1' + song.sel % NTRK);
    str_cpy(hui.msg + 7, " PLAYS", sizeof hui.msg - 7u);
}

/* REC tapped: the selected layer's next state; on a layer that plays already, the next empty layer is selected and
 * armed instead (the take goes on, nothing is lost; REC held clears a layer) */
static void hui_rec_tap(void)
{
    uint32_t sel = song.sel % HCL_LAYERS, j;
    if (hcl.l[sel].state == HLS_PLAY) {
        for (j = 1; j < HCL_LAYERS; j++)
            if (hcl.l[(sel + j) % HCL_LAYERS].state == HLS_OFF)
                break;
        if (j == HCL_LAYERS) {
            hui_say("EVERY LAYER PLAYS");
            return;
        }
        track_select((sel + j) % HCL_LAYERS);
        hcl.sel = (uint8_t)song.sel;
    }
    hcl_rec_press();
    hui_say("LAYER ");
    hui.msg[6] = (char)('1' + hcl.sel % NTRK);
    hui.msg[7] = ' ';
    str_cpy(hui.msg + 8, HLS_NAME[hcl.l[hcl.sel].state & 3u], sizeof hui.msg - 8u);
}

/* --------------------------------------------------------------- input --- */
/* The lists (HU_PICK): the sounds and the layers, shown over the screen while the PRESETS / ALGORITHM knob turns
 * and gone 1.5 s after (or at a press) */
enum { PK_SOUND, PK_LAYER };
static uint32_t hui_pick_count(uint32_t k) { return k == PK_SOUND ? (uint32_t)HC_NSOUNDS : NTRK; }
static const char *hui_pick_name(uint32_t k, uint32_t i)
{
    return k == PK_SOUND ? HC_SOUNDS[i % HC_NSOUNDS].name : HC_SOUNDS[hc.t[i % NTRK].sound % HC_NSOUNDS].name;
}
static uint32_t hui_pick_cur(uint32_t k) { return k == PK_SOUND ? hui_c()->sound : song.sel; }
static void hui_pick_move(int32_t s)                    /* (SELECT while a list shows: its choice) */
{
    uint32_t k = hui.pick_kind, i = (uint32_t)clamp((int32_t)hui_pick_cur(k) + s, 0, (int32_t)hui_pick_count(k) - 1);
    if (k == PK_SOUND)
        hc_sound_load(TSEL, i);
    else
        track_select(i);
    hui.pick_ms = fm1_ms;
}
static void hui_pick_open(uint32_t k)
{
    hui.pick_kind = (uint8_t)k;
    hui.pick_ms = fm1_ms;
    if (hui.screen != HU_PICK) {
        hui.pick_back = hui.screen;
        hui.screen = HU_PICK;
        hui.force = 1;
    }
}
static void hui_pick_close(void)
{
    hui.screen = hui.pick_back < HU_N && hui.pick_back != HU_PICK ? hui.pick_back : (uint8_t)HU_HOME;
    hui.force = 1;
}

/* a menu's button tapped: in, on the page it was left on (PRESETS with SAVE armed: SAVE, OCT+ saves); already in,
 * on: the next page, round; PRESETS the next slot, the one-page LOOPER back HOME */
static void hui_open(uint32_t sc)
{
    hui.force = 1;
    if (hui.screen != sc) {
        hui.screen = (uint8_t)sc;
        hui.armed = 0;
        if (sc == HU_PRESET) {
            hui_preset_scan();
            hui.armed = HK_SAVE;
        }
        return;
    }
    if (sc == HU_PRESET) {
        hui.slot = (uint8_t)((hui.slot + 1u) % 4u);
        hui.armed = HK_SAVE;
    } else if (HU_SEC[sc].n > 1u) {
        hui.page[sc] = (uint8_t)((hui.page[sc] + 1u) % HU_SEC[sc].n);
        hui.armed = 0;
    } else {
        hui.screen = HU_HOME;
    }
}
/* SOUND on one of its pages (the ENV and LFO buttons); pressed again there: HOME */
static void hui_open_page(uint32_t sc, uint32_t page)
{
    hui.force = 1;
    hui.armed = 0;
    if (hui.screen == sc && hui.page[sc] == page) {
        hui.screen = HU_HOME;
        return;
    }
    hui.screen = (uint8_t)sc;
    hui.page[sc] = (uint8_t)page;
}

static void hui_tap_tempo(void)
{
    uint32_t now = fm1_ms, i;
    if (hui.ntap && now - hui.tap_ms[hui.ntap - 1u] > 1500u)   /* a pause: a new series (40 BPM: 1.5 s) */
        hui.ntap = 0;
    if (hui.ntap < 4u)
        hui.tap_ms[hui.ntap++] = now;
    else {
        for (i = 1; i < 4u; i++)
            hui.tap_ms[i - 1u] = hui.tap_ms[i];
        hui.tap_ms[3] = now;
    }
    if (hui.ntap >= 3u) {
        uint32_t span = hui.tap_ms[hui.ntap - 1u] - hui.tap_ms[0], bpm = 60000u * (hui.ntap - 1u) / (span ? span : 1u);
        song.g[G_BPM] = (int16_t)clamp((int32_t)bpm, GP[G_BPM].min, GP[G_BPM].max);
        hui_say("TAP TEMPO");
        hui_apply();
    }
}

/* ------------------------------------------------------- held layers --- */
/* After SLOOP and JIANT: a menu button held is a layer. While it is down its 16 white keys and KNOB 1..4 change job,
 * their map over the screen (after HL_OPEN_MS, or at once with a key or a knob); let go and they play again. Tapped
 * (let go before HL_TAP_MS with nothing else touched): its menu, as before.
 *   SCL   KEY      any key: that note is the key (a black key: its sharp); KNOB 1 SCALE, 2 BASS, 3 VOICES, 4 JOYSTICK
 *   FX    SOUND    a white key: a sound of the page (KNOB 1: 16 a page); 2 ENVELOPE, 3 REVERB, 4 DELAY
 *   EDIT  MODE     white keys 1..11: the layer's play style; KNOB 1 STRUM, 2 ARP, 3 ARP RATE, 4 DRUM KIT
 *   GLO   MIX      white keys 1..4: the layers' mutes, 7 the metronome, 16 tap tempo; KNOB 1..4 the layers' levels
 *   SAVE  PRESETS  white keys 1..4 load P1..P4, 5..8 save to them
 * Keys pressed with the button down are the layer's (seq.c keyboard_block: silent, never recorded); keys held
 * before stay notes. One layer at a time. */
enum { HL_NONE, HL_KEY, HL_SOUND, HL_MODE, HL_MIX, HL_PRESET, HL_N };
static const uint8_t HL_BTN[HL_N] = {0, B_SCL, B_FX, B_EDIT, B_GLO, B_SAVE};
static const uint8_t HL_KC[HL_N] = {0, KC_SCL, KC_FX, KC_EDIT, KC_GLO, KC_SAVE};
static const char *const HL_TITLE[HL_N] = {"", "KEY", "SOUND", "MODE", "MIX", "PRESETS"};
static const uint8_t HL_ROLE[HL_N] = {0, UR_KEY, UR_SOUND, UR_MODE, UR_LOOP, UR_PRESET};
#define HL_OPEN_MS 250u                                  /* held this long alone: the map */
#define HL_TAP_MS 600u                                   /* let go sooner, nothing else touched: a tap */
#define HL_PAGES ((HC_NSOUNDS + 15u) / 16u)
static uint32_t hl_bit(uint32_t l) { return 1u << panel.btn[HL_BTN[l % HL_N]]; }
static uint32_t hl_bits(void)
{
    uint32_t l, m = 0;
    for (l = HL_KEY; l < HL_N; l++)
        m |= hl_bit(l);
    return m;
}
static const char *const HL_WNOTE[7] = {"F", "G", "A", "B", "C", "D", "E"};   /* the white keys from F3 */
static const uint8_t HL_WSEMI[7] = {5, 7, 9, 11, 0, 2, 4};

/* a white key's tile: its word, and 2 lit (what is on), 1 a choice, 0 nothing there */
static uint32_t hl_tile(uint32_t l, uint32_t w, char *b)
{
    const hc_trk_t *c = hui_c();
    uint32_t i;
    b[0] = 0;
    switch (l) {
    case HL_KEY:
        str_cpy(b, HL_WNOTE[w % 7u], 4);
        return (uint32_t)TSEL->p[P_ROOT] % 12u == HL_WSEMI[w % 7u] ? 2u : 1u;
    case HL_SOUND:
        i = hui.ly_page * 16u + w;
        if (i >= HC_NSOUNDS)
            return 0;
        str_cpy(b, HC_SOUNDS[i].name, 16);
        return c->sound == i ? 2u : 1u;
    case HL_MODE:
        if (w >= HU_NSTYLES)
            return 0;
        str_cpy(b, HP_SHORT[HU_STYLE[w]], 16);
        return c->play == HU_STYLE[w] ? 2u : 1u;
    case HL_MIX:
        if (w < HCL_LAYERS) {
            b[0] = 'L'; b[1] = (char)('1' + w); b[2] = 0;
            return trk[w].p[P_MUTE] ? 1u : 2u;               /* lit: sounding */
        }
        if (w == 6u) {
            str_cpy(b, "METRO", 16);
            return hcl.metro ? 2u : 1u;
        }
        if (w == 15u) {
            str_cpy(b, "TAP", 16);
            return 1u;
        }
        return 0;
    default:
        if (w >= 8u)
            return 0;
        str_cpy(b, w < 4u ? "LOAD P" : "SAVE P", 16);
        b[6] = (char)('1' + w % 4u);
        b[7] = 0;
        return w < 4u && !hui.preset_used[w] ? 0u : 1u;
    }
}

/* a knob's card: its name and its value; 0 = the layer has no such knob */
static int hl_knob_text(uint32_t l, uint32_t i, const char **name, char *v)
{
    static const char *const NAMES[HL_N][4] = {{0}, {"SCALE", "BASS", "VOICES", "JOY"}, {"PAGE", "ENV", "REVERB", "DELAY"},
                                               {"STRUM", "ARP", "RATE", "KIT"}, {"L1", "L2", "L3", "L4"}, {0}};
    const hc_trk_t *c = hui_c();
    *name = NAMES[l % HL_N][i & 3u];
    v[0] = 0;
    if (!*name)
        return 0;
    switch (l * 4u + i) {
    case HL_KEY * 4u: str_cpy(v, HC_SCALE_NAME[hui_scale_get()], 16); break;
    case HL_KEY * 4u + 1u: str_cpy(v, HB_NAME[c->bass % 3u], 16); break;
    case HL_KEY * 4u + 2u: str_cpy(v, HV_NAME[c->voices % 4u], 16); break;
    case HL_KEY * 4u + 3u: str_cpy(v, HM_NAME[c->mode % HM_COUNT], 16); break;
    case HL_SOUND * 4u: fmt_int(v, hui.ly_page + 1); str_cpy(v + str_len(v), " / ", 16 - str_len(v)); fmt_int(v + str_len(v), HL_PAGES); break;
    case HL_SOUND * 4u + 1u: str_cpy(v, HE_NAME[c->env % HE_COUNT], 16); break;
    case HL_SOUND * 4u + 2u: str_cpy(v, HRV_NAME[c->rev % HRV_COUNT], 16); break;
    case HL_SOUND * 4u + 3u: str_cpy(v, HDL_NAME[c->dly % HDL_COUNT], 16); break;
    case HL_MODE * 4u: str_cpy(v, HST_NAME[c->strum % 3u], 16); break;
    case HL_MODE * 4u + 1u: str_cpy(v, HA_NAME[c->arp_pat % HA_COUNT], 16); break;
    case HL_MODE * 4u + 2u: str_cpy(v, HR_NAME[c->arp_rate % HR_COUNT], 16); break;
    case HL_MODE * 4u + 3u: str_cpy(v, N_DRUM_KIT[c->kit % 4u], 16); break;
    default: fmt_int(v, trk[i & 3u].p[P_LEVEL]); break;      /* MIX: the layers' levels */
    }
    return 1;
}

static void hl_key(uint32_t l, uint32_t k)
{
    uint32_t w = key_black(k) ? 99u : key_place(k), i;
    switch (l) {
    case HL_KEY:                                         /* key 0 is F */
        hui_key_set((int32_t)(k + 5u));
        hc.dirty = 1;
        break;
    case HL_SOUND:
        i = hui.ly_page * 16u + w;
        if (w < 16u && i < HC_NSOUNDS)
            hc_sound_load(TSEL, i);
        break;
    case HL_MODE:
        if (w < HU_NSTYLES)
            hui_mode_set(TSEL, HU_STYLE[w]);
        break;
    case HL_MIX:
        if (w < HCL_LAYERS)
            trk[w].p[P_MUTE] = (int16_t)!trk[w].p[P_MUTE];
        else if (w == 6u)
            hcl.metro = (uint8_t)!hcl.metro;
        else if (w == 15u)
            hui_tap_tempo();
        break;
    case HL_PRESET:
        if (w < 4u)
            hui_preset_load(w);
        else if (w < 8u)
            hui_preset_save(w - 4u);
        break;
    }
}

static void hl_knob(uint32_t l, uint32_t i, int32_t s)
{
    hc_trk_t *c = hui_c();
    switch (l * 4u + i) {
    case HL_KEY * 4u: hui_scale_set(hui_cycle(hui_scale_get(), s, 10)); hc.dirty = 1; break;
    case HL_KEY * 4u + 1u: c->bass = (uint8_t)hui_cycle(c->bass, s, 3); hc.dirty = 1; break;
    case HL_KEY * 4u + 2u: c->voices = (uint8_t)hui_cycle(c->voices, s, 4); hc.dirty = 1; break;
    case HL_KEY * 4u + 3u: c->mode = (uint8_t)hui_cycle(c->mode, s, HM_COUNT); hc.dirty = 1; break;
    case HL_SOUND * 4u: hui.ly_page = (uint8_t)clamp((int32_t)hui.ly_page + s, 0, (int32_t)HL_PAGES - 1); return;
    case HL_SOUND * 4u + 1u: c->env = (uint8_t)hui_cycle(c->env, s, HE_COUNT); c->atk = c->rel = 0; break;
    case HL_SOUND * 4u + 2u: c->rev = (uint8_t)hui_cycle(c->rev, s, HRV_COUNT); break;
    case HL_SOUND * 4u + 3u: c->dly = (uint8_t)hui_cycle(c->dly, s, HDL_COUNT); break;
    case HL_MODE * 4u: c->strum = (uint8_t)hui_cycle(c->strum, s, 3); break;
    case HL_MODE * 4u + 1u: c->arp_pat = (uint8_t)hui_cycle(c->arp_pat, s, HA_COUNT); break;
    case HL_MODE * 4u + 2u: c->arp_rate = (uint8_t)hui_cycle(c->arp_rate, s, HR_COUNT); break;
    case HL_MODE * 4u + 3u: c->kit = (uint8_t)hui_cycle(c->kit, s, 4); break;
    default:
        if (l == HL_MIX)
            trk[i & 3u].p[P_LEVEL] = (int16_t)clamp(trk[i & 3u].p[P_LEVEL] + s * 4, 0, 127);
        return;
    }
    hui_apply();
}

/* EDIT tapped: the next play style, ARP the one before, on HOME (whose knobs are the style's) */
static void hui_style_step(int32_t d)
{
    hui_mode_set(TSEL, HU_STYLE[hui_cycle(hui_style_index(hui_c()->play), d, HU_NSTYLES)]);
    hui_say(HP_NAME[hui_c()->play % HP_COUNT]);
    hui.screen = HU_HOME;
    hui.armed = 0;
    hui.force = 1;
}
/* the button tapped: its menu (in, or on to the next page: hui_open); EDIT the next style */
static void hl_tap(uint32_t l)
{
    static const uint8_t SCREEN[HL_N] = {HU_HOME, HU_KEY, HU_SOUND, HU_HOME, HU_MIX, HU_PRESET};
    if (l == HL_MODE)
        hui_style_step(1);
    else
        hui_open(SCREEN[l % HL_N]);
}

/* each pass: which held button takes the keys (seq.c keyboard_block); one pressed now arms; its keys and knobs;
 * let go: a tap, or the map closes. 1 = a layer is held (KNOB 1..4 are its) */
static int hui_layer_input(uint32_t pressed)
{
    uint32_t l, k, keys;
    kb_mask = hui.ly ? hl_bit(hui.ly) : hl_bits() & ~fm1_in.buttons;
    perf_mask = 0;
    if (!hui.ly)
        for (l = HL_KEY; l < HL_N; l++)
            if (pressed & hl_bit(l)) {
                hui.ly = (uint8_t)l;
                hui.ly_ms = fm1_ms;
                hui.ly_open = hui.ly_used = 0;
                hui.ly_seen = 0;
                kb_mask = hl_bit(l);
                break;
            }
    if (!hui.ly)
        return 0;
    l = hui.ly;
    if (fm1_in.buttons & hl_bit(l)) {
        keys = kb_layer & ~hui.ly_seen;
        hui.ly_seen = kb_layer;
        for (k = 0; k < 27u; k++)
            if ((keys >> k) & 1u) {
                hl_key(l, k);
                hui.ly_used = 1;
            }
        for (k = 0; k < 4u; k++) {
            int32_t s = panel_enc(EN_K1 + k);
            if (s) {
                hl_knob(l, k, s);
                hui.ly_used = 1;
            }
        }
        if (pressed & ~hl_bit(l))                        /* (PLAY, REC .. work inside it, and make it no tap) */
            hui.ly_used = 1;
        if (!hui.ly_open && (hui.ly_used || fm1_ms - hui.ly_ms >= HL_OPEN_MS)) {
            hui.ly_open = 1;
            hui.force = 1;
        }
        return 1;
    }
    hui.ly = 0;                                          /* let go */
    if (hui.ly_open)
        hui.force = 1;
    hui.ly_open = 0;
    if (!hui.ly_used && fm1_ms - hui.ly_ms < HL_TAP_MS)
        hl_tap(l);
    return 0;
}

static void hui_input(void)
{
    uint32_t pressed = fm1_input_edges(0), now = fm1_ticks(), i;
    uint32_t home = btn_hold(&hui.home_t0, B_HOME, now, 0);
    hc_trk_t *c = hui_c();
    int32_t s;
    uint32_t sc = hui.screen, held;
    fm1_input_note_edges();                              /* (the keys are the ISR's: nothing of the UI's) */
    fm6_poll();
    hui_live_poll();
    if (sc == HU_PICK && (pressed || (uint32_t)(fm1_ms - hui.pick_ms) > 1500u))
        hui_pick_close();                                /* a knob's list: over (a button acts on the screen under it) */
    song.grid = 0;
    song.seq_mode = 0;
    hui_track_init(TSEL);
#define DOWN(b) ((pressed >> panel.btn[b]) & 1u)
    if (home == BT_TAP) {
        hui.screen = HU_HOME;
        hui.armed = 0;
        hui.force = 1;
    }
    held = hui_layer_input(pressed);                     /* SCL FX EDIT GLO SAVE: held a layer, tapped a menu */
    if (DOWN(B_ENV))                                     /* SOUND's AMP page (again: HOME) */
        hui_open_page(HU_SOUND, HU_PG_AMP);
    if (DOWN(B_LFO))                                     /* SOUND's WOBBLE page, the vibrato */
        hui_open_page(HU_SOUND, HU_PG_WOBBLE);
    if (DOWN(B_ARP))                                     /* the play style before (EDIT: the next) */
        hui_style_step(-1);
    if (DOWN(B_SEQ))
        hui_open(HU_LOOP);
    sc = hui.screen;
    {   /* OCT- / OCT+: the octave, or the armed action (OCT+ does it, OCT- lets it be); both together: RANDOMIZE */
        uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
        static uint8_t chord_seen;
        if ((fm1_in.buttons & both) == both) {
            if (!chord_seen) {
                chord_seen = 1;
                hui.armed = 0;
                if (sc == HU_SOUND) hui_random_sound();
                else hui_random_all();
            }
        } else if (!(fm1_in.buttons & both)) {
            chord_seen = 0;
        } else if (!chord_seen && (DOWN(B_OCTDN) || DOWN(B_OCTUP))) {
            int32_t d = DOWN(B_OCTUP) ? 1 : -1;
            if (sc == HU_HOME && c->play == HP_MIXER) {
                TSEL->p[P_LEVEL] = (int16_t)clamp(TSEL->p[P_LEVEL] + d * 8, 0, 127);
            } else if (hui.armed && hui_paged(sc)) {
                if (d > 0)
                    hui_card_do(hui.armed);
                hui.armed = 0;
            } else {
                song.octave = (int8_t)clamp(song.octave + d, -2, 2);
            }
        }
    }
    if ((s = panel_enc(EN_SELECT)) != 0) {               /* a menu's pages (PRESETS: the slot), or
                                                          * the key; MIXER: the layer; SEQUENCER: the step */
        if (sc == HU_HOME && c->play == HP_MIXER) {
            track_select((uint32_t)clamp((int32_t)song.sel + s, 0, NTRK - 1));
        } else if (sc == HU_HOME && c->play == HP_SEQ) {
            hcs.cur = (uint8_t)((hcs.cur + (uint32_t)(s % (int32_t)hcs.len + (int32_t)hcs.len)) % hcs.len);
        } else if (sc == HU_HOME) {
            hui_key_set(TSEL->p[P_ROOT] + s);
            hc.dirty = 1;
        } else if (sc == HU_PICK) {
            hui_pick_move(s);
        } else if (sc == HU_PRESET) {
            hui_card_turn(HK_SLOT, s);
        } else if (hui_paged(sc)) {
            uint32_t p = (uint32_t)clamp((int32_t)hui.page[sc] + s, 0, (int32_t)HU_SEC[sc].n - 1);
            if (p != hui.page[sc])
                hui.armed = 0;
            hui.page[sc] = (uint8_t)p;
        }
    }
    if ((s = panel_enc(EN_ALGO)) != 0) {                 /* the layer (1..4, its own sound and style), the four
                                                          * shown while the knob turns */
        track_select((uint32_t)clamp((int32_t)song.sel + s, 0, NTRK - 1));
        hui_track_init(TSEL);
        hui_pick_open(PK_LAYER);
    }
    if ((s = panel_enc(EN_PRESET)) != 0) {               /* the sound, its list shown while the knob turns */
        hc_sound_load(TSEL, hui_cycle(c->sound, s, HC_NSOUNDS));
        hui_pick_open(PK_SOUND);
    }
    if (!held && hui_paged(sc == HU_PICK ? hui.pick_back : sc)) {   /* KNOB 1..4 on a menu: its four cards */
        const hu_page_t *pg = hui_page_of(sc == HU_PICK ? hui.pick_back : sc, hui.page[sc == HU_PICK ? hui.pick_back : sc]);
        for (i = 0; i < 4u; i++)
            if ((s = panel_enc(EN_K1 + i)) != 0 && pg->card[i])
                hui_card_turn(pg->card[i], s);
    }
    if (!held && (sc == HU_HOME || (sc == HU_PICK && hui.pick_back == HU_HOME)))   /* HOME: KNOB 1..4 the style's */
        for (i = 0; i < 4u; i++) {
            uint32_t k = HU_HOME_KNOBS[c->play % HP_COUNT][i];
            if ((s = panel_enc(EN_K1 + i)) != 0 && k) {
                hui_card_turn(k, s);
                hui_say_value(HK_NAME[k], k);
            }
        }
    {   /* the looper: REC cycles the layer (held: clears it), PLAY pauses / resumes, SEQ shows the LOOPER screen */
        uint32_t rec = btn_hold(&hui.rec_t0, B_REC, now, 1);
        hcl.sel = (uint8_t)song.sel;
        if (rec == BT_TAP) {
            hui_rec_tap();
        } else if (rec == BT_HOLD) {                     /* held 0.7 s: the selected layer (the last one: all) */
            hcl_clear(hcl.sel);
            if (!hcl_count(HLS_PLAY) && !hcl_count(HLS_REC)) {
                hcl_clear_all();
                hui_say("ALL CLEARED");
            } else {
                hui_say("CLEARED. HOLD: ALL");
            }
        } else if ((hui.rec_t0 & 3u) == 3u && now - (hui.rec_t0 & ~3u) > 2000u * 1000u * FM1_TICKS_PER_US && hcl.len) {
            hcl_clear_all();                             /* held on to 2 s: every layer, the loop's length */
            hui_say("ALL CLEARED");
        }
        if (DOWN(B_PLAY)) {
            if (hcl.len && !hcl.playing && hcl.any) {    /* a loop paused (back from power-off, or PLAY before):
                                                          * PLAY goes on with it first, whatever the layer's style */
                hcl_play_press();
                hui_say("PLAYING");
            } else if (c->play == HP_HIRO) {                    /* the song starts or stops */
                if (hcg.running) hcg_hiro_stop(TSEL); else hcg_hiro_start();
                hui_say(hcg.running ? "GO!" : "STOPPED");
            } else if (c->play == HP_EAR) {              /* a new round */
                hcg_ear_start();
                hui_say("LISTEN");
            } else if (c->play == HP_SEQ) {                     /* the sequencer runs or stops */
                if (hcs.running) hcs_stop(TSEL); else hcs_start(TSEL);
                hui_say(hcs.running ? "SEQ RUNNING" : "SEQ STOPPED");
            } else if (c->play == HP_DRUMLOOP) {
                if (hcs.dl_running) hcd_loop_stop(); else hcd_loop_start();
                hui_say(hcs.dl_running ? "LOOP RUNNING" : "LOOP STOPPED");
            } else {
                hcl_play_press();
                hui_say(hcl.playing ? "PLAYING" : hcl.len ? "PAUSED" : "NO LOOP");
            }
        }
        if (hcl.advance)
            hui_looper_advance();
    }
    for (i = 0; i < NE; i++)                             /* (nothing else takes a turn) */
        panel_enc(i);
#undef DOWN
}

/* ---------------------------------------------------------------- LEDs --- */
static void hui_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0};
    uint32_t k, c, held = fm1_in.notes | hc.latched;
    static const uint8_t MENU_BTN[HU_N] = {B_HOME, B_SCL, B_FX, B_EDIT, B_SAVE, B_SEQ, B_HOME, B_GLO};
    static uint8_t ready;
    uint32_t sc = hui.screen == HU_PICK ? hui.pick_back % HU_N : hui.screen;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    led_put(nl, panel.btn[MENU_BTN[sc]], 1);
    if (hui.ly)
        led_put(nl, panel.btn[HL_BTN[hui.ly % HL_N]], 1);
    if (sc == HU_SOUND && (hui.page[HU_SOUND] == HU_PG_AMP || hui.page[HU_SOUND] == HU_PG_WOBBLE))
        led_put(nl, panel.btn[hui.page[HU_SOUND] == HU_PG_AMP ? B_ENV : B_LFO], 1);
    led_put(nl, panel.btn[B_REC], hcl_count(HLS_REC) != 0u || (hcl_count(HLS_ARMED) != 0u && ((fm1_ms / 250u) & 1u) == 0u));
    led_put(nl, panel.btn[B_PLAY], hcl.playing);
    led_put(nl, panel.btn[B_OCTDN], song.octave < 0);
    led_put(nl, panel.btn[B_OCTUP], song.octave > 0);
    for (k = 0; k < 27u; k++) {
        led_put(nl, 14u + k, (int)((held >> k) & 1u) || (key_black(k) && key_place(k) == 2u && hc.hold));
        led_put(nd, 14u + k, 1);
    }
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = nd[c];
        fm1_led[c] = nl[c];
    }
}

/* ------------------------------------------------------------- drawing --- */
#define HU_HEAD 24
#define HU_ROW 23
#define HU_ROWS_Y 28
#define HU_FOOT_Y 214

static uint32_t hui_fx_mask(const hc_trk_t *c)
{
    return (c->rev ? 1u : 0u) | (c->dly ? 2u : 0u) | (c->cho ? 4u : 0u) | (c->flg ? 8u : 0u) | (c->trem ? 16u : 0u) |
           (c->lfo ? 32u : 0u) | (c->glide ? 64u : 0u) | (c->drive ? 128u : 0u) | (c->tape ? 256u : 0u) |
           (c->filt ? 512u : 0u) | (c->hp ? 1024u : 0u) | (c->stereo ? 2048u : 0u);
}

/* a chip: a short word on a 16 px tile, r 3; filled (a state: INK on the colour) or outlined (a tag: the word in
 * the colour). Its width */
#define HU_CHIP_H 16
static int32_t hui_chip_w(const char *s) { return text_w(&AF_S, s) + 10; }
static int32_t hui_chip(int32_t x, int32_t y, const char *s, uint16_t col, int filled, uint16_t under)
{
    int32_t w = hui_chip_w(s);
    cv_rrect(x, y, w, HU_CHIP_H, 3, col, under);
    if (!filled)
        cv_rrect(x + 1, y + 1, w - 2, HU_CHIP_H - 2, 2, under, col);
    cv_text_c(x + w / 2, y + 1, &AF_S, s, filled ? T_INK : col, filled ? col : under);
    return w;
}

/* a message over a header: centred on a surface band, a rule in the sound's colour under it */
static void hui_draw_msg(void)
{
    cv_rect(0, 0, 240, HU_HEAD - 1, T_SURF);
    cv_text_c(120, 3, &AF_M, hui.msg, T_TEXT, T_SURF);
    cv_rect(0, HU_HEAD - 2, 240, 2, HC_YELLOW);
}

/* the header: the layer (a chip in its colour), its play style (a tag in the mode's colour), the key and scale, the
 * octave shift; the tempo, USB, the battery at the right; a rule under it */
static void hui_draw_head(void)
{
    char b[24];
    const hc_trk_t *c = hui_c();
    int32_t x;
    uint32_t sig = (uint32_t)TSEL->p[P_ROOT] * 3u + hui_scale_get() * 37u + c->play * 101u + (uint32_t)song.g[G_BPM] * 1009u +
                   (uint32_t)batt_shown() * 7777u + (usb.config && !usb.suspended) * 99991u + (hui.msg_t ? str_hash(5u, hui.msg) : 0u) +
                   (uint32_t)(song.octave + 3) * 50021u + ux.gen * 7919u + song.sel * 131071u;
    int32_t bx;
    if (!hui.force && sig == hui.sig[0])
        return;
    hui.sig[0] = sig;
    cv_begin(240, HU_HEAD, T_BG);
    if (hui.msg_t) {
        hui_draw_msg();
        cv_blit(0, 0);
        return;
    }
    b[0] = 'L'; b[1] = (char)('1' + song.sel % NTRK); b[2] = ' ';             /* the layer and its style */
    str_cpy(b + 3, HP_SHORT[c->play % HP_COUNT], sizeof b - 3u);
    x = 4 + hui_chip(4, 4, b, HU_LCOL(song.sel), 1, T_BG) + 7;
    str_cpy(b, HC_NOTE_NAME[(uint32_t)TSEL->p[P_ROOT] % 12u], sizeof b);      /* the key */
    str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
    str_cpy(b + str_len(b), HC_SCALE_NAME[hui_scale_get()], sizeof b - str_len(b));
    {   /* the tempo, the USB icon and the battery; "BPM" after the tempo when the key leaves room for it */
        char t[8];
        int32_t need = x + text_w(&AF_M, b) + 8 + (song.octave ? 30 : 0);
        fmt_int(t, song.g[G_BPM]);
        bx = 192 - text_w(&AF_M, t) - 3 - text_w(&AF_S, "BPM") >= need
             ? cv_text_r(cv_text_r(192, 5, &AF_S, "BPM", T_MID, T_BG) - 3, 2, &AF_M, t, T_TEXT, T_BG) - 8
             : cv_text_r(192, 2, &AF_M, t, T_TEXT, T_BG) - 8;
    }
    if (song.octave) {                                   /* the octave shift: a tag, room kept for it */
        char o[6];
        str_cpy(o, song.octave > 0 ? "+" : "", sizeof o);
        fmt_int(o + str_len(o), song.octave);
        hui_chip(bx - hui_chip_w(o), 4, o, HC_GREY, 0, T_BG);
        bx -= hui_chip_w(o) + 5;
    }
    cv_text_fit(x, 2, &AF_M, b, T_TEXT, T_BG, bx - x);
    if (usb.config && !usb.suspended)
        cv_icon_mid(198, HU_HEAD / 2, 16, ICON_X_USB, T_MID, T_BG);
    draw_battery(214);
    cv_rect(4, HU_HEAD - 1, 232, 1, T_LINE);
    cv_blit(0, 0);
}

typedef struct { uint32_t kc; const char *act, *alt; uint16_t fill, ink; } hint_t;
static const char *hint_word(const hint_t *h, uint32_t pass, uint32_t i)
{
    (void)i;
    if (pass >= 2u && h->kc == KC_HOME)
        return "";
    return pass ? h->alt : h->act;
}

/* HOME's foot: the four knobs as they are now, KNOB 1..4 left to right under the knobs' order: the name in the
 * colour of what it sets (yellow the sound, red the style, the mix's for the tempo), the value under it */
#define HU_KNOBS_Y 200
static uint16_t hui_card_col(uint32_t k)
{
    return k >= HK_FILT && k <= HK_TAPE ? HC_YELLOW : k >= HK_STRUM && k <= HK_LEVEL ? HC_RED : k <= HK_JOY ? HC_GREY : HU_COL(HU_MIX);
}
static void hui_draw_knobs(void)
{
    const uint8_t *kn = HU_HOME_KNOBS[hui_c()->play % HP_COUNT];
    uint32_t i, sig = 0x4B4E0Bu + ux.gen * 7919u + song.sel * 31u;
    char v[20];
    int32_t f;
    for (i = 0; i < 4u; i++)
        if (kn[i]) {
            hui_card(kn[i], v, &f);
            sig = str_hash(sig + kn[i] * 13u + i, v);
        }
    if (!hui.force && sig == hui.sig[3])
        return;
    hui.sig[3] = sig;
    cv_begin(240, 240 - HU_KNOBS_Y, T_BG);
    cv_rect(4, 0, 232, 1, T_LINE);
    for (i = 0; i < 4u; i++) {
        int32_t x = 4 + (int32_t)i * 59;
        if (!kn[i])
            continue;
        hui_card(kn[i], v, &f);
        cv_text_c(x + 28, 3, &AF_S, HK_NAME[kn[i]], hui_card_col(kn[i]), T_BG);
        if (text_w(&AF_M, v) <= 54)
            cv_text_c(x + 28, 19, &AF_M, v, T_TEXT, T_BG);
        else
            cv_text_fit(x + 1, 21, &AF_S, v, T_TEXT, T_BG, 54);
    }
    cv_blit(0, HU_KNOBS_Y);
}

/* the footer: a menu's hints (HOME: its knobs) */
static void hui_draw_foot(void)
{
    uint32_t sig = hui.screen * 7u + 1u + hui.armed * 101u + (hui.ly && hui.ly_open ? 65537u * hui.ly : 0u) +
                   (hui.screen == HU_PICK ? hui.pick_kind * 11u : 0u) + ux.gen * 7919u;
    if (hui.screen == HU_HOME && !(hui.ly && hui.ly_open)) {
        hui_draw_knobs();
        return;
    }
    if (!hui.force && sig == hui.sig[3])
        return;
    hui.sig[3] = sig;
    cv_begin(240, 240 - HU_FOOT_Y, T_BG);
    {   /* keycaps and their actions, spread over the width; shorter words when the long ones do not fit */
        hint_t h[5];
        uint32_t n = 0, i, pass;
        if (hui.ly && hui.ly_open) {                     /* a held layer: what to do with it */
            h[n++] = (hint_t){KC_KEYS, "KEYS", "KEYS", T_KEY, T_INK};
            h[n++] = (hint_t){KC_K14, "KNOBS", "KNOBS", T_KEY, T_INK};
            h[n++] = (hint_t){HL_KC[hui.ly % HL_N], "LET GO", "UP", T_ROLE(HL_ROLE[hui.ly % HL_N]), HC_INK};
        } else if (hui.screen == HU_PICK) {
            h[n++] = (hint_t){KC_SELECT, "CHOOSE", "PICK", T_KEY, T_INK};
            h[n++] = (hint_t){KC_HOME, "BACK", "BACK", T_KEY, T_INK};
        } else if (hui.armed) {                          /* an action card armed: OCT+ does it */
            h[n++] = (hint_t){KC_OCTUP, HK_NAME[hui.armed % HK_N], HK_NAME[hui.armed % HK_N], HU_COL(hui.screen), HC_INK};
            h[n++] = (hint_t){KC_OCTDN, "CANCEL", "NO", T_KEY, T_INK};
            if (hui.screen == HU_PRESET)
                h[n++] = (hint_t){KC_SAVE, "NEXT", "NEXT", HU_COL(HU_PRESET), HC_INK};
            h[n++] = (hint_t){KC_HOME, "BACK", "BACK", T_KEY, T_INK};
        } else {                                         /* a menu: the knobs, SELECT, its button again, HOME */
            static const uint8_t KC_OF[HU_N] = {0, KC_SCL, KC_FX, 0, KC_SAVE, KC_SEQ, 0, KC_GLO};
            uint32_t sc = hui.screen;
            h[n++] = (hint_t){KC_K14, "SET", "SET", T_KEY, T_INK};
            if (sc != HU_LOOP)
                h[n++] = (hint_t){KC_SELECT, sc == HU_PRESET ? "SLOT" : "PAGE", sc == HU_PRESET ? "SLOT" : "PAGE", T_KEY, T_INK};
            h[n++] = (hint_t){KC_OF[sc], sc == HU_LOOP ? "BACK" : "NEXT", sc == HU_LOOP ? "BACK" : "NEXT", HU_COL(sc), HC_INK};
            if (sc != HU_LOOP)
                h[n++] = (hint_t){KC_HOME, "BACK", "BACK", T_KEY, T_INK};
        }
        for (pass = 0; pass < 3u; pass++) {             /* the long words; the short ones; the short ones without
                                                          * HOME's (its keycap says BACK) */
            int32_t w = 0, x = 4, gap;
            for (i = 0; i < n; i++)
                w += kc_w(h[i].kc) + (hint_word(&h[i], pass, i)[0] ? 3 + text_w(&AF_S, hint_word(&h[i], pass, i)) : 0);
            gap = (232 - w) / (int32_t)(n - 1u);
            if (gap < 4 && pass < 2u)
                continue;                                /* too wide: shorter */
            if (gap > 14)
                gap = 14;
            if (gap < 2)
                gap = 2;
            for (i = 0; i < n; i++) {
                const char *a = hint_word(&h[i], pass, i);
                x = cv_keycap(x, 6, h[i].kc, h[i].fill, h[i].ink, T_BG);
                if (a[0])
                    x = cv_text_on(x + 3, 5, &AF_S, a, T_MID, T_BG);   /* (on the keycap centre line) */
                x += gap;
            }
            break;
        }
    }
    cv_rect(4, 0, 232, 1, T_LINE);
    cv_blit(0, HU_FOOT_Y);
}

/* HOME in SEQUENCER, DRUM, DRUM LOOP and MIXER mode: the steps, the pads, the loop, the layers (the chord area
 * and the keyboard strip: 190 rows from HU_HEAD) */
static void hui_loop_strip(int32_t y);                 /* (below) */
static void hui_draw_mixer(void);
static void hui_draw_mode_body(void)
{
    const hc_trk_t *c = hui_c();
    uint32_t i, j, held = fm1_in.notes;
    char b[20];
    cv_begin(240, 124, T_BG);
    if (c->play == HP_SEQ) {                             /* 16 cells, 4 x 4: the step's chord, the cursor, the step playing */
        for (i = 0; i < 16u; i++) {
            int32_t x = 4 + (int32_t)(i % 4u) * 59, y = 4 + (int32_t)(i / 4u) * 30;
            int in = i < hcs.len, cur = i == hcs.cur, now = hcs.running && i == hcs.step;
            hchord_t ch;
            uint16_t bg = !in ? T_BG : now ? T_DEG(hcs.st[i].deg % 7u) : T_SURF;
            uint16_t fg = !in ? T_DIM : now ? HC_INK : T_TEXT;
            cv_rrect(x, y, 56, 27, 3, bg, T_BG);
            if (cur)
                cv_rrect(x, y, 56, 27, 3, T_ACCENT, T_BG), cv_rrect(x + 2, y + 2, 52, 23, 2, bg, T_ACCENT);
            if (in && hcs_step_chord(TSEL, i, &ch, b, 0))
                cv_text_c(x + 28, y + 5, &AF_M, b, hcs.st[i].deg < 7u && !now ? T_DEG(hcs.st[i].deg) : fg, bg);
            else if (in)
                cv_text_c(x + 28, y + 7, &AF_S, "-", T_DIM, bg);
        }
        cv_blit(0, HU_HEAD);
        cv_begin(240, HU_KNOBS_Y - HU_HEAD - 124, T_BG);
        cv_text_on(4, 2, &AF_S, "KEYS WRITE   F#3 REST   PLAY RUNS", T_MID, T_BG);
        cv_text_on(4, 26, &AF_M, HC_SOUNDS[c->sound % HC_NSOUNDS].name, HC_YELLOW, T_BG);
        cv_blit(0, HU_HEAD + 124);
        return;
    }
    if (c->play == HP_DRUM) {                            /* the seven pads, lit while held; the direction = AUTO-DRUM */
        for (i = 0; i < 7u; i++) {
            int32_t x = 4 + (int32_t)(i % 4u) * 59, y = 10 + (int32_t)(i / 4u) * 52;
            uint32_t on = 0, k;
            for (k = 0; k < 27u; k++)
                if (!key_black(k) && key_place(k) % 7u == i && ((held >> k) & 1u))
                    on = 1;
            cv_rrect(x, y, 56, 46, 3, on ? T_DEG(i) : T_SURF, T_BG);
            cv_text_c(x + 28, y + 15, &AF_S, HC_PAD_NAME[i], on ? HC_INK : T_TEXT, on ? T_DEG(i) : T_SURF);
        }
        cv_rrect(181, 62, 56, 46, 3, hc.dir != HD_NONE ? HC_YELLOW : T_SURF, T_BG);
        cv_text_c(209, 68, &AF_S, "AUTO", hc.dir != HD_NONE ? HC_INK : T_MID, hc.dir != HD_NONE ? HC_YELLOW : T_SURF);
        cv_text_c(209, 84, &AF_S, hc.dir != HD_NONE ? HR_NAME[hcd_rate_of_dir(hc.dir)] : "OFF", hc.dir != HD_NONE ? HC_INK : T_DIM,
                  hc.dir != HD_NONE ? HC_YELLOW : T_SURF);
        cv_blit(0, HU_HEAD);
        cv_begin(240, HU_KNOBS_Y - HU_HEAD - 124, T_BG);
        cv_text_on(4, 2, &AF_S, "WHITE KEYS: PADS   BLACK KEY: REPEAT", T_MID, T_BG);
        hui_loop_strip(26);
        cv_blit(0, HU_HEAD + 124);
        return;
    }
    if (c->play == HP_DRUMLOOP) {                        /* the styles, the variation, the 16 steps of 6 lanes */
        for (i = 0; i < DL_STYLES; i++) {                /* the styles: four, then three */
            int32_t x = 4 + (int32_t)(i % 4u) * 59, y = 2 + (int32_t)(i / 4u) * 20;
            int on = i == c->dl_style;
            cv_rrect(x, y, 56, 18, 3, on ? HC_RED : T_SURF, T_BG);
            cv_text_c(x + 28, y + 3, &AF_S, DL_NAME[i], on ? T_INK : T_MID, on ? HC_RED : T_SURF);
        }
        cv_rrect(181, 22, 56, 18, 3, hcs.dl_running ? HC_GREEN : T_SURF, T_BG);
        cv_text_c(209, 25, &AF_S, hcs.dl_running ? "RUNNING" : "STOPPED", hcs.dl_running ? HC_INK : T_DIM, hcs.dl_running ? HC_GREEN : T_SURF);
        cv_text_on(4, 44, &AF_S, DV_NAME[c->dl_var % DV_VARS], HC_YELLOW, T_BG);
        for (j = 0; j < 6u; j++)
            for (i = 0; i < 16u; i++) {
                uint32_t on = (hcd_lanes(c->dl_style, c->dl_var, i) >> j) & 1u, now = hcs.dl_running && i == hcs.dl_step;
                int32_t x = 4 + (int32_t)i * 14, y = 60 + (int32_t)j * 10;
                cv_rrect(x, y, 12, 8, 2, on ? (now ? T_TEXT : T_DEG(j)) : now ? T_RAISE : T_SURF, T_BG);
            }
        cv_blit(0, HU_HEAD);
        cv_begin(240, HU_KNOBS_Y - HU_HEAD - 124, T_BG);
        cv_text_on(4, 2, &AF_S, "WHITE KEYS: STYLE   PLAY RUNS", T_MID, T_BG);
        hui_loop_strip(26);
        cv_blit(0, HU_HEAD + 124);
        return;
    }
    if (c->play == HP_HIRO) {                           /* the song's chords coming, the one due big, the score */
        const hcg_song_t *sg = &HCG_SONG[hcg.song % HG_SONGS];
        cv_text_on(4, 2, &AF_S, sg->name, HC_RED, T_BG);
        cv_text_r(236, 2, &AF_S, HGD_NAME[hcg.diff % HGD_COUNT], T_MID, T_BG);
        if (!hcg.running && !hcg.done) {
            cv_text_c(120, 44, &AF_M, "PLAY STARTS THE SONG", T_DIM, T_BG);
            cv_text_c(120, 66, &AF_S, "PRESS EACH CHORD ON ITS BEAT", T_DIM, T_BG);
        } else {
            for (i = 0; i < 4u && hcg.idx + i < sg->n; i++) {   /* the chord due and the three after it */
                uint32_t d = sg->ev[hcg.idx + i].deg, dir = sg->ev[hcg.idx + i].dir;
                int32_t x = 4 + (int32_t)i * 59;
                uint16_t bg = i ? T_SURF : (hcg.hit ? (hcg.last == HGR_MISS ? HC_RED : HC_GREEN) : T_DEG(d % 7u));
                str_cpy(b, HC_DEG_NAME[d % 7u], sizeof b);
                if (dir == HD_UR) str_cpy(b + str_len(b), "7", sizeof b - str_len(b));
                if (dir == HD_RIGHT) str_cpy(b + str_len(b), "M7", sizeof b - str_len(b));
                cv_rrect(x, 20, 56, 40, 3, bg, T_BG);
                cv_text_c(x + 28, 28, i ? &AF_M : &AF_L, b, i ? T_TEXT : HC_INK, bg);
            }
            if (hcg.running) {                           /* the beat coming: a bar fills up to the chord's time */
                int32_t left = (int32_t)hcg.next_at - (int32_t)hcg.pos, full = (int32_t)(hcg_beat() * 4u);
                int32_t w = left <= 0 ? 232 : left >= full ? 0 : 232 - left * 232 / full;
                cv_rrect(4, 66, 232, 3, 2, T_LINE, T_BG);
                cv_rrect(4, 66, w, 3, 2, HC_YELLOW, T_LINE);
            }
            cv_text_on(4, 80, &AF_M, HGR_NAME[hcg.last % 5u], hcg.last == HGR_MISS ? HC_RED : HC_GREEN, T_BG);
            fmt_int(b, hcg.score);
            cv_text_r(236, 80, &AF_M, b, T_TEXT, T_BG);
            str_cpy(b, "COMBO ", sizeof b);
            fmt_int(b + str_len(b), hcg.combo);
            cv_text_on(4, 102, &AF_S, b, T_MID, T_BG);
            if (hcg.done) {
                str_cpy(b, "DONE  ", sizeof b);
                fmt_int(b + str_len(b), hcg.perfect + hcg.great + hcg.ok);
                str_cpy(b + str_len(b), " HIT  ", sizeof b - str_len(b));
                fmt_int(b + str_len(b), hcg.miss);
                str_cpy(b + str_len(b), " MISSED", sizeof b - str_len(b));
                cv_text_r(236, 102, &AF_S, b, HC_YELLOW, T_BG);
            }
        }
        cv_blit(0, HU_HEAD);
        cv_begin(240, HU_KNOBS_Y - HU_HEAD - 124, T_BG);
        cv_text_on(4, 2, &AF_S, "PLAY STARTS AND STOPS THE SONG", T_MID, T_BG);
        cv_text_on(4, 26, &AF_M, HC_SOUNDS[c->sound % HC_NSOUNDS].name, HC_YELLOW, T_BG);
        cv_blit(0, HU_HEAD + 124);
        return;
    }
    if (c->play == HP_EAR) {                             /* the level, the phase, the streak */
        str_cpy(b, "LEVEL ", sizeof b);
        fmt_int(b + str_len(b), hcg.level + 1);
        cv_text_on(4, 2, &AF_S, b, HC_RED, T_BG);
        cv_text_r(236, 2, &AF_S, HGE_LEVEL_NAME[hcg.level % HGE_LEVELS], T_MID, T_BG);
        if (!hcg.phase)
            cv_text_c(120, 44, &AF_M, "PLAY STARTS A ROUND", T_DIM, T_BG);
        else if (hcg.phase == 1u)
            cv_text_c(120, 40, &AF_L, "LISTEN", HC_YELLOW, T_BG);
        else if (hcg.phase == 2u) {
            cv_text_c(120, 40, &AF_L, "YOUR TURN", HC_GREEN, T_BG);
            if (hcg.qn > 1u) {
                str_cpy(b, "CHORD ", sizeof b);
                fmt_int(b + str_len(b), hcg.ans + 1);
                str_cpy(b + str_len(b), " OF ", sizeof b - str_len(b));
                fmt_int(b + str_len(b), hcg.qn);
                cv_text_c(120, 80, &AF_S, b, T_MID, T_BG);
            }
        } else if (hcg.result == 1u)
            cv_text_c(120, 40, &AF_L, "CORRECT", HC_GREEN, T_BG);
        else {
            cv_text_c(120, 40, &AF_L, "NO", HC_RED, T_BG);
            str_cpy(b, "IT WAS ", sizeof b);
            str_cpy(b + str_len(b), HC_DEG_NAME[hcg.wrong_was % 7u], sizeof b - str_len(b));
            cv_text_c(120, 80, &AF_M, b, T_MID, T_BG);
        }
        str_cpy(b, "STREAK ", sizeof b);
        fmt_int(b + str_len(b), hcg.streak);
        cv_text_on(4, 102, &AF_S, b, T_MID, T_BG);
        fmt_int(b, hcg.right);
        str_cpy(b + str_len(b), " / ", sizeof b - str_len(b));
        fmt_int(b + str_len(b), hcg.total);
        cv_text_r(236, 102, &AF_S, b, T_MID, T_BG);
        cv_blit(0, HU_HEAD);
        cv_begin(240, HU_KNOBS_Y - HU_HEAD - 124, T_BG);
        cv_text_on(4, 2, &AF_S, "F#3 REPLAYS   PLAY: A NEW ROUND", T_MID, T_BG);
        cv_text_on(4, 26, &AF_M, HC_SOUNDS[c->sound % HC_NSOUNDS].name, HC_YELLOW, T_BG);
        cv_blit(0, HU_HEAD + 124);
        return;
    }
    hui_draw_mixer();
}

/* the looper on the drum screens, to time REC (the bounce): the layers' dots, the live layer's state
 * (recording: the bar it is in), a bar of the loop with its bars ticked (a free first recording fills
 * against BARS, or the bar it is in) */
static void hui_loop_strip(int32_t y)
{
    uint32_t i, bar = beat_samples() * 4u, sel = hcl.sel % HCL_LAYERS, st = hcl.l[sel].state, total = hcl.len, pos = hcl.pos;
    char b[24];
    if (!bar)
        bar = 1;
    for (i = 0; i < HCL_LAYERS; i++) {
        uint32_t s = hcl.l[i].state;
        uint16_t col = s == HLS_PLAY ? HC_GREEN : s == HLS_REC ? T_REC : s == HLS_ARMED ? HC_YELLOW : T_RAISE;
        cv_rrect(4 + (int32_t)i * 12, y + 2, 9, 9, 4, col, T_BG);
        if (i == sel)
            cv_rrect(6 + (int32_t)i * 12, y + 4, 3, 5, 2, s == HLS_OFF || s == HLS_ARMED ? T_TEXT : HC_INK, col);
    }
    if (st == HLS_REC) {
        uint32_t el = hc.clock - hcl.l[sel].start;
        if (!total) {                                    /* the first layer, free: against BARS, else the bar it is in */
            total = hcl.bars ? hcl.bars * bar : bar;
            pos = el % total;
        }
        str_cpy(b, "REC  BAR ", sizeof b);
        fmt_int(b + str_len(b), (int32_t)(el / bar) + 1);
        cv_text_on(56, y, &AF_S, b, T_REC, T_BG);
    } else {
        str_cpy(b, "LAYER ", sizeof b);
        fmt_int(b + str_len(b), (int32_t)sel + 1);
        str_cpy(b + str_len(b), st == HLS_PLAY ? "  PLAY" : st == HLS_ARMED ? "  ARMED" : "  OFF", sizeof b - str_len(b));
        cv_text_on(56, y, &AF_S, b, st == HLS_PLAY ? HC_GREEN : st == HLS_ARMED ? HC_YELLOW : T_MID, T_BG);
    }
    cv_text_r(236, y, &AF_S, st == HLS_REC ? "REC: STOP" : "REC: BOUNCE", T_DIM, T_BG);
    cv_rrect(4, y + 15, 232, 4, 1, T_LINE, T_BG);
    if (total) {
        uint32_t bars = (total + bar / 2u) / bar;
        for (i = 1; i < bars && i < 32u; i++)
            cv_rrect(4 + (int32_t)(i * 232u / bars), y + 14, 1, 3, 0, T_MID, T_BG);
        cv_rrect(4, y + 15, 2 + (int32_t)((uint64_t)pos * 230u / total), 4, 1,
                 st == HLS_REC ? T_REC : hcl.playing ? HC_GREEN : T_MID, T_LINE);
    }
}

/* MIX (GLO): the four layers, their level, mute, state, the selected one raised; the metronome */
static void hui_draw_mixer(void)
{
    uint32_t i;
    char b[20];
    for (i = 0; i < HCL_LAYERS; i++) {
        int32_t x = 4 + (int32_t)i * 59, lvl = trk[i].p[P_LEVEL];
        uint32_t st = hcl.l[i].state;
        int mute = trk[i].p[P_MUTE] != 0;
        uint16_t col = st == HLS_PLAY ? HC_GREEN : st == HLS_REC ? T_REC : T_MID;
        cv_rrect(x, 4, 56, 116, 3, i == song.sel ? T_RAISE : T_SURF, T_BG);
        b[0] = 'L'; b[1] = (char)('1' + i); b[2] = 0;
        cv_text_c(x + 28, 8, &AF_M, b, i == song.sel ? HU_LCOL(i) : col, i == song.sel ? T_RAISE : T_SURF);
        cv_rrect(x + 22, 30, 12, 60, 3, T_LINE, i == song.sel ? T_RAISE : T_SURF);
        cv_rrect(x + 22, 30 + 60 - lvl * 60 / 127, 12, lvl * 60 / 127, 3, mute ? T_DIM : HU_LCOL(i), T_LINE);
        cv_rrect(x + 6, 96, 44, 18, 4, mute ? HC_RED : T_LINE, i == song.sel ? T_RAISE : T_SURF);
        cv_text_c(x + 28, 98, &AF_S, mute ? "MUTE" : HLS_NAME[st & 3u], mute ? T_INK : T_MID, mute ? HC_RED : T_LINE);
    }
    cv_blit(0, HU_HEAD);
    cv_begin(240, HU_KNOBS_Y - HU_HEAD - 124, T_BG);
    cv_text_on(4, 2, &AF_S, "ALGORITHM LAYER   OCT LEVEL", T_MID, T_BG);
    hui_chip(4, 24, hcl.metro ? "METRONOME ON" : "METRONOME OFF", hcl.metro ? HC_GREEN : T_MID, hcl.metro, T_BG);
    cv_blit(0, HU_HEAD + 124);
}


/* LEAD: the notes the held (or latched) white keys of track k play, ascending, into hui_lead_buf; how many */
static uint8_t hui_lead_buf[16];
static uint32_t hui_lead_notes(uint32_t k, uint32_t held)
{
    uint32_t i, j, n = 0;
    for (i = 0; i < 27u; i++)
        if (((held >> i) & 1u) && !key_black(i) && kb_chn[i] && kb_trk[i] % NTRK == k && n < 16u) {
            uint8_t x = kb_chord[i][0];
            for (j = n; j > 0 && hui_lead_buf[j - 1u] > x; j--)
                hui_lead_buf[j] = hui_lead_buf[j - 1u];
            hui_lead_buf[j] = x;
            n++;
        }
    return n;
}

/* the chord that sounds, on a piano of four octaves: its notes in the degree's colour, the root marked
 * with a dot, a note beyond the window as a dot at that edge. The window starts at the C at or below the
 * lowest note, moved up when the top note would not fit */
static void hui_draw_piano(uint32_t deg)
{
    static const uint8_t WHITE_OF[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};   /* the white key at or below */
    static const uint8_t IS_BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    uint16_t col = deg < 7u ? T_DEG(deg) : HC_YELLOW, ccol;
    uint32_t i, lo = 127, hi = 0, start, root = (uint32_t)(hc.cur.root + 1200) % 12u, held = fm1_in.notes | hc.latched;
    uint8_t on[128];                                     /* 1 a note of the chord, 2 a strumplate note */
    memset(on, 0, sizeof on);
    if (hui_c()->play == HP_LEAD) {                      /* LEAD: the notes held */
        uint32_t n = hui_lead_notes(song.sel, held);
        for (i = 0; i < n; i++)
            on[hui_lead_buf[i]] = 1;
    } else {
        for (i = 0; i < hc.cur.n; i++)
            if (hc.cur.note[i] <= 127u)
                on[hc.cur.note[i]] = 1;
    }
    for (i = 0; i < 27u; i++)
        if (((held >> i) & 1u) && hc_plate[i])
            on[hc_plate[i]] = 2;
    for (i = 0; i < 128u; i++)
        if (on[i]) {
            if (i < lo) lo = i;
            if (i > hi) hi = i;
        }
    if (lo > hi)
        return;
    ccol = hc.nheld || hc.latched ? col : T_MID;         /* the chord in colour while a chord key sounds, else dim:
                                                          * the plate strums the chord last played */
    start = lo - lo % 12u;
    if (hui_c()->play == HP_LEAD) {                      /* LEAD: the window stands on the first key's octave, so a
                                                          * note climbing the keys climbs the piano (no re-basing) */
        uint32_t d, f = (uint32_t)hc_lead_note(TSEL, 0u, &d);
        if (f - f % 12u <= lo)
            start = f - f % 12u;
    }
    if (hi >= start + 48u) {
        start = hi - 47u;
        start += (12u - start % 12u) % 12u;
    }
    for (i = 0; i < 28u; i++)                            /* the white keys, raised; the black ones cut out of them */
        cv_rrect(8 + (int32_t)i * 8, 2, 7, 54, 1, T_RAISE, T_BG);
    for (i = start; i < start + 48u && i < 128u; i++)
        if (on[i] && !IS_BLACK[i % 12u]) {
            int32_t x = 8 + (int32_t)((i - start) / 12u * 7u + WHITE_OF[i % 12u]) * 8;
            uint16_t c = on[i] == 2u ? T_TEXT : ccol;
            cv_rrect(x, 2, 7, 54, 1, c, T_BG);
            if (on[i] == 1u && i % 12u == root)
                cv_rrect(x + 2, 48, 3, 3, 1, T_INK, c);
        }
    for (i = start; i < start + 48u && i < 128u; i++)
        if (IS_BLACK[i % 12u]) {
            int32_t x = 8 + (int32_t)((i - start) / 12u * 7u + WHITE_OF[i % 12u]) * 8 + 5;
            uint16_t c = on[i] == 2u ? T_TEXT : on[i] ? ccol : T_BG;
            cv_rect(x - 1, 2, 7, 33, T_BG);
            cv_rrect(x, 2, 5, 32, 1, c, T_BG);
            if (on[i] == 1u && i % 12u == root)
                cv_rrect(x + 1, 26, 3, 3, 1, T_INK, c);
        }
    for (i = 0; i < 128u; i++)                           /* a note beyond the window: a dot at that edge */
        if (on[i] && (i < start || i >= start + 48u))
            cv_rrect(i < start ? 2 : 234, 26, 4, 4, 2, on[i] == 2u ? T_TEXT : ccol, T_BG);
}

/* HOME's middle: the chord big, its degree and modifier; the keyboard; the sound and the effects */
static void hui_draw_home(void)
{
    const hc_trk_t *c = hui_c();
    uint32_t k = song.sel, held = fm1_in.notes | hc.latched, i, fxm = hui_fx_mask(c);
    const char *name = chord_last[k].n && chord_last[k].name[0] ? chord_last[k].name : hc.name[0] ? hc.name : "";
    uint32_t deg = 7, lock = 0, invk = 0, sig;
    char lead[40];
    if (c->play == HP_LEAD) {                            /* LEAD: the notes that sound, named low to high (D4 F4 A4) */
        uint32_t n = hui_lead_notes(k, held), j;
        lead[0] = 0;
        if (!n && hc.lead_note)                          /* nothing held: the last one */
            hui_lead_buf[n++] = (uint8_t)(hc.lead_note - 1u);
        for (j = 0; j < n && str_len(lead) < 30u; j++) {
            uint32_t r = hui_lead_buf[j];
            if (j)
                str_cpy(lead + str_len(lead), " ", sizeof lead - str_len(lead));
            str_cpy(lead + str_len(lead), HC_NOTE_NAME[r % 12u], sizeof lead - str_len(lead));
            fmt_int(lead + str_len(lead), (int32_t)(r / 12u) - 1);
        }
        name = lead;
    }
    sig = str_hash(11u, name) + held * 31u + hc.dir * 7u + hc.cur_dir * 13u + fxm * 1013u + c->sound * 4099u +
          hc.hold * 65537u + chord_last[k].gen * 3u;
    for (i = hc.nheld; i-- > 0;)                         /* the degree and lock of the chord key: pressed last
                                                          * (BASS SLASH: the key held first is only the bass) */
        if (!key_black(hc.order[i])) {
            int32_t o;
            deg = hc_degree_of_key(TSEL, hc.order[i], &o) % 7u;
            lock |= hc.lock[hc.order[i]].on;
            invk = hc.inv[hc.order[i]];
            break;
        }
    if (deg == 7u)
        for (i = 0; i < 27u; i++)                        /* (latched keys: HOLD) */
            if (((held >> i) & 1u) && !key_black(i)) {
                int32_t o;
                deg = hc_degree_of_key(TSEL, i, &o) % 7u;
                lock |= hc.lock[i].on;
            }
    if (c->play == HP_LEAD) {                            /* LEAD: the degree of a single note (a seven-note scale) */
        deg = hc.lead_note && hc.lead_deg < 7u && hui_lead_notes(k, held) < 2u ? hc.lead_deg : 7u;
        lock = invk = 0;
    }
    sig += deg * 101u + lock * 7u + invk * 131u + hc.cur.n * 131u + (uint32_t)(hc.cur.root + 1200) * 3u + hc.lead_note * 5u;
    for (i = 0; i < hc.cur.n && i < HS_N; i++)           /* the piano: the chord's notes */
        sig += (uint32_t)hc.cur.note[i] * (7u + i * 3u);
    for (i = 0; i < HCL_LAYERS; i++)
        sig += (hcl.l[i].state + 1u) * (3001u << i);
    sig += hcl.playing * 7u + (hcl.len ? hcl.pos * 24u / hcl.len : 0u) * 51u + hcl.sel * 5u;
    if (hcl.l[hcl.sel % HCL_LAYERS].state == HLS_REC)   /* a recording: its eighths (the drum screens' strip) */
        sig += ((hc.clock - hcl.l[hcl.sel % HCL_LAYERS].start) / (beat_samples() / 2u + 1u)) * 29u;
    sig += hcg.running * 53u + hcg.done * 59u + hcg.idx * 61u + hcg.last * 67u + hcg.score * 71u + hcg.combo * 73u +
           hcg.phase * 79u + hcg.ans * 83u + hcg.result * 89u + hcg.streak * 97u + hcg.total * 101u + hcg.level * 103u +
           hcg.song * 107u + (hcg.running ? (hcg.pos / 2048u) * 109u : 0u);
    sig += c->play * 7919u + hcs.cur * 13u + hcs.step * 17u + hcs.running * 3u + hcs.dl_running * 5u + hcs.dl_step * 19u +
           hcs.len * 23u + c->dl_style * 29u + c->dl_var * 31u + hcs.pads * 37u + hcl.metro * 41u;
    for (i = 0; i < NTRK; i++)
        sig += (uint32_t)(trk[i].p[P_LEVEL] + 1) * (43u << i) + (uint32_t)trk[i].p[P_MUTE] * (47u << i);
    if (c->play >= HP_SEQ) {                             /* SEQUENCER, DRUM, DRUM LOOP, MIXER: their own body */
        if (!hui.force && sig == hui.sig[1])
            return;
        hui.sig[1] = sig;
        hui_draw_mode_body();
        return;
    }
    if (!hui.force && sig == hui.sig[1])
        return;
    hui.sig[1] = sig;
    /* the chord */
    cv_begin(240, 98, T_BG);
    for (i = 0; i < HCL_LAYERS; i++) {                   /* the looper's layers: OFF dim, ARMED yellow, REC red, PLAY green */
        uint32_t st = hcl.l[i].state;
        uint16_t c = st == HLS_PLAY ? HC_GREEN : st == HLS_REC ? T_REC : st == HLS_ARMED ? HC_YELLOW : T_RAISE;
        cv_rrect(8 + (int32_t)i * 14, 6, 10, 10, 5, c, T_BG);
        if (i == song.sel)
            cv_rrect(10 + (int32_t)i * 14, 8, 6, 6, 3, st == HLS_OFF || st == HLS_ARMED ? T_TEXT : HC_INK, c);
    }
    if (hcl.len) {
        cv_rrect(8, 19, 52, 3, 1, T_LINE, T_BG);
        cv_rrect(8, 19, 2 + (int32_t)(hcl.pos * 50u / hcl.len), 3, 1, hcl.playing ? HC_GREEN : T_MID, T_LINE);
    }
    {
        uint16_t col = deg < 7u ? T_DEG(deg) : T_THEME;
        int32_t w = text_w(&AF_X, name[0] ? name : "-");
        if (w > 232) {
            cv_text_c(120, 30, &AF_L, name, col, T_BG);
        } else {
            cv_text_c(120, 18, &AF_X, name[0] ? name : "-", col, T_BG);
        }
        if (deg < 7u) {
            char b[32];
            str_cpy(b, HC_DEG_NAME[deg], sizeof b);
            if (hc.dir != HD_NONE) {
                str_cpy(b + str_len(b), "  ", sizeof b - str_len(b));
                str_cpy(b + str_len(b), HC_DIR_NAME[hc.dir], sizeof b - str_len(b));
            }
            if (invk) {                                  /* the key's inversion (INVERT): 1ST / 2ND */
                str_cpy(b + str_len(b), "  ", sizeof b - str_len(b));
                str_cpy(b + str_len(b), invk == 1u ? "1ST INV" : "2ND INV", sizeof b - str_len(b));
            }
            cv_text_c(120, 76, &AF_M, b, T_MID, T_BG);
        } else if (!name[0]) {
            cv_text_c(120, 76, &AF_M, "PLAY A WHITE KEY", T_DIM, T_BG);
        }
        if (lock)
            hui_chip(236 - hui_chip_w("LOCKED"), 80, "LOCKED", HC_RED, 1, T_BG);
        if (hc.hold)
            hui_chip(4, 80, "HOLD", HC_GREEN, 1, T_BG);
    }
    cv_blit(0, HU_HEAD);
    /* the keyboard: 16 white keys, 11 black, the held ones in their degree's colour, the directions dim */
    cv_begin(240, HU_KNOBS_Y - HU_HEAD - 98, T_BG);
    if (held && (c->play == HP_LEAD ? hui_lead_notes(k, held) > 0u : hc.cur.n > 0u)) {   /* a chord (LEAD: notes)
                                                                                           * sounds: the piano */
        hui_draw_piano(deg);
    } else {
        int32_t ww = 13, x0 = 8;                        /* 16 white keys 14 apart, centred */
        for (i = 0; i < 27u; i++) {
            uint32_t p = key_place(i), on = (held >> i) & 1u;
            if (!key_black(i)) {
                int32_t o, j = hc_plate_of_key(TSEL, i), x = x0 + (int32_t)p * (ww + 1);
                uint32_t d = hc_degree_of_key(TSEL, i, &o) % 7u;
                char dn[4] = {(char)('1' + d), 0};
                uint16_t f = on ? T_DEG(d) : T_RAISE;
                if (c->play == HP_LEAD) {                /* LEAD: every key its degree of the scale, 1 up */
                    uint32_t ld;
                    hc_lead_note(TSEL, i, &ld);
                    fmt_int(dn, (int32_t)ld + 1);
                    d = ld % 7u;
                    f = on ? T_DEG(d) : T_RAISE;
                }
                if (j >= 0) {                            /* a strumplate key: a bar as high as its note */
                    int32_t h = 4 + j * 2;
                    f = on ? T_TEXT : T_SURF;
                    cv_rrect(x, 2, ww, 54, 2, f, T_BG);
                    cv_rrect(x + 4, 52 - h, ww - 8, h, 1, on ? T_BG : T_DIM, f);
                    continue;
                }
                cv_rrect(x, 2, ww, 54, 2, f, T_BG);
                if (!on)
                    cv_rrect(x + 3, 50, ww - 6, 3, 1, T_DEG(d), f);   /* the degree's colour */
                cv_text_c(x + ww / 2 + 1, 34, &AF_S, dn, on ? T_INK : T_TEXT, f);   /* its number */
            }
        }
        for (i = 0; i < 27u; i++) {
            uint32_t p = key_place(i), on = (held >> i) & 1u;
            if (key_black(i)) {
                static const uint8_t AFTER[HC_NBLACK] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};   /* the white key it follows */
                int32_t x = x0 + (int32_t)AFTER[p] * (ww + 1) + ww - 4;
                uint16_t col = on ? (p < 3u ? HC_GREEN : HC_YELLOW) : T_BG;
                cv_rect(x - 1, 2, 11, 33, T_BG);         /* a gap round it */
                if (!on)
                    cv_rrect(x, 2, 9, 32, 2, T_LINE, T_BG), cv_rrect(x + 1, 2, 7, 31, 2, T_BG, T_LINE);
                else
                    cv_rrect(x, 2, 9, 32, 2, col, T_BG);
                if (p < 3u) {                            /* INVERT LOCK HOLD */
                    char fl[2] = {"ILH"[p], 0};
                    cv_text_c(x + 5, 10, &AF_S, fl, on ? T_INK : T_MID, col);
                } else {                                 /* a joystick direction: the dot where it points */
                    static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1}, DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
                    uint32_t dir = HC_BLACK_FN[p] - HD_UP;
                    cv_rrect(x + 3 + DX[dir % 8u] * 2, 16 + DY[dir % 8u] * 6, 3, 3, 1, on ? T_INK : T_MID, col);
                }
            }
        }
    }
    {
        /* the sound and its effects */
        cv_text_on(4, 59, &AF_M, HC_SOUNDS[c->sound % HC_NSOUNDS].name, HC_YELLOW, T_BG);
        {
            static const char *const CHIP[12] = {"REV", "DLY", "CHO", "FLG", "TRM", "LFO", "GLD", "DRV", "TAPE", "FLT", "HP", "ST"};
            int32_t x = 236;
            for (i = 12; i > 0; i--)
                if ((fxm >> (i - 1u)) & 1u) {
                    x -= hui_chip_w(CHIP[i - 1u]) + 3;
                    hui_chip(x, 60, CHIP[i - 1u], HC_YELLOW, 0, T_BG);
                }
        }
    }
    cv_blit(0, HU_HEAD + 98);
}

/* a menu's or a list's title bar: the title on a chip in its colour, what it holds at the right, a rule */
static void hui_title(const char *title, const char *sub, uint16_t col)
{
    cv_begin(240, HU_ROWS_Y, T_BG);
    if (hui.msg_t) {
        hui_draw_msg();
    } else {
        int32_t w = text_w(&AF_M, title) + 14;
        cv_rrect(4, 3, w, 18, 3, col, T_BG);
        cv_text_c(4 + w / 2, 2, &AF_M, title, T_INK, col);
        cv_text_r(236, 4, &AF_S, sub, T_MID, T_BG);
        cv_rect(4, HU_HEAD - 1, 232, 1, T_LINE);
    }
    cv_blit(0, 0);
}

/* a row of a menu or a list: the selected one on a surface, a stripe in the menu's colour at its left; the label
 * (or a list's name) at the left, the value at the right */
static void hui_row(int32_t y, const char *label, const char *val, int sel, int act, uint16_t col)
{
    uint16_t bg = sel ? T_SURF : T_BG;
    if (sel) {
        cv_rrect(4, y, 230, HU_ROW - 2, 3, bg, T_BG);
        cv_rrect(4, y, 3, HU_ROW - 2, 1, col, bg);
    }
    if (!val) {                                          /* a list: the name alone, larger */
        cv_text_on(14, y + 1, &AF_M, label, sel ? col : T_TEXT, bg);
        return;
    }
    cv_text_on(14, y + 3, &AF_S, label, sel ? T_TEXT : T_MID, bg);
    cv_text_r(226, y + 1, &AF_M, act ? "GO" : val, act ? (sel ? col : T_DIM) : sel ? col : T_TEXT, bg);
}

/* a menu's page (after JIANT): the header (the menu's name on a chip in its colour, the page's name; at the right
 * the layer it sets and the page's place, n/N); the groups as tabs under it, a dot a page (a one-page menu: what its
 * button does again; LOOPER: the loop); then the four cards, KNOB 1..4 left to right */
#define HU_TABS_Y 26
#define HU_BODY_Y 48
#define HU_CARD_H (HU_FOOT_Y - 4 - HU_BODY_Y)
static void hui_tile_text(int32_t x, int32_t y, int32_t w, int32_t h, const char *s, uint16_t fg, uint16_t bg);   /* (below) */
static void hui_draw_page_head(void)
{
    uint32_t sc = hui.screen, n = HU_SEC[sc].n, pi = hui.page[sc] % n, i, g, per = 0;
    const hu_page_t *pg = hui_page();
    uint16_t col = HU_COL(sc);
    uint32_t sig = sc * 5u + pi * 131u + song.sel * 31u + str_hash(3u, pg->name) + (hui.msg_t ? str_hash(5u, hui.msg) : 0u) +
                   ux.gen * 7919u;
    int32_t x;
    for (i = 0; i < 4u; i++)
        per |= pg->card[i] && !hui_card_global(pg->card[i]);
    if (sc == HU_LOOP)
        for (i = 0; i < HCL_LAYERS; i++)
            sig += (hcl.l[i].state + 1u) * (7u << (3u * i)) + hcl.playing * 997u + (hcl.len ? hcl.pos * 64u / hcl.len : 0u) * 977u +
                   (hcl.l[i].state == HLS_REC ? (hc.clock - hcl.l[i].start) / (beat_samples() * 4u + 1u) * 31u : 0u);
    if (!hui.force && sig == hui.sig[0])
        return;
    hui.sig[0] = sig;
    cv_begin(240, HU_BODY_Y - 2, T_BG);
    if (hui.msg_t) {
        hui_draw_msg();
    } else {
        int32_t w = text_w(&AF_M, HU_SEC[sc].title) + 14, xr = 236;
        char b[12];
        cv_rrect(4, 3, w, 18, 3, col, T_BG);
        cv_text_c(4 + w / 2, 2, &AF_M, HU_SEC[sc].title, T_INK, col);
        if (n > 1u) {
            fmt_int(b, (int32_t)pi + 1);
            str_cpy(b + str_len(b), "/", sizeof b - str_len(b));
            fmt_int(b + str_len(b), (int32_t)n);
            xr = cv_text_r(236, 5, &AF_S, b, T_MID, T_BG) - 8;
        }
        if (per) {                                       /* the layer this page sets */
            char l[3] = {'L', (char)('1' + song.sel % NTRK), 0};
            xr -= hui_chip_w(l);
            hui_chip(xr, 4, l, HU_LCOL(song.sel), 1, T_BG);
            xr -= 8;
        }
        cv_text_fit(4 + w + 8, 2, &AF_M, pg->name, T_TEXT, T_BG, xr - (4 + w + 8));
        cv_rect(4, HU_HEAD - 1, 232, 1, T_LINE);
    }
    if (sc == HU_LOOP) {
        hui_loop_strip(HU_TABS_Y);
    } else if (n == 1u) {
        cv_text_on(4, HU_TABS_Y + 1, &AF_S, "SAVE AGAIN: THE NEXT SLOT", T_MID, T_BG);
    } else {
        for (g = 0, x = 4; g < 3u && HU_SEC[sc].group[g]; g++) {   /* the tabs: the groups, a dot a page */
            uint32_t np = 0;
            int on = pg->group == g;
            x += hui_chip(x, HU_TABS_Y, HU_SEC[sc].group[g], on ? col : T_MID, on, T_BG) + 4;
            for (i = 0; i < n; i++)
                np += HU_SEC[sc].pg[i].group == g;
            for (i = 0; i < n && np > 1u; i++)
                if (HU_SEC[sc].pg[i].group == g) {
                    cv_rrect(x, HU_TABS_Y + 6, 5, 5, 2, i == pi ? col : T_LINE, T_BG);
                    x += 8;
                }
            x += 8;
        }
    }
    cv_blit(0, 0);
}

/* the four cards: the name, the value (large, or smaller, or on two lines), a line under it (MIX: the layer's state;
 * PRESETS: the slot's name), a gauge for a number; an armed action filled in the menu's colour, saying OCT+; no card
 * an outline. MIX's cards each in their layer's colour */
static void hui_draw_page(void)
{
    uint32_t sc = hui.screen, i, pass, sig = 0xCA4Du + sc * 3u + hui.page[sc] * 17u + hui.armed * 101u + song.sel * 7u + ux.gen * 7919u;
    const hu_page_t *pg = hui_page();
    uint16_t col = HU_COL(sc);
    char v[20], sub[20];
    int32_t f;
    for (i = 0; i < 4u; i++) {
        sig = str_hash(sig + pg->card[i] * 13u, HK_NAME[pg->card[i] % HK_N]);
        if (pg->card[i]) {
            hui_card(pg->card[i], v, &f);
            hui_card_sub(pg->card[i], sub);
            sig = str_hash(str_hash(sig + (uint32_t)f, v), sub);
        }
        if (pg->card[i] >= HK_L1 && pg->card[i] <= HK_L4)
            sig += (uint32_t)trk[pg->card[i] - HK_L1].p[P_MUTE] * (31u << i);
    }
    if (!hui.force && sig == hui.sig[1])
        return;
    hui.sig[1] = sig;
    for (pass = 0; pass < 2u; pass++) {                  /* (two strips: the canvas is 124 rows; the text all in the
                                                          * first, the gauges across both) */
        int32_t top = HU_BODY_Y + (pass ? 82 : 0);
        uint32_t h = pass ? (uint32_t)(HU_FOOT_Y - HU_BODY_Y - 82) : 82u;
        cv_begin(240, h, T_BG);
        cv_oy = -top;
        for (i = 0; i < 4u; i++) {
            uint32_t k = pg->card[i];
            int32_t x = 4 + (int32_t)i * 59, y = HU_BODY_Y;
            uint16_t cc = k >= HK_L1 && k <= HK_L4 ? HU_LCOL(k - HK_L1) : col, bg, fg;
            int act, armed;
            if (!k) {
                cv_rrect(x, y, 56, HU_CARD_H, 3, T_LINE, T_BG);
                cv_rrect(x + 1, y + 1, 54, HU_CARD_H - 2, 2, T_BG, T_LINE);
                continue;
            }
            act = hui_card(k, v, &f);
            hui_card_sub(k, sub);
            armed = act && hui.armed == k;
            bg = armed ? cc : T_SURF;
            fg = armed ? T_INK : act ? cc : T_TEXT;
            cv_rrect(x, y, 56, HU_CARD_H, 3, bg, T_BG);
            cv_rrect(x, y, 56, 3, 1, cc, bg);
            cv_text_c(x + 28, y + 8, &AF_S, HK_NAME[k], armed ? T_INK : T_MID, bg);   /* (every name fits 52 px) */
            if (armed)
                str_cpy(v, "OCT+", sizeof v);
            if (text_w(&AF_M, v) <= 52)                 /* the value: M, or on one or two S lines */
                cv_text_c(x + 28, y + 28, &AF_M, v, fg, bg);
            else
                hui_tile_text(x, y + 24, 56, 34, v, fg, bg);
            if (sub[0] && text_w(&AF_S, sub) <= 52)
                cv_text_c(x + 28, y + 56, &AF_S, sub, armed ? T_INK : T_MID, bg);
            else if (sub[0])
                cv_text_fit(x + 3, y + 56, &AF_S, sub, armed ? T_INK : T_MID, bg, 50);
            if (f >= 0) {                                /* the gauge, filled from the foot */
                int32_t gh = HU_CARD_H - 88, fh = gh * f / 1000;
                int mute = k >= HK_L1 && k <= HK_L4 && trk[k - HK_L1].p[P_MUTE];
                cv_rrect(x + 22, y + 80, 12, gh, 3, T_LINE, bg);
                if (fh > 0)
                    cv_rrect(x + 22, y + 80 + gh - fh, 12, fh, fh >= 6 ? 3 : 1, mute ? T_DIM : cc, T_LINE);
            }
        }
        cv_oy = 0;
        cv_blit(0, (uint32_t)top);
    }
}

/* a list over the screen while its knob turns: SOUND the choice large between the one before and the one after
 * (no scrolling), LAYERS the four with their sound and style */
static void hui_draw_pick(void)
{
    static const char *const TITLE[2] = {"SOUND", "LAYERS"};
    static const char *const SUB[2] = {"PRESETS KNOB", "ALGORITHM KNOB"};
    uint32_t k = hui.pick_kind % 2u, n = hui_pick_count(k), cur = hui_pick_cur(k) % n, i;
    uint16_t col = k == PK_LAYER ? HU_LCOL(cur) : HC_YELLOW;
    uint32_t sig = 0x50494Bu + k * 7u + cur * 13u + n * 3u + ux.gen * 7919u;
    if (k == PK_LAYER)
        for (i = 0; i < NTRK; i++)
            sig += (hc.t[i].sound * 7u + hc.t[i].play * 3u + hcl.l[i].state + (uint32_t)trk[i].p[P_MUTE] * 5u) * (97u << i);
    if (!hui.force && sig == hui.sig[1])
        return;
    hui.sig[1] = sig;
    hui.sig[0] = 0;
    hui_title(TITLE[k], SUB[k], col);
    if (k == PK_LAYER) {
        cv_begin(240, (uint32_t)(NTRK * HU_ROW), T_BG);
        cv_oy = -HU_ROWS_Y;
        for (i = 0; i < NTRK; i++) {                     /* [L2] JUNO POLY       STRUM */
            int32_t y = HU_ROWS_Y + (int32_t)i * HU_ROW;
            uint16_t bg = i == cur ? T_SURF : T_BG;
            char lb[4] = {'L', (char)('1' + i), 0, 0};
            hui_row(y, "", trk[i].p[P_MUTE] ? "MUTED" : HP_SHORT[hc.t[i].play % HP_COUNT], i == cur, 0, HU_LCOL(i));
            hui_chip(14, y + 2, lb, HU_LCOL(i), 1, bg);
            cv_text_on(48, y + 3, &AF_S, hui_pick_name(k, i), i == cur ? T_TEXT : T_MID, bg);
        }
        cv_oy = 0;
        cv_blit(0, HU_ROWS_Y);
        return;
    }
    {
        char b[12];
        cv_begin(240, 124, T_BG);
        if (cur > 0u)
            cv_text_c(120, 8, &AF_M, hui_pick_name(k, cur - 1u), T_DIM, T_BG);
        cv_rrect(4, 38, 232, 34, 4, T_SURF, T_BG);
        cv_rrect(4, 38, 4, 34, 2, col, T_SURF);
        cv_text_c(122, 45, &AF_M, hui_pick_name(k, cur), col, T_SURF);
        if (cur + 1u < n)
            cv_text_c(120, 84, &AF_M, hui_pick_name(k, cur + 1u), T_DIM, T_BG);
        fmt_int(b, (int32_t)cur + 1);
        str_cpy(b + str_len(b), " / ", sizeof b - str_len(b));
        fmt_int(b + str_len(b), (int32_t)n);
        cv_text_c(120, 107, &AF_S, b, T_MID, T_BG);
        cv_blit(0, 56);
    }
}

/* a word centred in a tile w x h: on one line, or on two split at a space, or cut */
static void hui_tile_text(int32_t x, int32_t y, int32_t w, int32_t h, const char *s, uint16_t fg, uint16_t bg)
{
    char a[24];
    uint32_t i, sp = 0;
    if (text_w(&AF_S, s) <= w - 1) {                    /* (ACOUSTIC, the widest word, 55.4 px: a 57 px tile) */
        cv_text_c(x + w / 2, y + (h - 15) / 2, &AF_S, s, fg, bg);
        return;
    }
    for (i = 0; s[i] && i < sizeof a - 1u; i++)          /* the last space whose first part fits */
        if (s[i] == ' ') {
            str_cpy(a, s, i + 1u);
            if (text_w(&AF_S, a) <= w - 6)
                sp = i;
        }
    if (!sp) {
        cv_text_fit(x + 3, y + (h - 15) / 2, &AF_S, s, fg, bg, w - 6);
        return;
    }
    str_cpy(a, s, sp + 1u);
    cv_text_c(x + w / 2, y + h / 2 - 14, &AF_S, a, fg, bg);
    str_cpy(a, s + sp + 1u, sizeof a);
    if (text_w(&AF_S, a) <= w - 6)
        cv_text_c(x + w / 2, y + h / 2 - 2, &AF_S, a, fg, bg);
    else
        cv_text_fit(x + 3, y + h / 2 - 2, &AF_S, a, fg, bg, w - 6);
}

/* a held layer's map: the 16 white keys as tiles (4 x 4, keys 1..4 the top row), what is on lit in the layer's
 * colour; its knobs as four cards under them */
static void hui_draw_layer(void)
{
    uint32_t l = hui.ly % HL_N, w, i, sig = 0x4C59u + l * 7u + hui.ly_page * 13u + ux.gen * 7919u + song.sel * 31u;
    uint16_t col = T_ROLE(HL_ROLE[l]);
    char b[24];
    const char *name;
    for (w = 0; w < 16u; w++)
        sig = str_hash(sig + hl_tile(l, w, b) * 3u, b);
    for (i = 0; i < 4u; i++)
        if (hl_knob_text(l, i, &name, b))
            sig = str_hash(sig + i, b);
    if (!hui.force && sig == hui.sig[1] && !hui.msg_t)
        return;
    hui.sig[1] = sig;
    hui.sig[0] = 0;
    hui_title(HL_TITLE[l], l == HL_KEY ? "BLACK KEYS: SHARPS" : l == HL_SOUND ? "KNOB 1: PAGE" : "LET GO: PLAY", col);
    cv_begin(240, 124, T_BG);
    for (w = 0; w < 16u; w++) {
        int32_t x = 3 + (int32_t)(w % 4u) * 59, y = 2 + (int32_t)(w / 4u) * 30;
        uint32_t st = hl_tile(l, w, b);
        uint16_t f = st == 2u ? col : st ? T_SURF : T_BG;
        if (st)
            cv_rrect(x, y, 57, 27, 3, f, T_BG);
        else
            cv_rrect(x, y, 57, 27, 3, T_LINE, T_BG), cv_rrect(x + 1, y + 1, 55, 25, 2, T_BG, T_LINE);
        if (l == HL_MIX && w < HCL_LAYERS && st)         /* MIX: each layer in its colour */
            cv_rrect(x + 4, y + 22, 49, 2, 1, st == 2u ? T_INK : HU_LCOL(w), f);
        if (st)
            hui_tile_text(x, y, 57, 27, b, st == 2u ? T_INK : T_TEXT, f);
    }
    cv_blit(0, HU_ROWS_Y);
    cv_begin(240, (uint32_t)(HU_FOOT_Y - HU_ROWS_Y - 124), T_BG);
    for (i = 0; i < 4u; i++) {
        int32_t x = 4 + (int32_t)i * 59;
        if (!hl_knob_text(l, i, &name, b))
            continue;
        cv_rrect(x, 4, 56, 54, 3, T_SURF, T_BG);
        cv_rrect(x, 4, 56, 2, 1, l == HL_MIX ? HU_LCOL(i) : col, T_SURF);
        cv_text_fit(x + 5, 10, &AF_S, name, T_MID, T_SURF, 46);
        if (text_w(&AF_M, b) <= 48)                     /* the value: M, or S when M is too wide, or cut */
            cv_text_on(x + 5, 30, &AF_M, b, T_TEXT, T_SURF);
        else
            cv_text_fit(x + 5, 33, &AF_S, b, T_TEXT, T_SURF, 49);
    }
    cv_blit(0, HU_ROWS_Y + 124);
}

static void hui_draw(void)
{
    hui.force |= ui.force;                               /* (a load, the editor, a cancelled update: ui.c) */
    if (hui.force)
        lcd_fill(0, 0, 240, 240, T_BG);
    if (hui.msg_t)
        hui.msg_t--;
    if (hui.ly && hui.ly_open) {                         /* a held layer: its map over whatever screen */
        hui_draw_layer();
    } else if (hui.screen == HU_HOME) {
        hui_draw_head();
        hui_draw_home();
    } else if (hui.screen == HU_PICK) {
        hui_draw_pick();
    } else if (hui_paged(hui.screen)) {
        hui_draw_page_head();
        hui_draw_page();
    }
    hui_draw_foot();
    hui.force = 0;
    ui.force = 0;
}
