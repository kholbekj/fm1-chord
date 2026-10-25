/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* A web emulator session played back on the chord machine UI's host build (the real firmware code, the stub
 * display, the audio clock): every key, button and knob at its time, and after each one what the UI and the
 * looper are doing. The emulator logs a session from Start (its local server, tools/server.py: one JSON line an
 * event, {"e":"key","id":..,"d":1,"t":ms} / {"e":"enc","i":..,"c":..} / {"e":"pot","v":..}).
 *   build/host/hui_replay SESSION.jsonl [SHOTDIR]      (SHOTDIR: the screen after every event, NNN.ppm)
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/hui_replay tests/hui_replay.c -lm
 * The session starts from power-on defaults: a browser that kept a save of this firmware started from that
 * instead (its key, scale, sounds and styles), so a replay can differ there. */
#include "ui_host.c"

/* the emulator's ids: 0..13 the buttons in its panel order, 14.. the keys (F3 up); its knobs in panel order */
static const uint8_t RP_BTN[14] = {B_OCTDN, B_OCTUP, B_FX, B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ,
                                   B_PLAY, B_REC};
static const char *const RP_BTN_NAME[14] = {"OCT-", "OCT+", "FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP",
                                            "SEQ", "PLAY", "REC"};
static const uint8_t RP_ENC[7] = {EN_SELECT, EN_PRESET, EN_ALGO, EN_K1, EN_K2, EN_K3, EN_K4};
static const char *const RP_ENC_NAME[7] = {"SELECT", "PRESETS", "ALGORITHM", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4"};
static const char *const RP_SCREEN[HU_N] = {"HOME", "KEY", "SOUND", "MODE", "PRESETS", "LOOPER", "LIST", "MIX"};
static const char *const RP_NOTE[12] = {"F", "F#", "G", "G#", "A", "A#", "B", "C", "C#", "D", "D#", "E"};

static uint32_t rp_ms;                                   /* the replay's clock: ms since Start */
static uint64_t rp_samples;                              /* the audio clock, in samples (FS a second, CTL a block) */
static void rp_run(uint32_t to)                          /* both to `to`, a UI frame every 16 ms */
{
    while (rp_ms < to) {
        rp_ms++;
        while (rp_samples + CTL <= (uint64_t)rp_ms * FS / 1000u) {
            events_block(CTL);
            rp_samples += CTL;
        }
        if (rp_ms % 16u == 0u)
            frame();
    }
}

static void rp_state(FILE *o)
{
    uint32_t i;
    fprintf(o, "  -> %-7s L%u %-6s %-12s", hui.ly && hui.ly_open ? "(held)" : RP_SCREEN[hui.screen % HU_N], song.sel + 1u,
            HP_SHORT[hc.t[song.sel].play % HP_COUNT], HC_SOUNDS[hc.t[song.sel].sound % HC_NSOUNDS].name);
    for (i = 0; i < HCL_LAYERS; i++)
        fprintf(o, " L%u:%s/%u%s", i + 1u, HLS_NAME[hcl.l[i].state & 3u], hcl.l[i].n, trk[i].p[P_MUTE] ? "m" : "");
    if (hcl.len)
        fprintf(o, "  loop %.2f beats @%.2f%s", (double)hcl.len / beat_samples(), (double)hcl.pos / beat_samples(),
                hcl.playing ? "" : " paused");
    if (hcs.dl_running && hc.t[song.sel].play == HP_DRUMLOOP)
        fprintf(o, "  drums running");
    if (hui.msg_t)
        fprintf(o, "  \"%s\"", hui.msg);
    fputc('\n', o);
}

static void rp_ppm(const char *dir, uint32_t n)
{
    char path[512];
    uint32_t i;
    FILE *f;
    snprintf(path, sizeof path, "%s/%03u.ppm", dir, n);
    if (!(f = fopen(path, "wb")))
        return;
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint32_t c = host_screen[i], v = ((c & 0xFFu) << 8) | (c >> 8);
        uint8_t rgb[3] = {(uint8_t)(((v >> 11) & 31u) * 255u / 31u), (uint8_t)(((v >> 5) & 63u) * 255u / 63u),
                          (uint8_t)((v & 31u) * 255u / 31u)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static int rp_num(const char *line, const char *key, long *v)   /* "key":123 in a JSON line */
{
    char pat[16];
    const char *p;
    snprintf(pat, sizeof pat, "\"%s\":", key);
    if (!(p = strstr(line, pat)))
        return 0;
    *v = strtol(p + strlen(pat), 0, 10);
    return 1;
}

int main(int argc, char **argv)
{
    char line[512];
    long t, id, d, i, c, v;
    uint32_t n = 0;
    FILE *in;
    if (argc < 2 || !(in = fopen(argv[1], "r"))) {
        fprintf(stderr, "usage: hui_replay SESSION.jsonl [SHOTDIR]\n");
        return 2;
    }
    ui_power_on();
    hc_init();
    memset(&hui, 0, sizeof hui);
    hui.force = 1;
    hui.rnd = 0x9E3779B9u;
    settings.palette = UI_DEFAULT_INDEX;
    palette_set(UI_DEFAULT_INDEX);
    usb.config = 1;
    song.g[G_BPM] = 120;
    frame(); frame();
    printf("%8s  %-24s", "0", "power on");
    rp_state(stdout);
    while (fgets(line, sizeof line, in)) {
        char what[48];
        if (!rp_num(line, "t", &t))
            continue;                                    /* (the start line) */
        rp_run((uint32_t)t);
        if (strstr(line, "\"e\":\"key\"") && rp_num(line, "id", &id) && rp_num(line, "d", &d)) {
            if (id < 14) {
                uint32_t bit = 1u << panel.btn[RP_BTN[id]];
                if (d) { fm1_in.buttons |= bit; host_pressed |= bit; } else fm1_in.buttons &= ~bit;
                snprintf(what, sizeof what, "%s %s", RP_BTN_NAME[id], d ? "down" : "up");
            } else {
                uint32_t k = (uint32_t)(id - 14);
                if (d) key_down(k); else key_up(k);
                snprintf(what, sizeof what, "key %u %s%u %s", k, RP_NOTE[k % 12u], 3u + (k + 5u) / 12u, d ? "down" : "up");
            }
        } else if (strstr(line, "\"e\":\"enc\"") && rp_num(line, "i", &i) && rp_num(line, "c", &c) && i >= 0 && i < 7) {
            uint32_t r = RP_ENC[i];
            host_enc[panel.enc[r]] += (int32_t)c * panel.dir[r];
            host_ticks += 200000u;
            snprintf(what, sizeof what, "%s %+ld", RP_ENC_NAME[i], c);
        } else if (strstr(line, "\"e\":\"pot\"") && rp_num(line, "v", &v)) {
            snprintf(what, sizeof what, "MASTER %ld", v);
        } else {
            continue;
        }
        frame();
        printf("%8.2f  %-24s", t / 1000.0, what);
        rp_state(stdout);
        if (argc > 2)
            rp_ppm(argv[2], n);
        n++;
    }
    rp_run(rp_ms + 3000u);                               /* and three seconds on */
    printf("%8.2f  %-24s", rp_ms / 1000.0, "(3 s later)");
    rp_state(stdout);
    return 0;
}
