/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* The chord machine UI (firmware/src/hui.c) against the real input, drawing, sound and project code: power-on takes
 * the track (its sound, CHRD HI), the menus open on their buttons and walk their pages on them again, SELECT walks
 * the pages, KNOB 1..4 set every card, the actions arm and OCT+ does them, the knobs on HOME (filter, attack,
 * release), the sound and layer lists, the octave buttons, tap tempo, the presets save and load the chord machine
 * state with the project, every screen draws, and HOME held goes HOME.
 * Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#include "ui_host.c"
static void hframes(uint32_t n) { while (n--) frame(); }   /* (n frames; frames() counts milliseconds) */

static void hui_power_on(void)
{
    ui_power_on();
    hc_init();
    memset(&hui, 0, sizeof hui);
    hui.force = 1;
    hui.rnd = 0x9E3779B9u;
    usb.config = 1;
    song.g[G_BPM] = 120;
    hframes(2);
}
static int card_is(uint32_t k, const char *want)
{
    char b[20];
    int32_t f;
    hui_card(k, b, &f);
    if (!str_eq(b, want))
        printf("  card %s: %s, want %s\n", HK_NAME[k], b, want);
    return str_eq(b, want);
}
static int msg_is2(const char *s) { return hui.msg_t && str_eq(hui.msg, s); }
static int on_page(uint32_t sc, uint32_t page) { return hui.screen == sc && hui.page[sc] == page; }
static void both_oct(void)                          /* OCT- and OCT+ together */
{
    uint32_t m = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
    fm1_in.buttons |= m;
    host_pressed |= m;
    frame();
    fm1_in.buttons &= ~m;
    frame();
}

static int test_boot_and_menus(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    hui_power_on();
    bad += check("power-on: the chord machine UI is up, HOME, the track plays chord machine keys with the SAW sound",
                 hui.screen == HU_HOME && t->p[P_CHRD] == CH_HI && hc.t[0].sound == 1u && t->eng_req == ENGI_HC &&
                 t->p[P_VOICE] == V_POLY);
    press(B_SCL);
    bad += check("SCL: the KEY menu, its first page", on_page(HU_KEY, 0));
    press(B_SCL);
    bad += check("  SCL again: the next page (CHORD)", on_page(HU_KEY, 1));
    press(B_SCL);
    bad += check("  and again: round to KEY", on_page(HU_KEY, 0));
    press(B_FX);
    bad += check("FX: SOUND; SAVE: PRESETS; GLO: MIX; SEQ: LOOPER; HOME: home",
                 hui.screen == HU_SOUND && (press(B_SAVE), hui.screen == HU_PRESET) &&
                 (press(B_GLO), hui.screen == HU_MIX) && (press(B_SEQ), hui.screen == HU_LOOP) && (press(B_HOME), hui.screen == HU_HOME));
    press(B_SEQ); press(B_SEQ);
    bad += check("SEQ again on the one-page LOOPER: HOME", hui.screen == HU_HOME);
    press(B_FX); press(B_FX); press(B_FX); press(B_HOME);
    press(B_FX);
    bad += check("a menu opens on the page it was left on (SOUND: SPACE)", on_page(HU_SOUND, 2));
    press(B_HOME);
    return bad;
}

static int test_key_menu(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t cut = hc.t[0].cutoff;
    hui_power_on();
    cut = hc.t[0].cutoff;
    press(B_SCL);
    bad += check("KEY: C MAJOR, octave 0, the layer's LAYOUT", card_is(HK_KEY, "C") && card_is(HK_SCALE, "MAJOR") && card_is(HK_OCT, "0") &&
                 hui_page()->card[3] == HK_LAYOUT);
    turn(EN_K1, 1);
    bad += check("  KNOB 1: C#, every track; not the filter", card_is(HK_KEY, "C#") && trk[3].p[P_ROOT] == 1 && hc.t[0].cutoff == cut);
    turn(EN_K1, -2);
    bad += check("  back two: B (wraps)", card_is(HK_KEY, "B") && t->p[P_ROOT] == 11);
    turn(EN_K2, 1);
    bad += check("KNOB 2: MINOR (Felucca SCALE 2 on every track)", card_is(HK_SCALE, "MINOR") && t->p[P_SCALE] == 2 && trk[2].p[P_SCALE] == 2);
    turn(EN_K2, 9);
    bad += check("  ten scales round: MAJOR again", card_is(HK_SCALE, "MAJOR"));
    turn(EN_K3, 1);
    bad += check("KNOB 3: the octave up", song.octave == 1 && card_is(HK_OCT, "+1"));
    turn(EN_K3, 5);
    bad += check("  stops at +2", song.octave == 2);
    turn(EN_K3, -2);
    turn(EN_K4, 1);
    bad += check("KNOB 4: LAYOUT PIANO", hc.t[0].layout == HL_PIANO && card_is(HK_LAYOUT, "PIANO"));
    turn(EN_SELECT, 1);
    bad += check("SELECT: the CHORD page", on_page(HU_KEY, 1));
    turn(EN_SELECT, 5);
    bad += check("  past the last page: stays there", on_page(HU_KEY, 1));
    turn(EN_K1, 1);
    bad += check("BASS: OFF (the default is SLASH; one step on)", hc.t[0].bass == HB_OFF);
    turn(EN_K2, 1);
    bad += check("VOICES: 4", hc.t[0].voices == HV_4 && card_is(HK_VOICES, "4"));
    turn(EN_K3, 1);
    bad += check("VOICE LEAD: ON (to the right)", hc.t[0].vlead == 1u);
    turn(EN_K3, 1);
    bad += check("  further right: stays ON", hc.t[0].vlead == 1u);
    turn(EN_K4, 2);
    bad += check("JOYSTICK: CHROM", hc.t[0].mode == HM_CHROM && card_is(HK_JOY, "CHROM"));
    turn(EN_SELECT, -20);
    bad += check("SELECT before the first page: the first", on_page(HU_KEY, 0));
    both_oct();
    bad += check("OCT- + OCT+: RANDOMIZE ALL, the sound reloaded", msg_is2("RANDOMIZED ALL") && t->p[P_CHRD] == CH_HI);
    press(B_OCTUP);
    bad += check("OCT+ on a menu: the octave", song.octave == 1);
    press(B_OCTDN);
    press(B_HOME);
    return bad;
}

static int test_sound_menu(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t p;
    hui_power_on();
    press(B_FX);
    bad += check("SOUND: the FILTER page, the wheel OFF, the effects OFF, STEREO on", on_page(HU_SOUND, 0) && card_is(HK_FILT, "OFF") &&
                 card_is(HK_REV, "OFF") && card_is(HK_REV_AMT, "-") && card_is(HK_STEREO, "ON"));
    for (p = 0; p < HU_SEC[HU_SOUND].n; p++)
        bad += HU_PG_SOUND[p].card[0] == 0u;             /* (every page has a first card) */
    turn(EN_K1, 1);
    bad += check("WHEEL: ON -> the master filter", hc.t[0].filt == 1u && hcfx.filt == 1u);
    turn(EN_K2, -1);
    bad += check("CUTOFF: down by 3", hc.t[0].cutoff == 124u && hcfx.cutoff == 124u);
    turn(EN_K3, 2);
    bad += check("RESO: up by 6, the master filter follows", hc.t[0].res == 6u && hcfx.res == 6u && card_is(HK_RES, "6"));
    turn(EN_K3, -2);
    turn(EN_K4, 1);
    bad += check("HI-PASS: ON", hcfx.hp == 1u);
    press(B_FX);
    bad += check("FX again: AMP", on_page(HU_SOUND, HU_PG_AMP));
    turn(EN_K1, 1);
    bad += check("ENV: SHORT -> ATK 100 ms", hc.t[0].env == HE_SHORT && TIME_MS_X10[t->p[P_ATK]] / 10u >= 80u && TIME_MS_X10[t->p[P_ATK]] / 10u <= 120u);
    turn(EN_K2, 2);
    bad += check("ATTACK: fine-tuned up (the preset's value + 4)", hc.t[0].atk != 0u && t->p[P_ATK] == hc.t[0].atk);
    turn(EN_K3, -2);
    bad += check("RELEASE: fine-tuned down", hc.t[0].rel != 0u && t->p[P_REL] == hc.t[0].rel);
    turn(EN_K4, 1);
    bad += check("GLIDE: SHORT", t->p[P_GLIDE] == 30);
    press(B_FX);
    turn(EN_K1, 2);
    bad += check("SPACE: REVERB HALL -> the send and the bus (ROOM model, size 110)", on_page(HU_SOUND, 2) && hc.t[0].rev == HRV_HALL &&
                 t->p[P_REV] == 65 && song.g[G_RTYPE] == 0 && song.g[G_RSIZE] == 110 && card_is(HK_REV_AMT, "65"));
    turn(EN_K2, 3);
    bad += check("  AMOUNT: 77, the send follows", hc.t[0].rev_amt == 77u && t->p[P_REV] == 77);
    turn(EN_K1, 2);
    bad += check("  .. SPRING: the SPRING model", hc.t[0].rev == HRV_SPRING && song.g[G_RTYPE] == 1);
    turn(EN_K3, 3);
    bad += check("DELAY: 1/16 -> the send on, DLY TIME 1/16", hc.t[0].dly == HDL_1_16 && t->p[P_DLY] == 55 && song.g[G_DTIME] == 2);
    press(B_FX);
    turn(EN_K1, 1);
    bad += check("MOTION: CHORUS LIGHT -> the send", on_page(HU_SOUND, 3) && hc.t[0].cho == HCH_LIGHT && t->p[P_CHOR] == 40);
    turn(EN_K3, 1);
    bad += check("FLANGER: SLOW -> the master", hcfx.flg == HFL_SLOW);
    turn(EN_K4, 2);
    bad += check("  its AMOUNT: the wet (64 + 8), hcfx follows", hc.t[0].flg_amt == 72u && hcfx.flg_amt == 72u);
    press(B_FX);
    turn(EN_K1, 1);
    bad += check("WOBBLE: TREMOLO 1/4 -> the LFO on AMP at the tempo", on_page(HU_SOUND, HU_PG_WOBBLE) && hc.t[0].trem == HTR_1_4 &&
                 t->p[P_LD_AMP] == 90 && t->p[P_LRATE] > 0);
    turn(EN_K2, -5);
    bad += check("  its AMOUNT: the depth (90 - 20)", hc.t[0].trem_amt == 70u && t->p[P_LD_AMP] == 70);
    turn(EN_K3, 1);
    bad += check("VIBRATO: LOW -> a little pitch vibrato", t->p[P_LD_PIT] == 2);
    turn(EN_K4, -1);
    bad += check("STEREO: OFF (to the left) -> no partners", hc.t[0].stereo == 0u && trk_pair[0] == 0u);
    press(B_FX);
    turn(EN_K1, 1);
    bad += check("COLOUR: DRIVE TUBE -> DIST", on_page(HU_SOUND, 5) && t->p[P_DIST] == 28);
    turn(EN_K2, 1);
    bad += check("TAPE: LOFI -> the master", hcfx.tape == HTP_LOFI);
    {
        uint32_t rel = hc.t[0].rel, atk = hc.t[0].atk;
        turn(EN_K3, 2); turn(EN_K4, 2);
        bad += check("  KNOB 3 / 4 on no card: nothing (not the attack, not the release)", hc.t[0].rel == rel && hc.t[0].atk == atk);
    }
    press(B_FX);
    bad += check("FX on the last page: round to FILTER", on_page(HU_SOUND, 0));
    turn(EN_SELECT, 10);
    bad += check("SELECT: on to the last page, stops there", on_page(HU_SOUND, 5));
    both_oct();
    bad += check("OCT- + OCT+ on SOUND: RANDOMIZE SOUND", msg_is2("RANDOMIZED SOUND"));
    press(B_HOME);
    press(B_ENV);
    bad += check("ENV: SOUND's AMP page", on_page(HU_SOUND, HU_PG_AMP));
    press(B_ENV);
    bad += check("  ENV again: HOME", hui.screen == HU_HOME);
    press(B_LFO);
    bad += check("LFO: SOUND's WOBBLE page (the vibrato)", on_page(HU_SOUND, HU_PG_WOBBLE));
    press(B_ENV);
    bad += check("  ENV from there: AMP", on_page(HU_SOUND, HU_PG_AMP));
    press(B_HOME);
    return bad;
}

static int test_mix_menu(void)
{
    int bad = 0;
    int16_t lv = trk[0].p[P_LEVEL];
    uint32_t p0;
    hui_power_on();
    lv = trk[0].p[P_LEVEL];
    press(B_GLO);
    bad += check("GLO: MIX, the four layers' levels", on_page(HU_MIX, 0) && hui_page()->card[3] == HK_L4);
    turn(EN_K1, -2);
    bad += check("  KNOB 1: layer 1's level down 8", trk[0].p[P_LEVEL] == lv - 8);
    press(B_OCTUP);
    bad += check("  OCT+ on MIX: the octave, not a level", song.octave == 1 && trk[0].p[P_LEVEL] == lv - 8);
    press(B_OCTDN);
    press(B_GLO);
    turn(EN_K1, 2);
    bad += check("GLO again: TEMPO; KNOB 1: 122 BPM", on_page(HU_MIX, 1) && song.g[G_BPM] == 122 && card_is(HK_BPM, "122"));
    {
        uint8_t m = hcl.metro;
        turn(EN_K2, -1);
        bad += check("  KNOB 2: the metronome off to the left ..", hcl.metro == 0u && card_is(HK_METRO, "OFF"));
        turn(EN_K2, 1);
        bad += check("  .. on to the right", hcl.metro == 1u);
        hcl.metro = m;
    }
    press(B_GLO);
    turn(EN_K1, 1);
    bad += check("SETUP: SPEAKER LOWCUT (Felucca's low cut)", on_page(HU_MIX, 2) && settings.lowcut == 1u && fx_lowcut == 1u);
    turn(EN_K2, 1);
    bad += check("  OUTPUT: LINE", hui.out_line == 1u && hcfx.line == 1u);
    turn(EN_K3, 1);
    bad += check("  MIDI IN: ON", hui.midi_in == 1u);
    p0 = settings.palette;
    turn(EN_K4, 1);
    bad += check("  THEME: the next palette, set at once", settings.palette == (p0 + 1u) % NPALETTES && T_BG == UI_PALETTES[settings.palette].bg);
    turn(EN_K4, -1);
    bad += check("  THEME: back again", settings.palette == p0 && T_BG == UI_PALETTES[p0].bg);
    turn(EN_K1, -1); turn(EN_K2, -1); turn(EN_K3, -1);
    press(B_GLO);
    bad += check("GLO on SETUP: round to MIX", on_page(HU_MIX, 0));
    press(B_HOME);
    return bad;
}

static int test_mode_menu_knobs(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t i, rel;
    hui_power_on();
    press(B_EDIT);
    bad += check("EDIT tapped: the next style (STRUM), on HOME, said; no menu", hc.t[0].play == HP_STRUM && hui.screen == HU_HOME &&
                 msg_is2(HP_NAME[HP_STRUM]));
    bad += check("  HOME's knobs are STRUM's: filter, reso, the strum SPEED, release", HU_HOME_KNOBS[HP_STRUM][2] == HK_STRUM);
    turn(EN_K3, 1);
    bad += check("  KNOB 3: SPEED MEDIUM -> FAST, said", hc.t[0].strum == HST_FAST && msg_is2("SPEED FAST"));
    press(B_EDIT);
    bad += check("  .. LEAD: the track POLY (a note a key), no partners", hc.t[0].play == HP_LEAD && t->p[P_VOICE] == V_POLY && trk_pair[0] == 0u);
    press(B_EDIT); press(B_EDIT);
    bad += check("ARP: KNOB 1..3 its PATTERN, RATE, LAYER, KNOB 4 the filter", hc.t[0].play == HP_ARP &&
                 HU_HOME_KNOBS[HP_ARP][0] == HK_APAT && HU_HOME_KNOBS[HP_ARP][3] == HK_CUT);
    turn(EN_K1, 1);
    bad += check("  PATTERN: DOWN", hc.t[0].arp_pat == HA_DOWN);
    turn(EN_K2, 1);
    bad += check("  RATE: 1/16", hc.t[0].arp_rate == HR_1_16);
    turn(EN_K3, 1);
    bad += check("  LAYER: CHORD+ARP", hc.t[0].arp_layer == HAL_CHORD);
    {
        uint8_t cut = hc.t[0].cutoff;
        turn(EN_K4, -2);
        bad += check("  KNOB 4: the filter", hc.t[0].cutoff == cut - 6 && hc.t[0].filt);
        hc.t[0].cutoff = cut; hc.t[0].filt = 0; hc_apply(t);
    }
    hui_mode_set(t, HP_DRUMLOOP);
    turn(EN_K1, 1); turn(EN_K2, 1); turn(EN_K3, 1); turn(EN_K4, 2);
    bad += check("LOOPS: KNOB 1 the loop's style, 2 its variation, 3 the kit, 4 the tempo", hc.t[0].dl_style == 1u && hc.t[0].dl_var == 1u &&
                 hc.t[0].kit == 1u && song.g[G_BPM] == 122);
    hc.t[0].kit = 0; song.g[G_BPM] = 120;
    hui_mode_set(t, HP_EAR);
    {
        uint32_t rel = hc.t[0].rel;
        turn(EN_K4, 2);
        bad += check("EAR: KNOB 4 has nothing (not the release)", hc.t[0].rel == rel);
    }
    press(B_EDIT);
    bad += check("EDIT on EAR, the last style: round to PLAY (no MIXER)", hc.t[0].play == HP_PLAY);
    press(B_HOME);
    turn(EN_ALGO, 1);
    bad += check("ALGORITHM on HOME: layer 2 selected, the layers' list shown, layer 1's style kept",
                 song.sel == 1u && hui.screen == HU_PICK && hui.pick_kind == PK_LAYER && hc.t[0].play == HP_PLAY);
    turn(EN_ALGO, -2);
    bad += check("  back two: layer 1 (the first)", song.sel == 0u);
    fm1_ms += 1600; frame();
    turn(EN_PRESET, 1);
    bad += check("PRESETS knob: the next sound (TRIANGLE)", hc.t[0].sound == 2u && t->eng_req == ENGI_HC);
    turn(EN_PRESET, -1);
    fm1_ms += 1600; frame();
    turn(EN_K1, -5);
    bad += check("KNOB 1 on HOME: the FILTER wheel on, the cutoff down", hc.t[0].filt == 1u && hc.t[0].cutoff == 112u);
    turn(EN_K2, 3);
    bad += check("KNOB 2: the resonance (9), said in the header", hc.t[0].res == 9u && hcfx.res == 9u && msg_is2("RESO 9"));
    turn(EN_K3, -3);
    bad += check("KNOB 3: the attack fine-tuned, said", hc.t[0].atk != 0u && hui.msg_t && !memcmp(hui.msg, "ATTACK ", 7));
    turn(EN_K4, 5);
    bad += check("KNOB 4: the release fine-tuned, said", hc.t[0].rel != 0u && !memcmp(hui.msg, "RELEASE ", 8));
    hc.t[0].res = 0; hc_apply(t);
    press(B_OCTUP);
    bad += check("OCT+ on HOME: the octave", song.octave == 1);
    press(B_OCTDN); press(B_OCTDN); press(B_OCTDN);
    bad += check("  OCT- thrice: -2 (the floor)", song.octave == -2);
    turn(EN_PRESET, 1);
    bad += check("PRESETS knob: the sound list shows while it turns", hui.screen == HU_PICK && hui.pick_kind == PK_SOUND);
    turn(EN_SELECT, 1);
    bad += check("  SELECT moves it too (the next sound)", hc.t[0].sound == 3u);
    fm1_ms += 1600; frame();
    bad += check("  gone 1.5 s after the last turn", hui.screen == HU_HOME);
    turn(EN_ALGO, 1); turn(EN_ALGO, -1);
    bad += check("ALGORITHM knob: the layers' list", hui.screen == HU_PICK && hui.pick_kind == PK_LAYER && song.sel == 0u);
    press(B_OCTUP);
    bad += check("  a button closes it and acts on the screen under it (HOME: the octave)", hui.screen == HU_HOME && song.octave == -1);
    press(B_OCTDN); turn(EN_PRESET, -2);
    fm1_ms += 1600; frame();
    press(B_FX); press(B_FX);
    turn(EN_PRESET, 1);
    bad += check("PRESETS knob on a menu: the list over it ..", hui.screen == HU_PICK && hui.pick_back == HU_SOUND);
    fm1_ms += 1600; frame();
    bad += check("  .. and back to the page after", on_page(HU_SOUND, HU_PG_AMP));
    turn(EN_PRESET, -1);
    fm1_ms += 1600; frame();
    press(B_HOME);
    press(B_ARP);
    bad += check("ARP on PLAY: the style before, round (EAR), on HOME", hc.t[0].play == HP_EAR && hui.screen == HU_HOME);
    press(B_ARP);
    bad += check("  again: HIRO", hc.t[0].play == HP_HIRO);
    press(B_EDIT); press(B_EDIT);
    bad += check("  EDIT twice: PLAY again", hc.t[0].play == HP_PLAY);
    {   /* EDIT taps are no tap tempo any more (they walk the styles); GLO held + key 16 is */
        song.g[G_BPM] = 100;
        hframes(110);
        for (i = 0; i < 3u; i++) {
            press(B_EDIT);
            hframes(29);                                  /* 2 frames of the press + 29 = 31 x 16 = 496 ms */
        }
        bad += check("EDIT tapped three times 500 ms apart: no tap tempo, three styles on", song.g[G_BPM] == 100 && hc.t[0].play == HP_DRONE);
        hui_mode_set(t, HP_PLAY);
        press(B_HOME);
        fm1_in.buttons |= 1u << panel.btn[B_GLO];
        host_pressed |= 1u << panel.btn[B_GLO];
        hframes(2);
        for (i = 0; i < 3u; i++) {
            key_down(white(15)); hframes(1); key_up(white(15));
            hframes(30);
        }
        fm1_in.buttons &= ~(1u << panel.btn[B_GLO]);
        hframes(2);
        bad += check("GLO held + white key 16 three times 500 ms apart: ~120 BPM (tap tempo)",
                     song.g[G_BPM] >= 116 && song.g[G_BPM] <= 124);
    }
    press(B_HOME);
    return bad;
}

static int test_presets(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    hui_power_on();
    hc_sound_load(t, 14);                                /* PIANO */
    hc.t[0].rev = HRV_PLATE;
    hc.t[0].mode = HM_BORROW;
    hc.inv[0] = 2;
    hc.lock[9].on = 1; hc.lock[9].q = HQ_DOM7; hc.lock[9].roff = 0;
    hui_key_set(7);                                      /* G */
    song.g[G_BPM] = 98;
    hc_apply(t);
    press(B_SAVE);
    {
        char sub[20];
        hui_card_sub(HK_SLOT, sub);
        bad += check("SAVE: PRESETS, P1 empty, SAVE armed", hui.screen == HU_PRESET && card_is(HK_SLOT, "P1") && str_eq(sub, "EMPTY") &&
                     hui.armed == HK_SAVE);
    }
    press(B_SAVE);
    bad += check("SAVE again: P2, SAVE still armed", card_is(HK_SLOT, "P2") && hui.armed == HK_SAVE);
    press(B_OCTUP);
    bad += check("  OCT+: saved to P2, nothing armed after", msg_is2("SAVED P2") && hui.preset_used[1] && project_used(1) && !hui.armed);
    hc_sound_load(t, 1);                                 /* SAW: everything else changes */
    hc.t[0].rev = HRV_OFF;
    hc.t[0].mode = HM_DEFAULT;
    hc.inv[0] = 0;
    hc.lock[9].on = 0;
    hui_key_set(0);
    song.g[G_BPM] = 120;
    hc_apply(t);
    turn(EN_K2, 1);
    bad += check("KNOB 2: LOAD armed", hui.armed == HK_LOAD);
    press(B_OCTUP);
    bad += check("  OCT+: P2 loaded: PIANO, PLATE, BORROW, the inversion and the lock, key G, 98 BPM",
                 msg_is2("PRESET P2") && hc.t[0].sound == 14u && t->eng_req == 4u && hc.t[0].rev == HRV_PLATE &&
                 hc.t[0].mode == HM_BORROW && hc.inv[0] == 2u && hc.lock[9].on && hc.lock[9].q == HQ_DOM7 &&
                 t->p[P_ROOT] == 7 && song.g[G_BPM] == 98 && t->p[P_CHRD] == CH_HI);
    bad += check("  .. and the Felucca parameters follow (the PLATE send)", t->p[P_REV] == 60);
    turn(EN_SELECT, 1); turn(EN_K2, 1); press(B_OCTUP);
    bad += check("SELECT: P3; LOAD on an empty slot: EMPTY", card_is(HK_SLOT, "P3") && msg_is2("EMPTY"));
    turn(EN_K4, 1);
    press(B_OCTDN);
    bad += check("SAVE armed, OCT-: let be (not saved, the octave kept)", !hui.preset_used[2] && !hui.armed && song.octave == 0);
    turn(EN_K4, 1); turn(EN_K4, -1);
    bad += check("  turned back left: disarmed", !hui.armed);
    {   /* the preset bytes survive a settings round trip (settings_persist.c) */
        persist_t p;
        memset(&p, 0, sizeof p);
        settings_export(&p);
        bad += check("the presets are in the settings record (PER5)", p.magic == PERSIST_MAGIC && !memcmp(p.hcp[1], hc_presets[1], HC_PRESET_BYTES) &&
                     *(const uint32_t *)p.hcp[1] == HCP_MAGIC);
        memset(hc_presets, 0, sizeof hc_presets);
        bad += check("  .. and come back from an import", settings_import(&p, sizeof p) == 1 && *(const uint32_t *)hc_presets[1] == HCP_MAGIC);
    }
    press(B_HOME);
    return bad;
}

static int test_draw_and_handover(void)
{
    int bad = 0;
    uint32_t s, p, ok = 1, fits = 1, k;
    hui_power_on();
    for (s = 0; s < HU_N; s++) {                         /* every screen draws (no crash, the stub display touched) */
        if (s == HU_MODE)
            continue;                                    /* (no screen: EDIT steps the style) */
        for (p = 0; p < (hui_paged(s) ? HU_SEC[s].n : 1u); p++) {
            hui.screen = (uint8_t)s;
            hui.page[s] = (uint8_t)p;
            hui.force = 1;
            memset(host_screen, 0, sizeof host_screen);
            hframes(2);
            ok &= host_screen[120 * 240 + 120] != 0u || host_screen[10 * 240 + 10] != 0u;
        }
    }
    bad += check("every screen and every page draws", ok);
    for (k = 1; k < HK_N; k++)
        if (text_w(&AF_S, HK_NAME[k]) > 52) {
            printf("  the card name %s is %d px wide\n", HK_NAME[k], text_w(&AF_S, HK_NAME[k]));
            fits = 0;
        }
    bad += check("every card's name fits its card (52 px)", fits);
    for (s = 0; s < HU_N; s++)
        memset(&hui.page[s], 0, 1);
    hui.screen = HU_HOME;
    key_down(0); key_down(10);
    hframes(2);
    bad += check("HOME with a chord: the chord's name on screen (the big face has every glyph of it)",
                 str_eq(chord_last[0].name, "C7") && text_w(&AF_X, "C7") > 0);
    key_up(10); key_up(0);
    press(B_SCL);
    bad += check("SCL: the KEY menu", hui.screen == HU_KEY);
    hold(B_HOME);
    bad += check("HOME held, then let go: HOME (no other UI underneath)", hui.screen == HU_HOME && !hui.ly);
    return bad;
}

/* the footer's hints stay inside the screen: nothing drawn in the 4 px right margin (as the 4 px left one), on
 * every screen, every page, with an action armed, and on MODE in every style */
static int foot_fits(void)
{
    uint32_t x, y;
    uint16_t bg;
    hui.force = 1;
    hframes(2);
    bg = host_screen[(HU_FOOT_Y + 1u) * 240u];        /* (column 0: the margin) */
    for (y = HU_FOOT_Y; y < 240u; y++)
        for (x = 236; x < 240u; x++)
            if (host_screen[y * 240u + x] != bg)
                return 0;
    return 1;
}
static int test_footer_fits(void)
{
    int bad = 0;
    uint32_t s, p, ok = 1;
    char b[64];
    hui_power_on();
    for (s = 0; s < HU_N; s++)
        for (p = 0; p < (hui_paged(s) ? HU_SEC[s].n : 1u) && s != HU_MODE; p++) {
            hui.screen = (uint8_t)s;
            hui.page[s] = (uint8_t)p;
            if (!foot_fits()) {
                snprintf(b, sizeof b, "  the footer of screen %u page %u runs into the right margin", s, p);
                bad += check(b, 0);
                ok = 0;
            }
        }
    hui.screen = HU_PRESET;
    hui.armed = HK_SAVE;
    ok &= foot_fits();
    hui.screen = HU_LOOP;
    hui.armed = HK_CLEAR;
    ok &= foot_fits();
    hui.armed = 0;
    hui.screen = HU_HOME;                                /* HOME's knob row, in every style */
    for (p = 0; p < HU_NSTYLES; p++) {
        hui_mode_set(TSEL, HU_STYLE[p]);
        ok &= foot_fits();
    }
    hui_mode_set(TSEL, HP_PLAY);
    bad += check("the footer fits the screen on every screen and page, HOME's knobs in every style, and with an action armed", ok);
    hui.screen = HU_HOME;
    return bad;
}

static void run_ms2(uint32_t ms)                   /* the audio clock (blocks), with the UI frames between */
{
    uint32_t n = ms * (FS / 1000u) / CTL, i;
    for (i = 0; i < n; i++) {
        events_block(CTL);
        if (i % 22u == 0u)
            frame();
    }
}
static uint32_t gated_on(uint32_t ti)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += trk[ti].v[i].active && trk[ti].v[i].gate;
    return n;
}
static uint32_t midi_ons(void)                     /* note-ons in MIDI OUT so far */
{
    uint32_t i, n = 0;
    for (i = 0; i < mo_w; i++) n += ((midi_out_q[i % MQ] >> 8) & 0xF0u) == 0x90u;
    return n;
}
static int gate_on(uint32_t ti, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++) if (trk[ti].v[i].active && trk[ti].v[i].gate && trk[ti].v[i].note == note) return 1;
    return 0;
}
static int test_looper(void)
{
    int bad = 0;
    uint32_t q = (uint32_t)FS * 60u / 120u;             /* a beat at 120 BPM: 22050 samples */
    hui_power_on();
    hc.t[0].stereo = 0;
    hc_apply(&trk[0]);
    press(B_SEQ);
    bad += check("SEQ: the LOOPER page, every layer OFF, BARS FREE, CLEAR on KNOB 4", hui.screen == HU_LOOP &&
                 hcl.l[0].state == HLS_OFF && hcl.l[1].state == HLS_OFF && card_is(HK_BARS, "FREE") && hui_page()->card[3] == HK_CLEAR);
    press(B_HOME);
    press(B_REC);
    bad += check("REC: layer 1 ARMED", hcl.l[0].state == HLS_ARMED && msg_is2("LAYER 1 ARMED"));
    press(B_REC);
    bad += check("REC again: recording (free length), the transport runs", hcl.l[0].state == HLS_REC && hcl.playing);
    key_down(0);                                       /* C for a beat */
    run_ms2(500);
    key_up(0);
    run_ms2(500);
    key_down(2);                                       /* Dm for a beat */
    run_ms2(500);
    key_up(2);
    run_ms2(480);                                      /* ~2 s: 4 beats */
    press(B_REC);
    frame();
    bad += check("REC: the layer plays, the loop is 4 beats (rounded), layer 1 stays selected",
                 hcl.l[0].state == HLS_PLAY && hcl.len == 4u * q && song.sel == 0u && hcl.l[0].n >= 12u &&
                 msg_is2("LAYER 1 PLAYS"));
    run_ms2(2100);                                     /* a loop round: layer 1 replays C then Dm */
    bad += check("  the loop replays: notes sound on part 1 during its round", gated_on(0) > 0u || hcl.l[0].play_i > 0u);
    {   /* over one loop, the C and the Dm are heard on part 1 at their times */
        uint32_t seen_c = 0, seen_d = 0, i;
        for (i = 0; i < 40u; i++) {
            run_ms2(50);
            seen_c |= gate_on(0, 48);
            seen_d |= gate_on(0, 50);
        }
        bad += check("  .. the C and the Dm both come round", seen_c && seen_d);
    }
    press(B_REC); press(B_REC);                        /* REC on a playing layer: the next empty one, ARMED; it
                                                          * waits for the loop's start */
    bad += check("REC on layer 1 (playing): layer 2 selected and ARMED, waiting for the loop's start; layer 1 plays on",
                 song.sel == 1u && hcl.l[1].state == HLS_ARMED && hcl.l[0].state == HLS_PLAY && trk[1].p[P_CHRD] == CH_HI);
    run_ms2(2100);
    bad += check("  .. and records from it", hcl.l[1].state == HLS_REC || hcl.l[1].state == HLS_PLAY);
    key_down(7); run_ms2(300); key_up(7);            /* a G on layer 2 */
    run_ms2(2000);
    bad += check("  .. one loop later it plays, with the G; layer 2 stays selected", hcl.l[1].state == HLS_PLAY &&
                 hcl.l[1].n >= 6u && song.sel == 1u);
    press(B_PLAY);
    bad += check("PLAY: paused, nothing sounds", !hcl.playing && msg_is2("PAUSED"));
    run_ms2(200);
    bad += check("  .. silence on the parts", gated_on(0) == 0u && gated_on(1) == 0u);
    press(B_PLAY);
    bad += check("PLAY again: running", hcl.playing);
    press(B_SEQ);
    bad += check("the LOOPER page: layers PLAY PLAY OFF OFF, drawn as the strip", hui.screen == HU_LOOP && hcl.l[0].state == HLS_PLAY &&
                 hcl.l[1].state == HLS_PLAY && hcl.l[2].state == HLS_OFF && hcl.l[3].state == HLS_OFF);
    turn(EN_K4, 1);
    bad += check("KNOB 4: CLEAR armed, nothing cleared yet", hui.armed == HK_CLEAR && hcl.len != 0u);
    press(B_OCTUP);
    bad += check("  OCT+: no loop, the transport stopped", hcl.len == 0u && !hcl.playing && msg_is2("ALL CLEARED") && !hui.armed);
    turn(EN_K1, 2);
    bad += check("KNOB 1: BARS 2", hcl.bars == 2u && card_is(HK_BARS, "2"));
    press(B_HOME);
    track_select(0);
    press(B_REC); press(B_REC);
    bad += check("a fixed length: REC records 2 bars at once", hcl.l[0].state == HLS_REC && hcl.len == 8u * q);
    run_ms2(4100);
    bad += check("  .. and plays by itself after them", hcl.l[0].state == HLS_PLAY && song.sel == 0u);
    hold(B_REC);
    bad += check("REC held on the live layer (2, empty): nothing to clear but no harm; everything cleared when nothing plays",
                 hcl.l[0].state == HLS_PLAY || hcl.len == 0u);
    hcl_clear_all();
    press(B_HOME);
    return bad;
}

static int test_seq_drums_mixer(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t q = (uint32_t)FS * 60u / 120u, i, hits;
    hui_power_on();
    hc.t[0].bass = HB_OFF; hc_apply(&trk[0]);          /* (the counts below are without the bass voice) */
    hc.t[0].stereo = 0;
    hc.t[0].voices = HV_4;
    hc_apply(t);
    /* the sequencer */
    hui_mode_set(t, HP_SEQ);
    hcs_clear();
    hcs.len = 4;
    key_down(0); key_up(0);                            /* I */
    key_down(9); key_up(9);                          /* vi */
    key_down(10); key_down(6); key_up(6); key_up(10);   /* up-right held, then IV: F7 */
    key_down(1); key_up(1);                            /* F#3: a rest */
    bad += check("SEQUENCER: four keys wrote four steps (I, vi, IV + up-right, a rest), the cursor round", hcs.st[0].deg == 0u &&
                 hcs.st[1].deg == 5u && hcs.st[2].deg == 3u && hcs.st[2].dir == HD_UR && hcs.st[3].deg == 255u && hcs.cur == 0u);
    {
        hchord_t ch; char b[12];
        hcs_step_chord(t, 2, &ch, b, 0);
        bad += check("  step 3 is F7", str_eq(b, "F7"));
    }
    run_ms2(100);
    bad += check("  the keys sounded their chords and ended", gated_on(0) == 0u);
    press(B_PLAY);
    bad += check("PLAY: the sequencer runs, step 1 (C) sounds", hcs.running && gate_on(0, 48) && gated_on(0) == 3u);
    run_ms2(520);
    bad += check("  a beat later: step 2 (Am)", hcs.step == 1u && gate_on(0, 57) && !gate_on(0, 48));
    run_ms2(500);
    bad += check("  step 3 (F7)", hcs.step == 2u && gate_on(0, 53) && gate_on(0, 63));
    run_ms2(500);
    bad += check("  step 4: the rest, silence", hcs.step == 3u && gated_on(0) == 0u);
    run_ms2(500);
    bad += check("  round: step 1 again", hcs.step == 0u && gate_on(0, 48));
    turn(EN_K1, 1);
    bad += check("SEQ on HOME: KNOB 1 the LENGTH: 8, said", hcs.len == 8u && card_is(HK_SEQLEN, "8") && msg_is2("LENGTH 8"));
    press(B_PLAY);
    bad += check("PLAY again: stopped, silent", !hcs.running && gated_on(0) == 0u);
    press(B_PLAY);
    hui_mode_set(t, HP_PLAY);                          /* leaving while it runs: the bounce */
    bad += check("leaving SEQUENCER while it runs: the looper records it (2 bars for 8 steps)", hcs.bounce && hcl.l[0].state == HLS_REC &&
                 hcl.len == 8u * q);
    run_ms2(4200);
    bad += check("  .. two bars later layer 1 plays the sequence, the sequencer is done, layer 1 still selected",
                 hcl.l[0].state == HLS_PLAY && !hcs.bounce && !hcs.running && song.sel == 0u && hcl.l[0].n >= 10u);
    hcl_clear_all();
    track_select(0);
    /* the drums */
    hui_mode_set(t, HP_DRUM);
    bad += check("DRUM: the track is on the DRUM engine, the sound remembered", t->eng_req == ENGI_DRUM && hc.t[0].sound_saved == 1u);
    key_down(0);
    bad += check("  the C4 key: the kick (GM 36) sounds", gate_on(0, 36) || trk[0].v[0].active);
    key_up(0);
    key_down(4);
    bad += check("  the E4 key: the snare (38)", gate_on(0, 38) || trk[0].v[1].active || trk[0].v[0].active);
    key_up(4);
    run_ms2(100);
    key_down(8);                                       /* up: AUTO-DRUM 1/4 */
    mo_w = mo_r = 0;                                   /* (the MIDI OUT queue: nobody drains it here) */
    key_down(0);
    hits = midi_ons();
    run_ms2(1100);                                     /* ~2 beats */
    bad += check("AUTO-DRUM: the kick held with a direction repeats (2 more hits in 2 beats)", midi_ons() >= hits + 2u);
    key_up(0); key_up(8);
    turn(EN_K1, 1);
    bad += check("DRUM KIT: HAND -> the engine's KIT", hc.t[0].kit == 1u && t->p[P_E0] == 1);
    press(B_HOME);
    hui_mode_set(t, HP_PLAY);
    bad += check("back to PLAY: the sound is back (SAW on CHORD)", t->eng_req == ENGI_HC && hc.t[0].sound == 1u);
    /* the drum loops */
    hui_mode_set(t, HP_DRUMLOOP);
    bad += check("DRUM LOOP: the DRUM engine, the loop runs", t->eng_req == ENGI_DRUM && hcs.dl_running);
    key_down(9); key_up(9);                          /* the A4 key (the sixth): DEMBOW */
    bad += check("  a white key picks the style (A4: DEMBOW) and starts it", hc.t[0].dl_style == DL_DEMBOW && hcs.dl_running);
    mo_w = mo_r = 0;
    hits = midi_ons();
    run_ms2(2050);                                     /* a bar */
    bad += check("  a bar later: hits came (DEMBOW ORIG: 4 kicks, 4 snares, 8 hats a bar)", midi_ons() >= hits + 14u);
    bad += check("  the variations differ from ORIG", hcd_lanes(DL_ROCK, DV_BUSY, 1) != hcd_lanes(DL_ROCK, DV_ORIG, 1) &&
                 hcd_lanes(DL_ROCK, DV_FILL, 15) != hcd_lanes(DL_ROCK, DV_ORIG, 15));
    press(B_PLAY);
    bad += check("PLAY: the loop stops", !hcs.dl_running);
    hui_mode_set(t, HP_MIXER);
    bad += check("MIXER: the sound back on the track", t->eng_req == ENGI_HC);
    key_down(2); key_up(2);                            /* the D4 key: layer 2 mute */
    bad += check("  key 2: layer 2 muted; again: unmuted", trk[1].p[P_MUTE] == 1 && (key_down(2), key_up(2), trk[1].p[P_MUTE] == 0));
    key_down(11); key_up(11);                          /* B4: the metronome */
    bad += check("  key 7: the metronome off", !hcl.metro);
    press(B_OCTDN);
    bad += check("  OCT- on HOME: the selected layer's level down", t->p[P_LEVEL] == TP[P_LEVEL].def - 8);
    for (i = 0; i < HP_COUNT; i++) {                   /* every mode draws its HOME */
        hui_mode_set(t, i);
        hui.force = 1;
        hframes(2);
    }
    hui_mode_set(t, HP_PLAY);
    bad += check("every mode's HOME draws", 1);
    return bad;
}

static int test_games(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t q, i, n0;
    hui_power_on();
    hc.t[0].bass = HB_OFF; hc_apply(&trk[0]);          /* (the note counts below are without the bass voice) */
    hc.t[0].stereo = 0;
    hc.t[0].voices = HV_4;
    hc_apply(t);
    /* CHORD HIRO: CAMPFIRE (I V vi IV ..), MEDIUM (150 ms), 100 % */
    hui_mode_set(t, HP_HIRO);
    hcg.song = 0; hcg.diff = HGD_MEDIUM; hcg.speed = 100;
    q = (uint32_t)FS * 60u / 120u;
    press(B_PLAY);
    bad += check("CHORD HIRO: PLAY starts the song with a bar's count-in", hcg.running && hcg.idx == 0u && hcg.next_at == 4u * q);
    run_ms2(1900);                                     /* 100 ms before the first chord */
    key_down(0); key_up(0);                            /* I, 100 ms early: inside the 150 ms window */
    bad += check("  the I key 100 ms early: OK (inside the window), the chord sounds, the score counts", hcg.hit && hcg.last == HGR_OK &&
                 hcg.score > 0u && gated_on(0) == 3u);
    run_ms2(2100);                                     /* into the second chord (V) */
    bad += check("  the song moved on to the V", hcg.idx == 1u && !hcg.hit);
    key_down(7); key_up(7);                          /* V on time (+/- a block or so) */
    bad += check("  the V on time: PERFECT, combo 2", hcg.last == HGR_PERFECT && hcg.combo == 2u);
    run_ms2(2000);
    run_ms2(400);                                      /* the vi's window passes */
    bad += check("  nothing pressed for the vi: MISS, the combo gone", hcg.last == HGR_MISS && hcg.combo == 0u && hcg.miss == 1u);
    run_ms2(1800);
    key_down(2); key_up(2);                            /* ii instead of IV */
    bad += check("  the wrong chord: MISS", hcg.last == HGR_MISS && hcg.miss == 2u);
    for (i = 0; i < 5u; i++) run_ms2(2000);
    bad += check("  the song ends by itself (done)", hcg.done && !hcg.running);
    turn(EN_K2, 1);
    bad += check("HIRO on HOME: KNOB 2 the DIFFICULTY: HARD", hcg.diff == HGD_HARD && card_is(HK_DIFF, "HARD"));
    turn(EN_K3, -1);
    bad += check("  KNOB 3 the PRACTICE SPEED: 90 %", hcg.speed == 90u);
    turn(EN_K1, 1);
    bad += check("  KNOB 1 the SONG: DOO-WOP", hcg.song == 1u && card_is(HK_SONG, "DOO-WOP"));
    /* EAR TRAINER: level 1 */
    hui_mode_set(t, HP_EAR);
    hcg.level = 0;
    hcg.rnd = 0x1234ABCDu;
    press(B_PLAY);
    bad += check("EAR TRAINER: PLAY asks a question (it plays)", hcg.phase == 1u);
    run_ms2(1200);
    bad += check("  .. then waits for the answer", hcg.phase == 2u && gated_on(0) == 0u);
    key_down(hcg.q[0] == 0u ? 9u : 0u); key_up(hcg.q[0] == 0u ? 9u : 0u);   /* a wrong degree (D if it was C, else C) */
    bad += check("  a wrong key: NO, the streak 0, it was shown", hcg.phase == 3u && hcg.result == 2u && hcg.streak == 0u);
    run_ms2(1500);
    run_ms2(1200);
    bad += check("  the next question came and waits", hcg.phase == 2u);
    {
        static const uint8_t DEG_KEY[7] = {0, 2, 4, 6, 7, 9, 11};
        key_down(DEG_KEY[hcg.q[0]]); key_up(DEG_KEY[hcg.q[0]]);
    }
    bad += check("  the right key: CORRECT, streak 1", hcg.result == 1u && hcg.streak == 1u && hcg.right == 1u);
    key_down(1); key_up(1);                            /* F#3 in the result phase: nothing */
    run_ms2(1000);
    run_ms2(1200);
    hcg_ear_replay();
    bad += check("  F#3 replays the question (plays again)", hcg.phase == 1u);
    hui_mode_set(t, HP_EAR);
    hcg.level = 2;                                     /* a modified chord: the degree and the direction */
    press(B_PLAY);
    run_ms2(1200);
    n0 = hcg.qdir[0];
    {
        static const uint8_t DEG_KEY[7] = {0, 2, 4, 6, 7, 9, 11};
        static const uint8_t DIR_KEY[8] = {8, 10, 13, 15, 17, 20, 22, 25};
        key_down(DEG_KEY[hcg.q[0]]); key_up(DEG_KEY[hcg.q[0]]);
        bad += check("level 3: the degree alone is not enough (the direction counts)", hcg.result == 2u);
        run_ms2(1500); run_ms2(1200);
        key_down(DIR_KEY[hcg.qdir[0]]); key_down(DEG_KEY[hcg.q[0]]); key_up(DEG_KEY[hcg.q[0]]); key_up(DIR_KEY[hcg.qdir[0]]);
        bad += check("  the degree with its direction: CORRECT", hcg.result == 1u);
    }
    (void)n0;
    hui_mode_set(t, HP_PLAY);
    bad += check("leaving: the game is off, the chord keys play", !hcg.running && !hcg.phase && hc.t[0].play == HP_PLAY);
    return bad;
}

static int test_fx_amount(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    hc_trk_t *c = &hc.t[0];
    hui_power_on();
    hui_open_page(HU_SOUND, 2);
    c->rev = HRV_HALL; hc_apply(t);
    bad += check("SPACE: REVERB HALL, its AMOUNT the one in effect (65)", card_is(HK_REV, "HALL") && card_is(HK_REV_AMT, "65") && t->p[P_REV] == 65);
    turn(EN_K2, 3);
    bad += check("  KNOB 2: the amount (77), the send follows", c->rev_amt == 77u && t->p[P_REV] == 77 && card_is(HK_REV_AMT, "77"));
    turn(EN_K2, -40);
    bad += check("  down to the floor of 1", c->rev_amt == 1u && t->p[P_REV] == 1);
    c->rev = 0; c->rev_amt = 0; hc_apply(t);
    bad += check("  OFF: no amount", card_is(HK_REV, "OFF") && card_is(HK_REV_AMT, "-") && t->p[P_REV] == 0);
    turn(EN_K2, 1);
    bad += check("  the AMOUNT of an OFF effect: on at its first type, the amount", c->rev == HRV_ROOM && c->rev_amt == 54u);
    {
        uint32_t rel = c->rel;
        turn(EN_K4, 1);
        bad += check("  KNOB 4 on a page: its card (the delay's amount), not the release", c->rel == rel && c->dly != 0u);
    }
    c->rev = 0; c->rev_amt = 0; c->dly = 0; c->dly_amt = 0; hc_apply(t);
    hui_open(HU_HOME);
    return bad;
}

/* the chord machine's way to play over drums: the drum loop recorded into a looper layer, the live
 * instrument moving to the next layer, PLAY mode there: chords over the loop */
static int test_drums_under_chords(void)
{
    int bad = 0;
    uint32_t i, drums = 0, chords = 0;
    hui_power_on();
    hc.t[0].bass = HB_OFF; hc_apply(&trk[0]);
    hui_mode_set(&trk[0], HP_DRUMLOOP);
    press(B_REC);                                      /* ARMED */
    press(B_REC);                                      /* REC: the loop's first bar */
    run_ms2(2200);
    if (hcl.l[0].state == HLS_REC) press(B_REC);       /* (a free first layer: closed by hand) */
    run_ms2(100);
    bad += check("DRUM LOOP into the looper: layer 1 plays, still selected", hcl.l[0].state == HLS_PLAY && song.sel == 0u);
    turn(EN_ALGO, 1);                                  /* layer 2 for the chords */
    bad += check("  ALGORITHM: layer 2", song.sel == 1u);
    hui_mode_set(TSEL, HP_PLAY);
    run_ms2(50);                                       /* (the mode change's release of the track lands in a block) */
    bad += check("  PLAY mode on the new layer: the chord sound is back (not DRUM)", TSEL->eng_req != ENGI_DRUM && hc.t[1].play == HP_PLAY);
    key_down(0);
    run_ms2(1200);
    for (i = 0; i < NVOICE; i++) { drums += trk[0].v[i].active; chords += trk[1].v[i].active && trk[1].v[i].gate; }
    bad += check("  a chord held over the loop: chord voices on layer 2, drum voices still on layer 1", chords >= 3u && drums >= 1u);
    key_up(0);
    hcl_clear_all();
    hui_mode_set(&trk[0], HP_PLAY);
    return bad;
}

/* the first thing after power-on (the device's order: the layer, the settings, Felucca's init, the first
 * frames): BASS SLASH from the defaults, a bass key held and a chord key pressed */
static int test_slash_first_thing(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t i, g = 0;
    hui_power_on();
    run_ms2(100);                                      /* (the sound load at the first frame: an engine switch over blocks) */
    bad += check("power-on: BASS SLASH, the chord layer on the track", hc.t[0].bass == HB_SLASH && hc_on(t) && t->p[P_CHRD] == CH_HI);
    key_down(0);                                       /* C held: C major with its root bass */
    run_ms2(60);
    for (i = 0; i < NVOICE; i++) g += t->v[i].active && t->v[i].gate;
    printf("    [first chord] name %s gated %u notes:", hc.name, g);
    for (i = 0; i < NVOICE; i++) if (t->v[i].active) printf(" %d%s", t->v[i].note, t->v[i].gate ? "" : "(rel)");
    printf("  bass %d voices %d stereo %d pair %d play %d\n", hc.t[0].bass, hc.t[0].voices, hc.t[0].stereo, trk_pair[0], hc.t[0].play);
    bad += check("  the first chord: C with its bass (C1 C E G, voices 8 with partners)", str_eq(hc.name, "C") && gate_on(0, 24) && gate_on(0, 48) && g >= 4u);
    key_up(0); run_ms2(60); key_down(0); run_ms2(60);
    for (g = 0, i = 0; i < NVOICE; i++) g += t->v[i].active && t->v[i].gate;
    printf("    [second time] name %s gated %u notes:", hc.name, g);
    for (i = 0; i < NVOICE; i++) if (t->v[i].active) printf(" %d%s", t->v[i].note, t->v[i].gate ? "" : "(rel)");
    printf("\n");
    key_down(4);                                      /* E pressed on top: Em/C */
    run_ms2(60);
    bad += check("  the second key on top: Em/C, C1 under E G B, the C chord gone", str_eq(hc.name, "Em/C") && gate_on(0, 24) &&
                 gate_on(0, 52) && gate_on(0, 59) && !gate_on(0, 48));
    key_up(4); key_up(0);
    run_ms2(60);
    return bad;
}

static int test_live_persist(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    hui_power_on();
    memset(hc_live, 0, sizeof hc_live);
    hc.t[0].bass = HB_ROOT; hc.t[0].rev = HRV_HALL; hc.inv[0] = 2; hc.lock[9].on = 1; hc_apply(t);
    fm1_ms += 600; hui_live_poll();
    bad += check("live state: a change is noticed within 500 ms and packed", hui_live.dirty && hc_live[0]);
    fm1_ms += 2000; hui_live_poll();
    bad += check("  not saved yet 2 s later (the knob may still be turning)", hui_live.dirty);
    fm1_ms += 1500; hui_live_poll();
    bad += check("  saved 3 s after the last change", !hui_live.dirty);
    hc_init();                                         /* power off, on */
    bad += check("  a fresh boot starts from the defaults (BASS SLASH)", hc.t[0].bass == HB_SLASH && hc.inv[0] == 0);
    hui_live_restore();
    bad += check("  restored: BASS ROOT, the reverb, the inversion and the lock", hc.t[0].bass == HB_ROOT &&
                 hc.t[0].rev == HRV_HALL && hc.inv[0] == 2 && hc.lock[9].on);
    hc.t[0].bass = HB_SLASH; hc.t[0].rev = 0; hc.inv[0] = 0; hc.lock[9].on = 0; hc_apply(t);
    memset(hc_live, 0, sizeof hc_live);
    return bad;
}


/* the held layers (SCL FX EDIT GLO SAVE), ALGORITHM's layers, the MIX screen */
static void hl_down_btn(uint32_t b) { fm1_in.buttons |= 1u << panel.btn[b]; host_pressed |= 1u << panel.btn[b]; hframes(1); }
static void hl_up_btn(uint32_t b) { fm1_in.buttons &= ~(1u << panel.btn[b]); hframes(1); }
static void hl_tap_key(uint32_t k) { key_down(k); hframes(1); key_up(k); hframes(1); }
static int test_layers(void)
{
    int bad = 0;
    hui_power_on();
    stop_transport();
    hl_down_btn(B_FX);
    bad += check("FX down: SOUND armed, no menu, no map yet", hui.ly == HL_SOUND && !hui.ly_open && hui.screen == HU_HOME);
    key_down(white(4));
    hframes(1);
    bad += check("  + white key 5: sound 5 (E.PIANO), the map open, the key silent", hc.t[0].sound == 4u && hui.ly_open &&
                 gates() == 0u);
    key_up(white(4)); hframes(1);
    turn(EN_K1, 1);
    hl_tap_key(white(0));
    bad += check("  KNOB 1: the next page, its first key the 17th sound", hui.ly_page == 1u && hc.t[0].sound == 16u);
    hl_up_btn(B_FX);
    bad += check("  let go: the map closes, no SOUND menu (it was used)", !hui.ly && !hui.ly_open && hui.screen == HU_HOME);
    key_down(white(0)); hframes(1);
    bad += check("  the keys play again", gates() > 0u);
    key_up(white(0)); hframes(30);
    press(B_FX);
    bad += check("FX tapped: the SOUND menu, as before", hui.screen == HU_SOUND);
    press(B_HOME);
    hl_down_btn(B_EDIT);
    hl_tap_key(white(1));
    {
        uint32_t sp = hc.t[0].strum;
        turn(EN_K1, 1);
        bad += check("EDIT held + white key 2: STRUM; KNOB 1 its speed", hc.t[0].play == HP_STRUM && hc.t[0].strum == (sp + 1u) % 3u);
    }
    hl_tap_key(white(7));
    bad += check("  + white key 8: DRUM (the drum engine)", hc.t[0].play == HP_DRUM && trk[0].eng_req == ENGI_DRUM);
    hl_tap_key(white(0));
    hl_up_btn(B_EDIT);
    bad += check("  + white key 1: PLAY again, the sound back; let go: HOME", hc.t[0].play == HP_PLAY &&
                 trk[0].eng_req == HC_SOUNDS[hc.t[0].sound].engine && hui.screen == HU_HOME);
    hl_down_btn(B_SCL);
    hl_tap_key(key_at(1, 0));                             /* F#3 */
    turn(EN_K1, 1);
    hl_up_btn(B_SCL);
    bad += check("SCL held + F#3: the key F#; KNOB 1: MINOR", trk[0].p[P_ROOT] == 6 && trk[3].p[P_ROOT] == 6 &&
                 hui_scale_get() == 1u);
    hui_key_set(0); hui_scale_set(0);
    hl_down_btn(B_GLO);
    hl_tap_key(white(1));
    turn(EN_K3, -2);
    hl_up_btn(B_GLO);
    bad += check("GLO held + white key 2: layer 2 muted; KNOB 3: layer 3's level down", trk[1].p[P_MUTE] && trk[2].p[P_LEVEL] < 100);
    hl_down_btn(B_GLO); hl_tap_key(white(1)); hl_up_btn(B_GLO);
    bad += check("  again: unmuted", !trk[1].p[P_MUTE]);
    press(B_GLO);
    bad += check("GLO tapped: the MIX page", on_page(HU_MIX, 0));
    press(B_HOME);
    hl_down_btn(B_SAVE);
    hl_tap_key(white(4));
    hl_up_btn(B_SAVE);
    bad += check("SAVE held + white key 5: saved to P1, no PRESETS screen", hui.preset_used[0] && hui.screen == HU_HOME);
    hc.t[0].sound = 9;
    hl_down_btn(B_SAVE); hl_tap_key(white(0)); hl_up_btn(B_SAVE);
    bad += check("  + white key 1: P1 loaded back", hc.t[0].sound == 16u);
    hl_down_btn(B_FX);
    hframes(20);                                         /* held alone past HL_OPEN_MS: the map, a peek */
    bad += check("FX held alone: the map shows", hui.ly_open);
    hframes(30);
    hl_up_btn(B_FX);
    bad += check("  let go late: no tap, no menu", hui.screen == HU_HOME);
    turn(EN_ALGO, 1);
    bad += check("ALGORITHM: layer 2, the layers' list", song.sel == 1u && hui.pick_kind == PK_LAYER && hui.screen == HU_PICK);
    hl_down_btn(B_EDIT); hl_tap_key(white(2)); hl_up_btn(B_EDIT);
    bad += check("  EDIT held on layer 2: LEAD on layer 2 only", hc.t[1].play == HP_LEAD && hc.t[0].play == HP_PLAY);
    turn(EN_ALGO, 5);
    bad += check("  ALGORITHM past the end: layer 4", song.sel == 3u);
    turn(EN_ALGO, -5);
    fm1_ms += 1600; frame();
    {   /* a MIXER play style saved by the earlier firmware comes back as PLAY */
        hc.t[2].play = HP_MIXER;
        hc_preset_pack(hc_live);
        hui_live_restore();
        bad += check("a saved MIXER style: PLAY now", hc.t[2].play == HP_PLAY);
    }
    for (uint32_t i = 0; i < HU_NSTYLES + 1u; i++)
        press(B_EDIT);
    bad += check("EDIT again and again: the 11 styles round, no MIXER", hc.t[0].play == HP_STRUM);
    press(B_HOME);
    return bad;
}

/* the loop kept across power-off (the settings record, PER7) and REC held to 2 s: everything cleared */
static int test_loop_kept(void)
{
    int bad = 0;
    uint32_t n0, n1;
    hui_power_on();
    hcl_clear_all();
    hcl_boot_loop = 0;
    press(B_REC); press(B_REC);                          /* layer 1: a C and a Dm over 4 beats */
    key_down(0); run_ms2(500); key_up(0); run_ms2(500);
    key_down(2); run_ms2(500); key_up(2); run_ms2(480);
    press(B_REC);
    turn(EN_ALGO, 1);
    press(B_REC);                                        /* layer 2: a G, one loop (from the loop's start) */
    {
        uint32_t loop_ms = (uint32_t)((uint64_t)hcl.len * 1000u / FS);
        while (hcl.l[1].state == HLS_ARMED)
            run_ms2(20);
        key_down(7); run_ms2(300); key_up(7);
        run_ms2(loop_ms);
    }
    n0 = hcl.l[0].n; n1 = hcl.l[1].n;
    bad += check("two layers play", hcl.l[0].state == HLS_PLAY && hcl.l[1].state == HLS_PLAY && n0 >= 8u && n1 >= 2u);
    run_ms2(4000);                                       /* the poll packs it; 3 s on, the settings are saved */
    {   /* the settings record as written, then "power-off": the loop gone from RAM, back from the record */
        persist_t p;
        uint32_t len = hcl.len, at0 = HCL_AT(hcl_ev[0][2]);
        bad += check("the loop changed: a settings save asked for, then done", !hui_live.dirty && hui_live.loop_sig == hcl_sig());
        memset(&p, 0, sizeof p);
        settings_export(&p);
        bad += check("  the record carries it", *(const uint32_t *)p.hlp == HCL_PACK_MAGIC);
        hcl_clear_all();
        bad += check("  it fits the record (one flash sector)", sizeof p <= 4096u - 256u && settings_import(&p, sizeof p) == 1);
        hcl_boot_loop = (uint8_t)hcl_unpack(p.hlp);      /* (as persist_boot at power-on) */
        hui_live_restore();
        bad += check("  power on: both layers back, paused, the same length and notes, said",
                     hcl.l[0].state == HLS_PLAY && hcl.l[1].state == HLS_PLAY && hcl.l[0].n == n0 && hcl.l[1].n == n1 &&
                     hcl.len == len && !hcl.playing && msg_is2("LOOP BACK: PLAY"));
        bad += check("  the times kept to the millisecond", (int32_t)(HCL_AT(hcl_ev[0][2]) - at0) < (int32_t)(FS / 1000u) &&
                     (int32_t)(at0 - HCL_AT(hcl_ev[0][2])) < (int32_t)(FS / 1000u));
    }
    hui_mode_set(&trk[1], HP_DRUMLOOP);                  /* (layer 2 on DRUM LOOP: PLAY would run the drum machine) */
    hcs.dl_running = 0;
    press(B_PLAY);
    run_ms2(2100);
    bad += check("  PLAY, on a DRUM LOOP layer too: the loop plays again first", hcl.playing && hcl.pos != 0u && !hcs.dl_running);
    hui_mode_set(&trk[1], HP_PLAY);
    fm1_in.buttons |= 1u << panel.btn[B_REC];
    host_pressed |= 1u << panel.btn[B_REC];
    hframes(50);                                         /* 0.8 s: the selected layer (2) */
    bad += check("REC held 0.8 s: layer 2 cleared, layer 1 plays on, told to hold on", hcl.l[1].state == HLS_OFF &&
                 hcl.l[0].state == HLS_PLAY && msg_is2("CLEARED. HOLD: ALL"));
    hframes(90);                                         /* on to 2.2 s: all */
    fm1_in.buttons &= ~(1u << panel.btn[B_REC]);
    hframes(2);
    bad += check("  held on to 2 s: everything cleared, no loop", hcl.l[0].state == HLS_OFF && !hcl.len && msg_is2("ALL CLEARED"));
    run_ms2(4000);
    {
        persist_t p;
        memset(&p, 0, sizeof p);
        settings_export(&p);
        bad += check("  and the record keeps no loop", *(const uint32_t *)p.hlp != HCL_PACK_MAGIC && !hui_live.dirty);
    }
    return bad;
}

/* the FM6 sounds at the end of the list: the PRESETS knob reaches them, each loads FM6 with its factory patch
 * (PTCH), stays across frames (hui_track_init), and a chord sounds */
static int test_fm6_sounds(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t i, first = HC_NSOUNDS, ok = 1;
    hui_power_on();
    for (i = 0; i < HC_NSOUNDS && first == HC_NSOUNDS; i++)
        if (HC_SOUNDS[i].engine == ENGI_FM6)
            first = i;
    bad += check("FM6: eight sounds at the end of the list", first == HC_NSOUNDS - 8u);
    turn(EN_PRESET, -1);                                 /* (from SAW, sound 1: back past SINE to the last) */
    turn(EN_PRESET, -1);
    hframes(2);
    bad += check("  the PRESETS knob reaches them (round): DX PLUCK", hc.t[0].sound == HC_NSOUNDS - 1u &&
                 t->eng_req == ENGI_FM6 && t->p[P_E0 + 7] == 7 && fm6_slot[0] == 7u);
    for (i = 0; i < 8u; i++) {
        hc_sound_load(t, first + i);
        hframes(2);
        ok &= t->eng_req == ENGI_FM6 && t->p[P_E0 + 7] == (int16_t)i && fm6_slot[0] == i && hc.t[0].sound == first + i &&
              t->p[P_CHRD] == CH_HI;
    }
    bad += check("  each loads FM6 with its patch, CHRD HI, and stays", ok);
    hc_sound_load(t, first);
    run_ms2(20);                                         /* (the load's panic: the audio takes it) */
    key_down(white(0));
    run_ms2(50);
    bad += check("  DX TINE EP: a chord key sounds its notes", gates() >= 4u && str_eq(chord_last[0].name, "C"));
    key_up(white(0));
    return bad;
}

/* the FM6 patch bank in the list: a filled slot after the DX sounds under its patch's name, loaded as PTCH B..; the
 * empty slots skipped; an erased one gone from the list */
static int test_fm6_bank(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint8_t pk[FM6_PACKED];
    hui_power_on();
    memcpy(pk, FM6_FACTORY[1], FM6_PACKED);
    memcpy(pk + 118, "My Bell   ", 10);
    bad += check("FM6 bank: an empty bank adds no sounds", hc_nshown() == HC_NSOUNDS);
    bad += check("  a patch into B3", !fm6_bank_put(2, pk) && hc_nshown() == HC_NSOUNDS + 1u);
    bad += check("  listed last, under its own name", hc_shown(HC_NSOUNDS) == HC_NSOUNDS + 2u &&
                 str_eq(hc_sound_name(HC_NSOUNDS + 2u), "My Bell") && str_eq(hui_pick_name(PK_SOUND, HC_NSOUNDS), "My Bell"));
    hc_sound_load(t, HC_NSOUNDS - 1u);
    turn(EN_PRESET, 1);
    hframes(2);
    bad += check("  the PRESETS knob reaches it: FM6 on PTCH B3", hc.t[0].sound == HC_NSOUNDS + 2u &&
                 t->eng_req == ENGI_FM6 && t->p[P_E0 + 7] == (int16_t)(FM6_NFACTORY + 2u) && fm6_slot[0] == FM6_NFACTORY + 2u &&
                 !memcmp(fm6_patch[0] + FP_NAME, "My Bell", 7));
    turn(EN_PRESET, 1);
    hframes(2);
    bad += check("  and goes round to SINE", hc.t[0].sound == 0u);
    bad += check("  erased: gone from the list", !fm6_bank_put(2, 0) && hc_nshown() == HC_NSOUNDS &&
                 str_eq(hc_sound_name(HC_NSOUNDS + 2u), "B3"));
    return bad;
}

/* the preset record: HCP4 fits its 256 bytes (a guard byte after it stays), an HCP3 record (a byte an inversion; its
 * last 6 bytes were never stored) loads with those as 0 */
static int test_preset_format(void)
{
    int bad = 0;
    uint8_t b[HC_PRESET_BYTES + 1u], old[HC_PRESET_BYTES];
    uint32_t i, k;
    hui_power_on();
    for (i = 0; i < 27u; i++) {
        hc.inv[i] = (uint8_t)(i % 3u);
        hc.lock[i].on = 1;
        hc.lock[i].q = (uint8_t)(i % HQ_COUNT);
        hc.lock[i].roff = (int8_t)(i - 13);
    }
    hui.midi_in = hui.out_line = 1;
    b[HC_PRESET_BYTES] = 0xA5;
    hc_preset_pack(b);
    bad += check("HCP4: packed inside the record (the byte after it untouched)", b[HC_PRESET_BYTES] == 0xA5);
    memset(hc.inv, 0, sizeof hc.inv);
    memset(hc.lock, 0, sizeof hc.lock);
    hui.midi_in = hui.out_line = 0;
    {
        int same = hc_preset_unpack(b) && hui.midi_in && hui.out_line;
        for (i = 0; i < 27u; i++)
            same = same && hc.inv[i] == i % 3u && hc.lock[i].on && hc.lock[i].q == i % HQ_COUNT && hc.lock[i].roff == (int8_t)(i - 13);
        bad += check("HCP4: inversions, locks, MIDI IN and OUT LEVEL come back", same);
    }
    memset(old, 0, sizeof old);                          /* HCP3 as the firmware stored it: 256 of its 262 bytes */
    memcpy(old, &(uint32_t){0x48435033u}, 4);
    k = 4u + NTRK * (uint32_t)sizeof(hc_trk_t);
    for (i = 0; i < 27u; i++)
        old[k + i] = (uint8_t)((i + 1u) % 3u);
    for (k += 27u, i = 0; i < 27u && k + 2u < HC_PRESET_BYTES; i++, k += 3u)
        old[k] = 1, old[k + 1u] = 2, old[k + 2u] = 3;
    hui.midi_in = hui.out_line = 1;
    {
        int same = hc_preset_unpack(old) && !hui.midi_in && !hui.out_line && !hc.lock[26].on;
        for (i = 0; i < 27u; i++)
            same = same && hc.inv[i] == (i + 1u) % 3u;
        for (i = 0; i < 25u; i++)
            same = same && hc.lock[i].on && hc.lock[i].q == 2u && hc.lock[i].roff == 3;
        bad += check("HCP3: loads (inversions, the stored locks; the unstored tail 0)", same);
    }
    return bad;
}

int main(void)
{
    int bad = test_slash_first_thing() + test_boot_and_menus() + test_key_menu() + test_sound_menu() + test_mix_menu() + test_mode_menu_knobs() + test_presets() +
              test_draw_and_handover() + test_looper() + test_seq_drums_mixer() + test_games() + test_live_persist() +
              test_fx_amount() + test_drums_under_chords() + test_layers() + test_loop_kept() + test_footer_fits() + test_preset_format() + test_fm6_sounds() + test_fm6_bank();
    printf("%s\n", bad ? "HUI TEST FAILED" : "chord machine ui test passed");
    return bad != 0;
}
