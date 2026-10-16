#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2023 Andrey Grigoryev (kagaimiq), 2026 M-VAVE, 2026 fm1-chord contributors
# A Python port of sdk/Package.cs from m-vave/fm1_open (MIT; the JieLi formats and ciphers from
# jl-misctools): the FM1 Open flash image and its .fwsc update wrapper, byte for byte.
#   fm1_package.py APP_IMAGE.bin RESOURCE_DIR OUT.fwsc PRODUCT
#   APP_IMAGE: the ELF's .text .data .ram0_data .cache_ram_data concatenated; RESOURCE_DIR: sdk/tools/pack
import re
import sys
from pathlib import Path

AREA, ENTRY, APPDATA = 0x4000, 0x02000120, 0x120
FLASH_LIMIT, USER_SIZE, BT_SIZE, MIN_VM = 0xFF000, 536576, 4096, 32768
PUBLIC = 0x980F
TABLE = []
for i in range(256):
    v = i << 8
    for _ in range(8):
        v = ((v << 1) ^ (0 if (v & 0x8000) == 0 else 0x1021)) & 0xFFFF
    TABLE.append(v)


def crc(b, begin, length):
    v = 0
    for i in range(begin, begin + length):
        v = ((v << 8) ^ TABLE[((v >> 8) ^ b[i]) & 0xFF]) & 0xFFFF
    return v


def cipher(b, begin, length, state):
    for i in range(begin, begin + length):
        b[i] ^= state & 0xFF
        state = ((state << 1) ^ (0 if (state & 0x8000) == 0 else 0x1021)) & 0xFFFF


def round_up(size, alignment):
    return (size + alignment - 1) & -alignment


class Image:
    def __init__(self, size, fill):
        self.b = bytearray([fill]) * size

    def number(self, offset, value, width):
        for n in range(width):
            self.b[offset + n] = (value >> (8 * n)) & 0xFF

    def copy(self, offset, data):
        self.b[offset:offset + len(data)] = data

    def text(self, offset, value):
        self.copy(offset, value.encode("ascii"))

    def checksum(self, field, offset, size):
        self.number(field, crc(self.b, offset, size), 2)

    def record(self, offset, name, c, address, size, attributes, last, extra):
        self.b[offset:offset + 32] = bytes(32)
        self.number(offset + 2, c, 2)
        self.number(offset + 4, address, 4)
        self.number(offset + 8, size, 4)
        self.number(offset + 12, attributes, 2)
        self.number(offset + 14, last, 2)
        for n in range(16, 32):
            self.b[offset + n] = 255
        self.text(offset + 16, name)
        self.b[offset + 16 + len(name)] = 0
        if extra is not None:
            self.number(offset + 28, extra, 4)
        self.checksum(offset, offset + 2, 30)


def read_resource(directory, name, size):
    data = (Path(directory) / name).read_bytes()
    if len(data) != size:
        raise ValueError("unexpected FM1 resource: " + name)
    return data


def u16(b, o):
    return b[o] | b[o + 1] << 8


def i32(b, o):
    return int.from_bytes(b[o:o + 4], "little", signed=True)


def encode_boot(plain):
    result = bytearray(plain)
    banks = u16(plain, 0)
    if banks < 1 or banks > len(plain) // 16:
        raise ValueError("invalid boot directory")
    previous_end = banks * 16
    for i in range(banks):
        header = i * 16
        length, start = u16(plain, header + 2), i32(plain, header + 8)
        if (length == 0 or start < previous_end or start > len(plain) - length or
                crc(plain, header, 14) != u16(plain, header + 14) or crc(plain, start, length) != u16(plain, header + 12)):
            raise ValueError("invalid boot bank")
        cipher(result, header, 16, 65535)
        cipher(result, start, length, 65535)
        previous_end = start + length
    return bytes(result)


def build_flash(application, directory, end, vm_end):
    boot = read_resource(directory, "uboot.boot", 14488)
    config = read_resource(directory, "boot-config.bin", 721)
    tool = read_resource(directory, "cfg_tool.bin", 383)
    image, settings = Image(end, 255), Image(34 + len(config), 0)
    for bit in range(16):                                # the jl-misctools chipkeybin descriptor of the public key
        settings.b[bit] = 16
        settings.b[16 + bit] = 16 ^ (255 if ((PUBLIC >> bit) & 1) == 0 else 0)
    settings.checksum(32, 0, 32)
    settings.copy(34, config)
    image.copy(0, bytes([0, 0, 0x20, 2, 0x2f, 0x10, 0x4c, 0xc9, 0, 0xf0, 0x0f, 0, 0x10, 0x10, 0xa4, 0xff,
                         0x4e, 0x5d, 0x0b, 0x41, 0xe0, 0xcd, 0x78, 0x1d, 0xc8, 0x56, 0x41, 0x7f, 0xb3, 0x67, 0xcf, 0x9f]))
    image.checksum(0, 2, 30)
    config_at = 160 + len(boot)
    image.record(32, "uboot.boot", crc(boot, 0, len(boot)), 160, len(boot), 0, 0, None)
    image.record(64, "isd_config.ini", crc(settings.b, 0, len(settings.b)), config_at, len(settings.b), 0x8002, 0, None)
    image.record(96, "app_dir_head", 65535, AREA, 0xFFFFFFFF, 0xff81, 0, None)
    image.record(128, "key_mac", 65535, FLASH_LIMIT, 4096, 0x0112, 65535, None)
    for p in range(0, 160, 32):
        cipher(image.b, p, 32, 65535)
    image.copy(160, encode_boot(boot))
    image.copy(config_at, settings.b)
    payload_size = APPDATA + len(application) + len(tool)
    app = AREA + APPDATA
    image.copy(app, application)
    image.copy(app + len(application), tool)
    image.record(AREA + 32, "app.bin", crc(application, 0, len(application)), APPDATA, len(application), 0xff82, 0, None)
    image.record(AREA + 64, "cfg_tool.bin", crc(tool, 0, len(tool)), APPDATA + len(application), len(tool), 0xff82, 0, None)
    image.record(AREA + 96, "VM", 65535, end, vm_end - end, 0x8112, 0, MIN_VM)
    image.record(AREA + 128, "PRCT", 65535, 0, end, 0x8292, 0, 0x45444f43)
    image.record(AREA + 160, "BTIF", 65535, vm_end, BT_SIZE, 0x8192, 1, 0x4f545541)
    image.record(AREA, "app_area_head", crc(image.b, AREA + 32, payload_size - 32), ENTRY, payload_size, 0xff83, 0, None)
    reserved = AREA + payload_size
    image.record(reserved + 32, "USER", 65535, FLASH_LIMIT - USER_SIZE, USER_SIZE, 0x8192, 1, 0x4f545541)
    image.record(reserved, "EXT_RESERVED", crc(image.b, reserved + 32, 32), 32, 64, 0xff93, 1, None)
    for p in range(0, payload_size + 64, 32):
        cipher(image.b, AREA + p, min(32, payload_size + 64 - p), PUBLIC ^ (p >> 2))
    return image


def wrap(flash, loader, product):
    if len(loader) < 1 or len(loader) >= 32768:
        raise ValueError("invalid FM1 update loader")
    loader_at = 1024 + round_up(len(flash.b), 32)
    total = loader_at + round_up(len(loader), 32)
    ufw = Image(total, 255)
    ufw.b[0:224] = bytes(224)
    files, names = (bytes(flash.b), loader), ("flash.bin", "ota.bin")
    for i in range(2):
        record = 64 + i * 80
        ufw.number(record, 0 if i == 0 else 100, 2)
        ufw.number(record + 2, i, 2)
        ufw.number(record + 4, crc(files[i], 0, len(files[i])), 2)
        ufw.number(record + 8, 1024 if i == 0 else loader_at, 4)
        ufw.number(record + 12, len(files[i]), 4)
        ufw.number(record + 16, round_up(len(files[i]), 32), 4)
        ufw.text(record + 64, names[i])
        cipher(ufw.b, record, 80, 65535)
    ufw.number(4, total, 4)
    ufw.number(8, 2, 2)
    ufw.number(10, 4, 2)
    ufw.number(12, 512, 4)
    ufw.text(16, "AC791N")
    ufw.checksum(2, 64, 160)
    ufw.checksum(0, 2, 62)
    cipher(ufw.b, 0, 64, 65535)
    ufw.copy(1024, files[0])
    ufw.copy(loader_at, loader)
    output = bytearray(total + 20)
    destination = 0
    for source in range(total):
        output[destination] = ufw.b[source]
        if source < 940 and (source + 1) % 47 == 0:
            index = source // 47
            destination += 1
            output[destination] = (ord(product[index]) + index + 1) & 0xFF if index < len(product) else 125
        destination += 1
    return bytes(output)


def create(app, resources, output, product):
    if not re.fullmatch(r"FM-1_8[0-9]{6}", product):
        raise ValueError("invalid FM1 update identity")
    if not app or len(app) > FLASH_LIMIT - USER_SIZE - BT_SIZE - MIN_VM - AREA - APPDATA - 383 - 64:
        raise ValueError("application does not fit the FM1 layout")
    image_size = round_up(AREA + APPDATA + len(app) + 383 + 64, 4096)
    user_at = FLASH_LIMIT - USER_SIZE
    vm_end = user_at - BT_SIZE
    if image_size > vm_end - MIN_VM:
        raise ValueError("insufficient FM1 settings space")
    image = build_flash(app, resources, image_size, vm_end)
    package = wrap(image, (Path(resources) / "ota.bin").read_bytes(), product)
    Path(output).write_bytes(package)
    print("package %s: app %d B, flash image %d B, VM %d B, user %d B, %d B" %
          (output, len(app), image_size, vm_end - image_size, USER_SIZE, len(package)))


if __name__ == "__main__":
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    create(Path(sys.argv[1]).read_bytes(), sys.argv[2], sys.argv[3], sys.argv[4])
