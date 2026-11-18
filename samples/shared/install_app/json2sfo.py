#!/usr/bin/env python3
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

# usage: json2sfo.py <param.json> <param.sfo>
# make the param.sfo of a PS4 app from the param.json of a PS5 app

import json
import struct
import sys

STR = 0x204
INT = 0x404

def make_sfo(entries):
    keys = b''
    data = b''
    index = b''

    for key, (fmt, value, size) in sorted(entries.items()):
        if fmt == STR:
            raw = value.encode('utf-8') + b'\0'
        else:
            raw = struct.pack('<I', value)

        if len(raw) > size:
            raise ValueError('%s is longer than %d bytes' % (key, size))

        index += struct.pack('<HHIII', len(keys), fmt, len(raw), size, len(data))
        keys += key.encode('ascii') + b'\0'
        data += raw.ljust(size, b'\0')

    keys = keys.ljust((len(keys) + 3) & ~3, b'\0')
    key_table = 20 + len(index)
    data_table = key_table + len(keys)

    return struct.pack('<4sIIII', b'\0PSF', 0x101, key_table, data_table,
                       len(entries)) + index + keys + data

def main():
    if len(sys.argv) != 3:
        sys.exit('usage: json2sfo.py <param.json> <param.sfo>')

    with open(sys.argv[1], 'r') as f:
        param = json.load(f)

    title_id = param['titleId']
    langs = param['localizedParameters']
    title = langs[langs['defaultLanguage']]['titleName']
    content_id = 'IV0000-%s_00-%s' % (title_id, title_id.ljust(16, '0'))

    entries = {
        'APP_TYPE':           (INT, 1, 4),
        'APP_VER':            (STR, '01.00', 8),
        'ATTRIBUTE':          (INT, 0, 4),
        'CATEGORY':           (STR, 'gd', 4),
        'CONTENT_ID':         (STR, content_id, 48),
        'DOWNLOAD_DATA_SIZE': (INT, 0, 4),
        'FORMAT':             (STR, 'obs', 4),
        'PARENTAL_LEVEL':     (INT, 1, 4),
        'SYSTEM_VER':         (INT, 0, 4),
        'TITLE':              (STR, title, 128),
        'TITLE_ID':           (STR, title_id, 12),
        'VERSION':            (STR, '01.00', 8),
    }

    with open(sys.argv[2], 'wb') as f:
        f.write(make_sfo(entries))

if __name__ == '__main__':
    main()
