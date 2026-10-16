/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (C) 2026 fm1-chord contributors
 * The band painter and the 5 x 7 font are from app/screen.c of m-vave/fm1_open,
 * Copyright (c) 2026 M-VAVE, GPL-3.0 with the same linking exception. */
/* fm1-chord on the FM1 Open SDK: the chord keys over the stock instrument and the HOME screen. Replaces
 * the official app/screen.c (groove_screen_init / groove_screen_control / groove_screen_mmc); the rest of
 * the official app (the board, the project store, the transfers) stays. What the keys build plays as MIDI
 * into the instrument's first track (fm1_midi_post), so every stock sound is a chord sound and the
 * Bluetooth MIDI link in the SDK carries the same notes out. The harmony is harmony.c, shared with the
 * Felucca build.
 *   the seven white keys at the left: the degrees (the first is the tonic); the nine at the right: the
 *   strumplate (PLAY) or, in LEAD, every white key one note of the scale up from the tonic
 *   black keys: F#3 INVERT, G#3 LOCK (unused yet), A#3 HOLD; C#4 .. F#5 the eight directions
 *   SELECT the key, ALGORITHM the mode (PLAY / LEAD), PRESETS the sound (the track's program),
 *   KNOB 1 the cutoff, SCL the scale, OCT- / OCT+ the octave */
#include "instrument.h"
#include "fm_patch.h"
#include "version.h"
#include "fm1_engine_control.h"
#include <string.h>

static uint32_t str_len(const char *s) { uint32_t n = 0; while (s[n]) n++; return n; }
static void str_cpy(char *d, const char *s, uint32_t n)
{
    uint32_t i;
    if (!n) return;
    for (i = 0; i + 1u < n && s[i]; i++) d[i] = s[i];
    d[i] = 0;
}
static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
#include "harmony.c"

/* ------------------------------------------------------------------ the painter (M-VAVE) --- */
enum { BACK = 0x0000, TEXT = 0xef5d, DIM = 0x738e, MID = 0xa534, GREY = 0x8410, WHITEK = 0xf79e };
#define RGB(r, g, b) ((uint16_t)(((r) >> 3) << 11 | ((g) >> 2) << 5 | ((b) >> 3)))
static const uint8_t font[36][5] = {
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, {0, 0x42, 0x7f, 0x40, 0},       {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4b, 0x31}, {0x18, 0x14, 0x12, 0x7f, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03}, {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1e}, {0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36},
    {0x3e, 0x41, 0x41, 0x41, 0x22}, {0x7f, 0x41, 0x41, 0x22, 0x1c}, {0x7f, 0x49, 0x49, 0x49, 0x41},
    {0x7f, 0x09, 0x09, 0x09, 0x01}, {0x3e, 0x41, 0x49, 0x49, 0x7a}, {0x7f, 0x08, 0x08, 0x08, 0x7f},
    {0, 0x41, 0x7f, 0x41, 0},       {0x20, 0x40, 0x41, 0x3f, 0x01}, {0x7f, 0x08, 0x14, 0x22, 0x41},
    {0x7f, 0x40, 0x40, 0x40, 0x40}, {0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f},
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, {0x7f, 0x09, 0x09, 0x09, 0x06}, {0x3e, 0x41, 0x51, 0x21, 0x5e},
    {0x7f, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7f, 0x01, 0x01},
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f}, {0x3f, 0x40, 0x38, 0x40, 0x3f},
    {0x63, 0x14, 0x08, 0x14, 0x63}, {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}};
enum { PAINT_ROWS = 16, PAINT_BANDS = FM1_SCREEN_HEIGHT / PAINT_ROWS };
static uint16_t paint_pixels[FM1_SCREEN_WIDTH * PAINT_ROWS];
static uint8_t paint_left[PAINT_BANDS], paint_right[PAINT_BANDS];
static unsigned paint_rendering, paint_top, paint_x, paint_end;

static void paint_mark(unsigned x, unsigned y, unsigned w, unsigned h)
{
    for (unsigned b = y / PAINT_ROWS; b <= (y + h - 1) / PAINT_ROWS && b < PAINT_BANDS; ++b) {
        if (x < paint_left[b]) paint_left[b] = (uint8_t)x;
        if (x + w > paint_right[b]) paint_right[b] = (uint8_t)(x + w);
    }
}
static __attribute__((noinline)) void paint_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t color)
{
    if (!w || !h || x >= FM1_SCREEN_WIDTH || y >= FM1_SCREEN_HEIGHT) return;
    if (w > FM1_SCREEN_WIDTH - x) w = FM1_SCREEN_WIDTH - x;
    if (h > FM1_SCREEN_HEIGHT - y) h = FM1_SCREEN_HEIGHT - y;
    if (!paint_rendering) { paint_mark(x, y, w, h); return; }
    if (x >= paint_end || x + w <= paint_x) return;
    unsigned right = x + w < paint_end ? x + w : paint_end;
    if (x < paint_x) x = paint_x;
    unsigned first = y < paint_top ? paint_top : y;
    unsigned end = y + h < paint_top + PAINT_ROWS ? y + h : paint_top + PAINT_ROWS;
    for (unsigned row = first; row < end; ++row)
        for (unsigned col = x; col < right; ++col)
            paint_pixels[(row - paint_top) * (paint_end - paint_x) + col - paint_x] = color;
}
/* the glyph of c: digits, letters (lower case shown as upper), a few signs */
static const uint8_t *glyph(char c, uint8_t special[5])
{
    char letter = c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c;
    memset(special, 0, 5);
    if (c >= '0' && c <= '9') return font[c - '0'];
    if (letter >= 'A' && letter <= 'Z') return font[10 + letter - 'A'];
    if (c == '-' || c == '+') { memset(special, 8, 5); if (c == '+') special[2] = 0x3e; }
    else if (c == '/') { special[0] = 0x60; special[1] = 0x18; special[2] = 0x06; special[3] = 1; }
    else if (c == '#') { special[0] = special[4] = 0x14; special[1] = special[3] = 0x7f; special[2] = 0x14; }
    else if (c == '.') { special[2] = 0x40; }
    else if (c == ':') { special[2] = 0x24; }
    else if (c == '<') { special[1] = 8; special[2] = 0x14; special[3] = 0x22; }
    else if (c == '>') { special[1] = 0x22; special[2] = 0x14; special[3] = 8; }
    return special;
}
static void text(unsigned x, unsigned y, const char *s, unsigned scale, uint16_t color, uint16_t background)
{
    if (!s || !scale || scale > 5 || x >= FM1_SCREEN_WIDTH || y >= FM1_SCREEN_HEIGHT || 7U * scale > FM1_SCREEN_HEIGHT - y) return;
    unsigned w = 6U * scale, h = 7U * scale;
    if (paint_rendering && (y >= paint_top + PAINT_ROWS || y + h <= paint_top)) return;
    if (!paint_rendering) {
        unsigned n = (unsigned)strlen(s), available = (FM1_SCREEN_WIDTH - x) / w;
        if (n > available) n = available;
        if (n) paint_mark(x, y, n * w, h);
        return;
    }
    for (; *s && x + w <= FM1_SCREEN_WIDTH; ++s, x += w) {
        uint8_t special[5];
        const uint8_t *shape;
        if (x >= paint_end) break;
        if (x + w <= paint_x) continue;
        shape = glyph(*s, special);
        unsigned first_row = y < paint_top ? paint_top : y;
        unsigned end_row = y + h < paint_top + PAINT_ROWS ? y + h : paint_top + PAINT_ROWS;
        unsigned first_col = x < paint_x ? paint_x : x;
        unsigned end_col = x + w < paint_end ? x + w : paint_end;
        for (unsigned row = first_row; row < end_row; ++row)
            for (unsigned col = first_col; col < end_col; ++col) {
                unsigned column = (col - x) / scale;
                paint_pixels[(row - paint_top) * (paint_end - paint_x) + col - paint_x] =
                    column < 5U && (shape[column] & (1U << ((row - y) / scale))) ? color : background;
            }
    }
}
static void text_c(unsigned cx, unsigned y, const char *s, unsigned scale, uint16_t color, uint16_t background)
{
    unsigned w = (unsigned)strlen(s) * 6U * scale;
    text(cx > w / 2 ? cx - w / 2 : 0, y, s, scale, color, background);
}

/* ------------------------------------------------------------------ the instrument --- */
enum { HP_PLAY, HP_LEAD, HP_COUNT };
enum { HB_OFF, HB_ROOT, HB_SLASH };
#define NKEY 27u
#define HC_BASE 48                                        /* the tonic key sounds C3 (the C4 key's pitch an octave down) */
#define PLATE_BASE 60                                     /* the strumplate's first note: the root at C4 */
#define LEAD_BASE 60
#define TRACK 0u                                          /* the instrument track the keys play (MIDI channel 1) */
#define NSCALE 10u
static const char *const SCALE_NAME[NSCALE] = {"MAJOR", "MINOR", "DORIAN", "MIXOLYD", "LYDIAN", "PHRYGIAN", "LOCRIAN", "HARM MIN", "MEL MIN", "PENTA"};
static const uint16_t SCALE_MASK[NSCALE] = {              /* bit n = pitch class n above the tonic */
    0xAB5, 0x5AD, 0x6AD, 0x6B5, 0xAD5, 0x5AB, 0x56B, 0x9AD, 0xAAD, 0x295};
static const char *const DEG_NAME[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
static const char *const DIR_NAME[9] = {"UP", "UP-RIGHT", "RIGHT", "DOWN-RIGHT", "DOWN", "DOWN-LEFT", "LEFT", "UP-LEFT", ""};
static const char *const MODE_NAME[HP_COUNT] = {"PLAY", "LEAD"};
static const uint8_t BLACK_FN[11] = {255, 254, 253, HD_UP, HD_UR, HD_RIGHT, HD_DR, HD_DOWN, HD_DL, HD_LEFT, HD_UL};   /* 255 INVERT 254 LOCK 253 HOLD */
static const uint16_t DEG_COL[7] = {RGB(255, 92, 92), RGB(255, 160, 48), RGB(250, 220, 70), RGB(96, 214, 120),
                                    RGB(72, 200, 236), RGB(120, 128, 255), RGB(206, 112, 240)};

static struct {
    uint8_t key, scale, mode, bass, voices, hmode;        /* the tonic 0..11, the scale, PLAY / LEAD, BASS, VOICES, the modifier table */
    int8_t octave;
    uint8_t dir, ndir, dstack[8];                         /* the direction held (the last pressed) */
    uint8_t hold;                                         /* HOLD latch on */
    uint32_t latched;                                     /* keys let go that still sound */
    uint8_t inv[NKEY];
    uint8_t order[NKEY], nheld;                           /* chord keys down, in press order (SLASH: the first is the bass) */
    struct { uint8_t n; uint8_t note[8]; } snd[NKEY];     /* the notes a key holds in the instrument */
    uint8_t plate[NKEY];                                  /* a plate key's note + 1 */
    hchord_t cur;                                         /* the chord last built */
    int32_t cur_root;
    uint8_t cur_q, cur_deg, cur_key;
    char name[12];
    uint32_t keys_prev, buttons_prev;
    uint32_t sig_drawn, redraw_at;
    char program[13];
    unsigned program_no;
    uint32_t boot_ms;
} S;

static int key_black(uint32_t k) { return (int)((0x54Au >> ((k + 5u) % 12u)) & 1u); }
static uint32_t key_place(uint32_t k)
{
    uint32_t i, n = 0;
    for (i = 0; i < k; i++)
        n += (uint32_t)(key_black(i) == key_black(k));
    return n;
}
static uint32_t scale_mask(void) { return SCALE_MASK[S.scale % NSCALE]; }

static void midi3(uint8_t a, uint8_t b, uint8_t c)
{
    uint8_t m[3] = {a, b, c};
    fm1_midi_post(m, 3);
}
static void note_on(uint32_t note) { if (note < 128u) midi3((uint8_t)(0x90u | groove_channel(TRACK)), (uint8_t)note, 100); }
static void note_off(uint32_t note) { if (note < 128u) midi3((uint8_t)(0x80u | groove_channel(TRACK)), (uint8_t)note, 0); }

/* another key still holds note x */
static int held_elsewhere(uint32_t k, uint32_t x)
{
    uint32_t i, j;
    for (i = 0; i < NKEY; i++)
        if (i != k)
            for (j = 0; j < S.snd[i].n; j++)
                if (S.snd[i].note[j] == x)
                    return 1;
    return 0;
}
/* key k holds exactly the notes nn[0..n): the ones that go end, the new ones start, the rest keep sounding */
static void key_sound(uint32_t k, const uint8_t *nn, uint32_t n)
{
    uint32_t i, j;
    for (i = 0; i < S.snd[k].n; i++) {
        uint32_t x = S.snd[k].note[i];
        for (j = 0; j < n && nn[j] != x; j++)
            ;
        if (j == n && !held_elsewhere(k, x))
            note_off(x);
    }
    for (j = 0; j < n; j++) {
        uint32_t x = nn[j];
        for (i = 0; i < S.snd[k].n && S.snd[k].note[i] != x; i++)
            ;
        if (i == S.snd[k].n && !held_elsewhere(k, x))
            note_on(x);
    }
    S.snd[k].n = (uint8_t)(n > 8u ? 8u : n);
    for (j = 0; j < S.snd[k].n; j++)
        S.snd[k].note[j] = nn[j];
}
static void key_silence(uint32_t k) { key_sound(k, NULL, 0); }

/* the root (MIDI) and quality of chord key k's degree, before the modifiers */
static uint32_t degree_chord(uint32_t k, int32_t *root)
{
    uint32_t p = key_place(k), d = p % 7u, q;
    int32_t r;
    q = hs_degree_chord(scale_mask(), d, &r);
    *root = HC_BASE + S.key + r + 12 * (S.octave + (int32_t)(p / 7u));
    return q;
}
/* the chord key k plays now: built into S.cur (the display) and out to nn; its notes */
static uint32_t build(uint32_t k, uint8_t *nn)
{
    int32_t root, roff = 0, bass = -1;
    uint32_t q = degree_chord(k, &root), i, n;
    q = hq_modify(S.hmode, S.dir, q, &roff);
    root += roff;
    if (S.bass != HB_OFF) {
        bass = root - 24;
        if (S.bass == HB_SLASH && S.nheld && S.order[0] != k) {
            int32_t r0;
            degree_chord(S.order[0], &r0);
            bass = r0 - 24;
        }
        while (bass < 24)
            bass += 12;
    }
    hs_voice(&S.cur, root, q, bass, S.voices, S.inv[k]);
    hs_name(S.name, S.key, root, q, bass < 0 && S.inv[k] && S.cur.n ? S.cur.note[0] : bass);
    S.cur_root = root;
    S.cur_q = (uint8_t)q;
    S.cur_deg = (uint8_t)(key_place(k) % 7u);
    S.cur_key = (uint8_t)k;
    n = S.cur.n > 8u ? 8u : S.cur.n;
    for (i = 0; i < n; i++)
        nn[i] = S.cur.note[i];
    return n;
}
static void revoice_held(void)                            /* the chord keys down: their chords as the modifiers are now */
{
    uint32_t i;
    for (i = 0; i < S.nheld; i++) {
        uint8_t nn[8];
        uint32_t k = S.order[i], n = build(k, nn);
        key_sound(k, nn, n);
    }
}
static void order_add(uint32_t k)
{
    uint32_t i;
    for (i = 0; i < S.nheld; i++)
        if (S.order[i] == k)
            return;
    S.order[S.nheld++] = (uint8_t)k;
}
static void order_remove(uint32_t k)
{
    uint32_t i, j = 0;
    for (i = 0; i < S.nheld; i++)
        if (S.order[i] != k)
            S.order[j++] = S.order[i];
    S.nheld = (uint8_t)j;
}
/* the strumplate: place j's note of the chord last built, its pitch classes from the root up */
static uint32_t plate_note(uint32_t j)
{
    uint8_t pc[12];
    uint32_t n = 0, i, m, root = (uint32_t)((S.cur_root % 12 + 12) % 12), x;
    for (i = 0; i < S.cur.n; i++) {
        uint32_t c = (S.cur.note[i] + 12u - root) % 12u;
        for (m = 0; m < n && pc[m] != c; m++)
            ;
        if (m < n)
            continue;
        for (m = n; m > 0 && pc[m - 1u] > c; m--)
            pc[m] = pc[m - 1u];
        pc[m] = (uint8_t)c;
        n++;
    }
    if (!n)
        return 0;
    x = (uint32_t)(PLATE_BASE + 12 * S.octave) + root + pc[j % n] + 12u * (j / n);
    return x > 127u ? 127u : x;
}
/* LEAD: white key k's note of the scale, up from the tonic at C4 */
static uint32_t lead_note(uint32_t k, uint32_t *deg)
{
    uint32_t mask = scale_mask(), p = key_place(k), n = 0, i;
    uint8_t iv[12];
    for (i = 0; i < 12u; i++)
        if ((mask >> i) & 1u)
            iv[n++] = (uint8_t)i;
    *deg = p % n;
    return (uint32_t)clamp(LEAD_BASE + (int32_t)S.key + iv[p % n] + 12 * ((int32_t)(p / n) + S.octave), 0, 127);
}

static void key_down(uint32_t k)
{
    uint8_t nn[8];
    uint32_t p = key_place(k), n;
    if (key_black(k)) {
        uint32_t fn = BLACK_FN[p % 11u];
        if (fn == 255u) {                                  /* INVERT: the held chord keys' inversion cycles */
            uint32_t i;
            for (i = 0; i < S.nheld; i++)
                S.inv[S.order[i]] = (uint8_t)((S.inv[S.order[i]] + 1u) % 3u);
            revoice_held();
        } else if (fn == 253u) {                           /* HOLD: on, or off (the latched keys end) */
            S.hold = (uint8_t)!S.hold;
            if (!S.hold) {
                uint32_t i;
                for (i = 0; i < NKEY; i++)
                    if ((S.latched >> i) & 1u)
                        key_silence(i);
                S.latched = 0;
            }
        } else if (fn < HD_NONE) {                         /* a direction: the last pressed wins */
            if (S.ndir < 8u)
                S.dstack[S.ndir++] = (uint8_t)fn;
            S.dir = (uint8_t)fn;
            revoice_held();
        }
        return;
    }
    if (S.mode == HP_LEAD) {
        uint32_t d, x = lead_note(k, &d);
        S.plate[k] = (uint8_t)(x + 1u);
        key_sound(k, (uint8_t[]){(uint8_t)x}, 1);
        S.cur_deg = (uint8_t)(d < 7u ? d : 7u);
        return;
    }
    if (p >= 7u) {                                         /* the strumplate */
        uint32_t x;
        if (!S.cur.n)
            build(0u, nn);                                 /* nothing played yet: the tonic's chord */
        x = plate_note(p - 7u);
        S.plate[k] = (uint8_t)(x + 1u);
        if (x)
            key_sound(k, (uint8_t[]){(uint8_t)x}, 1);
        return;
    }
    S.latched &= ~(1u << k);
    order_add(k);
    if (S.bass == HB_SLASH && S.nheld >= 2u && S.order[0] != k)   /* SLASH: the bass key's own chord stops */
        key_silence(S.order[0]);
    n = build(k, nn);
    key_sound(k, nn, n);
}
static void key_up(uint32_t k)
{
    uint32_t p = key_place(k);
    if (key_black(k)) {
        uint32_t fn = BLACK_FN[p % 11u];
        if (fn < HD_NONE) {
            uint32_t i, j = 0;
            for (i = 0; i < S.ndir; i++)
                if (S.dstack[i] != fn)
                    S.dstack[j++] = S.dstack[i];
            S.ndir = (uint8_t)j;
            S.dir = (uint8_t)(S.ndir ? S.dstack[S.ndir - 1u] : HD_NONE);
            revoice_held();
        }
        return;
    }
    if (S.plate[k]) {                                      /* a plate or LEAD note: ends */
        S.plate[k] = 0;
        key_silence(k);
        return;
    }
    if (S.hold && S.snd[k].n) {
        S.latched |= 1u << k;
        order_remove(k);
        return;
    }
    order_remove(k);
    S.inv[k] = 0;
    key_silence(k);
    if (S.bass == HB_SLASH && S.nheld && !S.snd[S.order[0]].n) {   /* the chord key let go, the bass key held: its own chord */
        uint8_t nn[8];
        uint32_t b = S.order[0], n = build(b, nn);
        key_sound(b, nn, n);
    }
}
static void all_off(void)
{
    uint32_t k;
    for (k = 0; k < NKEY; k++) {
        key_silence(k);
        S.plate[k] = 0;
        S.inv[k] = 0;
    }
    S.nheld = 0;
    S.latched = 0;
    S.hold = 0;
}

/* ------------------------------------------------------------------ the HOME screen --- */
static void draw(void)
{
    char b[32];
    uint32_t i, held = S.keys_prev | S.latched, any = 0;
    uint16_t col = S.cur_deg < 7u ? DEG_COL[S.cur_deg] : TEXT;
    struct fm1_ble_status ble;
    paint_fill(0, 0, 240, 240, BACK);
    /* the header: the key and scale, the mode, Bluetooth */
    paint_fill(0, 0, 118, 22, GREY);
    str_cpy(b, HN_SHARP[S.key % 12u], sizeof b);
    str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
    str_cpy(b + str_len(b), SCALE_NAME[S.scale % NSCALE], sizeof b - str_len(b));
    text(4, 4, b, 2, RGB(20, 20, 22), GREY);
    paint_fill(122, 0, 60, 22, RGB(226, 62, 62));
    text_c(152, 4, MODE_NAME[S.mode % HP_COUNT], 2, TEXT, RGB(226, 62, 62));
    fm1_ble_get_status(&ble);
    {
        static const char *const BLE[] = {"BT OFF", "BT IDLE", "BT ADV", "BT SCAN", "BT CONN", "BT DISC", "BT LINK", "BT OK", "BT END"};
        text(188, 8, BLE[ble.state < 9u ? ble.state : 0], 1, ble.state == FM1_BLE_READY ? RGB(70, 200, 120) : DIM, BACK);
    }
    /* the chord, big; its degree and direction under it */
    for (i = 0; i < NKEY; i++)
        any |= (held >> i) & 1u;
    if (S.mode == HP_LEAD) {
        uint32_t n = 0, x = 0;
        b[0] = 0;
        for (i = 0; i < NKEY && n < 3u; i++)
            if (((held >> i) & 1u) && S.plate[i]) {
                x = S.plate[i] - 1u;
                if (n)
                    str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
                str_cpy(b + str_len(b), HN_SHARP[x % 12u], sizeof b - str_len(b));
                b[str_len(b) + 1] = 0;
                b[str_len(b)] = (char)('0' + (x / 12u > 0u ? x / 12u - 1u : 0u));
                n++;
            }
        text_c(120, 44, b[0] ? b : "LEAD", b[0] ? (n > 1u ? 3u : 4u) : 2u, col, BACK);
        if (n == 1u && S.cur_deg < 7u)
            text_c(120, 86, DEG_NAME[S.cur_deg], 2, MID, BACK);
    } else if (S.cur.n && (any || S.name[0])) {
        uint32_t len = str_len(S.name), scale = len <= 5u ? 4u : len <= 7u ? 3u : 2u;
        text_c(120, 40, S.name, scale, any ? col : MID, BACK);
        str_cpy(b, DEG_NAME[S.cur_deg % 7u], sizeof b);
        if (S.dir != HD_NONE) {
            str_cpy(b + str_len(b), "  ", sizeof b - str_len(b));
            str_cpy(b + str_len(b), DIR_NAME[S.dir % 9u], sizeof b - str_len(b));
        }
        if (S.inv[S.cur_key]) {
            str_cpy(b + str_len(b), S.inv[S.cur_key] == 1u ? "  1ST INV" : "  2ND INV", sizeof b - str_len(b));
        }
        text_c(120, 86, b, 2, MID, BACK);
    } else {
        text_c(120, 60, "PLAY A WHITE KEY", 2, DIM, BACK);
    }
    if (S.hold)
        paint_fill(8, 84, 46, 18, RGB(70, 200, 120)), text(13, 89, "HOLD", 1, RGB(20, 20, 22), RGB(70, 200, 120));
    /* the keyboard: 16 white keys, 11 black, the held ones lit; the degrees numbered, the plate's bars */
    {
        const int32_t ww = 14, x0 = 4, ky = 112, kh = 66;
        for (i = 0; i < NKEY; i++)
            if (!key_black(i)) {
                uint32_t p = key_place(i), on = (held >> i) & 1u;
                int32_t x = x0 + (int32_t)p * (ww + 1);
                uint16_t fill = on ? (S.mode == HP_LEAD ? TEXT : p < 7u ? DEG_COL[p] : TEXT) : RGB(44, 48, 56);
                paint_fill((unsigned)x, ky, (unsigned)ww, (unsigned)kh, fill);
                if (S.mode == HP_LEAD || p < 7u) {
                    uint32_t d;
                    if (S.mode == HP_LEAD) lead_note(i, &d); else d = p;
                    b[0] = (char)('1' + d % 7u); b[1] = 0;
                    text((unsigned)x + 4, ky + 50, b, 1, on ? RGB(20, 20, 22) : MID, fill);
                    if (S.mode != HP_LEAD)
                        paint_fill((unsigned)x + 3, ky + kh - 4, (unsigned)ww - 6, 3, DEG_COL[p]);
                } else {
                    int32_t h = 4 + (int32_t)(p - 7u) * 2;
                    paint_fill((unsigned)x + 4, (unsigned)(ky + kh - 3 - h), (unsigned)ww - 8, (unsigned)h, on ? RGB(20, 20, 22) : MID);
                }
            }
        for (i = 0; i < NKEY; i++)
            if (key_black(i)) {
                static const uint8_t AFTER[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};
                uint32_t p = key_place(i), on = (held >> i) & 1u;
                int32_t x = x0 + (int32_t)AFTER[p % 11u] * (ww + 1) + ww - 4;
                paint_fill((unsigned)x, ky, 9, 40, on ? (p < 3u ? RGB(70, 200, 120) : RGB(245, 196, 0)) : RGB(90, 94, 104));
                if (p < 3u) {
                    b[0] = "ILH"[p]; b[1] = 0;
                    text((unsigned)x + 2, ky + 14, b, 1, on ? RGB(20, 20, 22) : MID, on ? (p < 3u ? RGB(70, 200, 120) : RGB(245, 196, 0)) : RGB(90, 94, 104));
                }
            }
    }
    /* the sound and the hints */
    text(4, 190, S.program, 2, RGB(245, 196, 0), BACK);
    text(4, 222, "SCL SCALE  OCT-/+  SELECT KEY  ALGO MODE  PRESET SND", 1, DIM, BACK);
}
static void paint_flush(void)
{
    unsigned band;
    paint_rendering = 0;
    memset(paint_left, FM1_SCREEN_WIDTH, sizeof paint_left);
    memset(paint_right, 0, sizeof paint_right);
    draw();                                                /* the mark pass */
    paint_rendering = 1;
    for (band = 0; band < PAINT_BANDS; ++band) {
        unsigned left = paint_left[band], width = paint_right[band] - left;
        uint32_t t0 = fm1_millis();
        if (left >= paint_right[band])
            continue;
        while (fm1_screen_pending() > 0 && fm1_millis() - t0 < 40u)
            ;
        if (fm1_screen_pending())
            break;
        paint_top = band * PAINT_ROWS;
        paint_x = left;
        paint_end = left + width;
        draw();
        if (fm1_screen_submit(left, paint_top, width, PAINT_ROWS, paint_pixels, width * PAINT_ROWS))
            break;
    }
    paint_rendering = 0;
}
static void program_refresh(void)
{
    struct groove_view v;
    groove_get_view(TRACK, 0, &v);
    S.program_no = v.program;
    if (v.type == GROOVE_DX7) {
        const char *n = groove_fm_program_name(v.program);
        str_cpy(S.program, n ? n : "FM", sizeof S.program);
    } else {
        static const char *const SRC[] = {"DX7", "303", "808", "PSG", "PLUCK", "SAMPLE"};
        str_cpy(S.program, v.type < 6u ? SRC[v.type] : "?", sizeof S.program);
    }
}
static uint32_t signature(void)
{
    uint32_t s = S.keys_prev * 31u + S.latched * 7u + S.dir * 13u + S.hold * 17u + S.key * 19u + S.scale * 23u + S.mode * 29u +
                 (uint32_t)(S.octave + 8) * 37u + S.cur_root * 41u + S.cur_q * 43u + S.program_no * 47u + S.cur.n * 53u;
    uint32_t i;
    for (i = 0; i < 12u && S.name[i]; i++)
        s = s * 131u + (uint8_t)S.name[i];
    for (i = 0; i < NKEY; i++)
        s += S.plate[i] * (59u + i) + S.inv[i] * (61u + i);
    return s;
}

/* ------------------------------------------------------------------ the SDK's calls --- */
void groove_screen_init(void)
{
    memset(&S, 0, sizeof S);
    S.bass = HB_SLASH;
    S.voices = HV_4;
    S.hmode = HM_DEFAULT;
    S.dir = HD_NONE;
    S.boot_ms = fm1_millis();
    fm1_controls_init();
    for (unsigned key = 0; key < 41; ++key)
        fm1_led_set(key, 0);
    fm1_screen_init();
    (void)groove_settings_radio(GROOVE_RADIO_ON | GROOVE_RADIO_MIDI, groove_settings_role_value());   /* BLE MIDI on */
    program_refresh();
    str_cpy(S.name, "", sizeof S.name);
    S.sig_drawn = ~0u;
}
void groove_screen_control(void)
{
    struct fm1_controls c;
    uint32_t now = fm1_millis(), down, up, i, pressed;
    fm1_controls_read(&c);
    (void)groove_master_update(c.knob);
    down = c.keys & ~S.keys_prev;
    up = S.keys_prev & ~c.keys;
    for (i = 0; i < NKEY; i++) {                           /* the keys: black ones first (a direction before its chord) */
        if (((down >> i) & 1u) && key_black(i)) key_down(i);
        if (((up >> i) & 1u) && key_black(i)) key_up(i);
    }
    for (i = 0; i < NKEY; i++) {
        if (((down >> i) & 1u) && !key_black(i)) key_down(i);
        if (((up >> i) & 1u) && !key_black(i)) key_up(i);
    }
    for (i = 0; i < NKEY; i++)
        if ((down | up) & (1u << i))
            fm1_led_set(14u + i, (int)((c.keys >> i) & 1u));
    S.keys_prev = c.keys;
    pressed = c.buttons & ~S.buttons_prev;
    S.buttons_prev = c.buttons;
    if (pressed & (1u << 3)) { all_off(); S.scale = (uint8_t)((S.scale + 1u) % NSCALE); }       /* SCL: the next scale */
    if (pressed & (1u << 0)) { all_off(); S.octave = (int8_t)clamp(S.octave - 1, -2, 2); }    /* OCT- */
    if (pressed & (1u << 1)) { all_off(); S.octave = (int8_t)clamp(S.octave + 1, -2, 2); }    /* OCT+ */
    if (pressed & (1u << 8)) { all_off(); }                                                   /* HOME: everything off */
    if (c.encoders[0]) { all_off(); S.key = (uint8_t)(((int32_t)S.key + c.encoders[0]) % 12 + 12) % 12u; }   /* SELECT: the key */
    if (c.encoders[1]) { all_off(); S.mode = (uint8_t)(((int32_t)S.mode + c.encoders[1]) % HP_COUNT + HP_COUNT) % HP_COUNT; }
    if (c.encoders[6]) { groove_control_edit(TRACK, c.encoders[6], 0, 0); program_refresh(); }   /* PRESETS: the sound */
    if (c.encoders[2]) { groove_control_edit(TRACK, 0, 0, c.encoders[2] * 4); }                 /* KNOB 1: the cutoff */
    if (c.encoders[3]) { groove_control_edit(TRACK, 0, c.encoders[3] * 2, 0); }                 /* KNOB 2: the track's volume */
    {
        uint32_t sig = signature();
        if (sig != S.sig_drawn || (int32_t)(now - S.redraw_at) >= 0) {
            S.sig_drawn = sig;
            S.redraw_at = now + 500u;                       /* (the Bluetooth state, on its own) */
            paint_flush();
        }
    }
}
void groove_screen_mmc(unsigned command) { (void)command; }
