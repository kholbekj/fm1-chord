#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 fm1-chord contributors
# The structural checks sdk/Build.ps1 (Test-Firmware) makes on the linked image, on Linux: the entry, the
# allocated sections, the required runtime and application symbols, the forbidden platform parts, the
# link inputs. Exits non-zero on the first failure.
#   fm1_check.py sdk.elf sdk.map TOOLCHAIN_BIN
import re
import struct
import subprocess
import sys
from pathlib import Path

elf, mapfile, tc = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
fail = []
nm = subprocess.run([str(tc / "nm"), "--print-size", str(elf)], capture_output=True, text=True).stdout
info = {}
for line in nm.splitlines():
    m = re.match(r"^([0-9a-fA-F]+)[ \t]+(?:([0-9a-fA-F]+)[ \t]+)?([A-Za-z])[ \t]+(\S+)", line)
    if m:
        info[m.group(4)] = (int(m.group(1), 16), int(m.group(2), 16) if m.group(2) else None, m.group(3))
        if re.match(r"^(__fat_|fat_sdfile_fat_ops$|jl_fat_vfs_ops$|nor_sdfile_vfs_ops$|sdfile_ext_vfs_ops$)", m.group(4)):
            fail.append("unused filesystem entered the image: " + m.group(4))
        if m.group(4).startswith("lv_"):
            fail.append("unexpected LVGL runtime: " + m.group(4))
usb_midi = any(re.search(r"usb.*midi|midi.*usb", n) for n in info)
for name, size, stride in (("initcall", 4, 4), ("early_initcall", 16, 4), ("late_initcall", 4, 4), ("platform_initcall", 4, 4),
                           ("module_initcall", 4, 4), ("sys_event_handler", 12, 12), ("syscfg_ops", 84, 28), ("lp_target", 72, 8),
                           ("sys_cpu_timer", 16, 16), ("clock_critical_handler", 16, 8), ("vfs_ops", 120, 120),
                           ("audio_decoder", 56, 56), ("audio_hwaccel", 32, 32)):
    b, e = "_%s_begin" % name, "_%s_end" % name
    if b not in info or e not in info:
        b, e = "%s_begin" % name, "%s_end" % name
    if b not in info or e not in info:
        fail.append("registration range missing: " + name)
        continue
    n = info[e][0] - info[b][0]
    if n < size or n % stride:
        fail.append("registration table invalid: %s (%d)" % (name, n))
    if name == "vfs_ops" and n != 120:
        fail.append("expected only the internal flash filesystem registration")
required = ("fm1_r_7919135c9963 fm1_r_1f449c2c7b8f vSoftwareInterruptISR fm1_r_e7d3da61dac2 fm1_r_1993f9b103e3 fm1_r_05431712fb7b "
            "fm1_update_init fm1_update_message fm1_update_busy fm1_build_id fm1_flash_init sdfile_vfs_ops sdfile_init syscfg_vm_init "
            "syscfg_btif_init wifi_get_xosc wifi_get_mcs_dgain wifi_get_pa_trim_data connectivity_wl82_bluetooth_start ble_profile_init "
            "att_server_init fm1_ble_set_role fm1_ble_scan fm1_ble_connect fm1_ble_disconnect fm1_ble_get_status fm1_ble_get_device "
            "audio_app_usb_microphone_enable audio_app_usb_speaker_enable connectivity_audio_usb_microphone_enable "
            "connectivity_audio_usb_speaker_enable audio_app_bluetooth_open sbc_decoder sbc_hwaccel avdtp_packet_handler "
            "bt_get_local_name bt_get_mac_addr fm1_bluetooth_configure fm1_bluetooth_get_status").split()
for name in required:
    if name not in info:
        fail.append("required runtime symbol absent: " + name)
for name in ("sdp_send_cmd_iotl", "sdp_master_packet_handler", "gatt_client_discover_primary_services_by_uuid128"):
    if name not in info or info[name][2] not in "TtNn" or (info[name][1] or 0) <= 16:
        fail.append("bluetooth discovery implementation missing or stubbed: " + name)
for name in ("fm1_r_dee1ea411566 fm1_r_d16405f329ed fm1_r_f03cc16d330d fm1_r_1993f9b103e3 fm1_io_trs_midi_enabled "
             "fm1_r_05431712fb7b fm1_r_0aa010fe1de1 fm1_r_33d5e5b63b6b fm1_app_sysex").split():
    if name not in info or info[name][2] not in "TtNn" or not info[name][1]:
        fail.append("application callback lost or weak: " + name)
if info.get("fm1_build_id", (0, 0))[1] != 32:
    fail.append("fm1_build_id must be 32 bytes")
if info.get("fm1_update_identity", (0, 0))[1] != 27:
    fail.append("fm1_update_identity must be 27 bytes")
if not usb_midi:
    fail.append("USB MIDI runtime missing")
for name in ("CurrentTCB", "highCurrentTCB", "xIdleTaskHandle"):
    if name not in info or info[name][1] != 4:
        fail.append("RTOS ABI invalid: " + name)
for name in ("__rt_local_irq_disable __rt_local_irq_enable bt_lock_cnt fm1_r_83b2de623a96 fm1_r_c327ea18da09 cpuSuspend audio_app_worker "
             "edr_name ble_name a2dp_source_event_handler a2dp_source_sbc_codec hfp_profile_init hfp_ag_profile_init spp_user_send_data "
             "avrcp_player_event hid_diy_regiest_callback obex_connection_req pan_cmd_io_ctrl gatt_client_discover_primary_services "
             "ancs_client_init ams_client_init sdio_host_init wl30_sdio_claim_irq wl30_sdio_release_irq wifi_on wifi_off wifi_rx_task "
             "wifi_tx_task ui_yuv420_to_rgb565_pairs ui_tiny_ttf_pack_scanline").split():
    if name in info:
        fail.append("unsupported platform runtime entered the image: " + name)
m = mapfile.read_text(errors="replace")
if "libfm1_engine.a(" not in m:
    fail.append("fixed native SDK archive missing from the link")
for s in ("libfm1_runtime.a(", "/system.a(", "audio_server.a(", "media_app.a(", "btstack.a(", "lib_ui.a(", "libjpeg_decode_only.a("):
    if s in m:
        fail.append("unsupported link input: " + s)
for member in ("a2dp_source ams_client ancs_client avctp avrcp bip bnep bt_audio client_user hfp_ag_profile hfp_profile hid iap_profile "
               "obex opp pan pbg_profile rfcomm spp_profile spp_update_profile spp_user vCard").split():
    if ".a(%s.c.o)" % member in m:
        fail.append("unsupported link member: " + member)
for d in ("tx", "rx"):
    if "acl_%s_pool" % d not in info or "acl_%s_pool_end" % d not in info:
        fail.append("controller pool missing: acl_" + d)
if "_start" not in info or info["_start"][0] != 0x02000120 or info.get("_text_rodata_begin", (0,))[0] != 0x02000120:
    fail.append("startup must be at the fixed ROM entry 0x02000120")
heap = info.get("_HEAP_END", info.get("HEAP_END", (0,)))[0] - info.get("_HEAP_BEGIN", info.get("HEAP_BEGIN", (0,)))[0]
if heap < 49152:
    fail.append("runtime heap too small: %d" % heap)
# the ELF: entry and the allocated sections
b = elf.read_bytes()
if b[:6] != b"\x7fELF\x01\x01":
    fail.append("not ELF32 little endian")
entry = struct.unpack_from("<I", b, 24)[0]
if entry != 0x02000120:
    fail.append("ELF entry %#x" % entry)
shoff, shentsize, shnum, shstrndx = struct.unpack_from("<I", b, 32)[0], struct.unpack_from("<H", b, 46)[0], \
    struct.unpack_from("<H", b, 48)[0], struct.unpack_from("<H", b, 50)[0]
stroff, strsize = struct.unpack_from("<II", b, shoff + shstrndx * shentsize + 16)
image = 0
for i in range(shnum):
    h = shoff + i * shentsize
    nameoff, typ, flags, addr, off, size = struct.unpack_from("<IIIIII", b, h)
    if not (flags & 2) or not size:
        continue
    name = b[stroff + nameoff:b.index(b"\0", stroff + nameoff)].decode()
    if name not in (".text", ".data", ".bss", ".ram0_data", ".ram0_bss", ".boot_info", ".cache_ram_data", ".cache_ram_bss"):
        fail.append("unsupported allocated section: " + name)
    if typ != 8 and name in (".text", ".data", ".ram0_data", ".cache_ram_data"):
        image += size
    print("  %-16s %#010x %8d%s" % (name, addr, size, " (nobits)" if typ == 8 else ""))
print("app image %d bytes, heap %d bytes" % (image, heap))
if fail:
    print("CHECK FAILED:")
    for f in fail:
        print("  " + f)
    sys.exit(1)
print("checks passed")
