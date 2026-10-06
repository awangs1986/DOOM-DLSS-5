#!/usr/bin/env python3
"""Extract a deterministic single-player vanilla demo prefix, without replacing files."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('iwad', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--lump', default='DEMO1')
parser.add_argument('--ticks', type=int, default=210)
args = parser.parse_args()
if not 1 <= args.ticks <= 1000000:
    parser.error('--ticks must be between 1 and 1000000')
wad = args.iwad.read_bytes()
if len(wad) < 12:
    parser.error('IWAD header is truncated')
magic, count, directory = struct.unpack_from('<4sII', wad)
if magic != b'IWAD' or count > 1000000 or directory + count * 16 > len(wad):
    parser.error('Invalid IWAD header or directory')
try:
    wanted = args.lump.upper().encode('ascii')
except UnicodeEncodeError:
    parser.error('Lump name must be ASCII')
demo = None
for index in range(count):
    start, size, name = struct.unpack_from('<II8s', wad, directory + index * 16)
    if name.rstrip(b'\0') == wanted:
        if start + size > len(wad):
            parser.error('Demo lump is truncated')
        demo = wad[start:start + size]
        break
if demo is None:
    parser.error('Demo lump not found')
# Version 109 has 13 header bytes and four input bytes per active player/tic.
if len(demo) < 14 or demo[0] != 109 or demo[9:13] != b'\x01\0\0\0':
    parser.error('Requires a vanilla version 109 single-player demo')
position = 13
for tic in range(args.ticks):
    if position >= len(demo) or demo[position] == 0x80 or position + 4 > len(demo):
        parser.error('Requested prefix exceeds the available demo tics')
    position += 4
result = demo[:position] + b'\x80'
with args.output.open('xb') as file:
    file.write(result)
print(json.dumps({
    'iwad': args.iwad.name,
    'iwad_sha256': hashlib.sha256(wad).hexdigest(),
    'lump': args.lump.upper(),
    'original_lump_sha256': hashlib.sha256(demo).hexdigest(),
    'ticks': args.ticks,
    'output': str(args.output),
    'output_sha256': hashlib.sha256(result).hexdigest(),
}, indent=2))
