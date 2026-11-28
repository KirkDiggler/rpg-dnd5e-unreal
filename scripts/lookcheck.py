#!/usr/bin/env python3
"""Check projected scene subjects against their screenshot footprints."""
import re
import sys
from PIL import Image


PIXEL = re.compile(r'\bPIXEL (\S+) (-?\d+) (-?\d+) (-?\d+) (-?\d+) (NATIVE|#[0-9A-Fa-f]{6})\b')
COLOUR = re.compile(r'\bCOLOUR (\S+) #[0-9A-Fa-f]{6}\b')
FIGURE = re.compile(r'\bFIGURE (\S+) (native=\S+|downed=shape|shape)\b')
LOST = re.compile(r'\bLOST (\S+)')


def pixels(image, rect):
    x0, y0, x1, y1 = rect
    if 0 <= x0 <= x1 < image.width and 0 <= y0 <= y1 < image.height:
        data = image.load()
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                yield data[x, y]


def near(pixel, target):
    return all(abs(value - ref) <= 16 for value, ref in zip(pixel, target))


def main():
    if len(sys.argv) != 3:
        print('usage: lookcheck.py <frame.png> <run-log>', file=sys.stderr)
        return 1
    subjects = {}
    members = {}
    with open(sys.argv[2], encoding='utf-8', errors='replace') as log:
        for line in log:
            if 'LogRpgPlay: Display: SHOT ' in line:
                break
            if match := PIXEL.search(line):
                name, *coords, colour = match.groups()
                subjects[name] = tuple(map(int, coords)), colour
            elif match := COLOUR.search(line):
                members[match[1]] = False
            elif match := FIGURE.search(line):
                members[match[1]] = match[2].startswith('native=')
            elif match := LOST.search(line):
                members.pop(match[1], None)

    failed = False
    for member, native in sorted(members.items()):
        if member not in subjects or (subjects[member][1] == 'NATIVE') != native:
            print(f'FAIL palette {member}: no PIXEL')
            failed = True
    if not subjects:
        print('FAIL palette no-subjects: 0')
        return 1
    with Image.open(sys.argv[1]) as frame:
        image = frame.convert('RGB')
        floor = set()
        for name, (rect, colour) in subjects.items():
            if name == 'start' or name.startswith('cell-'):
                target = tuple(int(colour[i:i + 2], 16) for i in (1, 3, 5))
                floor.update(pixel for pixel in pixels(image, rect) if near(pixel, target))
        for name, (rect, colour) in subjects.items():
            if name == 'start':
                continue
            if colour == 'NATIVE':
                count = sum(not any(near(pixel, tone) for tone in floor) for pixel in pixels(image, rect)) if floor else 0
            else:
                target = tuple(int(colour[i:i + 2], 16) for i in (1, 3, 5))
                count = sum(near(pixel, target) for pixel in pixels(image, rect))
            print(f'palette {name} {colour}: {count}')
            if count < 20:
                print(f'FAIL palette {name} {colour}: {count}')
                failed = True
    if not failed:
        print('PASS palette')
    return int(failed)


if __name__ == '__main__':
    sys.exit(main())
