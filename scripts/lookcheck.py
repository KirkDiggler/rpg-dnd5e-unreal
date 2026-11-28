#!/usr/bin/env python3
"""Check projected scene subjects against their screenshot footprints."""
import re
import sys
from PIL import Image


def main():
    if len(sys.argv) != 3:
        print('usage: lookcheck.py <frame.png> <run-log>', file=sys.stderr)
        return 1
    pattern = re.compile(r'\bPIXEL (\S+) (-?\d+) (-?\d+) (-?\d+) (-?\d+) (#[0-9A-Fa-f]{6})\b')
    colour_pattern = re.compile(r'\bCOLOUR (\S+) #[0-9A-Fa-f]{6}\b')
    with open(sys.argv[2], encoding='utf-8', errors='replace') as log:
        lines = list(log)
    subjects = [m.groups() for line in lines if (m := pattern.search(line))]
    members = {m.group(1) for line in lines if (m := colour_pattern.search(line))}
    missing = members - {subject[0] for subject in subjects}
    for member in sorted(missing):
        print(f'FAIL palette {member}: no PIXEL')
    failed = bool(missing)
    if not subjects:
        print('FAIL palette no-subjects: 0')
        return 1
    with Image.open(sys.argv[1]) as frame:
        image = frame.convert('RGB')
        for name, x0, y0, x1, y1, colour in subjects:
            if name == 'start':
                continue
            x0, y0, x1, y1 = map(int, (x0, y0, x1, y1))
            target = tuple(int(colour[i:i + 2], 16) for i in (1, 3, 5))
            count = 0
            if 0 <= x0 <= x1 < image.width and 0 <= y0 <= y1 < image.height:
                for y in range(y0, y1 + 1):
                    for x in range(x0, x1 + 1):
                        pixel = image.getpixel((x, y))
                        count += all(abs(pixel[i] - target[i]) <= 16 for i in range(3))
            print(f'palette {name} {colour}: {count}')
            if count < 20:
                print(f'FAIL palette {name} {colour}: {count}')
                failed = True
    if not failed:
        print('PASS palette')
    return int(failed)


if __name__ == '__main__':
    sys.exit(main())
