/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* The SDK port on the host: port/app/screen.c compiled against stubs of the SDK (FM1_HOST_TEST), driven
 * by scripted key presses; the MIDI it posts and the screen it paints are checked, and the screen is
 * written as a PPM to look at.
 *   cc -O1 -w -DFM1_HOST_TEST -Iport/tests -I<fm1_open>/sdk/include -I<fm1_open>/app -I<fm1_open>/app/synth \
 *      -Ifirmware/src -o build/host/port_test port/tests/port_test.c && build/host/port_test [OUT.ppm] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fm1.h"
#include "instrument.h"

/* ---- the SDK, stubbed: the controls scripted, MIDI recorded, the screen a framebuffer ---- */
static struct fm1_controls ctl;
static uint32_t ms;
static uint8_t midi_log[4096][3];
static uint32_t midi_n;
static uint16_t fb[240][240];
static int screen_pending;
int fm1_midi_post(const uint8_t *m, size_t n) { if (n == 3 && midi_n < 4096) memcpy(midi_log[midi_n++], m, 3); return 0; }
int fm1_midi_send(const uint8_t *m, size_t n) { (void)m; (void)n; return 0; }
void fm1_controls_init(void) {}
void fm1_controls_read(struct fm1_controls *s) { *s = ctl; memset(ctl.encoders, 0, sizeof ctl.encoders); }
void fm1_led_set(unsigned key, int on) { (void)key; (void)on; }
uint32_t fm1_millis(void) { return ms; }
void fm1_screen_init(void) {}
void fm1_screen_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t c) { for (unsigned j = y; j < y + h && j < 240; j++) for (unsigned i = x; i < x + w && i < 240; i++) fb[j][i] = c; }
int fm1_screen_blit(unsigned x, unsigned y, unsigned w, unsigned h, const uint16_t *p, size_t n) { (void)n; for (unsigned j = 0; j < h; j++) for (unsigned i = 0; i < w; i++) fb[y + j][x + i] = p[j * w + i]; return 0; }
int fm1_screen_submit(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t *p, size_t n) { return fm1_screen_blit(x, y, w, h, p, n); }
int fm1_screen_pending(void) { return screen_pending; }
void fm1_ble_get_status(struct fm1_ble_status *s) { memset(s, 0, sizeof *s); s->state = FM1_BLE_ADVERTISING; }
int fm1_ble_set_role(enum fm1_ble_role r) { (void)r; return 0; }
int fm1_bluetooth_configure(unsigned f) { (void)f; return 0; }
unsigned fm1_engine_output_level(void) { return 100; }
int groove_master_update(unsigned adc) { (void)adc; return 0; }
int groove_settings_radio(unsigned flags, unsigned role) { (void)flags; (void)role; return 0; }
unsigned groove_settings_role_value(void) { return 0; }
void groove_control_edit(unsigned t, int p, int v, int c) { (void)t; (void)p; (void)v; (void)c; }
void groove_get_view(unsigned t, unsigned s, struct groove_view *v) { (void)t; (void)s; memset(v, 0, sizeof *v); v->type = GROOVE_DX7; v->program = 3; }
const char *groove_fm_program_name(unsigned p) { (void)p; return "E.PIANO 1"; }

#include "../app/screen.c"

/* ---- the harness ---- */
static int bad;
static int str_eq_test(const char *a, const char *b) { return strcmp(a, b) == 0; }
static int check(const char *what, int ok)
{
    printf("port: %-70s %s\n", what, ok ? "ok" : "FAIL");
    return !ok;
}
static void tick(uint32_t n) { while (n--) { ms += 5; groove_screen_control(); } }
static void press(uint32_t k) { ctl.keys |= 1u << k; tick(1); }
static void release(uint32_t k) { ctl.keys &= ~(1u << k); tick(1); }
static void turn(unsigned e, int steps) { ctl.encoders[e] = (int16_t)steps; tick(1); }
static void button(unsigned b) { ctl.buttons |= 1u << b; tick(1); ctl.buttons &= ~(1u << b); tick(1); }
static uint32_t ons_since(uint32_t from, uint8_t *notes)   /* the note-ons posted since log index from */
{
    uint32_t i, n = 0;
    for (i = from; i < midi_n; i++)
        if ((midi_log[i][0] & 0xF0u) == 0x90u)
            notes[n++] = midi_log[i][1];
    return n;
}
static uint32_t offs_since(uint32_t from)
{
    uint32_t i, n = 0;
    for (i = from; i < midi_n; i++)
        n += (midi_log[i][0] & 0xF0u) == 0x80u;
    return n;
}
static int sounding(uint8_t note)                     /* on minus off, in order */
{
    int on = 0;
    uint32_t i;
    for (i = 0; i < midi_n; i++)
        if (midi_log[i][1] == note)
            on += (midi_log[i][0] & 0xF0u) == 0x90u ? 1 : -1;
    return on > 0;
}
static int screen_has_colour(uint16_t c)
{
    for (unsigned y = 0; y < 240; y++) for (unsigned x = 0; x < 240; x++) if (fb[y][x] == c) return 1;
    return 0;
}
static void ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6 240 240 255\n");
    for (unsigned y = 0; y < 240; y++)
        for (unsigned x = 0; x < 240; x++) {
            uint16_t c = fb[y][x];
            fputc((c >> 11) << 3, f); fputc(((c >> 5) & 63) << 2, f); fputc((c & 31) << 3, f);
        }
    fclose(f);
}

int main(int argc, char **argv)
{
    uint8_t nn[16];
    uint32_t m0, n;
    groove_screen_init();
    tick(3);
    bad += check("boot: the screen painted, the legend's degree colours on it", screen_has_colour(DEG_COL[0]) && screen_has_colour(DEG_COL[6]));
    bad += check("  nothing posted before a key", midi_n == 0);
    m0 = midi_n; press(0);                            /* the first key: I */
    n = ons_since(m0, nn);
    bad += check("the first key: C major with its bass (C1 C3 E3 G3) as note-ons on channel 1",
                 n == 4 && nn[0] == 24 && nn[1] == 48 && nn[2] == 52 && nn[3] == 55 && midi_log[m0][0] == 0x90);
    bad += check("  named C on the screen state", str_eq_test(S.name, "C"));
    m0 = midi_n; release(0);
    bad += check("  let go: four note-offs, nothing sounds", offs_since(m0) == 4 && !sounding(48) && !sounding(24));
    press(8);                                         /* C#4: UP */
    m0 = midi_n; press(0);
    bad += check("UP held, the first key: Cm (Eb instead of E)", sounding(51) && !sounding(52) && str_eq_test(S.name, "Cm"));
    release(8);
    bad += check("  the direction let go: back to C while held (E again)", sounding(52) && !sounding(51) && str_eq_test(S.name, "C"));
    press(2);                                         /* the second key (G3: ii) with the first held: SLASH: Dm/C */
    bad += check("SLASH: a second key over the first: Dm over the C bass, the C chord's own notes gone",
                 str_eq_test(S.name, "Dm/C") && sounding(50) && sounding(53) && sounding(57) && !sounding(52));
    release(2); release(0);
    bad += check("  all up: silence", !sounding(50) && !sounding(48) && !sounding(24));
    press(12);                                        /* F4: the first plate key: the chord last built was the bass key's own, C */
    bad += check("strumplate: the first plate key plays the last chord's root at C4 (the bass key's C came back)", sounding(60));
    press(14);
    bad += check("  the second: its third (E4)", sounding(64));
    release(12); release(14);
    bad += check("  up: silent", !sounding(60) && !sounding(64));
    press(5);                                         /* A#3: HOLD on */
    release(5);
    press(0); release(0);
    bad += check("HOLD: the chord stays after the key", S.hold && sounding(48) && sounding(52));
    press(5); release(5);
    bad += check("  HOLD off: it ends", !S.hold && !sounding(48));
    turn(1, 1);                                       /* ALGORITHM: LEAD */
    bad += check("ALGORITHM: LEAD", S.mode == HP_LEAD);
    press(0); press(2);
    bad += check("  LEAD: the first two keys C4 and D4 together, one note each", sounding(60) && sounding(62) && !sounding(48));
    release(0); release(2);
    turn(1, -1);
    turn(0, 2);                                       /* SELECT: the key of D */
    press(0);
    bad += check("SELECT: the key of D: the first key plays D (D3 F#3 A3)", sounding(50) && sounding(54) && sounding(57) && str_eq_test(S.name, "D"));
    release(0);
    button(3);                                        /* SCL: MINOR */
    press(0);
    bad += check("SCL: D minor: the first key Dm", str_eq_test(S.name, "Dm") && sounding(53));
    release(0);
    tick(2);
    if (argc > 1) ppm(argv[1]);
    printf("%s\n", bad ? "PORT TEST FAILED" : "port test passed");
    return bad != 0;
}
