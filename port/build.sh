#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 fm1-chord contributors
# Build an FM1 Open application against M-VAVE's binary SDK with JieLi's Linux toolchain: the same
# compiler flags, link line and layout as sdk/Build.ps1 in m-vave/fm1_open (GPL-3.0 with the SDK linking
# exception), without PowerShell or the Windows toolchain.
#   port/build.sh [APP_DIR]        APP_DIR: the app sources (default: the official app, FM1_OPEN/app)
#   FM1_OPEN          the m-vave/fm1_open checkout (default ~/fm1/fm1_open)
#   JIELI_TOOLCHAIN   the JieLi Linux toolchain (default ~/.jieli/toolchain)
#   PRODUCT           the update identity FM-1_8NNNNNN (default: app/version.h)
# Output: build/port/sdk.elf, sdk.map, app.bin (the packaged image), FM1_PORT.fwsc
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPEN="${FM1_OPEN:-$HOME/fm1/fm1_open}"
TC="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}/pi32v2/bin"
APP="$OPEN/app"
OVER="${1:-}"                                        # an overlay: its files replace the official app's
[ -z "$OVER" ] || OVER=$(cd "$OVER" && pwd)
OUT="${OUT_DIR:-$ROOT/build/port}"
src_of() { if [ -n "$OVER" ] && [ -f "$OVER/$1" ]; then echo "$OVER/$1"; else echo "$APP/$1"; fi; }
[ -x "$TC/clang" ] || { echo "build.sh: no toolchain at $TC"; exit 1; }
[ -f "$OPEN/sdk/lib/libfm1_engine.a" ] || { echo "build.sh: no SDK at $OPEN"; exit 1; }
PRODUCT="${PRODUCT:-$(sed -n 's/^#define FM1_UPDATE_PRODUCT "\(FM-1_8[0-9]*\)".*/\1/p' "$(src_of version.h)")}"
case "$PRODUCT" in FM-1_8[0-9][0-9][0-9][0-9][0-9][0-9]) ;; *) echo "build.sh: bad product $PRODUCT"; exit 1;; esac
rm -rf "$OUT/obj" "$OUT/generated"
mkdir -p "$OUT/obj" "$OUT/generated"
FLAGS="-target pi32v2 -integrated-as -mcpu=r3 -mfprev1 -Wuninitialized -Wno-invalid-noreturn -fno-common -flto
 -fallow-pointer-null -fprefer-gnu-section -femulated-tls -Wno-shift-negative-value -Wframe-larger-than=2560
 -fms-extensions -fno-unwind-tables -ffunction-sections -fdata-sections -fmessage-length=0
 -cl-single-precision-constant -mllvm -pi32v2-large-program=true -D__GCC_PI32V2__ -O2
 -I$OPEN/sdk/include -I$OPEN/sdk/include/libc -I$APP/synth -I$OPEN/sdk/config ${OVER:+-I$OVER -I$ROOT/firmware/src} -I$APP -I$APP/board"
OBJS=""
n=0
SRCS=$( (cd "$APP" && find . -type f \( -name '*.c' -o -name '*.S' \) ! -path './synth/*'; \
         [ -z "$OVER" ] || (cd "$OVER" && find . -type f \( -name '*.c' -o -name '*.S' \)) ) | sed 's|^\./||' | sort -u)
for src in $SRCS; do
    opt="$FLAGS -fwrapv -fno-fast-math -ffp-contract=off"
    case "$src" in
        screen.c|transfer.c) opt="$opt -fno-lto" ;;
        board/panel.c) opt="$opt -fno-lto -fno-pic" ;;
    esac
    case "$src" in project.c|transfer.c|samples.c|screen.c|presets.c) opt="$opt -Oz" ;; esac
    obj="$OUT/obj/${n}_$(echo "$src" | sed 's/[^a-zA-Z0-9._-]/_/g').obj"
    n=$((n + 1))
    echo "compile $src$( [ "$(src_of "$src")" = "$APP/$src" ] || echo ' (overlay)')"
    "$TC/clang" $opt -c "$(src_of "$src")" -o "$obj"
    OBJS="$OBJS $obj"
done
# the build identity: the app sources hashed; the layout and the update identity the package expects
ID=$( (cd "$APP" && find . -type f | sort | xargs sha256sum; [ -z "$OVER" ] || (cd "$OVER" && find . -type f | sort | xargs sha256sum)) | sha256sum | cut -c1-64)
IDB=$(echo "$ID" | sed 's/../0x&,/g; s/,$//')
cat > "$OUT/generated/build_identity.c" <<EOF
#include "layout.h"
const struct fm1_firmware_layout fm1_layout = {536576U, 7U};
const unsigned char fm1_build_id[32] = {$IDB};
const unsigned char fm1_update_identity[27] = "$PRODUCT";
EOF
"$TC/clang" $FLAGS -c "$OUT/generated/build_identity.c" -o "$OUT/obj/build_identity.obj"
OBJS="$OBJS $OUT/obj/build_identity.obj"
sed -e 's/@SBC_DECODER@/sbc_decoder\n/' -e 's/@ACL_TX_EXTRA@/ + 1024/' -e 's/@ACL_RX_EXTRA@/ + 4096/' \
    "$OPEN/sdk/config/linker.ld.in" > "$OUT/generated/linker"
echo "link"
cd "$OUT"
"$TC/ld" --plugin "$TC/LLVMgold.so" -o sdk.elf $OBJS -flto \
    --plugin-opt=-pi32v2-always-use-itblock=false --plugin-opt=-enable-ipra=true \
    --plugin-opt=-pi32v2-merge-max-offset=4096 --plugin-opt=-pi32v2-enable-simd=true --plugin-opt=mcpu=r3 \
    --plugin-opt=-global-merge-on-const --plugin-opt=-inline-threshold=5 --plugin-opt=-inline-max-allocated-size=32 \
    --plugin-opt=-inline-normal-into-special-section=true \
    --plugin-opt=-dont-used-symbol-list=malloc,free,sprintf,printf,puts,putchar \
    --plugin-opt=-pi32v2-enable-rep-memop --plugin-opt=-emulated-tls --sort-common \
    --plugin-opt=-pi32v2-large-program=true --plugin-opt=-pi32v2-csync-before-rti=true \
    --start-group --whole-archive "$OPEN/sdk/lib/libfm1_engine.a" --no-whole-archive \
    "$OPEN/sdk/lib/libc.a" "$OPEN/sdk/lib/libcompiler_rt.a" --end-group \
    --gc-sections -Tgenerated/linker -Map=sdk.map \
    --undefined=connectivity_wl82_bluetooth_start --undefined=audio_app_bluetooth_open \
    --undefined=bt_get_local_name --undefined=bt_get_mac_addr \
    --undefined=fm1_ble_set_role --undefined=fm1_ble_scan --undefined=fm1_ble_connect --undefined=fm1_ble_disconnect \
    --undefined=fm1_ble_get_status --undefined=fm1_ble_get_device --undefined=fm1_ble_forget_bonds \
    --undefined=fm1_bluetooth_configure --undefined=fm1_bluetooth_get_status
python3 "$ROOT/port/tools/fm1_check.py" sdk.elf sdk.map "$TC"
rm -f app.bin
for s in text data ram0_data cache_ram_data; do
    "$TC/objcopy" -O binary -j ".$s" sdk.elf "generated/$s.bin"
    cat "generated/$s.bin" >> app.bin
done
python3 "$ROOT/port/tools/fm1_package.py" app.bin "$OPEN/sdk/tools/pack" FM1_PORT.fwsc "$PRODUCT"
ls -l sdk.elf app.bin FM1_PORT.fwsc
