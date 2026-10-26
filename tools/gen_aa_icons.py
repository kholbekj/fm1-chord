#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Rasterise the icons the firmware draws from the Fukiai icon font (web/fukiai.ttf, MIT, (c) Hügelton
Instruments) into 4-bit alpha cells (run by tools/build.py generate()).

  gen_aa_icons.py OUT.h [--font web/fukiai.ttf] [--gamma G] [--list] [--report]

Output: ICON_<NAME> for each of ICONS, and per size S of SIZES
  AI<S>_DATA    cells of S x S px, 4-bit alpha, 2 px per byte, rows back to back
  AI<S>_IDX[ICON_COUNT]  cell number of every icon at that size, 0xFF = not rasterised at that size
--list prints every Fukiai glyph name and its codepoint (what is available).
--report prints the mapping and the byte cost.
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import aa_raster as ar  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]

# icon name -> Fukiai glyph: what the chord machine draws (hui.c the header's USB plug, ui_base.c the battery and
# the calibration title). A glyph given as a tuple: the first one the font has
ICONS = {
    "x_usb": "port_usb_c", "x_doctor": "symbol_doctor",
    # the battery (battery_2 is not used: the stock level 0..3 maps to battery_0 / 1 / 3 / 4, see ui_base.c batt_icon)
    "x_bat0": "system_battery_0", "x_bat1": "system_battery_1", "x_bat3": "system_battery_3",
    "x_bat4": "system_battery_4", "x_bat_chg": "system_battery_charging",
}

# glyphs broken in the font, mended before rasterising (a mended font needs no change here: a glyph already
# on the em, advance == unitsPerEm, is left alone). Fukiai 0.8.0 system_battery_0: drawn at the wrong scale
# (advance 4653, outline 4654 x 2048 units on a 2048 em) and with a filled copy of its counter (an extra
# contour of the inner box wound like the outside: the battery reads solid). The copy is dropped (a contour
# with the box of a counter but the outside's winding) and the rest fitted into the outline box of a sibling.
FIT_TO = {"system_battery_0": "system_battery_4"}

# the sizes each is drawn at: 16 px the header's USB and the calibration title, 24 px the header battery
# (Fukiai's battery is a wide, short glyph: at 16 px its body was 12 x 6)
SIZES = {16: ["x_usb", "x_doctor"], 24: ["x_bat0", "x_bat1", "x_bat3", "x_bat4", "x_bat_chg"]}


def glyph_table(font_path):
    from fontTools.ttLib import TTFont
    f = TTFont(str(font_path))
    return {name: cp for cp, name in f.getBestCmap().items()}


def resolve(glyph, cps):
    """a glyph name, or a tuple of them: the first one the font has (the last one if none, reported missing)"""
    if isinstance(glyph, tuple):
        return next((g for g in glyph if g in cps), glyph[-1])
    return glyph


def fitted_font(font_path, out_dir, names):
    """font_path, or a copy with the FIT_TO glyphs among names mended (see FIT_TO)"""
    from fontTools.pens.recordingPen import RecordingPen
    from fontTools.pens.ttGlyphPen import TTGlyphPen
    from fontTools.ttLib import TTFont
    f = TTFont(str(font_path), recalcBBoxes=False)     # (the copy keeps every other glyph's stored header
    flags = f["head"].flags                             #  and the head flags: they rasterise unchanged)
    upm, glyf, hmtx, gs = f["head"].unitsPerEm, f["glyf"], f["hmtx"], f.getGlyphSet()
    fixed = []

    def area(pts):
        return sum(pts[i][0] * pts[i - 1][1] - pts[i - 1][0] * pts[i][1] for i in range(len(pts))) / 2

    for g, ref in FIT_TO.items():
        if g not in names or g not in glyf.glyphs or hmtx[g][0] == upm:
            continue
        rec = RecordingPen()
        gs[g].draw(rec)
        contours, cur = [], []
        for op, args in rec.value:
            cur.append((op, args))
            if op in ("closePath", "endPath"):
                contours.append(cur)
                cur = []
        info = []
        for c in contours:
            pts = [p for _, args in c for p in args]
            xs, ys = [p[0] for p in pts], [p[1] for p in pts]
            info.append((area(pts), (min(xs), min(ys), max(xs), max(ys))))
        outer = max(range(len(info)), key=lambda i: abs(info[i][0]))
        sign = info[outer][0] > 0

        def near(p, q):
            return all(abs(u - v) <= 2 for u, v in zip(p, q))
        keep = [c for i, c in enumerate(contours)
                if not ((info[i][0] > 0) == sign and i != outer and
                        any(j != i and (info[j][0] > 0) != sign and near(info[i][1], info[j][1]) for j in range(len(info))))]
        x0 = min(info[i][1][0] for i in range(len(info)))
        y0 = min(info[i][1][1] for i in range(len(info)))
        x1 = max(info[i][1][2] for i in range(len(info)))
        y1 = max(info[i][1][3] for i in range(len(info)))
        r = glyf[ref]
        stored = (r.xMin, r.yMin, r.xMax, r.yMax)
        r.recalcBounds(glyf)
        sx, sy = (r.xMax - r.xMin) / (x1 - x0), (r.yMax - r.yMin) / (y1 - y0)
        pen = TTGlyphPen(None)
        for c in keep:
            for op, args in c:
                getattr(pen, op)(*[(r.xMin + (x - x0) * sx, r.yMin + (y - y0) * sy) for x, y in args])
        r.xMin, r.yMin, r.xMax, r.yMax = stored
        new = pen.glyph()
        new.xMin, new.yMin, new.xMax, new.yMax = stored  # (as stored for its sibling)
        glyf[g] = new
        hmtx[g] = hmtx[ref]
        fixed.append((g, len(contours) - len(keep)))
    if not fixed:
        return str(font_path), fixed
    p = Path(out_dir) / (Path(font_path).stem + "_fitted.ttf")
    f["head"].flags = flags
    f.save(str(p))
    return str(p), fixed


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?")
    ap.add_argument("--font", default=str(ROOT / "web" / "fukiai.ttf"))
    ap.add_argument("--gamma", type=float, default=1.0)
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--report", action="store_true")
    a = ap.parse_args()
    cps = glyph_table(a.font)
    if a.list:
        for name, cp in sorted(cps.items(), key=lambda kv: kv[1]):
            print(f"U+{cp:04X} {name}")
        return
    if not a.out:
        ap.error("OUT.h is required")
    names = list(ICONS)
    glyph_of = {n: resolve(g, cps) for n, g in ICONS.items()}
    bad = [f"{n} -> {g}" for n, g in glyph_of.items() if g not in cps]
    if bad:
        print("glyphs not in the font:", bad)
        raise SystemExit(1)
    font, fixed = fitted_font(a.font, Path(a.out).parent, set(glyph_of.values()))
    for g, dropped in fixed:
        print(f"icons: {g} is off the em in {Path(a.font).name}: fitted to {FIT_TO[g]}'s box"
              + (f", {dropped} filled counter copy dropped" if dropped else ""))
    for n, g in ICONS.items():
        if isinstance(g, tuple) and glyph_of[n] != g[0]:
            print(f"icons: {n}: {g[0]} not in the font yet, using {glyph_of[n]}")
    mended = {g for g, _ in fixed}                     # (only those come from the mended copy)
    out = [f"/* generated by tools/gen_aa_icons.py from {Path(a.font).name} (Fukiai, MIT, (c) 2026 Hügelton Instruments) */",
           "#pragma once", "#include <stdint.h>", "enum {"]
    out += [f"    ICON_{n.upper()}," for n in names]
    out += ["    ICON_COUNT", "};", ""]
    cost = {}
    for s, subset in SIZES.items():
        data, idx = [], [0xFF] * len(names)
        for n in subset:
            b, ink = ar.raster_icon(font if glyph_of[n] in mended else a.font, s, cps[glyph_of[n]], a.gamma)
            assert ink, f"{n} is empty at {s}px"
            idx[names.index(n)] = len(data)
            data.append(b)
        flat = [v for b in data for v in b]
        out.append(ar.c_array(f"AI{s}_DATA", "uint8_t", flat, 24, "0x{:02x}"))
        out.append(ar.c_array(f"AI{s}_IDX", "uint8_t", idx, 24))
        out.append("")
        cost[s] = (len(data), len(flat) + len(idx))
    Path(a.out).write_text("\n".join(out))
    for s, (n, b) in cost.items():
        print(f"icons {s}px: {n} cells, {b} B")
    print(f"icons total {sum(c[1] for c in cost.values())} B -> {a.out}")
    if a.report:
        for n in names:
            print(f"  ICON_{n.upper():12s} {glyph_of[n]}")


if __name__ == "__main__":
    main()
