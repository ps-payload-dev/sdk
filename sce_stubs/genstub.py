#!/usr/bin/env python3
# encoding: utf-8
# Copyright (C) 2023 John Törnblom
# Copyright (C) 2026 ItsJokerZz
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING. If not see
# <http://www.gnu.org/licenses/>.

import argparse
import logging
import os
import string
import struct
import sys

from pathlib import Path
from elftools.elf.elffile import ELFFile


logger = logging.getLogger('genstub')


tmpl_func = string.Template('''
asm(".global $name\\n"
    ".type $name @function\\n"
    "$name:\\n");''')


tmpl_obj = string.Template('''
asm(".global $name\\n"
    ".type   $name, @object\\n"
    "$name:\\n");''')


# read NIDs from aerolib.csv
NID_DB = (os.path.dirname(__file__) or '.') + '/aerolib.csv'
nid_map = {}
with open(NID_DB) as f:
    for line in f.readlines():
        nid, sym = line.strip().split(' ', 1)
        nid_map[nid] = sym


def sce_symbols(f, elf, sym_type):
    '''
    yield symbol names in PT_SCE_DYNLIBDATA using the NID lookup table
    'aerolib.csv'. Based on:
    https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/scripts/symbol_entries.py
    '''
    dynamic = None
    dynlibdata = None
    for segment in elf.iter_segments():
        if segment.header.p_type == 'PT_DYNAMIC':
            dynamic = segment.header

        elif segment.header.p_type == 0x61000000: # PT_SCE_DYNLIBDATA
            dynlibdata = segment.header

    if not dynamic:
        return

    dyncnt = int(dynamic.p_filesz / struct.calcsize('<QQ'))
    symtab = strtab = dynlibdata.p_offset
    symcnt = 0

    for i in range(dyncnt):
        f.seek(dynamic.p_offset + (i * struct.calcsize('<QQ')))
        d_tag, d_val = struct.unpack('<QQ', f.read(struct.calcsize('<QQ')))

        if d_tag == 0x61000039: # DT_SCE_SYMTAB
            symtab += d_val

        if d_tag == 0x61000035: # DT_SCE_STRTAB
            strtab += d_val

        if d_tag == 0x6100003F: # DT_SCE_SYMTABSZ
            symcnt = int(d_val / struct.calcsize('<IBBHQQ'))

    for i in range(symcnt):
        f.seek(symtab + (i * struct.calcsize('<IBBHQQ')))
        st_name, st_info, st_other, st_shndx, st_value, st_size = \
            struct.unpack('<IBBHQQ', f.read(struct.calcsize('<IBBHQQ')))

        st_bind = st_info >> 4
        st_type = st_info & 0xF

        if st_shndx == 0: # SHN_UNDEF
            continue

        if st_bind == 0: # STB_LOCAL
            continue

        if sym_type == 'STT_OBJECT' and st_type != 1:
            continue

        if sym_type == 'STT_FUNC' and st_type != 2:
            continue

        f.seek(strtab + st_name)
        symname = f.read(32).split(b'\0')[0].decode('ascii')
        nid = symname.split('#')[0]

        if not nid in nid_map:
            logger.warning(f'skipping unknown NID {nid}')
            continue

        yield nid_map[nid]


def elf_symbols(elf, sym_type):
    '''
    yield symbol names in PT_DYNAMIC segments using the NID lookup table
    'aerolib.csv'.
    '''
    for segment in elf.iter_segments():
        if segment.header.p_type != 'PT_DYNAMIC':
            continue

        for sym in segment.iter_symbols():
            if sym_type != sym.entry['st_info']['type']:
                continue

            if sym.entry['st_shndx'] == 'SHN_UNDEF':
                continue

            if not sym.name:
                continue

            nid, lid, mid = sym.name.split('#')
            if not nid in nid_map:
                logger.warning(f'skipping unknown NID {nid}')
                continue

            yield nid_map[nid]


def symbols(sym_type, filename):
    '''
    yield symbol names, PS4 modules keep them in PT_SCE_DYNLIBDATA
    '''
    with open(filename, 'rb') as f:
        elf = ELFFile(f)

        if any(s.header.p_type == 0x61000000 for s in elf.iter_segments()):
            yield from sce_symbols(f, elf, sym_type)
        else:
            yield from elf_symbols(elf, sym_type)


if __name__ == '__main__':
    logging.basicConfig(level=logging.WARNING)

    parser = argparse.ArgumentParser()
    parser.add_argument('SPRX_FILE')
    cli_args = parser.parse_args()

    funcs = sorted(set(symbols('STT_FUNC', cli_args.SPRX_FILE)))
    gvars = sorted(set(symbols('STT_OBJECT', cli_args.SPRX_FILE)))

    for name in sorted(funcs):
        print(tmpl_func.substitute(name=name))

    for name in sorted(gvars):
        print(tmpl_obj.substitute(name=name))

