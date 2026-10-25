/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* What the chord machine UI keeps of Felucca's UI and its tests (tests/ui_host.c):
 *   PANEL      a corrupt panel table (duplicate or out-of-range buttons and knobs) recovers to the default;
 *              a reversed knob stays reversed.
 *   L FACE     the large face (tools/gen_aa_font.py L_CHARS, a sparse face) holds every glyph of the strings
 *              drawn in it outside hui.c: the panel setup's control names, the boot screen.
 *   MIDI IN    a full input ring releases the held notes and the latched ARP and keeps the transport running;
 *              a fresh note plays after it.
 * Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#include "ui_host.c"

static int test_panel(void)
{
    int bad = 0;
    panel = PANEL_DEFAULT;
    panel.btn[B_PLAY] = panel.btn[B_REC];
    panel_init();
    bad += check("duplicate button mappings recover to the default panel", !memcmp(&panel, &PANEL_DEFAULT, sizeof panel));
    panel.enc[EN_K1] = panel.enc[EN_K2];
    panel_init();
    bad += check("duplicate knob mappings recover to the default panel", !memcmp(&panel, &PANEL_DEFAULT, sizeof panel));
    panel.btn[B_PLAY] = 255;
    panel_init();
    bad += check("out-of-range panel data recovers without an invalid shift", !memcmp(&panel, &PANEL_DEFAULT, sizeof panel));
    panel.dir[EN_K1] = -1;
    panel_init();
    bad += check("a valid reversed knob mapping is retained", panel.dir[EN_K1] == -1);
    return bad;
}

static int test_large_face(void)
{
    static const char *const FIXED[] = {"FELUCCA"};                 /* main.c: the boot screen */
    uint32_t i, missing = 0;
    const char *s;
    for (i = 0; i < NB + NE + 1u; i++)                              /* ui_base.c setup_show: the control names */
        for (s = i < NB ? B_NAME[i] : i < NB + NE ? E_NAME[i - NB] : FIXED[i - NB - NE]; *s; s++)
            missing += glyph_at(&AF_L, (uint8_t)*s) < 0;
    return check("the L face holds every glyph of the strings drawn in it", !missing);
}

static int test_midi_overflow(void)
{
    int bad = 0;
    uint32_t i, held = 0;
    ui_power_on();
    song.g[G_ROUTE] = 1;
    trk[1].p[P_AMODE] = trk[1].p[P_AHOLD] = 1;
    midi_event(0x90, 0, 60, 100);
    midi_event(0x90, 1, 64, 100);
    song.playing = 1;
    for (i = 0; i < MQ; i++) midi_in_event(0x643C9909u);
    midi_in_event(0x003C8908u);                          /* note-off lost to a full ring */
    events_block(CTL);
    for (i = 0; i < NTRK * NVOICE; i++) held += trk[i / NVOICE].v[i % NVOICE].gate;
    bad += check("MIDI overflow releases held notes and latched ARP, keeps transport running",
                 !held && !trk[1].nheld && !trk[1].arp_phys && !midi_sel_on[0][60] &&
                 !midi_sel_on[1][64] && !midi_in_overflow && mi_r == mi_w && song.playing);
    midi_in_event(0x643D9009u);
    events_block(CTL);
    bad += check("MIDI accepts a fresh note after overflow recovery", midi_sel_on[0][61] == 1u && trk[0].v[0].gate);
    return bad;
}

int main(void)
{
    int bad = test_panel() + test_large_face() + test_midi_overflow();
    printf(bad ? "base: %d FAILED\n" : "base: all ok\n", bad);
    return bad != 0;
}
